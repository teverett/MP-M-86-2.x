# Despool Print Utility UG

> OCR text extracted from `Despool_Print_Utility_UG.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `Despool_Print_Utility_UG.pdf`.

---

---
_Page 01_

HW DIGITAL RieskARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

DESPOOL BACKGROUND PRINT UTILITY

OPERATOR'S GUIDE

COPYRIGHT (c) 1979
DIGITAL RESEARCH

---
_Page 02_

Copyright (c) 1979 by Digital Research. All rights reserved.
No part of this publication may be reproduced, transmitted,
transcribed, stored in a retrieval system, or translated into
any language or computer language, in any form or by any
means, electronic, mechanical, magnetic, optical, chemical,
manual or otherwise, without the prior written permission
of Digital Research, Post Office Box 579, Pacific Grove,
California 93950.

Diselaimer

Digital Research makes no representations or warranties
with respect to the contents hereof and specifically
disclaims any implied warranties of merchantability or
fitness for any particular purpose. Further, Digital
Research reserves the right to revise this publication and
to make changes from time to time in the content hereof
without obligation of Digital Research to notify any person
of such revision or changes.

---
_Page 03_

DESPOOL Background Print Utility Operator's Guide

Copyright (c) 1979
Digital Research

1. General.

Most large computer systems perform a function called
printer “spooling” where printer output is directed to a
disk file rather than the physical printer, leading to the
notion that the stream of output characters is wound around
a spool. The spooled printer files are then written to the
physical printer device at a later time in “background mode"
while the computer is performing some other function. This
background print function can be thought of as the converse
of spooling, resulting in a “despooling"” function.

In the CP/M environment, it is often useful to have a
background printing facility, especially when operating with
a low speed printer device, since the time required to print
listings or documents may be inordinately high. In
particular, the “dead time" which occurs while the central
computer waits for console input provides an opportunity for
background operations since printing during this interval
does not affect the throughput of the processor.

The CP/M DESPOOL utility is a program specially
designed for operation with CP/M Versions 1.4 and 2.2, and
provides a background printing facility. When DESPOOL is
active, the operator can cause a disk file to be printed
during the inactive time that CP/M is waiting for console
input. Thus, background printing can take place during
normal interactive CP/M operations such as program editing
and data entry under ED, or test program debugging using SID
or DDT,

Due to the fact that the console command processor
(CCP) is retained in memory whenever DESPOOL is active,
there is a minimum requirement of 18K of main memory for
proper operation,

2. Activating DESPOOL,

The despooling program is started by typing

DESPOOL cr

at the CCP command level, where cr denotes the carriage
return key. The DESPOOL program loads, begins execution
with the sign-on message

\*\* DESPOOLER READY, TYPE CTL-F

and returns to the CCP for subsequent input commands, At
this point, the DESPOOL program is active in memory, and

---
_Page 04_

will remain active until removed by a CP/M cold start, or
ctl-B, as described below.

If the operator attempts to restart the DESPOOL
program when it is already active, the message

DESPOOL ALREADY PRESENT, TYPE CTL<-F
will appear at the console,

Following the initial DESPOOL sign-on message, _ the
operator may proceed with the usual CP/M interaction,
including DIR, ERA, TYPE, and SAVE commands. Further, the
operator can initiate a transient program such as ED, DDT,
PIP, or a user-defined program without affecting the
operation of DESPOOL. Valid activations are:

DESPOOL cr
B:DESPOOL cr
d:despool cr

where, again, cr denotes the carriage return key.
3. Printing a File.

When DESPOOL is’ properly activated, as described
above, the operator can cause DESPOOL to print a disk file
by first typing ctl-F (control and F keys, simultaneously),
which acts as an attention mechanism, whenever CP/M is
reading a console character for the CCP or ae transient
program. When ctl-F is typed, DESPOOL responds with the
prompt:

\*\* PRINT FILE:

indicating that DESPOOL is ready to accept the name of a
file to print. Note that if DESPOOL is already printing a
file, then the current printing operation is discontinued.

The operator then enters a file name using the normal
CCP line editing functions (see the manual “CP/M Features
and Facilities" for line editing functions). For example,
the input: ,

\*\* PRINT FILE: X.PRN cr
causes DESPOOL to print X.PRN from the currently logged disk
in background mode. If the file does not exist, or is an
ambiguous reference, the message:

\*\* NO PRINT FILE

is displayed at the console, If the file is present, the
background print process commences. In either case, DESPOOL
does not retain console control, but instead allows control

---
_Page 05_

to return normal input mode for the program which was
interrupted by the ctl-F key.

Note that the file named above can be preceded by a
drive name in order to override the default drive. Thus,
the following are all valid input file names:

B:GAMMA.ASM
d:delta.dat
X.Y
az:X.Y

Note that once that a background print operation begins, the
operator may change the default drive without affecting the
source drive specified in the DESPOOL input file name.

The following point must be emphasized: the initial
prompt given by the interrupted program cannot be reissued
by DESPOOL following file name input. The following
sequence might occur, for example:
A>despool cr (start the despooler)
\*\* DESPOOLER READY, TYPE CTL-F
A> F (get despooler's attention)
\*\* PRINT FILE:X.Y cr (input file name)
er (single cr gets another A>)
A>

which starts the background print operation for X.Y, and
returns the cursor to the left for more CCP input commands.
Note that in this particular case, the CCP prompt is
recovered by simply typing an additional carriage return, as
denoted by the cr above,

As a final note, you must take care not to erase a
file which is currently being printed by DESPOOL. Although
such action will not destroy the integrity of your data
files, the erase command releases data storage areas which
could be reallocated during the printing process, resulting
in improperly printed data.

4. Warm Starts.

A normal warm start of the CP/M system, caused by
typing ctl-C or at the termination of a transient program,
does not deactivate DESPOOL. Thus, if a file is being
printed while an edit session is in progress, for example,
the edit can be terminated without aborting the print
process. As noted above, however, cold starts reload the
entire CP/M system, and thus cause any background printing
to be discontinued.

5. Deactivating DESPOOL,

DESPOOL is deactivated and removed from memory by

---
_Page 06_

typing ctl-B (control and 65 keys, simultaneously) when in
command input mode. The ctl-B “boot" operation acts like
the .normal ctl-C warm start function when CP/M is operating
without DESPOOL. Note that unlike the cti-F attention key,
etl-B can only be issued in command line input mode, similar
to ctl-C under normal CP/M operation. Further, the ct1-65
key must be the first key typed in the command line, These
restrictions are present to reduce chances of accidental
ctl-B input during command or data entry. Thus, for
example, ctli~B can be entered during editing immediately
following the standard “"\*" ED prompt, while the ctl-B is
ignored when typing characters in input mode using the I
command,

6. Implementation Notes.

The DESPOOL utility is a self-relocating program,
Similar to DDT and SID. Upon initial startup, DESPOOL is

loaded into the base of the Transient Program Area where it: -°\*

receives control from the CCP. The DESPOOL program is then
moved into high memory, directly below the CCP where it
resides’ during the time it is active. The memory image
during DESPOOL operation in a 32K 1.4 CP/M system, for
example, appears as follows: ;

80@@H: ~---------~-——-——---—--
| BIOS |
TJEQ@GH; -~--—-~-————-~-~~—-—-—~—
|
| BDOS |
| |
719GH: ---------~----------
|
| CCP |
| |
69@@H: ---------~-~--—---~-~+
6709H: | PSEUDO BIOS |
| DESPOOL |
| JMP BDOS |
6303H:; --------------------
| |
| TPA |
@19@0H: ----------.-—-------~~
O@OO5SH: | JMP 6593 |
| JMP 6703H |
@GO@GH: -------+----~-—---——.

The normal BDOS jump address at location 0@95H is changed to
reflect the reduced amount of free memory (@188H through
6362H in the example given above). Thus, programs such as

ED, DDT, PIP, and similarly written user programs which-::

“size" memory using the BDOS jump address will operate
properly when DESPOOL is active, Due to the fact that
DESPOOL depends upon physical contiguity with the ccp
module, it cannot be operated under DDT or SID (although DDT
or SID can operate under DESPOOL).

---
_Page 07_

The PSEUDO BIOS portion of the memory image contains a
slightly altered version of the jump vector normally
contained at the beginning of the BIOS. The jump vector is
aligned on an even page boundary, so that it appears
programmatically similar to the jump vector in high memory.
The jump instruction at location @9098H is altered to address
this pseudo jump vector in order to accommodate programs
which directly access the BIOS without calling the CP/M
BDOS. Although there are no Digital Research programs which
directly access the BIOS, there are CP/M compatible language
processors, such as Microsoft and Tarbell BASIC, which
obtain their console characters directly through the BIOS.
Given that a particular program locates the BIOS jump vector
though the address field of the jump at 9900H, it will
operate properly under DESPOOL.

The jump addresses remain intact upon warm start using
etl-C, but are replaced with their original values when
ctl-B is processed.

7. $j% Improving DESPOOL Response,
The DESPOOL program operates by sampling console

status while printing a file in background mode. If no
console input is ready, then the DESPOOL program prints the

next output character. Due to the nature of some printer
devices, this can cause a somewhat "“sluggish" response to
console input characters, In order to alleviate this

condition, DESPOOL can be altered in the field to sample the
printer status as well as the console status. In this case,
printer output characters will not be sent until DESPOOL has
sampled the printer status and found that the printer is
ready to accept another character. When modified in this
manner, the only delays which will be encountered at the
keyboard will be during disk accesses,

The printer status change is accomplished as’ follows.
Write a subroutine in your BIOS (if it is not already there)
which interrogates the printer status and returns one of two
conditions in register A:
A = 66 indicates that the printer is not
ready to accept an output character

A = FF indicates that the printer is ready
to accept an output character (any
non-zero value is treated as FF).

Assume that this subroutine is called PRSTAT, for "printer
status." Augment the normal jump vector at the beginning of
your BIOS to include a vectored jump to the PRSTAT
subroutine, and include this jump at the end of the list
given in the Digital Research manual entitled "CP/M System
Alteration Guide," resulting in a jump vector which appears
as shown below:

---
_Page 08_

JMP BOOT ;COLD BOOT ENTRY

JMP WBOOT ;WARM BOOT ENTRY
JMP CONST ;CONSOLE STATUS
JMP CONIN ;CONSOLE INPUT

JMP CONOUT ;CONSOLE OUTPUT
JMP LIST ;LIST OUTPUT

JMP PUNCH ;PUNCH OUTPUT

JMP READER ;READER INPUT

JMP HOME ;HOME THE DISK

JMP SELDSK ;SELECT DISK DRIVE
JMP SETTRK ;SET TRACK NUMBER
JMP SETSEC ;SET SECTOR NUMBER
JMP SETDMA ;SET DMA ADDRESS
JMP READ READ SECTOR

JMP WRITE ;WRITE SECTOR

JMP PRSTAT ;SAMPLE PRINTER STATUS

The addition of the “JMP PRSTAT" element at the end of the
list results in a total of sixteen jump vectors elements in
the modified BIOS.

Incorporate your modified BIOS into a test CP/M system
using the standard procedures given in the “CP/M System
Alteration Guide.“ Cold start your test system, and load the
DESPOOL program, using the special form:

DESPOOL \*

where the “\*" indicates that DESPOOL is to take printer
Status into account. If the "“\*" is not included, DESPOOL
assumes that printer status is not available, and will not
perform calls on the sixteenth jump vector element (JMP
PRSTAT, above).

Once you have completed your tests, you may make a
permanent change to the DESPOOL memory image using DDT so
that it is not necessary to type the “\*" character each time
you start the DESPOOL program. The change is made by
loading the DESPOOL program into the DDT test program area,
altering a “printer status available" flag, followed by a
SAVE operation which records the altered memory image. The
procedure is shown below:

DDT DESPOOL.COM cr Load DESPOOL under DDT
NEXT PC

Bad G19 Response from DDT

S286 cr Set memory at @6296H
6206 @6 @locr Change flag @@ to @1
9207 cr Stop “S"“ mode

G@ cr Go to @@89H (warm start)
SAVE 16 UNSPOOL.COM cr Record altered program

At this point, you have a test version of DESPOOL, called
UNSPOOL, which has an assumed “\*" input parameter each time
it is operated. Test the new UNSPOOL and, when satisfied

---
_Page 09_

that it operates in the same manner as DESPOOL with the "“\*"
Parameter, propagate this new program throughout your disks
which contain the altered BIOS described above.

---
_Page 10_

---
_Page 11_

FCA S
beers
tit parse gant

: dae \*

ee Re pee ene

peas
te.

pod

---
_Page 12_

FCA S
beers
tit parse gant

: dae \*

ee Re pee ene

peas
te.

pod


