README.TXT                                                    2005-11-30 CDC
Flash BIOS version 1.80 for Portege M200
-----------------------------------------------------------------------------

This BIOS is applicable to the following models:

Portege M200
Portege M200-S218TD
Portege M200-S838
Portege M205
Portege M205-S809
Portege M205-S810

For purposes of this document, the term "Portege M200"
is used to generically refer to all of the models listed above.

This BIOS version is Windows XP compatible and supports Plug and Play, and ACPI functionality.

-------------------------------------------
This archive contains the following files:
-------------------------------------------

README.TXT      Installation/usage instructions in ASCII text format
README.COM      Self-displaying version of installation/usage instructions
1395D18.EXE    Self-extracting diskette image of BIOS install diskette
BIO0009T.COM    Flash BIOS 1.80 image for Portege M200
                (provided here for installation methods 3 and 5)
chgbiosa.exe    Flash BIOS update utility version version 4.34
                (provided here for installation methods 3 and 5)
P0009V180.EXE   Windows-installed BIOS package
LAUNCHER.EXE    BIOS Installation Launcher application. The Launcher
                displays a Windows dialog from which you can choose a BIOS
                installation option appropriate for your system and Windows
                version.  There are various other files in support of the
                BIOS Installation Launcher which are not listed here.
3M200V18.ISO    ISO Image of bootable BIOS Installation CD-ROM
                See instructions below regarding the use of this image


********************** CAUTION! *********************************************
*****************************************************************************

During the upgrading of your computers BIOS, if the computer loses power or
fails to complete the process of upgrading the BIOS, the existing BIOS in
the computer may become damaged.  Toshiba HIGHLY recommends that the computer
is powered via the AC adapter during the entire BIOS update process.

In the event of a power loss during the BIOS update process, the BIOS Key
Installation Method, described below, can sometimes be used to recover from
the failure.  If it does not, you'll need to have your computer serviced by a
Toshiba Authorized Service Provider (ASP).  You can select an ASP in your
area at http://pcrepair.toshiba.com

Toshiba also recommends that you disconnect all peripheral devices from your
computer before you perform the BIOS update.  Peripheral devices include, but
are not limited to, docking devices (Desk Stations, port replicators), PCMCIA
and CardBus cards, USB and IEEE 1394 devices, printers, scanners, digital
cameras, etc.

On some models, it is not unusual to see a CMOS checksum error when you first
reboot the computer after installing the new BIOS.  If you see this message
on your screen, press F1 as instructed.  This displays the "TSETUP" screen.
Press <End> to save the CMOS data.  You won't see the CMOS checksum error
again.

*****************************************************************************


-------------------------------
BIOS INSTALLATION INSTRUCTIONS
-------------------------------

There are five options for installing this BIOS release:

1) From the bootable BIOS installation diskette.  This is the recommeded
   procedure whenever a diskette drive (internal, USB, or PCMCIA) is
   available on the target system.

2) From within Windows NT-based operating systems (XP, 2000, NT 4.0) using
   the Windows-based BIOS installer.

3) At startup by holding down the key immediately to the left of the spacebar.

4) From the bootable BIOS installation CD-ROM

5) Run chgbiosa.exe at the command prompt.


---------------------------------------------------
1) BOOT DISKETTE INSTALLATION METHOD (Recommended)
---------------------------------------------------

The BIOS installation launcher offers the option to create a bootable BIOS
installation diskette.  Simply select 'Diskette Installation' from the
Launcher menu and click 'Ok'.  You'll need to have a diskette drive
(internal, USB, or PCMCIA) installed, and a formatted 1.44MB diskette
available.

Once the BIOS installation diskette has been created, install the BIOS
using the following steps:

      o Connect the computer to AC power using the AC adapter.
      o Close all open programs
      o Insert the BIOS installation diskette into your diskette drive (A:)
      o Shut down the operating system.  With Windows, you may choose to
        Restart rather than shutting down.
      o If necessary, restart the computer by pressing Ctrl-Alt-Del or
        turning the computer OFF then on again

The computer will boot MS-DOS from the BIOS installation diskette, and
initiate the Flash BIOS installation routine.  Please note that on systems
where only a USB-connected diskette drive is supported, you can force the
computer to boot from the USB-connected diskette by holding down the "U" key,
when powering-on the system.

When the Flash BIOS update is complete, a message like the one below will be
displayed:


        Please push the RESET SW (or turn AC power OFF/ON) to restart!

                             ROM Write Successful!
                               Utility Finished!



Eject the diskette from drive A: and turn the computer OFF then ON, or press
the reset switch to restart your computer.


=== ADDITIONAL INFORMATION REGARDING THIS INSTALLATION METHOD ===

If you are unable to use the Windows self-extractor, the self-extracting
ZIP archive can be extracted using PKUnZip, Info-Zip, or an equivalent UnZIP
utility.  The Zip archive was created with Info-Zip (http://www.info-zip.org)
and the self-extractor (SFX) is WinZip (http://www.winzip.com)

The WinImage self-extracting diskette image, 1395D18.EXE, is a 32-bit
Windows application that requires Windows 9x, Me, NT, 2000, or XP.
Information about WinImage is available at http://www.winimage.com.

For Linux or other operating systems, you can use the boot CD-ROM method or
you can create the BIOS installation diskette on a Windows-based computer,
and boot the Toshiba notebook using the BIOS installation diskette.


-----------------------------------------------
2) INSTALLATION FROM WITHIN WINDOWS NT/2000/XP
-----------------------------------------------

This release offers the option to install the BIOS directly from within
Windows NT-based operating systems.  To do so, simply select the 'Windows
Installation' option from the Launcher menu, and click 'Ok'.

IMPORTANT INFORMATION REGARDING THIS INSTALLATION METHOD
Please close all programs before executing the Windows NT-based installer.

Due to the somewhat increased risk of errors updating the BIOS from within
Windows (as compared to using the boot diskette method), it is HIGHLY
recommended that the computer be connected to AC power via the AC adapter,
and that the power-save mode is set to 'Full Power' or the equivalent while
installing the BIOS.

The Windows NT-based BIOS installer, P0009V180.EXE, will install the flash
BIOS when run under Windows NT 4.0, Windows 2000, or Windows XP.

The Windows NT-based installer requires Administrator priveleges on the
target system, and, for Windows 2000/XP, requires that the Toshiba Value-
added Logic Device driver (TVALD, or ACPI common modules) be installed and
working.

Windows 95, 98, and Me are not supported.  If executed under Windows 9x/Me,
the Launcher will not offer the option to install from within Windows.

--------------------------------
3) BIOS KEY INSTALLATION METHOD
--------------------------------

Your Toshiba computer has a special keyboard function to install an
updated BIOS image.  In this section, the term "BIOS key" refers to the
key immediately to the left of the spacebar.

To use this function, you can use the BIOS installation diskette created
using method 1, above, or simply copy chgbiosa.exe and BIO0009T.COM to a
diskette.  Then insert the diskette and hold down the BIOS key while the
computer is powered on.  The computer MUST be in BOOT mode, not Resume
or hibernate mode.

This installation method will only work with the internal keyboard.  If
your computer is docked with the display closed and you are using an external
keyboard, the computer will need to be undocked and the internal keyboard's
key immediately to the left of the spacebar used.

When the computer starts up with the BIOS key depressed, the following
message appears:

'Ready for BIOS update. Place the BIOS diskette in the drive, and press
any key when ready to proceed.'

On some models (Satellite 5005 and others) where a system status LCD is
included, this and all messages displayed during the BIOS key installation
processs will appear in the system status LCD rather than on the main
display panel.

Insert the diskette that contains chgbiosa.exe and BIO0009T.COM,
and press any key.  The chgbiosa program will automatically load the
BIO0009T.COM file.


---------------------------------------------------
4) BOOT CD-ROM INSTALLATION METHOD
---------------------------------------------------
If your Toshiba PC is equipped with a bootable CD-ROM device (CD-ROM,
DVD-ROM, DVD-RW, DVD-Multi, etc.), and you have access to a CD writer
(burner, either installed in your Toshiba PC or in another PC), you can create
a bootable BIOS installation CD-ROM using the ISO CD image, 3M200V18.ISO,
provided in this distribution.

If you have CD burning software on your computer, chances are good it has set
itself up as the default handler for ISO CD-ROM images.  If this is the case,
the option to create the bootable CD-ROM from the BIOS Launcher, will launch
your CD burning software, ready to burn the ISO image to CD.  If your software
hasn't set itself up as the default handler for ISO images, we'll recognize
that fact, and let you know.

Once you create the bootable CD-ROM, simply boot your computer from the
resulting CD.  To boot from CD, you can either hold down the "C" key while
you turn your computer on, or you can use HWSetup to change the boot priority
so the CD drive boots before the HDD.

Note, if your CD burner supports it, you can use a rewritable CD (CD-RW) disc
to create the bootable BIOS installation CD-ROM.  Be sure to erase the
rewritable disc (if it has data on it) before you proceed.  Once you are
finished updating your system's DMI values, you can erase and re-use the
rewritable disc.

IMPORTANT NOTE: Creating the bootable BIOS installation CD requires a specific
                set of steps that is unique to each CD authoring application.
                You cannot simply copy the ISO image to a blank CD and expect
                to be able to boot from it.

-----------------------------------------------------------------------------
Here are brief instructions for creating a CD from the ISO image using widely
available software.
-----------------------------------------------------------------------------

* Roxio Easy CD Creator:
  Easy CD Creator 5, bundled with some Toshiba PCs, allows you to initiate
  the ISO image to CD burning process by simply double-clicking the .ISO file
  in Windows Explorer. Simply browse to the folder into which you downloaded
  the ISO image file and double-click to launch Easy CD Creator. Then put a
  blank CD-R media in your drive and click on the 'Record' button to begin
  creating the CD.

  Information about Easy CD Creator is available at http://www.roxio.com


* Nero:
  Nero 6 is available for download as a fully-functional demo. Nero can be
  used to create a CD from your ISO image using either of the two following
  techniques:

	o In Nero Burning ROM go to menu option 'Recorder --> Burn Image' and
	  navigate to the drive/folder containing the ISO image you wish to
	  burn to CD.

	o In Nero Express select the option for 'Disc Image or Saved Project'
	  to select the ISO image file and burn to CD.

More information about Nero 6 and a downloadable demo is available at
http://www.nero.com


Drag 'n Drop CD:
The Drag 'n Drop CD application bundled with some Toshiba PC models does not
support creating a CD from an ISO image.  You'll need to use a different CD
burning application. If Roxio Easy CD Creator (see above) is not on your PC,
one application that can be used with Windows XP is Alex Feinman's free "ISO
Recorder Power Toy". This software allows you to right-click on .iso files and
use Windows XP's built-in CD writing wizard to create CDs from ISO image files.

Information about the ISO Recorder Power Toy is available at
http://isorecorder.alexfeinman.com/isorecorder.htm


Note: Information about non-bundled software is provided "for your
      information".  Toshiba neither recommends nor supports software that
      is not bundled with our computers or purchased from toshibadirect.com.


-------------------------------------------
5) RUN chgbiosa.exe AT THE COMMAND PROMPT
-------------------------------------------

The chgbiosa.exe program is designed to be run from the MS-DOS command
prompt.  It cannot be run in a DOS box inside of Windows, OS/2 or other
operating systems.  It also will not run if a Memory Manager, like EMM386,
is loaded.

The chgbiosa.exe program can be run from a floppy disk, hard drive, or
PCMCIA drive.  However, you will need a floppy diskette (high or low density)
in your A: drive at the time the chgbiosa.exe program is run.

When the chgbiosa.exe program is executed by itself, it will prompt you
to specify where the BIOS image file, BIO0009T.COM is located.  Enter
the complete path and file name. Example: C:\3m200v18\BIO0009T.COM.


NOTE!
If your path for BIO0009T.COM is anywhere other than the root directory
of the A: drive (A:\BIO0009T.COM), chgbiosa.exe will copy the BIOS image
file from its current location to the root directory of A: drive.


Optionally, the chgbiosa program can also be run from a single command
which includes the path to the BIO0009T.COM file.

   Example: chgbiosa C:\3m200v18\BIO0009T.COM


--------------------------------------
CHANGE HISTORY for Portege M200
--------------------------------------

Version 1.80  11-01-2005
   o Improved stability with a specific vendor's memory module.


Version 1.70  08-08-2005
   o An Intel Pentium-M microcode update was applied.


Version 1.60  07-27-2004
   o Corrected an issue where LAN (Network) boot was not available after
     setting LAN (Network) as any priority in "Boot Priority" setting.
   o Corrected an issue where the system would sometimes blue-screen after
     standby/resume on Windows.


Version 1.40  07-16-2004
   o Made a change to support CPU Dothan B1-step.


Version 1.30  03-22-2004
   o Added support to boot from SD cards of the following sizes: 16MB, 32MB,
     64MB, 128MB, 256MB and 512MB.


Version 1.20  02-25-2004
   o A change was made to include a description for Calexico2 in the Intel
     Logo at system start-up.
   o Made a change to ensure that an external USB HDD, USB memory, etc. when
     connected to the #4 USB port on the Advanced Port Replicator III will be
     safely removed after stopping the device.
   o A change was made to ensure the system will recognize the Advanced Port
     Replicator III or Tablet Multi-Dock at the next docking if the eject
     switch was pressed twice prior to ejecting the system.


Version 1.10  01-29-2004
   o Initial production BIOS for Portege M200/M205





 **end**
