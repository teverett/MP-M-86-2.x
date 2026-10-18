# DDT

> OCR text extracted from `DDT.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `DDT.pdf`.

---

---
_Page 01_

HW DIGITAL AESEARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

CP/M DYNAMIC DEBUGGING TOOL (DDT)
USER'S GUIDE

COPYRIGHT (ce) 1976, 1978
DIGITAL RESEARCH

---
_Page 02_

Copyright (ec) 1976, 1978 by Digital Research. All rights
reserved. No part of this publication may be reproduced,
transmitted, transcribed, stored in a retrieval system, or
translated into any language or computer language, in any
form or by any means, electronic, mechanical, magnetic,
optical, chemical, manual or otherwise, without the prior
written permission of Digital Research, Post Office Box 579,
Pacific Grove, California 93950.

Disclaimer

Digital Research makes no representations or warranties with
respect to the contents hereof and specifically disclaims any
implied warranties of merchantability or fitness for any
particular purpose. Further, Digital Research reserves the
right to revise this publication and to make changes from
time to time in the content hereof without obligation of
Digital Research to notify any person of such revision or
changes.

---
_Page 03_

I DIGITAL RESEARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

CP/M DYNAMIC DEBUGGING TOOL (DDT)
USER’S GUIDE

COPYRIGHT (c) 1976, 1978

DIGITAL RESEARCH

---
_Page 04_

Copyright (c) 1976, 1978 by Digital Research. All rights reserved.
No part of this publication may be reproduced, transmitted, transcribed,
stored in a retrieval system, or translated into any language or computer
language, in any form or by any means, electronic, mechanical, magnet-
ic optical, chemical, manual or otherwise, without the prior written
permission of Digital Research, Post Office Box 579, Pacific Grove,
California 93950.

Disclaimer

Digital Research makes no representations or warranties with respect to
the contents hereof and specifically disclaims any implied warranties of
merchantability or fitness for any particular purpose. Further, Digital
Research reserves the right to revise this publication and to make
changes from time to time in the content hereof without obligation of
Digital Research to notify any person of such revision or changes.

---
_Page 05_

Table of Contents
Section Page

I. INTRODUCTION @eeseecoesveooespsnesneeneese0eeaee0eee0020e@002080086
II. DDI COMMANDS @ee@eeoeoovoeevseeoeeseeaeseeecevovneseeseeneeeoen ees
1. The A (Assemble) Command ..cccccccccccccccccce
2. The D (Display) Command .ccccccccccscccccccces
3. The F (Fill) ComMMAN cccoccccccccccsccecccvecce
4. The G (Go) Command SCHRECHSHOHEHHEHOSCHEHSSESOCHHOEHEDO®O
5. The I (Input) Command ccccccccccccrsecccscccccs
66 The L (List) Command @eeeeoeeeeoeoeoeooeeeeaeeeaeeoeose eee
7e The M (Move) Command @eeeecesooeeoncooeaoseeee eeu eeeene
8. The R (Read) Command ceccccccccecsccccccccvccce
9. The S (Set) Command @eeeseeoeoeoooeseeeee ee eeeoe eed
16. The T (Trace) Command .cccccccccccccccccccccce
11. The U (Untrace) Command .ccccccccccccccsccccce
12. The X (Examine) Command cecccccccccccccccceccs
III. IMPLEMENTATION NOTES @eeeeneoeeeseeoosoeoeoseeeeeeeeeveneee @
IV. AN EXAMPLE @eeeoeaoceeeeeeeoeceeeeeceeeoaceeceeeoovoeaen eee eet

FO DDTWIAKAAU PS & PW WP

---
_Page 06_

---
_Page 07_

CP/M Dynamic Debugging Tool (DDT)

Users Guide

I. Introduction.

The DDI program allows dynamic interactive testing and debugging of
programs generated in the CP/M environment. The debugger is initiated by
typing one of the following commands at the CP/M Console Command level

DDI
DDI filename .HEX
DDI filename .COM

where "filename" is the name of the program to be loaded and tested. In both
cases, the DDI program is brought into main memory in the place of the Console
Command Processor (refer to the CP/M Interface Guide for standard memory
organization), and thus resides directly below the Basic Disk Operating System
portion of CP/M. The BDOS starting address, which is located in the address
field of the JMP instruction at location 5H, is altered to reflect the reduced
Transient Program Area size.

The second and third forms of the DDI command shown above perform the same
actions as the first, except there is a subsequent automatic load of the
specified HEX or COM file. The action is identical to the sequence of
commands

DPT
Ifilename HEX or Ifilename.COM
R

where the I and R cammands set up and read the specified program to test (see
the explanation of the I and R commands below for exact details).

Upon initiation, DDT prints a sign-on message in the format
nnK DDI-s VER mm

where nn is the memory size (which must match the CP/M system being used), s
is the hardware system which is assumed, corresponding to the codes

- Digital Research standard version
- MDS version

IMSAI standard version

- Omron systems

- Digital Systems standard version

MNOnHRBY
i}

and mm is the revision number.

---
_Page 08_

Following the sign on message, DDT prompts the operator with the character
“."" and waits for input commands from the console. The operator can type any
of several single character commands, terminated by a carriage return to
execute the cammand. Each line of input can be line-edited using the standard
CP/M controls

rubout remove the last character typed
ctl-U remove the entire line, ready for re-typing
cti-C system reboot

Any canmand can be up to 32 characters in length (an automatic carriage return
is inserted as the 33rd character), where the first character determines the
command type

enter assembly language mnemonics with operands
display memory in hexadecimal and ASCII

fill memory with constant data

begin execution with optional breakpoints

set up a standard input file control block

list memory using assembler mnemonics

move a memory segment from source to destination
read program for subsequent testing

substitute memory values

trace program execution

untraced program monitoring

examine and optionally alter the CPU state

KSCHNDEOHOAMO YS

The canmand character, in some cases, is followed by zero, one, two, or three
hexadecimal values which are separated by commas or single blank characters.
All DDI numeric output is in hexadecimal form. In all cases, the commands are
not executed until the carriage return is typed at the end of the command.

At any point in the debug run, the operator can stop execution of DDT
using either a ctl-C or G@ (jmp to location @@@0H), and save the current
memory image using a SAVE command of the form

SAVE n filename .COM

where n is the number of pages (256 byte blocks) to be saved on disk. The
number of blocks can be determined by taking the high order byte of the top
load address and converting this number to decimal. For example, if the
highest address in the Transient Program Area is 1234H then the number of
pages is 12H, or 18 in decimal. Thus the operator could type a ctl1-C during
the debug run, returning to the Console Processor level, followed by

SAVE 18 X.COM
The memory image is saved as X.COM on the diskette, and can be directly

executed by simply typing the name X. If further testing is required, the
memory image can be recalled by typing

---
_Page 09_

DDT X.COM

which reloads previously saved program from loaction 1@@H through page 18
(12FFH). The machine state is not a part of the COM file, and thus the
program must be restarted from the beginning in order to properly test it.

II. DDI COMMANDS.

The individual commands are given below in some detail. In each case, the
operator must wait for the prompt character (-) before entering the command,
If control is passed to a program under test, and the program has not reached
a breakpoint, control can be returned to DDI by executing a RST 7 from the
front panel (note that the rubout key should be used instead if the program is
executing a T or U command). In the explanation of each command, the command
letter is shown in some cases with numbers separated by cammas, where the
numbers are represented by lower case letters. These numbers are always
assumed to be in a hexadecimal radix, and from one to four digits in length
(longer numbers will be automatically truncated on the right).

Many of the commands operate upon a "CPU state” which corresponds to the
program under test. The CPU state holds the registers of the program being
debugged, and initially contains zeroes for all registers and flags except for
the program counter (P) and stack pointer (S), which default to 1@@H. The
program counter is subsequently set to the starting address given in the last
record of a HEX file if a file of this form is loaded (see the I and R
commands) .

1. The A (Assemble) Command. DDI allows inline assembly language to be
inserted into the current memory image using the A command which takes the
form

As

where Ss is the hexadecimal starting address for the inline assembly. DDI
prompts the console with the address of the next instruction to fill, and
reads the console, looking for assembly language mnemonics (see the Intel 8888
Assembly Language Reference Card for a list of mnemonics), followed by
register references and operands in absolute hexadecimal form. Each sucessive
load address is printed before reading the console. The A command terminates
when the first enpty line is input from the console.

Upon campletion of assembly language input, the operator can review the
memory segment using the DDT disassembler (see the L command).

Note that the assembler/disassembler portion of DDI can be overlayed by
the transient program being tested, in which case the DDI program responds
with an error condition when the A and L commands are used (refer to Section
IV).

---
_Page 10_

2. The D (Display) Command. The D command allows the operator to view
the contents of memory in hexadecimal and ASCII formats. The forms are

D
Ds
Ds ,f£

In the first case, memory is displayed from the current display address
(initially 1@@H), and continues for 16 display lines. Each display line takes
the form shown below

aaaa bb bb bb bb bb bb bb bb bb bb bb bb bb bb bb bb ccccccccccceccce

where aaaa is the display address in hexadecimal, and bb represents data
present in memory starting at aaaa. The ASCII characters starting at aaaa are
given to the right (represented by the sequence of c’s), where non-graphic
characters are printed as a period (.) symbol. Note that both upper and lower
case alphabetics are displayed, and thus will appear as upper case symbols on
a console device that supports only upper case. Each display line gives the
values of 16 bytes of data, except that the first line displayed is truncated
so that the next line begins at an address which is a multiple of 16.

The second form of the D command shown above is similar to the first,
except that the display address is first set to address s. The third form
causes the display to continue from address s through address f. In all
cases, the display address is set to the first address not displayed in this
command, so that a continuing display can be accomplished by issuing
successive D commands with no explicit addresses.

ExcesSively long displays can be aborted by pushing the rubout key.

3. The F (Fill) Command. The F command takes the form
Fs,f,c

where s is the starting address, f is the final address, and c is a
hexadecimal byte constant. The effect is as follows: DDI stores the constant
c at address s, increments the value of s and tests against f. If s exceeds f
then the operation terminates, otherwise the operation is repeated. Thus, the
fill canmand can be used to set a memory block to a specific constant value.

4. The G (Go) Command. Program execution is started using the G comand,
with up to two optional breakpoint addresses. The G command takes one ot the
forms

G
Gs
Gs ,b

---
_Page 11_

Gs,b,c
G,b
G,b,c

The first form starts execution of the program under test at the current value
of the program counter in the current machine state, with no breakpoints set
(the only way to regain control in DDI is through a RST 7 execution). The
current program counter can be viewed by typing an X or XP command. The
second form is similar to the first except that the program counter in the
current machine state is set to address s before execution begins. The third
form is the same as the second, except that program execution stops when
address b is encountered (b must be in the area of the program under test).
The instruction at location b is not executed when the breakpoint is
encountered, The fourth form is identical to the third, except that two
breakpoints are specified, one at b and the other at c. Encountering either
breakpoint causes execution to stop, and both breakpoints are subseguently
cleared. The last two forms take the program counter from the current machine
state, and set one and two breakpoints, respectively.

Execution continues from the starting address in real-time to the next
breakpoint. That is, there is no intervention between the starting address
and the break address by DDT. Thus, if the program under test does not reach
a breakpoint, control cannot return to DDI without executing a RST 7
instruction. Upon encountering a breakpoint, DDI stops execution and types

\*d

where d is the stop address. The machine state can be examined at this point
using the X (Examine) command, The operator must specify breakpoints which
differ from the program counter address at the beginning of the G command.
Thus, if the current program counter is 1234H, then the commands

G,1234
and
G400,406

both produce an immediate breakpoint, without executing any instructions
whatsoever.

5. The I (Input) Command. The I command allows the operator to insert a
file name into the default file control block at SCH (the file control block
created by CP/M for transient programs is placed at this location; see the
CP/M Interface Guide). The default FCB can be used by the program under test
as if it had been passed by the CP/M Console Processor. Note that this file
name is also used by DDT for reading additional HEX and COM files. The form
of the I command is

Ifilename
or

---
_Page 12_

Ifilename. filetype

If the second form is used, and the filetype is either HEX or COM, then
subsequent R commands can be used to read the pure binary or hex format
machine code (see the R command for further details).

6. The L (List) Command. The L command is used to list assembly language
mnemonics in a particular program region. The forms are

L
Ls
Ls ,f

The first cammand lists twelve lines of disassembled machine code from the
current list address. The second form sets the list address to s, and then
lists twelve lines of code. The last form lists disassembled code from s
through address f. In all three cases, the list address is set to the next
unlisted location in preparation for a subsequent L_ command. Upon
encountering an execution breakpoint, the list address is set to the current
value of the program counter (see the G and T commands). Again, long typeouts
can be aborted using the rubout key during the list process.

7. The M (Move) Command. The M command allows block movement of program
or data areas from one location to another in memory. The form is

Ms,f£,d

where s is the start address of the move, f is the final address of the move,
and d is the destination address. Data is first moved from s to d, and both
addresses are incremented. If s exceeds f£ then the move operation stops,
otherwise the move operation is repeated.

8. The R (Read) Command. The R command is used in conjunction with the I
command to read COM and HEX files from the diskette into the transient program
area in preparation for the debug run. The forms are

R
Ro

where b is an optional bias address which is added to each program or data
address as it is loaded. The load operation must not overwrite any of the
system parameters from @@@H through @FFH (i.e., the first page of memory). If
b is amitted, then b=-090@ is assumed. The R command requires a previous I
command, specifying the name of a HEX or COM file. The load address for each
record is obtained from each individual HEX record, while an assumed load
address of 10@H is taken for COM files. Note that any number of R commands
can be issued following the I command to re-read the program under test,

---
_Page 13_

assuming the tested program does not destroy the default area at SCH.
Further, any file specified with the filetype “COM” is assumed to contain
machine code in pure binary form (created with the LOAD or SAVE command), and
all others are assumed to contain machine code in Intel hex format (produced,
for example, with the ASM command).

Recall that the command
DDT filename.filetype
which initiates the DDT program is equivalent to the commands

DDI
-Ifilename.filetype

Whenever the R command is issued, DDI responds with either the error indicator
"2" (file cannot be opened, or a checksum error occurred in a HEX file), or
with a load message taking the form

NEXT PC
nnnn pppp

where nnnn is the next address following the loaded program, and pppp is the
assumed program counter (19@H for COM files, or taken from the last record if
a HEX file is specified).

9. The S (Set) Command. The S command allows memory locations to be
examined and optionally altered. The form of the command is

Ss

where s is the hexadecimal starting address for examination and alteration of
memory. DDI responds with a numeric prompt, giving the memory location, along
with the data currently held in the memory location. If the operator types a
carriage return, then the data is not altered. If a byte value is typed, then
the value is stored at the prompted address. In either case, DDI continues to
prompt with successive addresses and values until either a period (.) is typed
by the operator, or an invalid input value is detected.

19. The T (Trace) Command. The T command allows selective tracing of
program execution for 1 to 65535 program steps. The forms are

Tv
Tn

In the first case, the CPU state is displayed, and the next program step is
executed. The program terminates immediately, with the termination address

---
_Page 14_

displayed as
\*hhhh

where hhhh is the next address to execute. The display address (used in the D
command) is set to the value of H and L, and the list address (used in the L
command) is set to hhhh. The CPU state at program termination can then be
examined using the X command.

The second form of the T command is similar to the first, except that
execution is traced for n steps (n is a hexadecimal value) before a program
breakpoint is occurs. A breakpoint can be forced in the trace mode by typing
a rubout character. The CPU state is displayed before each program step is
taken in trace mde. The format of the display is the same as described in
the X command.

Note that program tracing is discontinued at the interface to CP/M, and
resumes after return from CP/M to the program under test. Thus, CP/M
functions which access I/O devices, such as the diskette drive, run in
real-time, avoiding I/O timing problems. Programs running in trace mode
execute approximately 50@ times slower than real time since DDI gets control
after each user instruction is executed. Interrupt processing routines can be
traced, but it must be noted that commands which use the breakpoint facility
(G, T, and U) accomplish the break using a RST 7 instruction, which means that
the tested program cannot use this interrupt location. Further, the trace
mode always runs the tested program with interrupts enabled, which may cause
problems if asynchronous interrupts are received during tracing.

Note also that the operator should use the rubout key to get control back
to DDI during trace, rather than executing a RST 7, in order to ensure that
the trace for the current instruction is completed before interruption.

11. The U (Untrace) Command. The U command is identical to the T command
except that intermediate program steps are not displayed. The wntrace mode
allows from 1 to 65535 (@FFFFH) steps to be executed in monitored mode, and is
used principally to retain control of an executing program while it reaches
steady state conditions. All conditions of the T command apply to the U
command,

12. The X (Examine) Command. The X command allows selective display and
alteration of the current CPU state for the program under test. The forms are

x
Xr

where r is one of the 808@ CPU registers

C Carry Flag (8/1)
Z Zero Flag (@/1)

---
_Page 15_

M Minus Flag (8/1)

E Even Parity Flag (8/1)

I Interdigit Carry (@/1)

A Accumulator (Q-FP)

B BC register pair (@-FFFF)
D ODE register pair (Q-FFFF)
H HL register pair (O-FFFF)
S Stack Pointer (O-FFFF)
P Program Counter (Q-FFFF)

In the first case, the CPU register state is displayed in the format
C£Z£M£ELI£ A=bb B=dddd D=dddd H=dddd S=dddd P=dddd inst

where f is a @ or 1 flag value, bb is a byte value, and dddd is a double byte
quantity corresponding to the register pair. The "inst" field contains the
disassembled instruction which occurs at the location addressed by the CPU
state’s program counter.

The second form allows display and optional alteration of register values,
where r is one of the registers given above (C, Z, M, E, I, A, B, D, H, S, or
P). In each case, the flag or register value is first displayed at the
console. The DDI program then accepts input from the console. If a carriage
return is typed, then the flag or register value is not altered. If a value
in the proper range is typed, then the flag or register value is altered.
Note that BC, DE, and HL are displayed as register pairs. Thus, the operator
types the entire register pair when B, C, or the BC pair is altered.

III. IMPLEMENTATION NOTES.

The organization of DDI allows certain non-essential portions to be
overlayed in order to gain a larger transient program area for debugging large
programs. The DDI program consists of two parts: the DDI nucleus and the
assembler/disassembler module. The DDI nucleus is loaded over the Console
Command Processor, and, although loaded with the DDI nucleus, _ the
assembler/disassembler is overlayable unless used to assemble or disassemble.

In particular, the BDOS address at location 6H (address field of the JMP
instruction at location 5H) is modified by DDT to address the base location of
the DDI nucleus which, in turn, contains a JMP instruction to the BDOS. Thus,
programs which use this address field to size memory see the logical end of
memory at the base of the DDI nucleus rather than the base of the BDOS.

The assembler/disassembler module resides directly below the DDT nucleus
in the transient program area. If the A, L, T, or X commands are used during
the debugging process then the DD? program again alters the address field at
6H to include this module, thus further reducing the logical end of memory.
If a program loads beyond the beginning of the assembler/disassembler module,
the A and L commands are lost (their use produces a "?" in response), and the

---
_Page 16_

trace and display (T and X) commands list the “inst” field of the display in
hexadecimal, rather than as a decoded instruction,

IV. AN EXAMPLE.

i j imple

The following example shows an edit, assemble, and debug for a simp]
program which reads a set of data values and determines the largest value in
the set. The largest value is taken from the vector, and stored into "LARGE

at the termination of the program

at °
"lL, f StU rotot ech
f2 ORG tr" 106H =—-_LALSTART OF TRANSIENT AREA,
MvI B-LEH iLENGTH OF VECTOR TO SCAN,
NVI c.6 /LARGER-RST VALUE 90 FAR,
LOOP_.P_O_0_L LXI He VECT i ,
Loop: Mov wr {GET VALUE,
eee nom LARGER VALUE IN €7,
qe ya NC NFQUND ;JUMP IF USRGER VALUE NO OUND
5 yu" NEW LARGEST VALUE, STORE IT To C)
Hoy CA
NFOUND: ‘INX a? sTO NEXT ELEMENT
DCR B pMORE TO SCAN? ° Create Source
. ONZ LOOP iFOR ANOTHER, Progvam. - underlined
i END OF SCAN. STORE C, Chataclers typed
HOV A.C iGET LARGEST VALUE
Sta LARGE , a ky Program mer,
JHP 2. ;REBOOT, "" vewesects Caiage
he TEST DATA return.
VECT: DB 2.0.4, 3,5, 6.1.5)
CEN EQu F=VECT LENGTH,
LARGE: DS a jLARGEST VALUE OW EXIT |
4, END »
+BaP,
ORG 1@8H iSTART OF TRANSIENT AREA
YI B-LEH  iLENGTH OF ¥ECTOR TO SCAN
HVI C.8 ;LARGEST VALUE SO FaR
Lx1 H»VECT ;B8ASE OF VECTOR
LOOP: Hoy Al iGET VALUE
SUB c iLARGER VALUE IN ©?
UNC NFOUND iJUMP IF LARGER YALUE NOT FOUND
i HEW LARGEST VALUE, STORE IT TOC
Hoy c.A |
NFOUND: INK H ;TO NEXT ELEMENT
DCR B sMORE TO SCAN?
NZ LOOP iFOR ANOTHER

---
_Page 17_

i ENB GF SCAN. STORE C

MOY AC jGET LARGEST VALUE
STA LARGE
JMP 2) iREBOOT
F TEST DATA
VECT: DB 2.0,4,3,5,6,1,5
LEN E QU $-VECT ;iLENGTH
LARGE: bs i sLARGEST VALUE ON ERITT

END ;
\*Ey a— End Edit

REM SERN? Start Ascewbler

CP/M ASSEMBLER - VER 1.86

at22
@@2K USE FACTOR

END oF assenecy Aggemlly Comolete - Lock at Froxram Lishng

TYPE SEAR PRN

Code hddwes £) fa ( Soule Flogram
t

ORG 1@@H ; START OF TRANSIENT AREA
0180 8668 My I B, LEN ;LENGTH OF VECTOR TO SCAN
9182 BEBe MY I C,8 ;LARGEST VALUE SO FAR
@1@4 211981 LXI H, ¥ECT «BASE OF YECTOR
@187 7E LOOP: MOY ADM ;GET VALUE
@168 91 SUB c ; LARGER VALUE IN' C7
e189 D2eDB1 JNC  NFOUND ; JUMP IF LARGER VALUE NOT FOUND
; NEW LARGEST VALUE; STORE IT To
@10C 4F MOY C:A
@18D 23 NFOUND: IN¥ H ;TO NEXT ELEMENT
O18E 65 BCR B ;MORE TO SCAN?
@16F C2e761 JNZ LOOP ;FOR ANOTHER
j END OF SCAN, STORE C
6112 79 MOY aC iGET LARGEST YALUE
@113 322181 STA LARGE
9116 crane ; JUMP a i REBOOT
Code / isting j
trunvated F TEST DATA
@119 @2GG04G305VECT: DB 2,8,4,3,5,6,1,5
oaas = ¢ LEN Eau $-VECT LENGTH
@121 Value cf LARGE: DS i sLARGEST VALUE ON EXIT
@122 quate END

---
_Page 18_

DUT SCAN. HEX

> Start Debumer usvey hex Swat machine Ode

16K DET VER 1.6

NEXT PC
0121, e8e6 .
~~ Sask Load addvess + | (pec struck

2 +o execute af
COZGMGEGIG A=00 B=8000 D=800H H=Ga08 S=@10G P=aaGa OUT 7F geo
-xP -
“8 NS came vegrsters loetae debug run
P=0608 168

—~ Change PC-0 (00
“X Look at veasere again PC clanged.
COZOMBEGI@ A=BO B=068G N=8G80 H=e008 S-Giae P=010a M¥I 8,48" )
L189
4 Nad iuctvuction

186 MMVI B,68 ty tecuce at Pe=\vo

8192 MY¥I C.9@
6104 LXI H-@119
8167 MOY ALM

8198 SUB C
8199 JNC @16D Disaccembled Machine
@iGC MOV CGA

@1@D INK H Code at 00H
Bie@e BCR B Kee Sauce is

Bi@F JNZ @1a7 W COMPOY (S 0

B112 MOV Alc

KL

=}

@iiS STA GBizi
i116 JUMP 6098
8119 STAX B
G@1LA NOP

@11B INR & A Ite move

B11 INX B wachine wade

11D DCR B

@i1E M¥I Bet (nate thal Program
6120 DCR & ends at location 16

@121 LXI 0. 2208 wet, a Tue 4 0000) .

alié over iuline assembly mode to change tie TMP 4 COW Into a RST 7, whid,
, will Cause the program under est to vetum 4o ODT of VbH

——F GY Oxecuted.

B117, (6ingle carnage velurn steps assemble node) .

“L113, Lack Coole at 13K 4o check Hal R517 was Proper in
e113 sTa 6121 lM place cS IMP

6116 RST 47

B116 RST ?

---
_Page 19_

Gii7 NOP

@iiS NOP
@119 STAX B
Bila NOP

@iile INR 8B
@iic INK B

-x Lok al veaiske

~~?

COZ@MBEGIG A=4G B=208G D=8G06 H=GGG8 $=G1G@ P9140 MVI

“Ly, Execute Progvamor One Ske. mutta CRU Stak.» before J is execuked

COZGMAE@1G A=46 B=4006 D=ReeG H=anGe

4) “Tae ont kP asain (note OFM in B)

COZ@MBEG@IG A=46 B=4968 D=8G0G H=Hb08

? “Tvace again ( Resist C « cleared)

CGOZOMGESIG@ A=AG B=H980 D=8G66 H=aeaB

12) Trace thee skes

CUZGMBEGIG A=66 GB=a860 D=H66% H=G119
COZOMGE@IG A=82 B=e868 b=860G H=8119
COZGMBEOI1 A=G62 B=48868 D=8e88 H=8119

“ELS, olay memos darting at (IA.

011952 Ge 84 a5 a5 a5 op Maram deh

si20\es/il o@ 22 21 80 @2 7E EB 77 13
8138 C2 27 Of C3 O3 25 G8 0 OB ae AG
B14 88 GG G2 OB 28 BG BB 26 G2 20 BG
8158 82 GG 68 OB 26 OG BB OG 2A a8 ae
B160 86 GO OG 3B 88 89 42 9G BB 2G ae
2176 BG G8 88 28 G6 BG GB OO 28 OB Be
8180 BG GG 4 88 GB BG BO BO 28 OB ag
8190 88 84 BG 2B 28 88 OO AG 28 26 BO
A1AG BG 08 20 86 a6 B88 BO 2G 28 20 Ae
B1BG 82 GA BA 2B 6B BH BO Ba BB ae Be

b1CG 88 GH OG BD OB 826 BE BH BB AaB Be

—) Current CPu state

COZG@MNGEGI1 A=82 B=8800 D=400@6 H=4119

yy Trace S tes fiom Current CPU Stak
COZ@MBE@I1 A=@2 B=R8806 N=0000 H=0119
COZOMBEG@I1 A=O2 B=8880 D=0G08 H=altA
COZGMBEGI1 A=62 B=0700 D=8008 H=G11A
COZGMBEQI1 A=62 B=G700 D=60@6 H=G11A
COZGMBEGI1 A=66 B=8700 f=8008 H=@1iA

“ey ? Trace without listing wwtenrmed tade- states

CUZIMBELI1L RA=86 B=G760 D=@608 H=411846
“Ky COU State at tudat US

COZ@MBELT1 A=04 B=4600 D=600G H=G118

$=@100 P=@1@G MY¥I B,&8\*G102
Gudowahe Wedkpowt J

S=G10@ P=9102 MVI C,@6\*0164

S=@10G P=@164 LXI H,6119\*0187

S=@1G0 P=G147 MOY A.M

S$=@10@9 P=9108 SUB Cc

S=91@0 P=@149 JNC G@1@D\*@10D

Giromahe veto spk

23 EB on (ay at © We. @

a@ aa ae.” S

aa aa 38 30 a@

0G @4 88 Bo Be Dab 1 dished

a@ 09 24 26 ae “iw ASCE te ae”

@@ 8G 6 Oa 46 ti 7

ae a9 ga aa oe MM wate not

GG 88 08 Ba 2@ arr

G0 94 aa 96 20 dhovatlers

ag G8 oa 3e 28

2@ 84 88 ag ae

S=@1GG P=@1GD INK H

S=Q@100 P=G1@D INK 4H

S=@10G P=@1@E NCR 8B die

S=@1G@ P=@1@F JNZ 8167 Bregkpout

S=G10@ P=@147 MOV a.

S=Gi@G@ P=@108 SUB C#a109

S=OQ1GG@ P=8149 JNC @16D\*0198

S=910@ P=H148 SUR C

B,&8

---
_Page 20_

“Ey Pun Program Som curect PC until ComPletion Lia real-time)
rate \preakeowt ot UGH: coused by executing RST 7 tn rnaclune Code
“Y CRU Skte at end & Pram

COZIMBE111 A=86 B=8006 D=8000 H=G121 S=G14@ P=@116 RST 87

“EP. vvamme ard change Progvown Counter

F=@116 180
—?

%)

CAZIMBELTI ABO B=BORe D=BGO H=d121 S-a16G Pooleo, HVI
TAS Tote (0 (ladeamal ) ceps fat da ioe
COZIMBELI1 A=@@ B=G000 D=e
COZIMBE111 A=80
COZIMBEII1 A=0a
C@ZIMBEII1 A=88
COZIMBELI1 A=@2d)
COZAMBEQI11 A=02 B=8800 D=800@ H=@119
COZGMBE@I1 A=82 B=0608 D=8008 H=e119 §
COZQMBE@I1 A=82 B=A80G D=8G08 H=@11A $
COZOMBE@I1 A=92 B=0788 D=8008 H=G1iA S=@1G@ P=@1OF JNZ
COZBMBEGI1 A=82 B=a786 D=800G H=G11A S=@1@ P=a107 MOY
COZUMBEGI1 A=08 B=0780 D=4000 H=G11A S=@1G@ P=9108 SUB
COZIMBE111 A=@8 B=0706 D=800G H=@11A S=@100 F=O189 UNC
COZIMBE1I1 A=G0 B=9780 D=800G H=@11A S=@19G P=o1eD INX
COZIMBELT1 A=@@ B=0700 D=800G H=G11B S=@1@0 P=@10E DCR &
COZOMBE1I1 A=0G B=G600 D=6G@8 H=G118 S=@10G P=O1GF JNZ 107
COZOMBE1I1 A=00 B=0600 D=8008 H=@11B S=O1G9 P=G107 MOV a, nNeBtas

Aon fl
i]
—_
S
gS
oa

Lt
Py
—
@
Lo
oe]
z

-ALGS9 Ld A .
wiag JC 165 he wachine Code Vat

—_——) +0 cant +: vont A wo @ Since Are.
B14C) tue to ae © Smee this Code was not execattd,
68, top DOT <o-tut & veeén of it @ppeave that the THC should

the Patched Program can be saved have been a TC iwstuchon

SAVE 1 SCAN. COM) lanam vesdes om 4irst Poge, so Saw 4 page .

A> DPT SCAN COM Restart Dor with the Saved mnemay mee +p cowhnue “testing

16k DET VER 1.4
NEXT FC
6206 0168

“hide, List some Cade

\#16@ MY¥I 6,46
8102 MV¥I C.&6 4
6104 LXI H.@1193 %, aM

eV us c im M
Bias UXT He vous Patcd is Presex X. C6
6196 SUB ¢C
8189 JC G16

---
_Page 21_

819C
810D
B10E
819F
- XP
—?
P=6166

AGY
INK
DCR
JNZ
HOY

e

“ie, Tre tv see \ww Poctclred yersun

COZGMBEO16
COZENBEOI1S
COZEMBESIA
COZEMGECIG
COZGMBESI19

A=66
A=686
A=66
A=69

B=6660
B=8808
B=6886

D=8608
D=8006
D=88686
D=8666
D=6890

H=4660
H=9008
H=9668

COZOMBESI 1
COZOMGESI 1
COZOMNBESI 1
COZ6MBEGI 1
COZOMBEOI1
COZGHBESI 1
COZO@NBEOI 1
CIZGM1IEGI16
CIZOMIEGIG
CiZOMIEGIG
CIZGMBE1LT1

-%
~~?

A=82
A=82
A=62
A=62

A=@82 B=8782 D=8608 H=811A
A=82
R=80
RA=FE B=8782 D=86608 H=@11fA
A=FE B=8782 D=8606 H=G11h S$=6189
A=FE D=8096
A=FE B=8682 D=86@6 H=@118

D=86 S$=6106

$=618@

CIZOMBELII A=FE B=@692 D=8@0080 H=@11B S=0180

Dats is mowed from Ab C

“G-108 | Runtiom Curvest PC and Wweakpownt at 108H

\*6168
“Ky

po het dah tau
CIZGMBE1I1 A=04 B=0602 D=8000 H=0118 S=e10e@

Siugle Sep tor G Feu cycles
CiZGMBEL111 A=84 B=B8602 D=B8088 H=8118 S=8108

—~

-T
—e

COZGMBEGI1 A=82 B=8682 D=8600 H=G1i1B S$=6198
-&
—

COZ@MBE@I1 A=82 B=8682 D=8808 H=811B S=e100
“S,  Runto Complehan
\*O@116

COZIMBEIT1 A=83 B=6003 D=8008 H=@121 S=6188

“S424, (ook ak the value cf LACE”
8121 83, Wong Value /

is

=8168 H¥I 8,68
P=@162 MV¥I C,8@
P=@104 LXI H,@119
P=@8107 MOY ALM
P=@108 SUB C
P=@189 JC @16D
P=@10C MOY C.A
P=818D INX H
P=@18E BCR 8B
P=@18F JNZ 0167
P=@167 MOY ALf—
P=@108 SUB C
P=@189 JC @16D
P=@1@D INK H
P=@18E DCR B
P=@10F JNZ 6167\*0187
lweakoatt afler Ib skps—
P=@9167 MOY ALM
P=@188 SUB C
P=@188 SUB C#e109
P=@109 JC 618Dee@18C
P=@1QC MOY CLA
P=@116 RST 87

---
_Page 22_

b1i24

-Liag@

vive
a1a4
@i14a8
81ag
818C
B19E
616F
@ile
-L
—
B1is
B116
S118
@119
B1ilLA
@i1B
@1ic
B10
BL1E
-RP
—~?

P=@116 180
—?

86,
2%
a1)

a2, End Fe S Command

’

MYT
MY¥I
LXI
MOV
SUB
JC

MOV
INK
DCR
JNZ
MOY

STA
RST
NOP
NOP
STAY
NOP
INR
INK
UCR
MY¥I
DCR

AC fevew the Code

Lea]

» Bi

nuaawoa

Reset the Pe

“L, Swale steps and watch data values

COZIMBELIL A=@3 B=B003 D=8646 H=9121 S=810@ P=@148 MYVI

-T
“9

COZIMBELIL A=43 B=4803 D=8606 H=G1i21 GS=@1@6 P=9142 MY¥I

-T
—?

+ set .
pon $! ® largest set

COZIMBELI1 R=@3 B=4886 D=8666 H=G121 S=@140 P=@104 LXI

“3

1 base addsess & deck. set

COZIMBELIL A=63 B=0666 D=8G66 H=@119 S=@144 P=6147 MOY

B. @8\*8182

C.60\*0184

H,»B119%6107

A.M+#\*8198

---
_Page 23_

-T ;
COZIM@EL1L &=62 B=@5860 D=84GH H=G119 S$=G18G P=61608 SUB C4189
“Ip

COZOMBEGI1 A=G2 B=4906 D=66606 H=4119 S=@106 P=@149 JC 61GD\*"616C

-T

~J
CBOZQMBEGI1 A=B2 B=4888 P=4606 H=att9 S$=@106 P=@14@C MOY C.A\*B190D
-T .

~) Fisk deo thom moved to ¢ covvectly
COZGMA@E@I 1 A=4@2 B=A8B2 D=BOSH H=G119 S=@10H P=416D INK H\*@iBe
-T

“~v

COZGMBEGI1 A=62 B=48b2 D=6060 H=@11A $=G106 P=@1GE BCR BrG1BF
-T

Vv
CGOZQ@MBEGIL A=02 B=a782 D=88@4@ H=G11h $=O10G P=B816F JINZ 8167%\*8167
-T

“~?
COZQMBEGI1 A=a2 B=a782 R=0eRa H=@1i S=@1e@@ Prata? MOY A, Meat aes
“yy [Second data. them lovouglet +o A

COZG@MBE@I! A=46 B=8702 D=86806 H=0@11A S=G@1G0 Pz=O148 SUB C\*@109
_ T .

~) L Subtrad declveys date Value which was loaded |
CIZ@MIE@IO A=FE B=@782 D=8600 H=G11A S=414@G@ P=@149 JC Bi1e@D\*e6is8DdD
-T

“~

CiZ@MiE@I@ A=FE B=6762 D=6008 H=O11H S=G146 P=@1@G INX H\*@1BE
“alee,

4@1i9@ MV¥I 6. a8
6t@2 MMVI C.4@
@164 LXI H,.@119
Big7 MOY ALR

i198 suB c «—Thc should have beer a CMP so ttt veqske A

8199 Jc 410d
Wou! .
alae NOY CoA ld ust be dectroyed

@19m INK H
B16E BCR B
B10F JNZ 6107
O1le@ MOY Ase
-A1LG8

”}

B145,

-80, stor WOT fer SNE

---
_Page 24_

SAVE 1 SCAN. COM
oP Save memory age

ADET SCAN. CON, Coctert DOT
16K DUT VER 1.@

NEXT PC
8286 8162
P=0108)
“L116,
B116 RST @7
Bil7 NOP vo
e1ie NOP + Lock at code tosee if it was Properly Loaded
B11A NOP (long typeout abated with Yubouct)
~ Cruloout)
“B116 > Rum From lOoito campleten
\*OL16
“Ey Look at Carry Cocdewtal type)
ct
]

— Look at Cru tate

CIZIMBELTL A=@6 B=G606 D=8606 H=@121 S=@10@ P=9116 RST G7

-Siz2t “t ,
2 Look at” Large’ - (appears to be correct

121 86)

Hi22 84)

B123 22 4)

“88 <bp DDT

ED SCANs ASH Re-edit he source Param, and make bath changes
\*NSUB

“alt, ? ef?

SU ia jLARGER VALUE IN C?
\*35U 4 LT,

cm Cc j;LARGER VALUE IN C?
"?

NC NFOQUND ;iJUMP IF LARGER VALUE NOT FOUND
\*xSN : LT,

NFOUND iJUMP IF LARGER VALUE NOT FOUND

---
_Page 25_

ASM SCAN. RAZ. Le Qe-acseule , selecting SOUNCE Srom dick A

CP/M ASSEMBLER - YER 1.6 Wwx-to disk A t file)
Prut to £ (selects vo Print Tile

A122

BG2H USE FACTOR

END OF ASSEMBLY

UDT SCAN. HEX von debugger do check Chavats

16K DET VER 1.%
HEST PC

a@izt @@aGa

-~-Lilte

arigé unr aaa Check -y ensue ends dill at I1bY

Wil19 STAX §&

\#110 NOP
@1Ligé INR B
— (ruloout)

“G10, 116) Go 4rom laeq wring wrth loveakpomt at evd.
\*aiie  lvcakPowt reached

“Bit ) Look at Laece" Cowvect Value Computed
Biz21 @@ 22 21 686 62 7E ER 77 13 @3 ERB @B 78 B1 "Ele,

8176 C2 427 61 C3 83 29 44 46 88 BH BH 44 OG OA BO GE .’...

614G 8@ 4@ 64 86 48 86 88 GA GB GU BG 44 4B BB UH AB...........,

-(rulemit) — alow Lowy eypeout

“ey sp DDT, debua session Complete

---
_Page 26_

---
_Page 27_

---
_Page 28_


