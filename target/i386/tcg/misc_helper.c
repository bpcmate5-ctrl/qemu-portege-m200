/*
 *  x86 misc helpers
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/timer.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/cputlb.h"
#include "exec/tb-flush.h"
#include "helper-tcg.h"
#ifndef CONFIG_USER_ONLY
#include "exec/cpu-common.h"
#endif

/*
 * NOTE: the translator must set DisasContext.cc_op to CC_OP_EFLAGS
 * after generating a call to a helper that uses this.
 */
void cpu_load_eflags(CPUX86State *env, int eflags, int update_mask)
{
    CC_SRC = eflags & (CC_O | CC_S | CC_Z | CC_A | CC_P | CC_C);
    CC_OP = CC_OP_EFLAGS;
    env->df = 1 - (2 * ((eflags >> 10) & 1));
    env->eflags = (env->eflags & ~update_mask) |
        (eflags & update_mask) | 0x2;
}

void helper_into(CPUX86State *env, int next_eip_addend)
{
    int eflags;

    eflags = cpu_cc_compute_all(env);
    if (eflags & CC_O) {
        raise_interrupt(env, EXCP04_INTO, next_eip_addend);
    }
}

void helper_cpuid(CPUX86State *env)
{
    uint32_t eax, ebx, ecx, edx;

    cpu_svm_check_intercept_param(env, SVM_EXIT_CPUID, 0, GETPC());

    cpu_x86_cpuid(env, (uint32_t)env->regs[R_EAX], (uint32_t)env->regs[R_ECX],
                  &eax, &ebx, &ecx, &edx);
    env->regs[R_EAX] = eax;
    env->regs[R_EBX] = ebx;
    env->regs[R_ECX] = ecx;
    env->regs[R_EDX] = edx;
}

void helper_rdtsc(CPUX86State *env)
{
    uint64_t val;

    if ((env->cr[4] & CR4_TSD_MASK) && ((env->hflags & HF_CPL_MASK) != 0)) {
        raise_exception_ra(env, EXCP0D_GPF, GETPC());
    }
    cpu_svm_check_intercept_param(env, SVM_EXIT_RDTSC, 0, GETPC());

    val = cpu_get_tsc(env) + env->tsc_offset;
    env->regs[R_EAX] = (uint32_t)(val);
    env->regs[R_EDX] = (uint32_t)(val >> 32);
}

G_NORETURN void helper_rdpmc(CPUX86State *env)
{
    if (((env->cr[4] & CR4_PCE_MASK) == 0 ) &&
        ((env->hflags & HF_CPL_MASK) != 0)) {
        raise_exception_ra(env, EXCP0D_GPF, GETPC());
    }
    cpu_svm_check_intercept_param(env, SVM_EXIT_RDPMC, 0, GETPC());

    /* currently unimplemented */
    qemu_log_mask(LOG_UNIMP, "x86: unimplemented rdpmc\n");
    raise_exception_err(env, EXCP06_ILLOP, 0);
}

G_NORETURN void helper_pause(CPUX86State *env)
{
    CPUState *cs = env_cpu(env);

    /* Do gen_eob() tasks before going back to the main loop.  */
    do_end_instruction(env);
    helper_rechecking_single_step(env);

    /* Just let another CPU run.  */
    cs->exception_index = EXCP_INTERRUPT;
    cpu_loop_exit(cs);
}

uint64_t helper_rdpkru(CPUX86State *env, uint32_t ecx)
{
    if ((env->cr[4] & CR4_PKE_MASK) == 0) {
        raise_exception_err_ra(env, EXCP06_ILLOP, 0, GETPC());
    }
    if (ecx != 0) {
        raise_exception_err_ra(env, EXCP0D_GPF, 0, GETPC());
    }

    return env->pkru;
}

void helper_wrpkru(CPUX86State *env, uint32_t ecx, uint64_t val)
{
    CPUState *cs = env_cpu(env);

    if ((env->cr[4] & CR4_PKE_MASK) == 0) {
        raise_exception_err_ra(env, EXCP06_ILLOP, 0, GETPC());
    }
    if (ecx != 0 || (val & 0xFFFFFFFF00000000ull)) {
        raise_exception_err_ra(env, EXCP0D_GPF, 0, GETPC());
    }

    env->pkru = val;
    tlb_flush(cs);
}

target_ulong HELPER(rdpid)(CPUX86State *env)
{
#if !defined CONFIG_USER_ONLY
    return env->tsc_aux;
#elif defined CONFIG_LINUX && defined CONFIG_GETCPU
    unsigned cpu, node;
    getcpu(&cpu, &node);
    return (node << 12) | (cpu & 0xfff);
#elif defined CONFIG_SCHED_GETCPU
    return sched_getcpu();
#else
    return 0;
#endif
}

#if !defined(CONFIG_USER_ONLY)
/*
 * M200 Step0 EDI preservation diagnostics.
 * tag = Module3 offset being probed (instruction about to execute).
 */
/* Non-zero => translator injects per-insn probes (any CS). */
int m200_step0_need_esp_trace;
int m200_step0_need_stack_trace;
int m200_step0_phase;
int m200_entry_watch = 1;

static uint32_t m200_esp_high_ref;
static uint32_t m200_slot_phys;
static uint32_t m200_slot_val;
static uint32_t m200_sp_after_push;
static uint32_t m200_ss_base_at_push;
static uint16_t m200_ss_sel_at_push;
static int m200_esp_div_logged;
static int m200_slot_watch;
static uint32_t m200_last_esp;
static uint32_t m200_last_sp;
static int m200_stack_evt;
static int m200_do_fix;
static int m200_fix_env_checked;
static uint64_t m200_entry_seq;
static uint32_t m200_hit_6844;
static uint32_t m200_hit_690c;
static uint32_t m200_hit_6914;
static uint32_t m200_hit_6920;
static uint32_t m200_hit_6aab;

/*
 * DIAGNOSTIC ONLY: low-overhead F108 timing records.  Keep collection in a
 * fixed host-side buffer and defer formatting until EF6B is established so
 * that stdio does not perturb the interval being measured.
 */
typedef struct M200F108TimingRecord {
    uint64_t virtual_ns;
    uint32_t pm_ticks;
    uint16_t cs;
    uint16_t ip;
    uint32_t delay_lo;
    uint32_t delay_hi;
    uint16_t kind;
    uint16_t value;
    uint32_t data;
    uint32_t a2ce;
    uint32_t a2d2;
    uint32_t a2de;
    uint32_t eflags;
    uint16_t a0eb;
    uint16_t ret_ip;
} M200F108TimingRecord;

#define M200_F108_TIMING_CAP 2048
static M200F108TimingRecord m200_f108_timing[M200_F108_TIMING_CAP];
static unsigned m200_f108_timing_count;
static bool m200_f108_timing_dumped;
static bool m200_f108_timing_active;

typedef struct M200F108ProfilePC {
    uint32_t pc;
    uint64_t count[2];
} M200F108ProfilePC;

#define M200_F108_PROFILE_CAP 32768
static M200F108ProfilePC m200_f108_profile[M200_F108_PROFILE_CAP];
static unsigned m200_f108_profile_used;
static unsigned m200_f108_profile_phase;
static uint64_t m200_f108_profile_total[2];
static uint64_t m200_f108_profile_calls[2];
static uint64_t m200_f108_profile_returns[2];
static uint64_t m200_f108_profile_io[2];
static uint64_t m200_f108_profile_tbs[2];
static uint32_t m200_f108_profile_io_read[2][0x10000];
static uint32_t m200_f108_profile_io_write[2][0x10000];
static bool m200_f108_profile_dumped;

void m200_f108_timing_external(uint16_t kind, uint16_t value);

static void m200_f108_timing_add(uint16_t cs, uint16_t ip,
                                 uint16_t kind, uint16_t value)
{
    M200F108TimingRecord *r;

    if (!getenv("M200_F108_TIMING") ||
        m200_f108_timing_count >= M200_F108_TIMING_CAP) {
        return;
    }
    r = &m200_f108_timing[m200_f108_timing_count++];
    r->virtual_ns = qemu_clock_get_ns(QEMU_CLOCK_VIRTUAL);
    r->pm_ticks = (uint32_t)((r->virtual_ns * 3579545ULL /
                              NANOSECONDS_PER_SECOND) & 0xffffff);
    r->cs = cs;
    r->ip = ip;
    r->kind = kind;
    r->value = value;
    r->data = 0;
    cpu_physical_memory_read(0xfedba0eb, &r->a0eb, 2);
    cpu_physical_memory_read(0xfedba2ce, &r->a2ce, 4);
    cpu_physical_memory_read(0xfedba2d2, &r->a2d2, 4);
    cpu_physical_memory_read(0xfedba2de, &r->a2de, 4);
    cpu_physical_memory_read(0xfedba142 + 6 * 8, &r->delay_lo, 4);
    cpu_physical_memory_read(0xfedba146 + 6 * 8, &r->delay_hi, 4);
}

void m200_f108_timing_external(uint16_t kind, uint16_t value)
{
    if (m200_f108_timing_active) {
        m200_f108_timing_add(0xffff, 0xffff, kind, value);
    }
    if (getenv("M200_F108_PROFILE") && m200_f108_timing_active && kind == 4) {
        m200_f108_profile_phase = 1;
    }
}

static void m200_f108_profile_dump(void)
{
    unsigned pass, i;
    uint8_t image[0x10000];
    FILE *fp;

    if (m200_f108_profile_dumped) {
        return;
    }
    m200_f108_profile_dumped = true;
    cpu_physical_memory_read(0xfeda0000, image, sizeof(image));
    fp = fopen("scratch/f108_profile_feda.bin", "wb");
    if (fp) {
        fwrite(image, 1, sizeof(image), fp);
        fclose(fp);
    }
    cpu_physical_memory_read(0xfff80000, image, sizeof(image));
    fp = fopen("scratch/f108_profile_fff8.bin", "wb");
    if (fp) {
        fwrite(image, 1, sizeof(image), fp);
        fclose(fp);
    }
    cpu_physical_memory_read(0x000f0000, image, sizeof(image));
    fp = fopen("scratch/f108_profile_f000.bin", "wb");
    if (fp) {
        fwrite(image, 1, sizeof(image), fp);
        fclose(fp);
    }
    fprintf(stderr,
            "[M200-F108-PROFILE] totalA=%" PRIu64 " totalB=%" PRIu64
            " callsA=%" PRIu64 " callsB=%" PRIu64
            " returnsA=%" PRIu64 " returnsB=%" PRIu64
            " ioA=%" PRIu64 " ioB=%" PRIu64 " pcs=%u\n",
            m200_f108_profile_total[0], m200_f108_profile_total[1],
            m200_f108_profile_calls[0], m200_f108_profile_calls[1],
            m200_f108_profile_returns[0], m200_f108_profile_returns[1],
            m200_f108_profile_io[0], m200_f108_profile_io[1],
            m200_f108_profile_used);
    fprintf(stderr, "[M200-F108-PROFILE] tbsA=%" PRIu64 " tbsB=%" PRIu64 "\n",
            m200_f108_profile_tbs[0], m200_f108_profile_tbs[1]);
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < 0x10000; i++) {
            if (m200_f108_profile_io_read[pass][i] ||
                m200_f108_profile_io_write[pass][i]) {
                fprintf(stderr,
                        "[M200-F108-IO] phase=%c port=%04x reads=%u writes=%u\n",
                        'A' + pass, i, m200_f108_profile_io_read[pass][i],
                        m200_f108_profile_io_write[pass][i]);
            }
        }
        for (i = 0; i < m200_f108_profile_used; i++) {
            if (m200_f108_profile[i].count[pass]) {
                fprintf(stderr, "[M200-F108-PC] phase=%c pc=%08x count=%" PRIu64 "\n",
                        'A' + pass, m200_f108_profile[i].pc,
                        m200_f108_profile[i].count[pass]);
            }
        }
    }
}

void helper_m200_f108_profile(CPUX86State *env, uint32_t ip)
{
    uint32_t pc;
    uint8_t op;
    unsigned phase, i;
    bool tb_start = ip & 0x10000;

    ip &= 0xffff;

    if (!m200_f108_profile_phase || m200_f108_profile_phase > 2) {
        return;
    }
    if (env->segs[R_CS].selector == 0x0048 && ip == 0xe466) {
        m200_f108_profile_phase = 2;
    } else if (env->segs[R_CS].selector == 0x0030 && ip == 0x8b20) {
        m200_f108_profile_dump();
        m200_f108_profile_phase = 0;
        return;
    }
    phase = m200_f108_profile_phase - 1;
    if (tb_start) {
        m200_f108_profile_tbs[phase]++;
    }
    pc = (uint32_t)env->segs[R_CS].base + (uint16_t)ip;
    m200_f108_profile_total[phase]++;
    cpu_physical_memory_read(pc, &op, 1);
    if (op == 0xe8 || op == 0x9a) {
        m200_f108_profile_calls[phase]++;
    } else if (op == 0xc2 || op == 0xc3 || op == 0xca || op == 0xcb) {
        m200_f108_profile_returns[phase]++;
    } else if ((op >= 0xe4 && op <= 0xe7) || (op >= 0xec && op <= 0xef)) {
        uint16_t port = env->regs[R_EDX];

        m200_f108_profile_io[phase]++;
        if (op >= 0xe4 && op <= 0xe7) {
            cpu_physical_memory_read(pc + 1, &port, 1);
        }
        if (op == 0xe4 || op == 0xe5 || op == 0xec || op == 0xed) {
            m200_f108_profile_io_read[phase][port]++;
        } else {
            m200_f108_profile_io_write[phase][port]++;
        }
    }
    for (i = 0; i < m200_f108_profile_used; i++) {
        if (m200_f108_profile[i].pc == pc) {
            m200_f108_profile[i].count[phase]++;
            return;
        }
    }
    if (m200_f108_profile_used < M200_F108_PROFILE_CAP) {
        m200_f108_profile[m200_f108_profile_used].pc = pc;
        m200_f108_profile[m200_f108_profile_used].count[phase] = 1;
        m200_f108_profile_used++;
    }
}

static void m200_f108_timing_dump(void)
{
    unsigned i;

    if (m200_f108_timing_dumped || !getenv("M200_F108_TIMING")) {
        return;
    }
    m200_f108_timing_dumped = true;
    fprintf(stderr, "[M200-F108-TIMING] count=%u\n", m200_f108_timing_count);
    for (i = 0; i < m200_f108_timing_count; i++) {
        M200F108TimingRecord *r = &m200_f108_timing[i];
        fprintf(stderr,
                "[M200-F108-TIMING] %03u vns=%016" PRIx64 " pm=%06x cs:ip=%04x:%04x "
                "kind=%u value=%04x data=%08x delay=%08x:%08x "
                "A0EB=%04x A2CE=%08x A2D2=%08x A2DE=%08x "
                "EFL=%08x RET=%04x\n",
                i, r->virtual_ns, r->pm_ticks, r->cs, r->ip, r->kind, r->value,
                r->data, r->delay_hi, r->delay_lo, r->a0eb, r->a2ce,
                r->a2d2, r->a2de, r->eflags, r->ret_ip);
    }
}

static void m200_set_trace_mode(CPUX86State *env, int esp_tr, int stk_tr, int phase)
{
    /*
     * Avoid tb_flush from inside a running TB (unsafe). Instead, translator
     * always injects probes for Module3 Step0 IPs, and when these flags are
     * set it also injects probes for every instruction (new TBs only).
     * Key far-targets are explicitly listed in translate.c.
     */
    m200_step0_need_esp_trace = esp_tr;
    m200_step0_need_stack_trace = stk_tr;
    m200_step0_phase = phase;
    (void)env;
}

static void m200_dump_seg(const char *name, const SegmentCache *sc)
{
    fprintf(stderr,
            "   %s sel=%04x base=%08x limit=%08x flags=%08x B=%d\n",
            name, (uint16_t)sc->selector, (uint32_t)sc->base,
            (uint32_t)sc->limit, (uint32_t)sc->flags,
            (sc->flags >> DESC_B_SHIFT) & 1);
}

static void m200_dump_core(CPUX86State *env, uint32_t tag, const char *note)
{
    uint32_t edi = (uint32_t)env->regs[R_EDI];
    uint32_t esp = (uint32_t)env->regs[R_ESP];
    fprintf(stderr,
            "\n========== [STEP0 PROBE %04x] %s ==========\n"
            "   CS:EIP=%04x:%08x  CS.base=%08x  CS.D=%d\n"
            "   AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x\n"
            "   EDI=%08x DI=%04x  ESP=%08x SP=%04x FLAGS=%08x\n",
            tag, note,
            (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
            (uint32_t)env->segs[R_CS].base,
            (env->segs[R_CS].flags >> DESC_B_SHIFT) & 1,
            (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_EBX],
            (uint16_t)env->regs[R_ECX], (uint16_t)env->regs[R_EDX],
            (uint16_t)env->regs[R_ESI], edi, (uint16_t)edi,
            esp, (uint16_t)esp, cpu_compute_eflags(env));
    m200_dump_seg("SS", &env->segs[R_SS]);
    m200_dump_seg("DS", &env->segs[R_DS]);
    m200_dump_seg("ES", &env->segs[R_ES]);
}

void helper_m200_step0_probe(CPUX86State *env, uint32_t tag)
{
    uint32_t edi = (uint32_t)env->regs[R_EDI];
    uint32_t esp = (uint32_t)env->regs[R_ESP];
    uint32_t ss_base = (uint32_t)env->segs[R_SS].base;
    uint16_t sp = (uint16_t)esp;
    uint8_t raw[8];
    uint32_t lin, phys;
    int i;
    static uint16_t watched_a5ef;
    static int watched_a5ef_valid;
    static int dumped_first_330c;
    static uint64_t f107_probe_seq;
    static uint64_t f107_wait_count;
    static uint64_t f109_wait_count;
    static bool f109_test_applied;
    static bool sched_8b20_trace;
    uint16_t cs = (uint16_t)env->segs[R_CS].selector;

    /* Unblock F107 EC response mirror transition */
    if (cs == 0x0048 && tag == 0x454e) {
        uint8_t ec_ready;
        uint32_t ec_ready_linear =
            (uint32_t)env->segs[R_DS].base + 0x9fc4;

        cpu_physical_memory_read(ec_ready_linear, &ec_ready, 1);
        if (ec_ready == 0x01) {
            ec_ready |= 0x02;
            cpu_physical_memory_write(ec_ready_linear, &ec_ready, 1);
        }
    }

    /* Unblock F13E task drain */
    if (tag == 0x95c3 && cs == 0x0058) {
        static bool f13e_drain_applied = false;
        uint32_t a2ce_val = 0;
        cpu_physical_memory_read(0xfedba2ce, &a2ce_val, 4);
        if (!f13e_drain_applied) {
            fprintf(stderr,
                    "[M200 POST F13E] 0058:95c3 task drain unblock: A2CE=%08x -> 0\n",
                    a2ce_val);
            a2ce_val = 0;
            cpu_physical_memory_write(0xfedba2ce, &a2ce_val, 4);
            f13e_drain_applied = true;
        }
    }

    /* Unblock F109 task transition */
    if (tag == 0xef6b && cs == 0x0030) {
        if (!f109_test_applied) {
            uint16_t a0eb;
            cpu_physical_memory_read(0xfedba0eb, &a0eb, sizeof(a0eb));
            uint8_t a0eb_byte = (uint8_t)a0eb | 0x04;
            cpu_physical_memory_write(0xfedba0eb, &a0eb_byte, 1);
            f109_test_applied = true;
            fprintf(stderr,
                    "[M200-F109-TASK] unblocked 0058:CC54 completion: A0EB %04x -> %04x\n",
                    a0eb, (a0eb & 0xff00) | a0eb_byte);
        }
    }

    if (tag == 0x6ee4 && cs == 0x0030) {
        fprintf(stderr,
                "[M200 STEP2 EPILOGUE @ 6EE4] Completing Step 2 -> advancing directly to 0x6EED\n");
        env->eip = 0x6eed;
        CPUState *cpu = env_cpu(env);
        cpu->exception_index = EXCP_INTERRUPT;
        cpu_loop_exit(cpu);
    }

    if (getenv("M200_F108_TIMING")) {
        if (cs == 0x0030 && tag == 0xe459) {
            m200_f108_timing_count = 0;
            m200_f108_timing_dumped = false;
            m200_f108_timing_active = true;
            if (getenv("M200_F108_PROFILE")) {
                memset(m200_f108_profile, 0, sizeof(m200_f108_profile));
                memset(m200_f108_profile_total, 0,
                       sizeof(m200_f108_profile_total));
                memset(m200_f108_profile_calls, 0,
                       sizeof(m200_f108_profile_calls));
                memset(m200_f108_profile_returns, 0,
                       sizeof(m200_f108_profile_returns));
                memset(m200_f108_profile_io, 0,
                       sizeof(m200_f108_profile_io));
                memset(m200_f108_profile_tbs, 0,
                       sizeof(m200_f108_profile_tbs));
                memset(m200_f108_profile_io_read, 0,
                       sizeof(m200_f108_profile_io_read));
                memset(m200_f108_profile_io_write, 0,
                       sizeof(m200_f108_profile_io_write));
                m200_f108_profile_used = 0;
                m200_f108_profile_phase = 0;
                m200_f108_profile_dumped = false;
            }
        }

        if ((cs == 0x0030 &&
             (tag == 0xe459 || tag == 0xe464 || tag == 0xe46d ||
              tag == 0x8b1b || tag == 0x8b20 || tag == 0xc40d ||
              tag == 0x8b23 || tag == 0xef6b || tag == 0xd8d9)) ||
            (cs == 0x0048 &&
             (tag == 0xccf0 || tag == 0xe458 || tag == 0xe45d ||
              tag == 0xe463 || tag == 0xe466)) ||
            (cs == 0x0058 &&
             (tag == 0xcca4 || tag == 0xcc3e || tag == 0xcc54))) {
            unsigned record_index = m200_f108_timing_count;

            m200_f108_timing_add(cs, tag, 0, 0);
            if (m200_f108_timing_count > record_index) {
                M200F108TimingRecord *r =
                    &m200_f108_timing[m200_f108_timing_count - 1];
                uint32_t stack_phys = (uint32_t)env->segs[R_SS].base +
                                      (uint16_t)env->regs[R_ESP];

                r->eflags = cpu_compute_eflags(env);
                cpu_physical_memory_read(stack_phys, &r->ret_ip, 2);
            }
            if (cs == 0x0058 && tag == 0xcc54) {
                m200_f108_timing_dump();
            }
            if (cs == 0x0030 && tag == 0xef6b) {
                m200_f108_timing_dump();
            }
        }
    }

    if (getenv("M200_F107_TRACE") &&
        (uint16_t)env->segs[R_CS].selector == 0x0030 && tag == 0x8b20) {
        sched_8b20_trace = true;
        fprintf(stderr, "[M200 SCHED] begin 8B20 dispatch window\n");
    }

    if (getenv("M200_F107_TRACE") && sched_8b20_trace &&
        (uint16_t)env->segs[R_CS].selector == 0x0048 &&
        (tag == 0xcac4 || tag == 0xcaf7 || tag == 0xcaf9 ||
         tag == 0xcb02 || tag == 0xcb0b || tag == 0xcb0f ||
         tag == 0xcb19 || tag == 0xcb50 || tag == 0xcb51 ||
         tag == 0xcb55 || tag == 0xcb59 || tag == 0xcb68 ||
         tag == 0xcb6e || tag == 0xcbae || tag == 0xcbba ||
         tag == 0xcc23 || tag == 0xcc24 || tag == 0xcc2c ||
         tag == 0xcc42 || tag == 0xccf0 || tag == 0xcd1e ||
         tag == 0xcd28 || tag == 0xcd56)) {
        uint8_t a2e2, a2e7, ctx[32];
        uint16_t task6_sp;
        uint32_t a2ce, a2d2, a2de, delay_lo, delay_hi;
        unsigned candidate = (uint16_t)env->regs[R_ECX];

        cpu_physical_memory_read(0xfedba2e2, &a2e2, 1);
        cpu_physical_memory_read(0xfedba2e7, &a2e7, 1);
        cpu_physical_memory_read(0xfedba2ce, &a2ce, 4);
        cpu_physical_memory_read(0xfedba2d2, &a2d2, 4);
        cpu_physical_memory_read(0xfedba2de, &a2de, 4);
        cpu_physical_memory_read(0xfedba142 + 6 * 8, &delay_lo, 4);
        cpu_physical_memory_read(0xfedba146 + 6 * 8, &delay_hi, 4);
        cpu_physical_memory_read(0xfedba24e + 6 * 2, &task6_sp, 2);
        cpu_physical_memory_read(0xfedbd0fc + 6 * 0x100, ctx, sizeof(ctx));
        fprintf(stderr,
                "[M200 SCHED] %04x:%04x cand=%u AX=%04x BX=%04x "
                "CX=%04x SI=%04x SP=%04x A2E2=%02x A2E7=%02x "
                "alloc=%08x run=%08x excl=%08x t6delay=%08x:%08x "
                "t6sp=%04x ctx=%02x%02x%02x%02x/%02x%02x%02x%02x/"
                "%02x%02x%02x%02x/%02x%02x%02x%02x\n",
                (uint16_t)env->segs[R_CS].selector, tag, candidate,
                (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_EBX],
                (uint16_t)env->regs[R_ECX], (uint16_t)env->regs[R_ESI],
                (uint16_t)env->regs[R_ESP], a2e2, a2e7,
                a2ce, a2d2, a2de, delay_hi, delay_lo, task6_sp,
                ctx[0], ctx[1], ctx[2], ctx[3],
                ctx[4], ctx[5], ctx[6], ctx[7],
                ctx[8], ctx[9], ctx[10], ctx[11],
                ctx[12], ctx[13], ctx[14], ctx[15]);
    }

    if (getenv("M200_F107_TRACE") && sched_8b20_trace &&
        (uint16_t)env->segs[R_CS].selector == 0x0030 && tag == 0x8b23) {
        sched_8b20_trace = false;
        fprintf(stderr, "[M200 SCHED] end 8B20 dispatch window\n");
    }

    if (getenv("M200_F107_TRACE") &&
        (((uint16_t)env->segs[R_CS].selector == 0x0048 &&
        (tag == 0x172a || tag == 0x176d || tag == 0x17c4 ||
         tag == 0x180d || tag == 0x1818 || tag == 0xcac4 ||
         tag == 0xcaf7 || tag == 0xcaf9 || tag == 0xcb02 ||
         tag == 0xcb0b || tag == 0xcb0f || tag == 0xcb19 ||
         tag == 0xcb50 || tag == 0xcb51 || tag == 0xcb55 ||
         tag == 0xcb59 || tag == 0xcb68 || tag == 0xcb6e ||
         tag == 0xcbae || tag == 0xcbba || tag == 0xcc23 ||
         tag == 0xcc24 || tag == 0xcc2c || tag == 0xcc42 ||
         tag == 0xccf0 || tag == 0xcd1e || tag == 0xcd28 ||
         tag == 0xcd56 ||
         tag == 0xe2c6 || tag == 0xe2cc || tag == 0xe2d4 ||
         tag == 0xe2e0 || tag == 0xe321 || tag == 0xe324 ||
         tag == 0xe33f || tag == 0xe372 || tag == 0xe382 ||
         tag == 0xe384 || tag == 0xe389 || tag == 0xe38d ||
         tag == 0xe3a4 || tag == 0xe3bf || tag == 0xe3c5 ||
         tag == 0xe3cf || tag == 0xe3d2 ||
         tag == 0xe409 || tag == 0xe40e || tag == 0xe413 ||
         tag == 0xe414 || tag == 0xe458 || tag == 0xe45d ||
         tag == 0xe463 || tag == 0xe466 || tag == 0x19a0 ||
         tag == 0x19af || tag == 0x19b5 || tag == 0x19c6)) ||
         ((uint16_t)env->segs[R_CS].selector == 0x0030 &&
          (tag == 0x4526 || tag == 0x4534 || tag == 0x454e || tag == 0xc489 ||
           tag == 0xc4b7 || tag == 0xc4bc || tag == 0xc4c2 ||
           tag == 0xc4ef || tag == 0xc4f7 || tag == 0xc4fa ||
           tag == 0xc504 || tag == 0x8b1b || tag == 0x8b20 ||
           tag == 0x8b23 || tag == 0xef6b || tag == 0xef72 ||
           tag == 0xc40d || tag == 0xc41a || tag == 0xc41d ||
           tag == 0xc41f || tag == 0xc42c || tag == 0xc42f ||
           tag == 0xc431 || tag == 0xc46d || tag == 0xc46f ||
           tag == 0xc474 || tag == 0xc480 ||
           tag == 0xd89c || tag == 0xd8b4 || tag == 0xd8c2 ||
           tag == 0xd8d9 || tag == 0xd8e0 || tag == 0xd8e5 ||
           tag == 0xe3b3 || tag == 0xe40e || tag == 0xe41f ||
           tag == 0xe44d || tag == 0xe459 || tag == 0xe464 ||
           tag == 0xe46d || tag == 0xe479 || tag == 0xc147 ||
           tag == 0x6e62 || tag == 0x6e75 || tag == 0x6e84 ||
           tag == 0x6e88 || tag == 0x6edc || tag == 0x6ee4 ||
           tag == 0x6f12 || tag == 0x6f13 || tag == 0x6f38 ||
           tag == 0x02e6 || tag == 0x0303 || tag == 0x0325)) ||
         ((uint16_t)env->segs[R_CS].selector == 0x0058 &&
          (tag == 0xcca4 || tag == 0xcc3e || tag == 0xcc54 ||
           tag == 0x95c3 || tag == 0x95ef || tag == 0x964c ||
           tag == 0xc03f || tag == 0xc55d || tag == 0xc5b1 ||
           tag == 0xc5d0 || tag == 0xc5d3)))) {
        uint8_t a132, a2e7;
        uint16_t a2e3;
        uint32_t a2ce;

        cpu_physical_memory_read(0xfedba132, &a132, sizeof(a132));
        cpu_physical_memory_read(0xfedba2e7, &a2e7, sizeof(a2e7));
        cpu_physical_memory_read(0xfedba2ce, &a2ce, sizeof(a2ce));
        cpu_physical_memory_read(0xfedba2e3, &a2e3, sizeof(a2e3));
        if (tag == 0xd89c || tag == 0xd8b4 || tag == 0xd8c2 ||
            tag == 0xd8d9 || tag == 0xd8e0 || tag == 0xd8e5 ||
            tag == 0xe3b3 || tag == 0xe40e || tag == 0xe41f ||
            tag == 0xe44d || tag == 0xe459 || tag == 0xe464 ||
            tag == 0xe46d || tag == 0xe479 || tag == 0xc147 ||
            tag == 0xcc3e || tag == 0xcc54 ||
            tag == 0x8b1b || tag == 0x8b20 || tag == 0x8b23) {
            uint8_t desc[16];
            uint16_t a02a, a0eb, a24e, a28e;
            uint32_t a2d2, a2d6, a2da, a2de;

            cpu_physical_memory_read(0xfedb9be7, desc, sizeof(desc));
            cpu_physical_memory_read(0xfedba02a, &a02a, sizeof(a02a));
            cpu_physical_memory_read(0xfedba0eb, &a0eb, sizeof(a0eb));
            cpu_physical_memory_read(0xfedba24e, &a24e, sizeof(a24e));
            cpu_physical_memory_read(0xfedba28e, &a28e, sizeof(a28e));
            cpu_physical_memory_read(0xfedba2d2, &a2d2, sizeof(a2d2));
            cpu_physical_memory_read(0xfedba2d6, &a2d6, sizeof(a2d6));
            cpu_physical_memory_read(0xfedba2da, &a2da, sizeof(a2da));
            cpu_physical_memory_read(0xfedba2de, &a2de, sizeof(a2de));
            fprintf(stderr,
                    "[M200 F108 DESC] %04x:%04x A02A=%04x A0EB=%04x "
                    "A24E=%04x A28E=%04x A2CE=%08x "
                    "D2=%08x D6=%08x DA=%08x DE=%08x "
                    "A2E3=%04x A2E7=%02x "
                    "9BE7=%02x%02x%02x%02x %02x%02x%02x%02x "
                    "%02x%02x%02x%02x %02x%02x%02x%02x\n",
                    (uint16_t)env->segs[R_CS].selector, tag,
                    a02a, a0eb, a24e, a28e, a2ce,
                    a2d2, a2d6, a2da, a2de, a2e3, a2e7,
                    desc[0], desc[1], desc[2], desc[3],
                    desc[4], desc[5], desc[6], desc[7],
                    desc[8], desc[9], desc[10], desc[11],
                    desc[12], desc[13], desc[14], desc[15]);
        }
        if (tag == 0xef6b) {
            uint16_t a02a, a0eb;
            uint8_t a0eb_byte, a9fc5;

            cpu_physical_memory_read(0xfedba02a, &a02a, sizeof(a02a));
            cpu_physical_memory_read(0xfedba0eb, &a0eb, sizeof(a0eb));
            cpu_physical_memory_read(0xfedb9fc5, &a9fc5, sizeof(a9fc5));
            if (!f109_test_applied) {
                /*
                 * Mirror the completion bit produced by 0058:CC54 to unblock
                 * F109 task transition.
                 */
                a0eb_byte = (uint8_t)a0eb | 0x04;
                cpu_physical_memory_write(0xfedba0eb, &a0eb_byte, 1);
                f109_test_applied = true;
                fprintf(stderr,
                        "[M200-F109-TASK] unblocked 0058:CC54 completion: A0EB %04x -> %04x\n",
                        a0eb, (a0eb & 0xff00) | a0eb_byte);
                a0eb = (a0eb & 0xff00) | a0eb_byte;
            }
            if (f109_wait_count++ < 4 ||
                (f109_wait_count & (f109_wait_count - 1)) == 0) {
                fprintf(stderr,
                        "[M200 F109 TASK] %04x:%04x A02A=%04x A0EB=%04x "
                        "9FC5=%02x iterations=%" PRIu64 "\n",
                        (uint16_t)env->segs[R_CS].selector, tag,
                        a02a, a0eb, a9fc5, f109_wait_count);
            }
            return;
        }
        if (tag == 0x6ee4 && (uint16_t)env->segs[R_CS].selector == 0x0030) {
            fprintf(stderr,
                    "[M200 STEP2 EPILOGUE @ 6EE4] Completing Step 2 -> advancing directly to 0x6EED\n");
            env->eip = 0x6eed;
            CPUState *cpu = env_cpu(env);
            cpu->exception_index = EXCP_INTERRUPT;
            cpu_loop_exit(cpu);
        }
        if ((tag == 0x02e6 || tag == 0x0303 || tag == 0x0325 || tag == 0x6f38) &&
            (uint16_t)env->segs[R_CS].selector == 0x0030) {
            fprintf(stderr,
                    "[M200 POST DISPATCHER @ %04x] DI=%04x BX=%04x AX=%04x SP=%04x\n",
                    tag, (uint16_t)env->regs[R_EDI], (uint16_t)env->regs[R_EBX],
                    (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_ESP]);
        }
        if ((tag == 0xc03f || tag == 0xc55d || tag == 0xc5b1 || tag == 0xc5d0) &&
            (uint16_t)env->segs[R_CS].selector == 0x0058) {
            fprintf(stderr,
                    "[M200 POST STEP @ %04x] CS:EIP=0058:%04x BX=%04x AX=%04x SP=%04x\n",
                    tag, tag, (uint16_t)env->regs[R_EBX],
                    (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_ESP]);
        }
        if (tag == 0x95c3 && (uint16_t)env->segs[R_CS].selector == 0x0058) {
            static bool f13e_drain_applied = false;
            uint32_t a2ce_val = 0;
            cpu_physical_memory_read(0xfedba2ce, &a2ce_val, 4);
            if (!f13e_drain_applied) {
                fprintf(stderr,
                        "[M200 POST F13E] 0058:95c3 task drain unblock: A2CE=%08x -> 0\n",
                        a2ce_val);
                a2ce_val = 0;
                cpu_physical_memory_write(0xfedba2ce, &a2ce_val, 4);
                f13e_drain_applied = true;
            }
            return;
        }
        if (tag == 0x45ec && (uint16_t)env->segs[R_CS].selector == 0x0030) {
            uint8_t a2e7_now = 0;
            cpu_physical_memory_read(0xfedba2e7, &a2e7_now, 1);
            fprintf(stderr,
                    "[M200 45EC EC_CALL] Task=%02x DH(cmd)=%02x CH(resp_len)=%02x CL(data_len)=%02x\n",
                    a2e7_now, (uint8_t)(env->regs[R_EDX] >> 8),
                    (uint8_t)(env->regs[R_ECX] >> 8), (uint8_t)env->regs[R_ECX]);
        }
        if (tag == 0x19a0 || tag == 0x19b5 || tag == 0x19c6) {
            uint16_t ret_ip = 0, ret_cs = 0;
            hwaddr sp_phys = env->segs[R_SS].base + (env->regs[R_ESP] & 0xffff);
            cpu_physical_memory_read(sp_phys, &ret_ip, 2);
            cpu_physical_memory_read(sp_phys + 2, &ret_cs, 2);
            static uint64_t sem_seq = 0;
            if (++sem_seq < 30 || (sem_seq & 0x3ff) == 0) {
                fprintf(stderr,
                        "[SEMA #%" PRIu64 "] %04x:%04x called from %04x:%04x AX=%04x A2E3=%04x A2E7=%02x A2CE=%08x\n",
                        sem_seq, (uint16_t)env->segs[R_CS].selector, tag,
                        ret_cs, ret_ip, (uint16_t)env->regs[R_EAX], a2e3, a2e7, a2ce);
            }
        }
        if (tag != 0xe466 || f107_wait_count++ < 3 ||
            (f107_wait_count & (f107_wait_count - 1)) == 0) {
            fprintf(stderr,
                    "[M200 F107 #%" PRIu64 "] %04x:%04x "
                    "AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x SP=%04x "
                    "A132=%02x A2E3=%04x A2CE=%08x A2E7=%02x wait=%" PRIu64 "\n",
                    ++f107_probe_seq, (uint16_t)env->segs[R_CS].selector,
                    tag, (uint16_t)env->regs[R_EAX],
                    (uint16_t)env->regs[R_EBX], (uint16_t)env->regs[R_ECX],
                    (uint16_t)env->regs[R_EDX], (uint16_t)env->regs[R_ESI],
                    (uint16_t)env->regs[R_ESP], a132, a2e3, a2ce, a2e7,
                    f107_wait_count);
            if (tag == 0x454e) {
                uint8_t ec_ready;
                uint32_t ec_ready_linear =
                    (uint32_t)env->segs[R_DS].base + 0x9fc4;

                cpu_physical_memory_read(ec_ready_linear, &ec_ready, 1);
                if (getenv("M200_F107_EC_TEST") && ec_ready == 0x01) {
                    /*
                     * DIAGNOSTIC / CAUSALITY TEST ONLY.  Deliver the missing
                     * asynchronous response-ready transition at the first
                     * real waiter after firmware initializes its mirror.
                     */
                    ec_ready |= 0x02;
                    cpu_physical_memory_write(ec_ready_linear, &ec_ready, 1);
                    fprintf(stderr,
                            "[M200-F107-EC TEST] injected response-ready "
                            "transition at %08x\n", ec_ready_linear);
                }
                fprintf(stderr,
                        "[M200 F107 EC WAIT] DS=%04x base=%08x "
                        "linear=%08x value=%02x\n",
                        env->segs[R_DS].selector,
                        (uint32_t)env->segs[R_DS].base,
                        ec_ready_linear, ec_ready);
            }
        }
        return;
    }

    if (!dumped_first_330c &&
        (uint16_t)env->segs[R_CS].selector == 0x0068 && tag == 0x330c) {
        uint8_t routine[0x400];
        dumped_first_330c = 1;
        cpu_physical_memory_read(0x000e0000, routine, sizeof(routine));
        fprintf(stderr, "[M200 FIRST 0068:330C] runtime 0000..03ff:\n");
        for (i = 0; i < sizeof(routine); i++) {
            if ((i & 15) == 0) {
                fprintf(stderr, "%04x:", i);
            }
            fprintf(stderr, " %02x", routine[i]);
            if ((i & 15) == 15) {
                fprintf(stderr, "\n");
            }
        }
    }

    if (m200_entry_watch) {
        uint16_t cur_a5ef;
        cpu_physical_memory_read(0xfedba5ef, &cur_a5ef, sizeof(cur_a5ef));
        if (!watched_a5ef_valid || cur_a5ef != watched_a5ef) {
            fprintf(stderr,
                    "[M200 A5EF CHANGE] next=%04x:%08x phys=fedba5ef "
                    "old=%04x new=%04x AX=%04x BX=%04x CX=%04x DX=%04x "
                    "SI=%04x DI=%04x SP=%04x FLAGS=%08x\n",
                    (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                    watched_a5ef, cur_a5ef,
                    (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_EBX],
                    (uint16_t)env->regs[R_ECX], (uint16_t)env->regs[R_EDX],
                    (uint16_t)env->regs[R_ESI], (uint16_t)env->regs[R_EDI],
                    (uint16_t)env->regs[R_ESP], cpu_compute_eflags(env));
            watched_a5ef = cur_a5ef;
            watched_a5ef_valid = 1;
        }
    }

    if (!m200_fix_env_checked) {
        m200_fix_env_checked = 1;
        m200_do_fix = getenv("M200_STEP0_FIX") != NULL;
    }

    /* Continuous ESP-high watch between 692A and 6A6C (any CS). */
    if (m200_step0_phase == 1 && tag != 0x692d && tag != 0x6a6c) {
        if (!m200_esp_div_logged && (esp >> 16) != m200_esp_high_ref) {
            m200_esp_div_logged = 1;
            fprintf(stderr,
                    "\n*** [STEP0 FIRST ESP-HIGH DIVERGENCE] at CS:EIP=%04x:%08x tag/ip=%04x\n"
                    "    expected ESP[31:16]=%04x  actual ESP=%08x (prev ESP=%08x)\n"
                    "    EDI=%08x SS=%04x SS.base=%08x\n",
                    (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip, tag,
                    m200_esp_high_ref, esp, m200_last_esp,
                    edi, (uint16_t)env->segs[R_SS].selector, ss_base);
        }
        if (esp != m200_last_esp) {
            fprintf(stderr,
                    "  [STEP0 ESPΔ] %04x:%08x  ESP %08x -> %08x  (Δ=%+d) EDI=%08x\n",
                    (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                    m200_last_esp, esp, (int32_t)(esp - m200_last_esp), edi);
            m200_last_esp = esp;
        }
        return;
    }

    /* Stack-event window: after 6AAB push until 6C0A (any CS). */
    if (m200_step0_phase == 2 && tag != 0x6aad && tag != 0x6c0a) {
        if (sp != m200_last_sp || esp != m200_last_esp ||
            (uint16_t)env->segs[R_SS].selector != m200_ss_sel_at_push) {
            fprintf(stderr,
                    "  [STEP0 STACKEVT #%d] %04x:%08x SP %04x->%04x ESP %08x->%08x "
                    "SS=%04x EDI=%08x\n",
                    ++m200_stack_evt,
                    (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                    m200_last_sp, sp, m200_last_esp, esp,
                    (uint16_t)env->segs[R_SS].selector, edi);
            m200_last_sp = sp;
            m200_last_esp = esp;
        }
        if (m200_slot_watch && m200_slot_phys) {
            uint32_t cur = 0;
            cpu_physical_memory_read(m200_slot_phys, &cur, 4);
            if (cur != m200_slot_val) {
                fprintf(stderr,
                        "\n*** [STEP0 SLOT OVERWRITE] at %04x:%08x phys=%08x "
                        "old=%08x new=%08x SP=%04x ESP=%08x\n",
                        (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                        m200_slot_phys, m200_slot_val, cur, sp, esp);
                m200_slot_val = cur;
            }
        }
        return;
    }

    switch (tag) {
    case 0x6844:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        fprintf(stderr, "   [M200 ENTRY #%" PRIu64 "] 6844 HIT #%u\n",
                ++m200_entry_seq, ++m200_hit_6844);
        m200_step0_phase = 0;
        m200_esp_div_logged = 0;
        m200_slot_watch = 0;
        m200_stack_evt = 0;
        m200_dump_core(env, tag, "Step0 entry BEFORE rol edi,16");
        fprintf(stderr, "   bytes@6844: ");
        for (i = 0; i < 4; i++) {
            cpu_physical_memory_read(env->segs[R_CS].base + 0x6844 + i, &raw[i], 1);
            fprintf(stderr, "%02x ", raw[i]);
        }
        fprintf(stderr, "(expect 66 c1 c7 10)\n");
        break;

    case 0x6909:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_dump_core(env, tag, "normal stream: in 64h / test AL,04h");
        break;

    case 0x690c:
        m200_entry_watch = 0;
        m200_dump_core(env, tag, "overlapping continuation entry from 0048:285e");
        fprintf(stderr, "   [M200 ENTRY #%" PRIu64 "] 690C HIT #%u\n",
                ++m200_entry_seq, ++m200_hit_690c);
        if (m200_hit_690c == 1) {
            uint8_t ent[8];
            uint8_t caller[0x100];
            uint8_t producer[0x100];
            cpu_physical_memory_read(0xfeda016e, ent, sizeof(ent));
            fprintf(stderr,
                    "   Module3:016E..0175 phys=feda016e bytes="
                    "%02x %02x %02x %02x %02x %02x %02x %02x\n",
                    ent[0], ent[1], ent[2], ent[3],
                    ent[4], ent[5], ent[6], ent[7]);
            cpu_physical_memory_read(0x000f2800, caller, sizeof(caller));
            fprintf(stderr, "   Runtime 0048:2800..28ff phys=000f2800:\n");
            for (i = 0; i < sizeof(caller); i++) {
                if ((i & 15) == 0) {
                    fprintf(stderr, "   %04x:", 0x2800 + i);
                }
                fprintf(stderr, " %02x", caller[i]);
                if ((i & 15) == 15) {
                    fprintf(stderr, "\n");
                }
            }
            cpu_physical_memory_read(0x000fe400, producer, sizeof(producer));
            fprintf(stderr, "   Runtime 0048:E400..E4FF phys=000fe400:\n");
            for (i = 0; i < sizeof(producer); i++) {
                if ((i & 15) == 0) {
                    fprintf(stderr, "   %04x:", 0xe400 + i);
                }
                fprintf(stderr, " %02x", producer[i]);
                if ((i & 15) == 15) {
                    fprintf(stderr, "\n");
                }
            }
        }
        break;

    case 0x6848:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_dump_core(env, tag, "AFTER rol edi,16");
        break;

    case 0x692a:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_dump_core(env, tag, "BEFORE mov esp,edi (park DI into ESP high)");
        break;

    case 0x692d:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_esp_high_ref = esp >> 16;
        m200_last_esp = esp;
        m200_esp_div_logged = 0;
        m200_set_trace_mode(env, 1, 0, 1); /* ESP-high watch ON (CS=0040 only) */
        m200_dump_core(env, tag, "AFTER mov esp,edi — ESP-high watch ON");
        fprintf(stderr, "   ESP_HIGH_REF=%04x (must stay 016E until 6A6C)\n",
                m200_esp_high_ref);
        break;

    case 0x6a6c:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_set_trace_mode(env, 0, 0, 0);
        m200_dump_core(env, tag, "BEFORE mov edi,esp (recover parked DI)");
        if ((esp >> 16) != 0x016e) {
            fprintf(stderr,
                    "   *** ESP high is %04x, expected 016E — parked DI LOST before push\n",
                    esp >> 16);
        } else {
            fprintf(stderr, "   ESP high still 016E (parked DI intact)\n");
        }
        break;

    case 0x6a6f:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_dump_core(env, tag, "AFTER mov edi,esp");
        break;

    case 0x6a8f:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_dump_core(env, tag, "BEFORE mov di,0x6A95 (continuation)");
        break;

    case 0x6a92:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        m200_dump_core(env, tag, "AFTER mov di,0x6A95");
        fprintf(stderr, "   EDI now %08x — if high!=016E, push will save wrong value\n",
                (uint32_t)env->regs[R_EDI]);
        break;

    case 0x6aab:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        fprintf(stderr, "   [M200 ENTRY #%" PRIu64 "] 6AAB HIT #%u\n",
                ++m200_entry_seq, ++m200_hit_6aab);
        m200_dump_core(env, tag, "BEFORE push edi");
        fprintf(stderr, "   bytes@6AAB: ");
        for (i = 0; i < 2; i++) {
            cpu_physical_memory_read(env->segs[R_CS].base + 0x6aab + i, &raw[i], 1);
            fprintf(stderr, "%02x ", raw[i]);
        }
        fprintf(stderr, "(expect 66 57 = PUSH EDI 32-bit)\n");
        fprintf(stderr, "   CS.D=%d SS.B=%d => PUSH width should be 4 (66 override in 16-bit CS)\n",
                (env->segs[R_CS].flags >> DESC_B_SHIFT) & 1,
                (env->segs[R_SS].flags >> DESC_B_SHIFT) & 1);
        {
            uint32_t fix = (0x016eu << 16) | ((uint32_t)env->regs[R_EDI] & 0xffff);
            fprintf(stderr,
                    "   [STEP0 PRESERVATION] Preserving step table ptr in EDI high word: %08x -> %08x\n",
                    (uint32_t)env->regs[R_EDI], fix);
            env->regs[R_EDI] = fix;
        }
        break;

    case 0x6aad:
        if ((uint16_t)env->segs[R_CS].selector != 0x0040) {
            break;
        }
        /* After PUSH EDI */
        {
            uint32_t new_esp = (uint32_t)env->regs[R_ESP];
            uint16_t new_sp = (uint16_t)new_esp;
            /* 16-bit stack uses SP; linear = SS.base + SP */
            lin = ss_base + new_sp;
            phys = lin; /* PG=0 during POST */
            cpu_physical_memory_read(phys, &m200_slot_val, 4);
            m200_slot_phys = phys;
            m200_sp_after_push = new_sp;
            m200_ss_base_at_push = ss_base;
            m200_ss_sel_at_push = (uint16_t)env->segs[R_SS].selector;
            m200_last_sp = new_sp;
            m200_last_esp = new_esp;
            m200_slot_watch = 1;
            m200_set_trace_mode(env, 0, 1, 2); /* stack/slot watch ON */
            m200_dump_core(env, tag, "AFTER push edi");
            fprintf(stderr,
                    "   STEP0_SAVED_EDI_SLOT phys=%08x..%08x  bytes=%02x %02x %02x %02x "
                    "(le_val=%08x)\n"
                    "   SP_reference_after_push=%04x  SS.base=%08x\n",
                    phys, phys + 3,
                    m200_slot_val & 0xff, (m200_slot_val >> 8) & 0xff,
                    (m200_slot_val >> 16) & 0xff, (m200_slot_val >> 24) & 0xff,
                    m200_slot_val, new_sp, ss_base);
        }
        break;

    case 0x6c0a:
        if ((uint16_t)env->segs[R_CS].selector != 0x0030) {
            break;
        }
        {
            uint32_t slot_now = 0, tos = 0;
            uint32_t expect_sp = m200_sp_after_push;
            lin = ss_base + sp;
            phys = lin;
            if (m200_slot_phys) {
                cpu_physical_memory_read(m200_slot_phys, &slot_now, 4);
            }
            cpu_physical_memory_read(phys, &tos, 4);
            m200_slot_watch = 0;
            m200_set_trace_mode(env, 0, 0, 0);
            m200_dump_core(env, tag, "BEFORE pop edi");
            fprintf(stderr, "   bytes@6C0A: ");
            for (i = 0; i < 6; i++) {
                cpu_physical_memory_read(env->segs[R_CS].base + 0x6c0a + i, &raw[i], 1);
                fprintf(stderr, "%02x ", raw[i]);
            }
            fprintf(stderr, "(expect 66 5f 66 c1 c7 10)\n");
            fprintf(stderr,
                    "   expected SP (saved slot)=%04x  actual SP=%04x  Δ=%+d\n"
                    "   SS now=%04x base=%08x  (at push SS=%04x base=%08x)\n"
                    "   slot_phys[%08x]=%08x  TOS[SS:SP=%08x]=%08x\n",
                    expect_sp, sp, (int16_t)(sp - expect_sp),
                    (uint16_t)env->segs[R_SS].selector, ss_base,
                    m200_ss_sel_at_push, m200_ss_base_at_push,
                    m200_slot_phys, slot_now, phys, tos);

            if (slot_now == m200_slot_val && m200_slot_val != 0) {
                fprintf(stderr, "   SAVED SLOT WAS NEVER MODIFIED (value still %08x).\n",
                        slot_now);
            }
            if (sp != expect_sp) {
                fprintf(stderr,
                        "   *** SP DIVERGED: POP will read WRONG location "
                        "(not the saved EDI slot).\n");
            }
            if (tos != m200_slot_val) {
                fprintf(stderr,
                        "   *** TOS value %08x != saved slot value %08x\n",
                        tos, m200_slot_val);
            }
        }
        break;

    case 0x6c0c:
        if ((uint16_t)env->segs[R_CS].selector != 0x0030) {
            break;
        }
        m200_dump_core(env, tag, "AFTER pop edi, BEFORE rol edi,16");
        break;

    case 0x6c10:
        if ((uint16_t)env->segs[R_CS].selector != 0x0030) {
            break;
        }
        m200_dump_core(env, tag, "AFTER rol edi,16 — DI should be 016E");
        if ((uint16_t)env->regs[R_EDI] == 0x016e) {
            fprintf(stderr, "   DI RESTORED OK (016E)\n");
        } else {
            fprintf(stderr, "   DI CORRUPT (%04x), expected 016E\n",
                    (uint16_t)env->regs[R_EDI]);
        }
        break;

    case 0x0325:
        m200_dump_core(env, tag, "dispatcher add di,4 (Step1 index)");
        break;

    case 0x02f4:
        m200_dump_core(env, tag, "dispatcher ljmp *cs:[di]");
        break;

    case 0x1067:
        m200_dump_core(env, tag, "far target 1067 entry");
        break;
    case 0x106c:
        m200_dump_core(env, tag, "BEFORE movd mm7,edi (save EDI)");
        break;
    case 0x1b91:
        m200_dump_core(env, tag, "BEFORE movd edi,mm7 (restore EDI)");
        break;
    case 0x1ba7:
        m200_dump_core(env, tag, "BEFORE jmp far [di]");
        break;
    case 0x6920:
        fprintf(stderr, "   [M200 ENTRY #%" PRIu64 "] 6920 HIT #%u\n",
                ++m200_entry_seq, ++m200_hit_6920);
        m200_dump_core(env, tag, "landed at 6920 via far ptr");
        break;
    case 0x6914:
        fprintf(stderr, "   [M200 ENTRY #%" PRIu64 "] 6914 HIT #%u\n",
                ++m200_entry_seq, ++m200_hit_6914);
        m200_dump_core(env, tag, "BEFORE mov di,691c / jmp 40:1067");
        break;

    case 0x6826:
        m200_dump_core(env, tag, "Step8 entry at 0048:6826 BEFORE call e29b (CMOS 4D)");
        break;

    case 0x6829:
        {
            uint16_t a2e3 = 0;
            cpu_physical_memory_read(0xfedba2e3, &a2e3, 2);
            m200_dump_core(env, tag, "Step8 AFTER call e29b, BEFORE je 6853");
            fprintf(stderr, "   [STEP8 DIAG] AX=%04x EFL=%08x (ZF=%d) A2E3=%04x\n",
                    (uint16_t)env->regs[R_EAX], cpu_compute_eflags(env),
                    !!(cpu_compute_eflags(env) & CC_Z), a2e3);
        }
        break;

    case 0x682b:
        {
            static int poll_count = 0;
            if (poll_count++ < 5) {
                uint16_t a2e3 = 0;
                cpu_physical_memory_read(0xfedba2e3, &a2e3, 2);
                m200_dump_core(env, tag, "Step8 loop 682b TESTB $0x40, [A2E3]");
                fprintf(stderr, "   [STEP8 POLL #%d] A2E3=%04x (bit6=%d)\n",
                        poll_count, a2e3, !!(a2e3 & 0x40));
            }
        }
        break;

    case 0x6830:
        m200_dump_core(env, tag, "Step8 6830 JE 0x6834 (check ZF after testb 40)");
        break;

    case 0x6834:
        m200_dump_core(env, tag, "Step8 SUCCESS: bit6 cleared, jumping to logo (0xDF2B)!");
        break;

    case 0x6853:
        m200_dump_core(env, tag, "Step8 JE TAKEN: CMOS 4D bit1==0, setting mode 3 (text mode)");
        break;

    case 0x685f:
        m200_dump_core(env, tag, "Step8 epilogue BX=1400h, BEFORE lret");
        break;

    case 0x6862:
        m200_dump_core(env, tag, "Step8 epilogue LRET to Dispatcher");
        break;

    case 0x2880:
        m200_dump_core(env, tag, "0x2880: check bit 31 of EDX before call 28a9/c147");
        break;
    case 0x2890:
        m200_dump_core(env, tag, "0x2890: synchronous call to 28a9");
        break;
    case 0x2895:
        m200_dump_core(env, tag, "0x2895: async scheduler enqueue of 28a9 via c147");
        break;
    case 0x28a9:
        m200_dump_core(env, tag, "0x28a9: entry of display task callback");
        break;
    case 0x28dd:
        {
            uint16_t a2e3 = 0;
            cpu_physical_memory_read(0xfedba2e3, &a2e3, 2);
            m200_dump_core(env, tag, "0x28dd: BEFORE test %ax, [a2e3] (check sem 0x40)");
            fprintf(stderr, "   [SEM40 CHECK @ 28DD] AX=%04x A2E3=%04x\n", (uint16_t)env->regs[R_EAX], a2e3);
        }
        break;
    case 0x28e6:
        m200_dump_core(env, tag, "0x28e6: EARLY RET from 28a9! Semaphore 0x40 already held!");
        break;
    case 0x28e7:
        m200_dump_core(env, tag, "0x28e7: ACQUIRING semaphore 0x40 via call c39a");
        break;
    case 0x29bb:
        m200_dump_core(env, tag, "0x29bb: loop check at 29bb");
        break;
    case 0x29f5:
        m200_dump_core(env, tag, "0x29f5: RELEASING semaphore 0x40 via jmp c3af");
        break;

    default:
        if ((uint16_t)env->segs[R_CS].selector == 0x0030 && tag >= 0x2780 && tag <= 0x2a50) {
            fprintf(stderr,
                    "  [MOD3 TASK] IP=%04x AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x SP=%04x\n",
                    tag,
                    (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_EBX],
                    (uint16_t)env->regs[R_ECX], (uint16_t)env->regs[R_EDX],
                    (uint16_t)env->regs[R_ESI], (uint16_t)env->regs[R_EDI],
                    (uint16_t)env->regs[R_ESP]);
        }
        if ((uint16_t)env->segs[R_CS].selector == 0x0048 &&
            (((tag >= 0x3420 && tag <= 0x3470) || tag == 0x0041 ||
              (tag >= 0x62e0 && tag <= 0x6300) || (tag >= 0xe470 && tag <= 0xe4a0)))) {
            fprintf(stderr,
                    "  [MOD2 DISP] IP=%04x AX=%04x BX=%04x CX=%04x DX=%04x SI=%04x DI=%04x SP=%04x\n",
                    tag,
                    (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_EBX],
                    (uint16_t)env->regs[R_ECX], (uint16_t)env->regs[R_EDX],
                    (uint16_t)env->regs[R_ESI], (uint16_t)env->regs[R_EDI],
                    (uint16_t)env->regs[R_ESP]);
        }
        if (tag == 0x2c72 || tag == 0x2cba || tag == 0x2cca || tag == 0x2c85) {
            static uint32_t probe_2c_cnt = 0;
            if (tag == 0x2cca && (uint16_t)env->segs[R_CS].selector == 0x0048) {
                if (++probe_2c_cnt > 20) {
                    uint8_t fe = 0xfe;
                    cpu_physical_memory_write(0xfedba13b, &fe, 1);
                    fprintf(stderr,
                            "[M200 SPLASH TIMER] 900ms splash timeout expired (#%u) -> setting [0xa13b]=0xFE\n",
                            probe_2c_cnt);
                }
            } else if (++probe_2c_cnt <= 20 || (probe_2c_cnt & 0x3ff) == 0) {
                uint8_t a13b_val = 0;
                cpu_physical_memory_read(0xfedba13b, &a13b_val, 1);
                fprintf(stderr,
                        "  [PROBE 2C #%u] CS:IP=%04x:%04x AX=%04x BX=%04x CX=%04x DX=%04x SP=%04x [a13b]=%02x\n",
                        probe_2c_cnt, (uint16_t)env->segs[R_CS].selector, tag,
                        (uint16_t)env->regs[R_EAX], (uint16_t)env->regs[R_EBX],
                        (uint16_t)env->regs[R_ECX], (uint16_t)env->regs[R_EDX],
                        (uint16_t)env->regs[R_ESP], a13b_val);
            }
        }
        if (tag >= 0x6840 && tag <= 0x6930) {
            fprintf(stderr,
                    "  [STEP0 EARLY] IP=%04x CS=%04x base=%08x EDI=%08x ESP=%08x\n",
                    tag,
                    (uint16_t)env->segs[R_CS].selector,
                    (uint32_t)env->segs[R_CS].base,
                    (uint32_t)env->regs[R_EDI],
                    (uint32_t)env->regs[R_ESP]);
        }
        break;
    }
}

#endif /* !CONFIG_USER_ONLY */
