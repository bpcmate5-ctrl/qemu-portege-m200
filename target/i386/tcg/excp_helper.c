/*
 *  x86 exception helpers
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
#include "cpu.h"
#include "qemu/log.h"
#include "system/runstate.h"
#include "exec/helper-proto.h"
#include "helper-tcg.h"
#include "qemu/plugin.h"

#if !defined(CONFIG_USER_ONLY)
extern int m200_step0_phase;
#endif

G_NORETURN void helper_raise_interrupt(CPUX86State *env, int intno,
                                          int next_eip_addend)
{
    raise_interrupt(env, intno, next_eip_addend);
}

G_NORETURN void helper_raise_exception(CPUX86State *env, int exception_index)
{
    raise_exception(env, exception_index);
}

/*
 * Check nested exceptions and change to double or triple fault if
 * needed. It should only be called, if this is not an interrupt.
 * Returns the new exception number.
 */
static int check_exception(CPUX86State *env, int intno, int *error_code,
                           uintptr_t retaddr)
{
    int first_contributory = env->old_exception == 0 ||
                              (env->old_exception >= 10 &&
                               env->old_exception <= 13);
    int second_contributory = intno == 0 ||
                               (intno >= 10 && intno <= 13);

    qemu_log_mask(CPU_LOG_INT, "check_exception old: 0x%x new 0x%x\n",
                env->old_exception, intno);

#if !defined(CONFIG_USER_ONLY)
    if (env->old_exception == EXCP08_DBLE) {
        if (env->hflags & HF_GUEST_MASK) {
            cpu_vmexit(env, SVM_EXIT_SHUTDOWN, 0, retaddr); /* does not return */
        }

        qemu_log_mask(CPU_LOG_RESET, "Triple fault\n");

        qemu_system_reset_request(SHUTDOWN_CAUSE_GUEST_RESET);
        return EXCP_HLT;
    }
#endif

    if ((first_contributory && second_contributory)
        || (env->old_exception == EXCP0E_PAGE &&
            (second_contributory || (intno == EXCP0E_PAGE)))) {
        intno = EXCP08_DBLE;
        *error_code = 0;
    }

    if (second_contributory || (intno == EXCP0E_PAGE) ||
        (intno == EXCP08_DBLE)) {
        env->old_exception = intno;
    }

    return intno;
}

/*
 * Signal an interruption. It is executed in the main CPU loop.
 * is_int is TRUE if coming from the int instruction. next_eip is the
 * env->eip value AFTER the interrupt instruction. It is only relevant if
 * is_int is TRUE.
 */
static G_NORETURN
void raise_interrupt2(CPUX86State *env, int intno,
                      int is_int, int error_code,
                      int next_eip_addend,
                      uintptr_t retaddr)
{
    CPUState *cs = env_cpu(env);
    uint64_t last_pc = env->eip + env->segs[R_CS].base;
    if (intno == 0x10) {
        uint16_t vbios_bdf = 0;
        cpu_physical_memory_read(0xc0214, &vbios_bdf, 2);
        uint32_t eax = (uint32_t)env->regs[R_EAX];
        uint32_t ebx = (uint32_t)env->regs[R_EBX];
        uint32_t ecx = (uint32_t)env->regs[R_ECX];
        uint32_t edx = (uint32_t)env->regs[R_EDX];

        static int int10_call_seq = 0;
        int10_call_seq++;

        fprintf(stderr, "\n============================================================\n"
                        "   [VBE / INT 10h CALL #%d] CS:EIP = %04x:%08x (CS_BASE = 0x%08x)\n"
                        "   AX = 0x%04x  BX = 0x%04x  CX = 0x%04x  DX = 0x%04x\n"
                        "   ES:EDI = %04x:%08x  VBIOS C0214 = 0x%04x\n",
                int10_call_seq, (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                (uint32_t)env->segs[R_CS].base,
                (uint16_t)eax, (uint16_t)ebx, (uint16_t)ecx, (uint16_t)edx,
                (uint16_t)env->segs[R_ES].selector, (uint32_t)env->regs[R_EDI],
                vbios_bdf);
        if ((eax & 0xFF00) == 0x4F00) {
            uint8_t func = eax & 0xFF;
            if (func == 0x02) {
                uint16_t mode = ebx & 0x3FFF;
                bool lfb = (ebx & 0x4000) != 0;
                bool preserve = (ebx & 0x8000) != 0;
                fprintf(stderr, "   >>> [VBE 4F02h SET VIDEO MODE] Mode=0x%04x (LFB=%s, PreserveMemory=%s) <<<\n",
                        mode, lfb ? "YES (Linear Framebuffer)" : "NO (Windowed/Planar)",
                        preserve ? "YES" : "NO");
            } else if (func == 0x10) {
                fprintf(stderr, "   >>> [VBE 4F10h FLAT PANEL / DDC / DPMS] SubFunction=0x%02x <<<\n", (uint8_t)ebx);
            } else if (func == 0x14) {
                fprintf(stderr, "   >>> [VBE 4F14h TOSHIBA FLAT PANEL EXTENSION] SubFunction=0x%02x <<<\n", (uint8_t)ebx);
            } else {
                fprintf(stderr, "   >>> [VBE 4F%02Xh FUNCTION] <<<\n", func);
            }
        } else if ((eax & 0xFF00) == 0x0000) {
            fprintf(stderr, "   >>> [VGA SET MODE 0x%02X] (Legacy Mode 0x%02x) <<<\n", (uint8_t)eax, (uint8_t)eax);
        }
        fprintf(stderr, "============================================================\n");
    }
    if (intno == 0x0d) {
        fprintf(stderr, "\n============================================================\n"
                        "   [#GP EXCEPTION 0x0D DETECTED] CS:EIP = %04x:%08x (CS_BASE = 0x%08x)\n"
                        "   EAX = 0x%08x  EBX = 0x%08x  ECX = 0x%08x  EDX = 0x%08x\n"
                        "   ESI = 0x%08x  EDI = 0x%08x  EBP = 0x%08x  ESP = 0x%08x\n"
                        "============================================================\n",
                (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                (uint32_t)env->segs[R_CS].base,
                (uint32_t)env->regs[R_EAX], (uint32_t)env->regs[R_EBX],
                (uint32_t)env->regs[R_ECX], (uint32_t)env->regs[R_EDX],
                (uint32_t)env->regs[R_ESI], (uint32_t)env->regs[R_EDI],
                (uint32_t)env->regs[R_EBP], (uint32_t)env->regs[R_ESP]);
    }
    if (intno == 0x19) {
        fprintf(stderr, "\n============================================================\n"
                        "   >>> [BOOT BOUNDARY INT 19h REACHED] <<<\n"
                        "============================================================\n");
        uint16_t ivt_10_off = 0, ivt_10_seg = 0;
        uint16_t ivt_13_off = 0, ivt_13_seg = 0;
        uint16_t ivt_15_off = 0, ivt_15_seg = 0;
        uint16_t ivt_19_off = 0, ivt_19_seg = 0;
        uint16_t ivt_1a_off = 0, ivt_1a_seg = 0;
        uint16_t vbios_c0214 = 0;

        cpu_physical_memory_read(0x10 * 4, &ivt_10_off, 2);
        cpu_physical_memory_read(0x10 * 4 + 2, &ivt_10_seg, 2);
        cpu_physical_memory_read(0x13 * 4, &ivt_13_off, 2);
        cpu_physical_memory_read(0x13 * 4 + 2, &ivt_13_seg, 2);
        cpu_physical_memory_read(0x15 * 4, &ivt_15_off, 2);
        cpu_physical_memory_read(0x15 * 4 + 2, &ivt_15_seg, 2);
        cpu_physical_memory_read(0x19 * 4, &ivt_19_off, 2);
        cpu_physical_memory_read(0x19 * 4 + 2, &ivt_19_seg, 2);
        cpu_physical_memory_read(0x1a * 4, &ivt_1a_off, 2);
        cpu_physical_memory_read(0x1a * 4 + 2, &ivt_1a_seg, 2);
        cpu_physical_memory_read(0xc0214, &vbios_c0214, 2);

        fprintf(stderr, "   [QEMU IVT INT 10h] = %04X:%04X (Linear 0x%08X)\n",
                ivt_10_seg, ivt_10_off, (ivt_10_seg << 4) + ivt_10_off);
        fprintf(stderr, "   [QEMU IVT INT 13h] = %04X:%04X (Linear 0x%08X)\n",
                ivt_13_seg, ivt_13_off, (ivt_13_seg << 4) + ivt_13_off);
        fprintf(stderr, "   [QEMU IVT INT 15h] = %04X:%04X (Linear 0x%08X)\n",
                ivt_15_seg, ivt_15_off, (ivt_15_seg << 4) + ivt_15_off);
        fprintf(stderr, "   [QEMU IVT INT 19h] = %04X:%04X (Linear 0x%08X)\n",
                ivt_19_seg, ivt_19_off, (ivt_19_seg << 4) + ivt_19_off);
        fprintf(stderr, "   [QEMU IVT INT 1Ah] = %04X:%04X (Linear 0x%08X)\n",
                ivt_1a_seg, ivt_1a_off, (ivt_1a_seg << 4) + ivt_1a_off);
        fprintf(stderr, "   [QEMU VBIOS C0214] = 0x%04X\n", vbios_c0214);
        fprintf(stderr, "============================================================\n");
    }
    fprintf(stderr, "[EXCP] intno=0x%02x is_int=%d err=0x%x eip=0x%08"PRIx64" cs_base=0x%08"PRIx64"\n",
            intno, is_int, error_code, (uint64_t)env->eip, (uint64_t)env->segs[R_CS].base);

#if !defined(CONFIG_USER_ONLY)
    if (m200_step0_phase) {
        fprintf(stderr,
                "  [STEP0 IRQ/EXCP] phase=%d intno=%02x is_int=%d "
                "CS:EIP=%04x:%08x ESP=%08x SS=%04x\n",
                m200_step0_phase, intno, is_int,
                (uint16_t)env->segs[R_CS].selector, (uint32_t)env->eip,
                (uint32_t)env->regs[R_ESP],
                (uint16_t)env->segs[R_SS].selector);
    }
#endif

    if (!is_int) {
        cpu_svm_check_intercept_param(env, SVM_EXIT_EXCP_BASE + intno,
                                      error_code, retaddr);
        intno = check_exception(env, intno, &error_code, retaddr);
    } else {
        cpu_svm_check_intercept_param(env, SVM_EXIT_SWINT, 0, retaddr);
    }

    cs->exception_index = intno;
    env->error_code = error_code;
    env->exception_is_int = is_int;
    env->exception_next_eip = env->eip + next_eip_addend;
    qemu_plugin_vcpu_exception_cb(cs, last_pc);
    cpu_loop_exit_restore(cs, retaddr);
}

/* shortcuts to generate exceptions */

G_NORETURN void raise_interrupt(CPUX86State *env, int intno, int next_eip_addend)
{
    raise_interrupt2(env, intno, 1, 0, next_eip_addend, 0);
}

G_NORETURN void raise_exception_err(CPUX86State *env, int exception_index,
                                    int error_code)
{
    raise_interrupt2(env, exception_index, 0, error_code, 0, 0);
}

G_NORETURN void raise_exception_err_ra(CPUX86State *env, int exception_index,
                                       int error_code, uintptr_t retaddr)
{
    raise_interrupt2(env, exception_index, 0, error_code, 0, retaddr);
}

G_NORETURN void raise_exception(CPUX86State *env, int exception_index)
{
    raise_interrupt2(env, exception_index, 0, 0, 0, 0);
}

G_NORETURN void raise_exception_ra(CPUX86State *env, int exception_index,
                                   uintptr_t retaddr)
{
    raise_interrupt2(env, exception_index, 0, 0, 0, retaddr);
}

G_NORETURN void helper_icebp(CPUX86State *env)
{
    CPUState *cs = env_cpu(env);

    do_end_instruction(env);

    /*
     * INT1 aka ICEBP generates a trap-like #DB, but it is pretty special.
     *
     * "Although the ICEBP instruction dispatches through IDT vector 1,
     * that event is not interceptable by means of the #DB exception
     * intercept".  Instead there is a separate fault-like ICEBP intercept.
     */
    cs->exception_index = EXCP01_DB;
    env->error_code = 0;
    env->exception_is_int = 0;
    env->exception_next_eip = env->eip;
    cpu_loop_exit(cs);
}

G_NORETURN void handle_unaligned_access(CPUX86State *env, vaddr vaddr,
                                        MMUAccessType access_type,
                                        uintptr_t retaddr)
{
    /*
     * Unaligned accesses are currently only triggered by SSE/AVX
     * instructions that impose alignment requirements on memory
     * operands. These instructions raise #GP(0) upon accessing an
     * unaligned address.
     */
    raise_exception_ra(env, EXCP0D_GPF, retaddr);
}
