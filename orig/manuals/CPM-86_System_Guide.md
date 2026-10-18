# CPM-86 System Guide

> OCR text extracted from `CPM-86_System_Guide.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `CPM-86_System_Guide.pdf`.

---

---
_Page 001_

CP/M-86™
Operating System
System Guide

Copyright © 1981

Digital Research
P.O. Box 579
801 Lighthouse Avenue
Pacific Grove, CA 93950
(408) 649-3896
TWX 910 360 5001

All Rights Reserved

---
_Page 002_

COPYRIGHT

Copyright © 1981 by Digital Research. All rights
reserved. No part of this publication may be
reproduced, transmitted, transcribed, stored in a
retrieval system, or translated into any language or
computer language, in any form or by any means,
electronic, mechanical, magnetic, optical, chemical,
manual or otherwise, without the prior written
permission of Digital Research, Post Office Box 579,
Pacific Grove, California, 93950.

DISCLAIMER

Digital Research makes no representations or
warranties with respect to the contents hereof and
specifically disclaims any implied warranties of
merchantability or fitness for any particular
purpose. Further, Digital Research reserves the
right to revise this publication and to make changes
from time to time in the content hereof without
obligation of Digital Research to notify any person
of such revision or changes.

TRADEMARKS

CP/M, CP/M-86, and CP/NET are registered trademarks
of Digital Research. ASM-86, CP/M-80, DDT-86, LINK-
80, MP/M, MP/M-86, and TEX are trademarks of Digital
Research. Intel is a registered trademark of Intel
Corporation.

The CP/M-86 Operating System System Guide was
prepared using the Digital Research TEX Text
Formatter and printed in the United States of
America.

HK IRR IKAII KERR AKK EERE HERERE

\* First Edition: June 1981 \*

\* Second Edition: June 1983 \*
worrrrrrrrrrrrrrr eer t ete ESE 22S

---
_Page 003_

Foreword

The CP/M-86 Operating System System Guide presents the system
programming aspects of CP/M-86®, a single-user operating system for
the Intel® 8086 and 8088 16-bit microprocessors. The discussion
assumes that you are familiar with CP/M®, the Digital Research 8-
bit operating system. To clarify specific differences with CP/M-86,
this document refers to the 8-bit version of CP/M as CP/M-80™.,
Elements common to both systems are simply called CP/M features.

The CP/M-86 package also includes the CP/M-86 Operating System

User's Guide and the CP/M-86 Operating System Programmer's
Guide, which describes ASM-86'™ and DDT-66'", Digital Research's

8086 assembler and interactive debugger.

This System Guide presents an overview of the CP/M-86
programming interface conventions. It also describes procedures for
adapting CP/M-86 to a custom hardware environment.

Section 1 gives an overview of CP/M-86 and summarizes its
differences with CP/M-80. Section 2 describes the general execution
environment while Section 3 tells how to generate command files.
Sections 4 and 5 respectively define the programming interfaces to
the Basic Disk Operating System and the Basic Input/Output System.
Section 6 discusses alteration of the BIOS to support custom disk
configurations, and Section 7 describes the loading operation and
the organization of the CP/M-86 system file.

iii

---
_Page 004_

Table of Contents

CP/M-86 System Overview

1.1 CP/M-86 General Characteristics ...
1.2 CP/M-80 and CP/M-86 Differences .. .

Command Setup and Execution Under CP/M-86

1 cCP Built-in and Transient Commands .
2 Transient Program Execution Models .
3 The 8080 Memory Model .....-+ + «
4 The Small Memory Model .....s.-.
5 The Compact Memory Model ..... .«
6 Base Page Initialization ......
7 Transient Program Load and Exit...

Command (CMD) File Generation

1 Intel Hex File Format...
2 Operation of GENCMD .....-+-. -
3 Operation of LMCMD ....
4 Command (CMD) File Format .

Basic Disk Operating System (BDOS) Functions

1 BDOS Parameters and Function Codes .
2 Simple BDOS Calls re
3 BDOS File Operations ... o 8
4 BDOS Memory Management and Load .

Basic I/O System (BIOS) Organization

1 Organization of the BIOS ......
2 The BIOS Jump Vector .... .-+« -»
3 Simple Peripheral Devices oe eo ow
4 BIOS Subroutine Entry Points ....
BIOS Disk Definition Tables

6.1 Disk Parameter Table Format .... .
6.2 Table Generation Using GENDEF ... .
6 es 3 GENDEF Ou tpu t e es eo e e e e e e . e e
CP/M-86 Bootstrap and Adaptation Procedures

7.1 The Cold Start Load Operation ee
7.2 Organization of CPM.SYS se oe we

---
_Page 005_

gyno Aa we DB

Appendixes

Blocking and Deblocking Algorithms ..... + +++ es - 87
Random Access Sample Program . . . 6 « «© «© «© «© «© © © « » 95
Listing of the Boot Rom... .. 2. 2 6 © © © © we © we ee 103
LDBIOS Listing . . 2. 6 2 ee ee ee we we we we we we we ew ew ee 6 11S
BIOS Listing . 2.6 2 ee ee ee ee we we we ew ee we we 1D

CBIOS Listing ° e e e ° e . ° . e e e ° ° e e ° e e e e e 137

vi

---
_Page 006_

Section 1!
CP/M-86 System Overview

1.1 CP/M-86 General Characteristics

CP/M-86 contains all facilities of CP/M-80 with additional
features to account for increased processor address space of up toa
megabyte (1,048,576) of main memory. Further, CP/M-86 maintains
file compatibility with all previous versions of CP/M. The file
structure of version 2 of CP/M is used, allowing as many as sixteen
drives with up to eight megabytes on each drive. Thus, CP/M-80 and
CP/M-86 systems may exchange files without modifying the file
format.

CP/M-86 resides in the file CPM.SYS, which is loaded into
memory by a cold start loader during system initialization. The
cold start loader resides on the first two tracks of the system
disk. CPM.SYS contains three program modules: the Console Command
Processor (CCP), the Basic Disk Operating System (BDOS), and the
user-configurable Basic I/O System (BIOS). The CCP and BDOS
portions occupy approximately 10K bytes, while the size of the BIOS
varies with the implementation. The operating system executes in
any portion of memory above the reserved interrupt locations, while
the remainder of the address space is partitioned into as many as
eight non-contiguous regions, as defined in a BIOS table. Unlike
CP/M-80, the CCP area cannot be used as a data area subsequent to
transient program load; all CP/M-86 modules remain in memory at all
times, and are not reloaded at a warm start.

Similar to CP/M-80, CP/M-86 loads and executes memory image
files from disk. Memory image files are preceded by a “header
record," defined in this document, which provides information
required for proper program loading and execution. Memory image
files under CP/M-86 are identified by a "CMD" file type.

Unlike CP/M-80, CP/M-86 does not use absolute locations for
system entry or default variables. The BDOS entry takes place
through a reserved software interrupt, while entry to the BIOS is
provided by a new BDOS call. Two variables maintained in low memory
under CP/M-80, the default disk number and I/O Byte, are placed in
the CCP and BIOS, respectively. Dependence upon absolute addresses
is minimized in CP/M-86 by maintaining initial "base page" values,
such as the default FCB and default command buffer, in the transient
program data area.

Utility programs such as ED, PIP, STAT and SUBMIT operate in
the same manner under CP/M-86 and CP/M-80. In its operation, DDT-86
resembles DDT supplied with CP/M-80. It allows interactive
debugging of 8086 and 8088 machine code. Similarly, ASM-86 allows
assembly language programming and development for the 8086 and 8088
using Intel-like mnemonics.

---
_Page 007_

CP/M-86 System Guide 1.1 CP/M-86 General Characteristics

The GENCMD (Generate CMD) utility replaces the LOAD program of
CP/M-80, and converts the hex files produced by ASM-86 or Intel
utilities into memory image format suitable for execution under
CP/M-86. Further, the LDCOPY (Loader Copy) program replaces SYSGEN,
and is used to copy the cold start loader from a system disk for
replication. In addition, a variation of GENCMD, called LMCMD,
converts output from the Intel LOC86 utility into CMD format.
Finally, GENDEF (Generate DISKDEF) is provided as an aid in
producing custom disk parameter tables. ASM-86, GENCMD, LMCMD, and
GENDEF are also supplied in "COM" file format for cross-development
under CP/M-80.

Several terms used throughout this manual are defined in Table
1-1 below:

Table 1-1. CP/M-86 Terms
ee

Nibble 4-bit half-byte

Byte 8-bit value

16-bit value

Word

Double Word 32-bit value

16 contiguous bytes

Paragraph

An address divisible evenly
by 16 (low order nibble 0)

Paragraph Boundary

Up to 64K contiguous bytes

Segment

One of CS, DS, ES, or SS

Segment Register

Offset 16-bit displacement from a
segment register

Group A segment-register-relative
relocatable program unit

Address The effective memory address

derived from the composition
of a segment register value
with an offset value

A group consists of segments that are loaded into memory as a single
unit. Since a group may consist of more than 64K bytes, it is the
responsibility of the application program to manage segment
registers when code or data beyond the first 64K segment is
accessed.

---
_Page 008_

CP/M-86 System Guide 1.1 CP/M-86 General Characteristics

CP/M-86 supports eight program groups: the code, data, stack
and extra groups as well as four auxiliary groups. When a code,
data, stack or extra group is loaded, CP/M-86 sets the respective
segment register (CS, DS, SS or ES) to the base of the group. CP/M-
86 can also load four auxiliary groups. A transient program manages
the location of the auxiliary groups using values stored by CP/M-86
in the users base page.

1.2 CP/M-80 and CP/M-86 Differences

The structure of CP/M-86 is as close to CP/M-80 as possible in
order to provide a familiar programming environment which allows
application programs to be transported to the 8086 and 8088
processors with minimum effort. This section points out the
specific differences between CP/M-80 and CP/M-86 in order to reduce
your time in scanning this manual if you are already familiar with
CP/M-80. The terms and concepts presented in this section are
explained in detail throughout this manual, so you will need to
refer to the Table of Contents to find relevant sections which
provide specific definitions and information.

Due to the nature of the 8086 processor, the fundamental
difference between CP/M-80 and CP/M-86 is found in the management of
the various relocatable groups. Although CP/M-80 references
absolute memory locations by necessity, CP/M-86 takes advantage of
the static relocation inherent in the 8086 processor. The operating
system itself is usually loaded directly above the interrupt
locations, at location 0400H, and relocatable transient programs
load in the best fit memory region. However, you can load CP/M-86
into any portion of memory without changing the operating system
(thus, there is no MOVCPM utility with CP/M-86), and transient
programs will load and run in any non-reserved region.

Three general memory models are presented below, but if you are
converting 8080 programs to CP/M-86, you can use either the 8080
Model or Small Model and leave the Compact Model for later when your
addressing needs increase. You\*1]l use GENCMD, described in Section
3.2, to produce an executable program file from a hex file. GENCMD
Parameters allow you to specify which memory model your program
requires.

CP/M-86 itself is constructed as an 8080 Model. This means
that all the segment registers are placed at the base of CP/M-86,
and your customized BIOS is identical, in most respects, to that of
CP/M-80 (with changes in instruction mnemonics, of course). In
fact, the only additions are found in the SETDMAB, GETSEGB, SETIOB,
and GETIOB entry points in the BIOS. Your warm start subroutine is
simpler since you are not required to reload the CCP and BDOS under
CP/M-86. One other point: if you implement the IOBYTE facility,
you\*ll have to define the variable in your BIOS. Taking these
changes into account, you need only perform a simple translation of
your CP/M-80 BIOS into 8086 code in order to implement your 8086
BIOS.

---
_Page 009_

CP/M-86 System Guide 1.2 CP/M-80 and CP/M-86 Differences

If you’ve implemented CP/M-80 Version 2, you already have disk
definition tables which will operate properly with CP/M-86. You may
wish to attach different disk drives, or experiment with sector skew
factors to increase performance. If so, you can use the new GENDEF
utility which performs the same function as the DISKDEF macro used
by MAC under CP/M-80. Youll find, however, that GENDEF provides
you with more information and checks error conditions better than
the DISKDEF macro.

Although generating a CP/M-86 system is generally easier than
generating a CP/M-80 system, complications arise if you are using
single-density floppy disks. CP/M-86 is too large to fit in the
two-track system area of a single-density disk, so the bootstrap
operation must perform two steps to load CP/M-86: first the
bootstrap must load the cold start loader, then the cold start
loader loads CP/M-86 from a system file. The cold start loader
includes a LDBIOS which is identical to your CP/M-86 BIOS with the
exception of the INIT entry point. You can simplify the LDBIOS if
you wish because the loader need not write to the disk. If you have
a double-density disk or reserve enough tracks on a single-density
disk, you can load CP/M-86 without a two-step boot.

To make a BDOS system call, use the reserved software interrupt
\#244, The jump to the BDOS at location 0005 found in CP/M-80 is not
present in CP/M-86. However, the address field at offset 0006 is
present so that programs which "size" available memory using this
word value will operate without change. CP/M-80 BDOS functions use
certain 8080 registers for entry parameters and returned values.
CP/M-86 BDOS functions use a table of corresponding 8086 registers.
For example, the 8086 registers CH and CL correspond to the 8080
registers B and C. Look through the List of BDOS function numbers
in Table 4-2. and you\*1l find that functions 0, 27, and 31 have
changed slightly. Several new functions have been added, but they
do not affect existing programs.

One major philosophical difference is that in CP/M-80, all
addresses sent to the BDOS are simply 16-bit values in the range
0000H to OFFFFH. In CP/M-86, however, the addresses are really just
16-bit offsets from the DS (Data Segment) register which is set to
the base of your data area. If you translate an existing CP/M-80
program to the CP/M-86 environment, your data segment will be less
than 64K bytes. In this case, the DS register need not be changed
following initial load, and thus all CP/M-80 addresses become simple
DS-relative offsets in CP/M-86.

Under CP/M-80, programs terminate in one of three ways: by
returning directly to the CCP, by calling BDOS function 0, or by
transferring control to absolute location 0000H. CP/M-86, however,
supports only the first two methods of program termination. This
has the side effect of not providing the automatic disk system reset
following the jump to 0000H which, instead, is accomplished by
entering a CONTROL-C at the CCP level.

---
_Page 010_

CP/M-86 System Guide 1.2 CP/M-80 and CP/M-86 Differences

You'll find many new facilities in CP/M-86 that will simplify
your programming and expand your application programming capability.
But, we've designed CP/M-86 to make it easy to get started: in
short, if you are converting from CP/M-80 to CP/M-86, there will be
no major changes beyond the translation to 8086 machine code.
Further, programs you design for CP/M-86 are upward compatible with
MP/M-86™ , Our multitasking operating system, as well as CP/NET-86
which provides a distributed operating system in a network
environment.

---
_Page 011_

Section 2
Command Setup and Execution Under CP/M-86

This section discusses the operation of the Console Command
Processor (CCP), the format of transient programs, CP/M-86 memory
models, and memory image formats.

2.1 CCP Built-in and Transient Commands

The operation of the CP/M-86 CCP is similar to that of CP/M-80.
Upon initial cold start, the CP/M sign-on message is printed, drive
A is automatically logged in, and the standard prompt is issued at
the console. CP/M-86 then waits for input command lines from the
console, which may include one of the built-in commands

DIR ERA REN TYPE USER

(note that SAVE is not supported under CP/M-86 since the equivalent
function is performed by DDT-86).

Alternatively, the command line may begin with the name of a
transient program with the assumed file type "CMD" denoting a
"command file." The CMD file type differentiates transient command
files used under CP/M-86 from COM files which operate under CP/M-80.

The CCP allows multiple programs to reside in memory, providing
facilities for background tasks. A transient program such as a
debugger may load additional programs for execution under its own
control. Thus, for example, a background printer spooler could
first be loaded, followed by an execution of DDT-86. DDT-86 may, in
turn, load a test program for a debugging session and transfer
control to the test program between breakpoints. CP/M-86 keeps
account of the order in which programs are loaded and, upon
encountering a CONTROL-C, discontinues execution of the most recent
program activated at the CCP level. A CONTROL-C at the DDT-86
command level aborts DDT-86 and its test program. A second CONTROL-
C at the CCP level aborts the background printer spooler. A third
CONTROL-C resets the disk system. Note that program abort due to
CONTROL-C does not reset the disk system, as is the case in CP/M-80.
A disk reset does not occur unless the CONTROL-C occurs at the CCP
command input level with no programs residing in memory.

When CP/M-86 receives a request to load a transient program
from the CCP or another transient program, it checks the program’s
memory requirements. If sufficient memory is available, CP/M-86
assigns the required amount of memory to the program and loads the
program. Once loaded, the program can request additional memory
from the BDOS for buffer space. When the program is terminated,
CP/M-86 frees both the program memory area and any additional buffer
space.

---
_Page 012_

CP/M-86 System Guide 2.2 Transient Program Execution Models

2.2 Transient Program Execution Models
The initial values of the segment registers are determined by
one of three "memory models" used by the transient program, and

described in the CMD file header. The three memory models are
summarized in Table 2-1 below.

Table 2-1. CP/M-86 Memory Models
Group Relationships

8080 Model Code and Data Groups Overlap

Small Model Independent Code and Data Groups

Three or More Independent Groups

Compact Model

The 8080 Model supports programs which are directly translated
from CP/M-80 when code and data areas are intermixed. The 8080
model consists of one group which contains all the code, data, and
stack areas. Segment registers are initialized to the starting
address of the region containing this group. The segment registers
can, however, be managed by the application program during execution
so that multiple segments within the code group can be addressed.

The Small Model is similar to that defined by Intel, where the
program consists of an independent code group and a data group. The
Small Model is suitable for use by programs taken from CP/M-80 where
code and data is easily separated. Note again that the code and
data groups often consist of, but are not restricted to, single 64K
byte segments.

The Compact Model occurs when any of the extra, stack, or
auxiliary groups are present in program. Each group may consist of
one or more segments, but if any group exceeds one segment in size,
or if auxiliary groups are present, then the application program
must manage its own segment registers during execution in order to
address all code and data areas.

The three models differ primarily in the manner in which
segment registers are initialized upon transient program loading.
The operating system program load function determines the memory
model used by a transient program by examining the program group
usage, as described in the following sections.

---
_Page 013_

CP/M-86 System Guide 2.3 The 8080 Memory Model

2.3 The 8080 Memory Model

The 8080 Model is assumed when the transient program contains
only a code group. In this case, the CS, DS, and ES registers are
initialized to the beginning of the code group, while the SS and SP
registers remain set to a 96-byte stack area in the CCP. The
Instruction Pointer Register (IP) is set to 100H, similar to CP/M-
80, thus allowing base page values at the beginning of the code
group. Following program load, the 8080 Model appears as shown in
Figure 2-1, where low addresses are shown at the top of the diagram:

SS:

SS + SP: ccP Stack
CS DS ES:
DS+0000H: base

page

CS+0100H: IP = 0100H
code

data

Figure 2-1. CP/M-86 8080 Memory Model

The intermixed code and data regions are indistinguishable. The
"base page" values, described below, are identical to CP/M-80,
allowing simple translation from 8080, 8085, or 280 code into the
8086 and 8088 environment. The following ASM-86 example shows how
to code an 8080 model transient program.

eseg
org 100h
. (code)
endcs equ $
dseg
org offset endcs
. (data)
end

---
_Page 014_

CP/M-86 System Guide 2.4 The Small Memory Model

2.4 The Small Memory Model

The Small Model is assumed when the transient program contains
both a code and data group. (In ASM-86, all code is generated
following a CSEG directive, while data is defined following a DSEG
directive with the origin of the data segment independent of the
code segment.) In this model, CS is set to the beginning of the
code group, the DS and ES are set to the start of the data group,
and the SS and SP registers remain in the CCP’s stack area as shown

in Figure 2-2.
pe |

CCP Stack

cs: IP = 0000H
code

SS:

SS + SP:

DS ES:

DS+0100H:

Figure 2-2. CP/M-86 Small Memory Model

The machine code begins at CS+0000H, the "base page" values begin at
DS+0000H, and the data area starts at DS+0100H. The following ASM-
86 example shows how to code a small model transient program.

cseg

(code)
dseg
org 100h
° (data)
end

---
_Page 015_

CP/M-86 System Guide 2.5 The Compact Memory Model

2.5 The Compact Memory Model

The Compact Model is assumed when code and data groups are
present, along with one or more of the remaining stack, extra, or
auxiliary groups. In this case, the CS, DS, and ES registers are
set to the base addresses of their respective areas. Figure 2-3
shows the initial configuration of segment registers in the Compact
Model. The values of the various segment registers can be
Programmatically changed during execution by loading from the
initial values placed in base page by the CCP, thus allowing access
to the entire memory space.

If the transient program intends to use the stack group as a
stack area, the SS and SP registers must be set upon entry. The SS
and SP registers remain in the CCP area, even if a stack group is
defined. Although it may appear that the SS and SP registers should
be set to address the stack group, there are two contradictions.
First, the transient program may be using the stack group as a data
area. In that case, the Far Call instruction used by the CCP to
transfer control to the transient program could overwrite data in
the stack area. Second, the SS register would logically be set to
the base of the group, while the SP would be set to the offset of
the end of the group. However, if the stack group exceeds 64K the
address range from the base to the end of the group exceeds a 16-bit
offset value.

The following ASM-86 example shows how to code a compact model
transient program.

cseg
. (code)

dseg

org 100h

. (data)

eseg

. (more data)
sseg

. (stack area)
end

ll

---
_Page 016_

CP/M-86 System Guide 2.5 The Compact Memory Model

SS:
CcP
SS + SP: CCP Stack

CS: IP = 0000H
code

DS:

DS+0100H:

ES:
data

Figure 2-3. CP/M-86 Compact Memory Model

---
_Page 017_

CP/M-86 System Guide 2.6 Base Page Initialization

2.6 Base Page Initialization

Similar to CP/M-80, the CP/M-86 base page contains default
values and locations initialized by the CCP and used by the
transient program. The base page occupies the regions from offset
0000H through OOFFH relative to the DS register. The values in the

base page for CP/M-86 include those of CP/M-80, and appear in the
same relative positions, as shown in Figure 2-4.

DS + 0000:
DS + 0003: }pco | Bc | M80 |
DS + 0006:
DS + 0009: spo | Bp1 | xxx |
DS + 000C:
DS + 000F: }BeO | BEL | xxx
DS + 0012: LS2
| BS |

DS + 0015: BS1
DS + 0018:

DS + 0018:

\*
be

DS + OO1E:
DS + 0021:
DS + 0024:
DS + 0027:
DS + 002A:

DS + 002D:

DS + 0030: Not
. Currently
OO5B: Used

Default FCB
Default Buffer
Begin User Data

Figure 2-4. CP/M-86 Base Page Values

DS +

DS + 005C:
DS + 0080:
DS + 0100:

---
_Page 018_

CP/M-86 System Guide 2.6 Base Page Initialization

Each byte is indexed by 0, 1, and 2, corresponding to the standard
Intel storage convention of low, middle, and high-order (most
significant) byte. "xxx" in Figure 2-4 marks unused bytes. LC is
the last code group location (24-bits, where the 4 high-order bits
equal zero).

In the 8080 Model, the low order bytes of LC (LCO and LCl)
never exceed OFFFFH and the high order byte (LC2) is always zero.
BC is base paragraph address of the code group (16-bits). LD and BD
provide the last position and paragraph base of the data group. The
last position is one byte less than the group length. It should be
noted that bytes LDO and LD1 appear in the same relative positions
of the base page in both CP/M-80 and CP/M-86, thus easing the
program translation task. The M80 byte is equal to 1 when the 8080
Memory Model is in use. LE and BE provide the length and paragraph
base of the optional extra group, while LS and BS give the optional.
stack group length and base. The bytes marked LX and BX correspond
to a set of four optional independent groups which may be required
for programs which execute using the Compact Memory Model. The
initial values for these descriptors are derived from the header
record in the memory image file, described in the following section.

2.7 Transient Program Load and Exit

Similar to CP/M-80, the CCP parses up to two filenames
following the command and places the properly formatted FCB’s at
locations 005CH and 006CH in the base page relative to the DS
register. Under CP/M-80, the default DMA address is initialized to
0080H in the base page. Due to the segmented memory of the 8086 and
8088 processors, the DMA address is divided into two parts: the DMA
segment address and the DMA offset. Therefore, under CP/M-86, the
default DMA base is initialized to the value of DS, and the default
DMA offset is initialized to 0080H. Thus, CP/M-80 and CP/M=86
Operate in the same way: both assume the default DMA buffer
occupies the second half of the base page.

The CCP transfers control to the transient program through an
8086 "Far Call." The transient program may choose to use the 96-byte
CCP stack and optionally return directly to the CCP upon program
termination by executing a "Far Return." Program termination also
occurs when BDOS function zero is executed. Note that function zero
can terminate a program without removing the program from memory Or
changing the memory allocation state (see Section 4.2). The
operator may terminate program execution by typing a single CONTROL-
C during line edited input which has the same effect as the program
executing BDOS function zero. Unlike the operation of CP/M-80, no
disk reset occurs and the CCP and BDOS modules are not reloaded from
disk upon program termination.

---
_Page 019_

Section 3
Command (CMD) File Generation

As mentioned previously, two utility programs are provided with
CP/M-86, called GENCMD and LMCMD, which are used to produce CMD
memory image files suitable for execution under CP/M-86. GENCMD
accepts Intel 8086 "hex" format files as input, while LMCMD reads
Intel L-module files output from the standard Intel LOC86 Object
Code Locator utility. GENCMD is used to process output from the
Digital Research ASM-86 assembler and Intel’s OH86 utility, while
LMCMD is used when Intel compatible developmental software is
available for generation of programs targeted for CP/M-86 operation.

3.1 Intel 8086 Hex File Format

GENCMD input is in Intel “hex" format produced by both the
Digital Research ASM-86 assembler and the standard Intel OH86
utility program (see Intel document #9800639-03 entitled "MCS-86
Software Development Utitities Operating Instructions for ISIS-II
Users"). The CMD file produced by GENCMD contains a header record
which defines the memory model and memory size requirements for
loading and executing the CMD file.

An Intel "hex" file consists of the traditional sequence of
ASCII records in the following format:

DP -f-lel=[elelfalal - -- lelele!

where the beginning of the record is marked by an ASCII colon, and
each subsequent digit position contains an ASCII hexadecimal digit
in the range 0-9 or A-F. The fields are defined in Table 3-1.

---
_Page 020_

CP/M-86 System Guide 3.1 Intel Hex File Format

Table 3-1. Intel Hex Field Definitions

Record Length 00-FF (0-255 in decimal)

Load Address

Record Type:

00 data record, loaded starting at offset
aaaa from current base paragraph

01 end of file, cc = FF

02 extended address, aaaa is paragraph
base for subsequent data records

03 start address is aaaa (ignored, IP set

according to memory model in use)

The following are output from ASM-86 only:
81 same as 00, data belongs to code segment

82 same as 00, data belongs to data segment

83 same as 00, data belongs to stack segment

84 same as 00, data belongs to extra segment

85 paragraph address for absolute code segment
86 paragraph address for absolute data segment
87 paragraph address for absolute stack segment
88 paragraph address for absolute extra segment

Data Byte

Check Sum (00 - Sum of Previous Digits)

All characters preceding the colon for each record are ignored.
(Additional hex file format information is included in the ASM-86
User’s Guide, and in Intel’s document #9800821A entitled "MCS-86
Absolute Object File Formats.")

3.2 Operation of GENCMD
The GENCMD utility is invoked at the CCP level by typing
GENCMD filename parameter-list

where the filename corresponds to the hex input file with an assumed
(and unspecified) file type of H86. GENCMD accepts optional
parameters to specifically identify the 8080 Memory Model and tc
describe memory requirements of each segment group. The GENCMD
parameters are listed following the filename, as shown in the
command line above where the parameter-list consists of a sequence
of keywords and values separated by commas or blanks. The keywords
are:

8080 CODE DATA EXTRA STACK Xl X2 x3 x4

---
_Page 021_

CP/M-86 System Guide 3.2 Operation of GENCMD

The 8080 keyword forces a single code group so that the BDOS load
function sets up the 8080 Memory Model for execution, thus allowing
intermixed code and data within a single segment. The form of this
command is

GENCMD filename 8080

The remaining keywords follow the filename or the 8080 option and
define specific memory requirements for each segment group,
corresponding one-to-one with the segment groups defined in the
previous section. In each case, the values corresponding to each
group are enclosed in square brackets and separated by commas. Each
value is a hexadecimal number representing a paragraph address or
segment length in paragraph units denoted by hhhh, prefixed by a
single letter which defines the meaning of each value:

Ahhhh Load the group at absolute Location hhhh

Bhhhh The group starts at hhhh in the hex file

Mhhhh The group requires a minimum of hhhh \* 16 bytes
Xhhhh The group can address a maximum of hhhh \* 16 bytes

Generally, the CMD file header values are derived directly from the
hex file and the parameters shown above need not be included. The
following situations, however, require the use of GENCMD parameters.

@ The 8080 keyword is included whenever ASM-86 is used in
the conversion of 8080 programs to the 8086/8088
environment when code and data are intermixed within a
single 64K segment, regardless of the use of CSEG and
DSEG directives in the source program.

@ An absolute address (A value) must be given for any group
which must be located at an absolute location. Normally,
this value is not specified since CP/M-86 cannot
generally ensure that the required memory region is
available, in which case the CMD file cannot be loaded.

e@ The B value is used when GENCMD processes a hex file
produced by Intel’s OH86, or similar utility program that
contains more than one group. The output from OH86
consists of a sequence of data records with no
information to identify code, data, extra, stack, or
auxiliary groups. In this case, the B value marks the
beginning address of the group named by the keyword,
causing GENCMD to load data following this address to the
named group (see the examples below). Thus, the B value
is normally used to mark the boundary between code and
data segments when no segment information is included in
the hex file. Files produced by ASM-86 do not require
the use of the B value since segment information is
included in the hex file.

---
_Page 022_

CP/M-86 System Guide 3.2 Operation of GENCMD

@ The minimum memory value (M value) is included only when
the hex records do not define the minimum memory
requirements for the named group. Generally, the code
group size is determined precisely by the data records
loaded into the area. That is, the total space required
for the group is defined by the range between the lowest
and highest data byte addresses. The data group,
however, may contain uninitialized storage at the end of
the group and thus no data records are present in the hex
file which define the highest referenced data item. The
highest address in the data group can be defined within
the source program by including a "DB 0" as the last data
item. Alternatively, the M value can be included to
allocate the additional space at the end of the group.
Similarly, the stack, extra, and auxiliary group sizes
must be defined using the M value unless the highest
addresses within the groups are implicitly defined by
data records in the hex file.

@e The maximum memory size, given by the X value, is
generally used when additional free memory may be needed
for such purposes as I/O buffers or symbol tables. If
the data area size is fixed, then the X parameter need
not be included. In this case, the X value is assumed to
be the same as the M value. The value XFFFF allocates
the largest memory region available but, if used, the
transient program must be aware that a three-byte length
field is produced in the base page for this group where
the high order byte may be non-zero. Programs converted
directly from CP/M-80 or programs that use a 2-byte
pointer to address buffers should restrict this value to
XFFF or less, producing a maximum allocation length of
OFFFOH bytes.

The following GENCMD command line transforms the file X.H86
into the file X.CMD with the proper header record:

gencmd x code[a40] data[m30,xfff]

In this case, the code group is forced to paragraph address 40H, or
equivalently, byte address 400H. The data group requires a minimum
of 300H bytes, but can use up to OFFFOH bytes, if available.

---
_Page 023_

CP/M-86 System Guide 3.2 Operation of GENCMD

Assuming a file Y.H86 exists on drive B containing Intel hex
records with no interspersed segment information, the command

gencmd b:y data[b30,m20] extra[b50] stack [m40] xl [m40]

produces the file Y.CMD on drive B by selecting records beginning
at address 0000H for the code segment, with records starting at
300H allocated to the data segment. The extra segment is filled
from records beginning at 500H, while the stack and auxiliary
segment #1 are uninitialized areas requiring a minimum of 400H
bytes each. In this example, the data area requires a minimum of
200H bytes. Note again, that the B value need not be included if
the Digital Research ASM-86 assembler is used.

3.3 Operation of LMCMD

The LMCMD utility operates in exactly the same manner as
GENCMD, with the exception that LMCMD accepts an Intel L-module
file as input. The primary advantage of the L-module format is
that the file contains internally coded information which defines
values which would otherwise be required as parameters to GENCMD,
such the beginning address of the group’s data segment. Currently,
however, the only language processors which use this format are the
standard Intel development packages, although various independent
vendors will, most likely, take advantage of this format in the
future.

---
_Page 024_

CP/M-86 System Guide 3.4 Command (CMD) File Format

3.4 Command (CMD) File Format

The CMD file produced by GENCMD and LMCMD consists of the
128-byte header record followed immediately by the memory image.
Under normal circumstances, the format of the header record is of
no consequence to a programmer. For completeness, however, the
‘various fields of this record are shown in Figure 3-1.

—_—_——128 Bytes

[confor an-aor

Code,
Data,
Extra,
Stack,
Auxiliary

Figure 3-1. CMD File Header Format

In Figure 3-1, GD#2 through GD#8 represent "Group Descriptors."
Each Group Descriptor corresponds to an independently loaded
program unit and has the following fields:

8-bit 16-bit 16-bit 16-bit 16-bit
Cs es

where G-Form describes the group format, or has the value zero if
no more descriptors follow. If G-Form is non-zero, then the 8-bit
value is parsed as two fields:

G-Form:
4-bit 4-bit

Pex | e806)

The G-Type field determines the Group Descriptor type. ‘The valid
Group Descriptors have a G-Type in the range 1 through 9, as shown
in Table 3-2 below.

---
_Page 025_

CP/M-86 System Guide 3.4 Command (CMD) File Format

Table 3-2. Group Descriptors

Code Group

Data Group

Extra Group

Stack Group
Auxiliary Group #1

Auxiliary Group #2

Auxiliary Group #3

Auxiliary Group #4

Shared Code Group

Unused, but Reserved

Escape Code for Additional Types

All remaining values in the group descriptor are given in
increments of 16-byte paragraph units with an assumed low-order 0
nibble to complete the 20-bit address. G-Length gives the number
of paragraphs in the group. Given a G-length of 0080H, for
example, the size of the group is 00800H = 2048D bytes. A-Base
defines the base paragraph address for a non-relocatable group
while G-Min and G-Max define the minimum and maximum size of the
memory area to allocate to the group. G-Type 9 marks a "pure" code
group for use under MP/M-86 and future versions of CP/M-86.
Presently a Shared Code Group is treated as a non-shared Program
Code Group under CP/M-86.

The memory model described by a header record is implicitly
determined by the Group Descriptors. The 8080 Memory Model is
assumed when only a code group is present, since no independent
data group is named. The Small Model is implied when both a code
and data group are present, but no additional group descriptors
occur. Otherwise, the Compact Model is assumed when the CMD file
is loaded.

---
_Page 026_

Section 4
Basic Disk Operating System Functions

This section presents the interface conventions which allow
transient program access to CP/M-86 BDOS and BIOS functions. The
BDOS calls correspond closely to CP/M-80 Version 2 in order to
simplify translation of existing CP/M-80 programs for operation
under CP/M-86. BDOS entry and exit conditions are described first,
followed by a presentation of the individual BDOS function calls.

4.1 BDOS Parameters and Function Codes

Entry to the BDOS is accomplished through the 8086 software
interrupt #224, which is reserved by Intel Corporation for use by
CP/M-86 and MP/M-86. The function code is passed in register CL
with byte parameters in DL and word parameters in DX. Single byte
values are returned in AL, word values in both AX and BX, and double
word values in ES and BX. All segment registers, except ES, are
saved upon entry and restored upon exit from the BDOS (corresponding
to PL/M-86 conventions). Table 4-1 summarizes input and output
parameter passing:

Table 4-1. BDOS Parameter Summary

BDOS Entry Registers BDOS Return Registers

Function Code Byte value returned in AL

Byte Parameter Word value returned in both AX and BX
Word Parameter Double-word value returned with
Data Segment offset in BX and

segment in ES

Note that the CP/M-80 BDOS requires an "information address" as
input to various functions. This address usually provides buffer or
File Control Block information used in the system call. In CP/M-86,
however, the information address is derived from the current DS
register combined with the offset given in the DX register. That
is, the DX register in CP/M-86 performs the same function as the DE
pair in CP/M-80, with the assumption that DS is properly set. This
poses no particular problem for programs which use only a single
data segment (as is the case for programs converted from CP/M-80),
but when the data group exceeds a single segment, you must ensure
that the DS register is set to the segment containing the data area
related to the call. It should also be noted that zero. values are
returned for function calls which are out-of-range.

---
_Page 027_

CP/M-86 System Guide 4.1 BDOS Parameters and Function Codes

A list of CP/M-86 calls is given in Table 4-2 with an asterisk
following functions which differ from or are added to the set of
CP/M-80 Version 2 functions.

Table 4-2. CP/M-86 BDOS Functions ©

a

System Reset Return Login Vector
1 Console Input 23 Return Current Disk
Console Output 26 Set DMA Address
3 Reader Input 27\* Get Addr (Alloc)
4 Punch Output 28 Write Protect Disk
5 List Output 29 Get Addr (R/O Vector)
6\* Direct Console I/0 30 Set File Attributes

Get I/O Byte 31\* Get Addr(Disk Parms)
Set I/O Byte 32 Set/Get User Code
Print String 33 Read Random
10 Read Console Buffer 34 Write Random
11 Get Console Status 35 Compute File Size
12 Return Version Number 36 Set Random Record
13 Reset Disk System 37\* Reset drive
14 Select Disk 40 Write Random with Zero Fill
15 Open File 50\* Direct BIOS Call
16 Close File 51\* Set DMA Segment Base
17 Search for First 52\* Get DMA Segment Base
18 Search for Next 53\* Get Max Memory Available
19 Delete File 54\* Get Max Mem at Abs Location
20 Read Sequential 55\* Get Memory Region
21 Write Sequential 56\* Get Absolute Memory Region
22 Make File 57\* Free memory region
Rename File Free all memory
Program load

The individual BDOS functions are described below in three
sections which cover the simple functions, file operations, and
extended operations for memory management and program loading.

---
_Page 028_

CP/M-86 System Guide 4.2 Simple BDOS Calls

4.2 Simple BDOS Calls

The first set of BDOS functions cover the range 0 through 12,
and perform simple functions such as system reset and single
character I/0.

_ Entry
eens ad

Return

CL: 00H FUNCTION. 0
DL: Abort SYSTEM RESET
Code

The system reset function returns control to the CP/M operating
system at the CCP command level. The abort code in DL has two
possible values: if DL = 00H then the currently active program is
terminated and control is returned to the CCP. If DL is a 01H, the
program remains in memory and the memory allocation state remains

unchanged.
Entry Return
—
CL: 01H FUNCTION 1 AL: ASCII Character

©} CONSOLE INPUT

The console input function reads the next character from the
logical console device (CONSOLE) to register AL. Graphic
characters, along with carriagé return, line feed, and backspace
(CONTROL-H) are echoed to the console. Tab characters (CONTROL-I)
are expanded in columns of eight characters. The BDOS does not
return to the calling program until a character has been typed, thus
suspending execution if a character is not ready.

Entry Return
CL: 02H FUNCTION 2
DL: ASCII CONSOLE OUTPUT
Character
The ASCII character from DL is sent to the logical console.
Tab characters expand in columns of eight characters. In addition,

a check is made for start/stop scroll (CONTROL-S).

---
_Page 029_

CP/M-86 System Guide 4.2 Simple BDOS Calls

Entry

Return
CL: 03H FUNCTION 3 AL: ASCII Character

READER INPUT

The Reader Input function reads the next character from the
logical reader (READER) into register AL. Control does not return
until the character has been read.

Entry

Return
LS
a _d ed
CL: 04H FUNCTION 4
DL: ASCII PUNCH OUTPUT
Character

The Punch Output function sends the character from register DL
to the logical punch device (PUNCH).

Entry
ee ad
CL: 05H FUNCTION 5
DL: ASCII LIST OUTPUT
Character

The List Output function sends the ASCII character in register
DL to the logical list device (LIST).

---
_Page 030_

|

CP/M-86 System Guide 4.2 Simple BDOS Calls

Entry Return
CL: 06H FUNCTION 6 AL: char or status
DL: OFFH (input) | |DIRECT CONSOLE 1/0 (no value)
OFEH (status)
or

char (output)

Direct console I/O is supported under CP/M-86 for those
specialized applications where unadorned console input and output is
required. Use of this function should, in general, be avoided since
it bypasses all of CP/M-86's normal control character functions
(e.g., CONTROL-S and CONTROL-P). Programs which perform direct 1/0
through the BIOS under previous releases of CP/M-80, however, should
be changed to use direct I/O under the BDOS so that they can be
fully supported under future releases of MP/M™ and CP/M.

Upon entry to function 6, register DL either contains (1) a
hexadecimal FF, denoting a CONSOLE input request, or (2) a
hexadecimal FE, denoting a CONSOLE status request, or (3) an ASCII
character to be output to CONSOLE where CONSOLE is the logical
console device. If the input value is FF, then function 6 directly
calls the BIOS console input primitive. The next console input
character is returned in AL. If the input value is FE, then function
6 returns AL = 00 if no character is ready and AL = FF otherwise.
If the input value in DL is not FE or FF, then function 6 assumes
that DL contains a valid ASCII character which is sent to the
console.

Entry Return
a A

CL: O7H FUNCTION 7 AL: I/O Byte Value

GET I/O BYTE

The Get I/O Byte function returns the current value of IOBYTE
in register AL. The IOBYTE contains the current assignments for the
logical devices CONSOLE, READER, PUNCH, and LIST provided the IOBYTE
facility is implemented in the BIOS.

---
_Page 031_

CP/M-86 System Guide 4.2 Simple BDOS Calls

Entry Return
CL: 08H FUNCTION 8
DL: I/O Byte SET I/O BYTE
Value

The Set I/O Byte function changes the system IOBYTR value to
that given in register DL. This function allows transient program
access to the IOBYTE in order to modify the current assignments for
the logical devices CONSOLE, READER, PUNCH, and LIST.

Entry SN Return
aw mm aa
CL: 09H FUNCTION 9
DX: String PRINT STRING
Offset

The Print String function sends the character string stored in
memory at the location given by DX to the logical console device
(CONSOLE), until a "$" is encountered in the string. Tabs are
expanded as in function 2, and checks are made for start/stop scroll
and printer echo.

Entry Return
an ad man aa
CL: OAH FUNCTION 10 Console Characters

DX: Buffer READ CONSOLE BUFFER in Buffer
Offset

---
_Page 032_

CP/M-86 System Guide 4.2 Simple BDOS Calls

The Read Buffer function reads a line of edited console input into a
buffer addressed by register DX from the logical console device
(CONSOLE). Console input is terminated when either the input buffer
is filled or when a return (CONTROL-M) or a line feed (CONTROL-J)
character is entered. The input buffer addressed by DX takes the
form:

DX: +0 +1 +2 +3 +4 +5 +6 +7 +8 eee +n

[m]nefenfez}eafes}esfesfer] «|?

where "mx" is the maximum number of characters which the buffer will
hold, and "nc" is the number of characters placed in the buffer.
The characters entered by the operator follow the "nc" value. The
value "mx" must be set prior to making a function 10 call and may
range in value from 1 to 255. Setting mx to zero is equivalent to
setting mx to one. The value "nc" is returned to the user and may
range from 0 to mx. If ne < mx, then uninitialized positions follow
the last character, denoted by "??" in the above figure. Note that
a terminating return or line feed character is not placed in the
buffer and not included in the count "nc".

A number of editing control functions are supported during
console input under function 10. These are summarized in Table 4-3.

Table 4-3. Line Editing Controls

rub/del removes and echoes the last character
CONTROL-C reboots when at the beginning of line
CONTROL-E causes physical end of line

CONTROL-H backspaces one character position
CONTROL-J (line feed) terminates input line
CONTROL-M (return) terminates input line
CONTROL-R retypes the current line after new line
CONTROL-U removes current line after new line
CONTROL-X backspaces to beginning of current line

Certain functions which return the carriage to the leftmost position
(e.g., CONTROL-X) do so only to the column position where the prompt
ended. This convention makes operator data input and line
correction more legible.

---
_Page 033_

CP/M-86 System Guide 4.2 Simple BDOS Calls

Entry Return

we
CL: O0BH FUNCTION 11 AL: Console Status

GET CONSOLE STATUS

The Console Status function checks to see if a character has
been typed at the logical console device (CONSOLE). If a character
is ready, the value 01H is returned in register AL. Otherwise a 00H
value is returned.

Entry Return

Ce ed
CL: OCH FUNCTION 12 BX: Version Number

RETURN VERSION NUMBER

Function 12 provides information which allows version
independent programming. A two-byte value is returned, with BH = 00
designating the CP/M release (BH = 01 for MP/M), and BL = 00 for all
releases previous to 2.0. CP/M 2.0 returns a hexadecimal 20 in
register BL, with subsequent version 2 releases in the hexadecimal
range 21, 22, through 2F. To provide version number compatibility,
the initial release of CP/M-86 returns a 2.2.

4.3 BDOS Pile Operations

Functions 12 through 52 are related to disk file operations
under CP/M-86. In many of these operations, DX provides the Ds-
relative offset to a file control block (FCB). The File Control
Block (FCB) data area consists of a sequence of 33 bytes for
sequential access, or a sequence of 36 bytes in the case that the
file is accessed randomly. The default file control block normally
located at offset 005CH from the DS register can be used for random
access files, since bytes 007NH, 007EH, and 007FH are available for
this purpose. Here is the FCB format, followed by definitions of
each of its fields:

---
_Page 034_

CP/M-86 System Guide 4.3 BDOS File Operations

ac ea} ea] /_/|eo|erfe2]es}ex er|s2]rc|ao]7 /Janfer| ro] a} r2

00 01 02 ... 08 09 10 11 12 13 14 15 16 ... 31 32 33 34 35

where

dr drive code (0 - 16)
0 => use default drive for file
1 => auto disk select drive A,
2 => auto disk select drive B,

16=> auto disk select drive P.

f1...£8 contain the file name in ASCII
upper case, with high bit = 0

t1,t2,t3 contain the file type in ASCII
upper case, with high bit = 0
t1\*, t2°, and t3\* denote the high
bit of these positions,

tl” = 1 => Read/Only file,
t2° = 1 => SYS file, no DIR list
ex contains the current extent number,

normally set to 00 by the user, but
in range 0 - 31 during file I/O

sl reserved for internal system use

$2 reserved for internal system use, set
to zero on call to OPEN, MAKE, SEARCH

re record count for extent "ex,"
takes on values from 0 - 128

d0...dn filled-in by CP/M, reserved for
system use

cr current record to read or write in
a sequential file operation, normally
set to zero by user

r0,rl,r2 optional random record number in the
range 0-65535, with overflow to r2, 00
rangé 0-65535, with overflow to r2,
r0,rl constitute a 16-bit value with
low byte r0, and high byte rl

_Por users of earlier versions of CP/M, it should be noted in
passing that both CP/M Version 2 and CP/M-86 perform directory
operations in a reserved area of memory that does not affect write
buffer content, except in the case of Search and Search Next where
the directory record is copied to the current DMA address.

---
_Page 035_

CP/M-86 System Guide 4.3 BDOS File Operations

There are three error situations that the BDOS may encounter during
file processing, initiated as a result of a BDOS File I/O function
call. When one of these conditions is detected, the BDOS issues the
following message to the console:

BDOS ERR ON xX: error

where x is the drive name of the drive selected when the error
condition is detected, and "error" is one of the three messages:

BAD SECTOR SELECT R/O

These error situations are trapped by the BDOS, and thus the
executing transient program is temporarily halted when the error is
detected. No indication of the error situation is returned to the
transient program.

The "BAD SECTOR" error is issued as the result of an error
condition returned to the BDOS from the BIOS module. The BDOS makes
BIOS sector read and write commands as part of the execution of BDOS
file related system calls. If the BIOS read or write routine
detects a hardware error, it returns an error code to the BDOS
resulting in this error message. The operator may respond to this
error in two ways: a CONTROL-C terminates the executing program,
while a RETURN instructs CP/M-86 to ignore the error and allow the
program to continue execution.

The "SELECT" error is also issued as the result of an error
condition returned to the BDOS from the BIOS module. The BDOS makes
a BIOS disk select call prior to issuing any BIOS read or write to a
particular drive. If the selected drive is not supported in the
BIOS module, it returns an error code to the BDOS resulting in this
error message. CP/M-86 terminates the currently running program and
returns to the command level of the CCP following any input from the
console.

The "R/O" message occurs when the BDOS receives a command to
write to a drive that is in read-only status. Drives may be placed
in read-only status explicitly as the result of a STAT command or
BDOS function call, or implicitly if the BDOS detects that disk
media has been changed without performing a "warm start." The
ability to detect changed media is optionally included in the BIOS,
and exists only if a checksum vector is included for the selected
drive. Upon entry of any character at the keyboard, the transient
program is aborted, and control returns to the CCP.

---
_Page 036_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
ae fe
CL: ODH FUNCTION 13

RESET DISK SYSTEM

The Reset Disk Function is used to programmatically restore the
file system to a reset state where all disks are set to read/write
(see functions 28 and 29), only disk drive A is selected. This
function can be used, for example, by an application program which
requires disk changes during operation. Function 37 (Reset Drive)
can also be used for this purpose.

Entry
ane can
CL: OEH FUNCTION 14
DL: Selected SELECT DISK
Disk

The Select Disk function designates the disk drive named in
register DL as the default disk for subsequent file operations, with
DL = 0 for drive A, 1 for drive B, and so-forth through 15
corresponding to drive P in a full sixteen drive system. In
addition, the designated drive is logged-in if it is currently in
the reset state. Logging-in a drive places it in "on-line" status
which activates the drive’s directory until the next cold start,
warm start, disk system reset, or drive reset operation. FCB’s
which specify drive code zero (dr = 00H) automatically reference the
currently selected default drive. Drive code values between 1 and
16, however, ignore the selected default drive and directly
reference drives A through P.

Entry Return
CN
ne ae ed
CL: OFH FUNCTION 15 AL: Return Code
DX: FCB OPEN FILE
Offset

The Open File operation is used to activate a FCB specifying a
file which currently exists in the disk directory for the currently
active user number. The BDOS scans the disk directory of the drive
specified by byte 0 of the FCB referenced by DX for a match in
positions 1 through 12 of the referenced FCB, where an ASCII
question mark (3FH) matches any directory character in any of these
positions. Normally, no question marks are included and, further,
byte "ex" of the FCB is set to zero before making the open call.

---
_Page 037_

CP/M-86 System Guide 4.3 BDOS File Operations

If a directory element is matched, the relevant directory
information is copied into bytes d0 through dn of the FCB, thus
allowing access to the files through subsequent read and write
operations. Note that an existing file must not be accessed until a
successful open operation is completed. Further, an FCB not
activated by either an open or make function must not be used in
BDOS read or write commands. Upon return, the open function returns
a "directory code" with the value 0 through 3 if the open was
successful, or OFFH (255 decimal) if the file cannot be found. If
question marks occur in the FCB then the first matching FCB is
activated. Note that the current record ("cr") must be zeroed by
the program if the file is to be accessed sequentially from the
first record.

Entry Return
od es ae
CL: 10H FUNCTION 16 AL: Return Code
DX: FCB CLOSE FILE
Offset

The Close File function performs the inverse of the open file
function. Given that the FCB addressed by DX has been previously
activated through an open or make function (see functions 15 and
22), the close function permanently records the new. FCB in the
referenced disk directory. The FCB matching process for the close
is identical to the open function. The directory code returned for
a successful close operation is 0, 1, 2, or 3, while a OFFH (255
decimal) is returned if the file name cannot be found in the
directory. A file need not be closed if only read operations have
taken place. If write operations have occurred, however, the close
operation is necessary to permanently record the new directory
information.

---
_Page 038_

ee

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
al SE mma ast
CL: 11H FUNCTION 17 AL: Directory
Code
DX: FCB SEARCH FOR FIRST
Offset

Search First scans the directory for a match with the file
given by the FCB addressed by DX. The value 255 (hexadecimal FF) is
returned if the file is not found, otherwise 0, l, 2, or 3 is
returned indicating the file is present. In the case that the file
is found, the buffer at the current DMA address is filled with the
record containing the directory entry, and its relative starting
position is AL \* 32 (i.e., rotate the AL register left 5 bits).
Although not normally required for application programs, the
directory information can be extracted from the buffer at this
position.

An ASCII question mark (63 decimal, 3Ff hexadecimal) in any
position from "fl" through "ex" matches the corresponding field of
any directory entry on the default or auto-selected disk drive. If
the "dr" field contains an ASCII question mark, then the auto disk
select function is disabled, the default disk is searched, with the
search function returning any matched entry, allocated or free,
belonging to any user number. This latter function is not normally
used by application programs, but does allow complete flexibility to
scan all current directory values. If the "dr" field is not a
question mark, the "s2" byte is automatically zeroed.

Entry Return
CL: 12H FUNCTION 18 AL: Directory
Code

SEARCH FOR NEXT

The Search Next function is similar to the Search First
function, except that the directory scan continues from the last
matched entry. Similar to function 17, function 18 returns the

decimal value 255 in A when no more directory items match. In terms

of execution sequence, a function 18 call must follow either a
function 17 or function 18 call with no other intervening BDOS disk
related function calls.

---
_Page 039_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
SEE aaee aa
CL: 13H FUNCTION 19 AL: Return Code
DX: FCB DELETE FILE
Offset

The Delete File function removes files which match the FCB
addressed by DX. The filename and type may contain ambiguous
references (i.e., question marks in various positions), but the
drive select code cannot be ambiguous, as in the Search and Search
Next functions. Function 19 returns a OFFH (decimal 255) if the
referenced file or files cannot be found, otherwise a value of zero
is returned.

Entry Return
rere
ey od es aad
CL: 14H FUNCTION 20 AL: Return Code
DX: FCB READ SEQUENTIAL
Offset

Given that the FCB addressed by DX has been activated through
an open or make function (numbers 15 and 22), the Read Sequential
function reads the next 128 byte record from the file into memory at
the current DMA address. The record is read from position "cr" of
the extent, and the "cr" field is automatically incremented to the
next record position. If the "cr" field overflows then the next
logical extent is automatically opened and the "cr" field is reset
to zero in preparation for the next read operation. The "cr" field
must be set to zero following the open call by the user if the
intent is to read sequentially from the beginning of the file. The
value 00H is returned in the AL register if the read operation was
successful, while a value of 01H is returned if no data exists at
the next record position of the file. Normally, the no data
situation is encountered at the end of a file. However, it can also
occur if an attempt is made to read a data block which has not been
previously written, or an extent which has not been created. These
situations are usually restricted to files created or appended by
use of the BDOS Write Random commmand (function 34).

---
_Page 040_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry
ae
CL: 15H FUNCTION 21
DX: FCB WRITE SEQUENTIAL
Offset

Given that the FCB addressed by DX has been activated through
an open or make function (numbers 15 and 22), the Write Sequential
function writes the 128 byte data record at the current DMA address
to the file named by the FCB. The record is placed at position "cr"
of the file, and the "cr" field is automatically incremented to the
next record position. If the "cr" field overflows then the next
logical extent is automatically opened and the "cr" field is reset
to zero in preparation for the next write operation. Write
operations can take place into an existing file, in which case newly
written records overlay those which already exist in the file. The
"cr" field must be set to zero following an open or make call by the
user if the intent is to write sequentially from the beginning of
the file. Register AL = 00H upon return from a successful write
operation, while a non-zero value indicates an unsuccessful write
due to one of the following conditions:

01 No available directory space - This condition occurs when
the write command attempts to create a new extent that
requires a new directory entry and no available directory
entries exist on the selected disk drive.

02 No available data block - This condition is encountered
when the write command attempts to allocate a new data
block to the file and no unallocated data blocks exist on
the selected disk drive.

Entry Return

Nee
ad es ae
CL: 16H FUNCTION 22 AL: Return Code
DX: FCB MAKE FILE
Offset

The Make File operation is similar to the open file operation
except that the FCB must name a file which does not exist in the
currently referenced disk directory (i.e., the one named explicitly
by a non-zero "dr" code, or the default disk if “dr" is zero). The
BDOS creates the file and initializes both the directory. and main
memory value to an empty file. The programmer must ensure that no
duplicate file names occur, and a preceding delete operation is
sufficient if there is any possibility of duplication. Upon return,
register A = 0, 1, 2, or 3 if the operation was successful and OFFH
(255 decimal) if no more directory space is available. The make
function has the side-effect of activating the FCB and thus a
subsequent open is not necessary.

---
_Page 041_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
ST ————
CL: 17H FUNCTION 23 AL: Return Code
DX: FCB RENAME FILE
Offset

The Rename function uses the FCB addressed by DX to change all
directory entries of the file specified by the file name in the
first 16 bytes of the FCB to the file name in the second 16 bytes.
It is the user’s responsibility to insure that the file names
specified are valid CP/M unambiguous file names. The drive code
"dr" at position 0 is used to select the drive, while the drive code.
for the new file name at position 16 of the FCB is ignored. Upon
return, register AL is set to a value of zero if the rename was
successful, and OFFH (255 decimal) if the first file name could not
be found in the directory scan.

Entry Return
a ee aa
CL: 18H FUNCTION 24 BX: Login Vector
BX: Login RETURN LOGIN
Vector VECTOR

The login vector value returned by CP/M-86 is a 16-bit value in
BX, where the least significant bit corresponds to the first drive
A, and the high order bit corresponds to the sixteenth drive,
labelled P. A "O" bit indicates that the drive is not on-line,
while a "1" bit marks an drive that is actively on-line due to an
explicit disk drive selection, or an implicit drive select caused by
a file operation which specified a non-zero "dr" field.

Entry Return
aa Na atl
CL: 19H FUNCTION 25 AL: Current Disk

RETURN CURRENT
DISK

Function 25 returns the currently selected default disk number
in register AL. The disk numbers range from 0 through 15
corresponding to drives A through P.

---
_Page 042_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
eS LN ne ae
CL: 1AH FUNCTION 26
DX: DMA SET DMA
Offset ADDRESS

"DMA" is an acronym for Direct Memory Address, which is often
used in connection with disk controllers which directly access the
memory of the mainframe computer to transfer data to and from the
disk subsystem. Although many computer systems use non-DMA access
(i.e., the data is transfered through programmed I/0 operations),
the DMA address has, in CP/M, come to mean the address at which the
128 byte data record resides before a disk write and after a disk
read. In the CP/M-86 environment, the Set DMA function is used to
specify the offset of the read or write buffer from the current DMA
base. Therefore, to svecify the DMA address, both a function 26
call and a function 51 call are required. Thus, the DMA address
becomes the value specified by DX plus the DMA base value until it
is changed by a subsequent Set DMA or set DMA base function.

Entry Return
Neer
ad OO ed
CL: 1BH FUNCTION 27 BX: ALLOC Offset

GET ADDR(ALLOC) ES: Segment base

An "allocation vector" is maintained in main memory for each
on-line disk drive. Various system programs use the information
provided by the allocation vector to determine the amount of
remaining storage (see the STAT program). Function 27 returns the
segment base and the offset address of the allocation vector for the
currently selected disk drive. The allocation information may,
however, be invalid if the selected disk has been marked read/only.

Entry Return
commen fA el
CL: 1CH FUNCTION 28

WRITE PROTECT DISK

The disk write protect function provides temporary write
protection for the currently selected disk. Any attempt to write to
the disk, before the next cold start, warm start, disk system reset,
or drive reset operation produces the message:

Bdos Err on a: R/O

---
_Page 043_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
—_—_——<—<—<—<—<—>- «___r—C NN ———
CL: 1DH FUNCTION 29 BX: R/O Vector Value

GET READ/ONLY
VECTOR

Function 29 returns a bit vector in register BX which indicates
drives which have the temporary read/only bit set. Similar to
function 24, the least significant bit corresponds to drive A, while
the most significant bit corresponds to drive P. The R/O bit is set
either by an explicit call to function 28, or by the automatic
software mechanisms within CP/M-86 which detect changed disks.

Entry Return
i ee
CL: lEH FUNCTION 30 AL: Return Code
DX: FCB SET FILE
Offset ATTRIBUTES

The Set File Attributes function allows programmatic
Manipulation of permanent indicators attached to files. Tn
particular, the R/O, System and Archive attributes (tl\*, t2°\*, and
t3°) can be set or reset. The DX pair addresses a FCB containing a
file name with the appropriate attributes set or reset. It is the
user\*s responsibility to insure that an ambiquous file name is not
specified. Function 30 searches the default disk drive directory
area for directory entries that belong to the current user number
and that match the FCB specified name and type fields. All matching
directory entries are updated to contain the selected indicators.
Indicators f1\* through £4° are not presently used, but may be useful
for applications programs, since they are not involved in the
matching process during file open and close operations. Indicators
£5° through £8\* are reserved for future system expansion. The
currently assigned attributes are defined as follows:

tl\*: The R/O attribute indicates if set that the file
is in read/only status. BDOS will not allow write
commands to be issued to files in R/O status.

t2°: The System attribute is referenced by the CP/M DIR
utility. If set, DIR will not display the file in
a directory display.

---
_Page 044_

CP/M-86 System Guide 4.3 BNOS File Operations

t3°: The Archive attribute is reserved but not actually
used by CP/M-86 If set it indicates that the file
has been written to back up storage by a user
written archive program. To implement this
facility, the archive program sets this attribute
when it copies a file to back up storage; any
Programs updating or creating files reset this
attribute. Further, the archive program backs up
only those files that have the Archive attribute
reset. Thus, an automatic back up facility
restricted to modified files can be easily
implemented.

Function 30 returns with register AL set to OFFH (255 decimal)
if the referenced file cannot be found, otherwise a value of zero is
returned.

Entry Return
CL: LFY FUNCTION 31 BX: DPB Offset

GET ADDR ES: Segment Rase
(DISK PARMS)

The offset and the seqment base of the BIOS resident disk
Darameter block of the currently selected drive are returned in BX
and ES as a result of this function cal]. This control block can be
used for either of two vurvoses. First, the disk parameter values
can be extracted for display and space computation purposes, or
transient programs can dynamically change the values of current disk
Parameters when the disk environment changes, if required.
Normally, application programs will not require this facility.
Section 6.3 defines the BINS disk parameter block.

Entry Return
CL: 20H FUNCTION 32 Al: Current Code
Or no value
DL: OFFH(get) SET /GET
or USER CODE
User Code

(set)

An application program can change or interrogate the currently
active user number by calling function 32. If register DL = OFFH,
then the value of the current user number is returned in register
AL, where the value is in the range 0 to 15. If register DL is not
OFFH, then the current user number is changed to the value of DL
(modulo 16).

---
_Page 045_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
ee ee NT
CL: 218 FUNCTION 33 AL: Return Code
DX: FCB READ RANDOM
Offset

The Read Random function is similar to the sequential file read
operation of previous releases, except that the read operation takes
place at a particular record number, selected by the 24-bit value
constructed from the three byte field following the FCB (byte
positions r0 at 33, rl at 34, and r2 at 35). Note that the sequence
of 24 bits is stored with least significant byte first (r0), middle
byte next (rl), and high byte last (r2). CP/M does not reference
byte r2, except in computing the size of a file (function 35). Byte
r2 must be zero, however, since a non-zero value indicates overflow
past the end of file.

Thus, the r0,rl byte pair is treated as a double-byte, or
"word" value, which contains the record to read. This value ranges
from 0 to 65535, providing access to any particular record of any
size file. In order to access a file using the Read Random
function, the base extent (extent 0) must first be opened. Although
the base extent may or may not contain any allocated data, this
ensures that the FCB is properly initialized for subsequent random
access operations. The selected record number is then stored into
the random record field (r0,rl), and the BDOS is called to read the
record. Upon return from the call, register AL either contains an
error code, as listed below, or the value 00 indicating the
operation was successful. In the latter case, the buffer at the
current DMA address contains the randomly accessed record. Note
that contrary to the sequential read operation, the record number is
not advanced. Thus, subsequent random read operations continue to
read the same record.

Upon each random read operation, the logical extent and current
record values are automatically set. Thus, the file can be
sequentially read or written, starting from the current randomly
accessed position. Note, however, that in this case, the last
randomly read record will be re-read as you switch from random mode
to sequential read, and the last record will be re-written as you
switch to a sequential write operation. You can, of course, simply
advance the random record position following each random read or
write to obtain the effect of a sequential I/O operation.

---
_Page 046_

CP/M-86 System Guide 4.3 BDOS File Operations

Error codes returned in register AL following a random read are
listed in Table 4-4, below.

© Table 4-4. Punction 33 (Read Random) Error Codes

01 Reading unwritten data - This error code is returned
when a random read operation accesses a data block which
has not been previously written.

(not returned by the Random Read command)

Cannot close current extent - This error code is
returned when BDOS cannot close the current extent prior
to moving to the new extent containing the record
specified by bytes r0,rl of the FCB. This error can be
caused by an overwritten FCB or a read random operation
on an FCB that has not been opened.

Seek to unwritten extent - This error code is returned
when a random read operation accesses an extent that has

not been created. This error situation is equivalent to
error Ol.

(not returned by the Random Read command)

Random record number out of, range - This error code is
returned whenever byte r2 of the FCB is non-zero.

Normally, non-zero return codes can be treated as missing data, with
zero return codes indicating operation complete.

---
_Page 047_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
——— ———
CL: 22H FUNCTION 34 AL: Return Code
DX: FCB WRITE RANDOM
Offset

The Write Random operation is initiated similar to the Read
Random call, except that data is written to the disk from the
current DMA address. Further, if the disk extent or data block
which is the target of the write has not yet been allocated, the
allocation is performed before the write operation continues. As in
the Read Random operation, the random record number is not changed
as a result of the write. The logical extent number and current
record positions of the file control block are set to correspond to
the random record which is being written. Sequential read or write
operations can commence following a random write, with the note that
the currently addressed record is either read or rewritten again as
the sequential operation begins. You can also simply advance the
random record position following each write to get the effect of a
sequential write operation. In particular, reading or writing the
last record of an extent in random mode does not cause an automatic
extent switch as it does in sequential mode.

In order to access a file using the Write Random function, the
base extent (extent 0) must first be opened. As in the Read Random
function, this ensures that the FCB is properly initialized for
subsequent random access operations. If the file is empty, a Make
File function must be issued for the base extent. Although the base
extent may or may not contain any allocated data, this ensures that
the file is properly recorded in the directory, and is visible in
DIR requests.

Upon return from a Write Random call, register AL either
contains an error code, as listed in Table 4-5 below, or the value
00 indicating the operation was successful.

Table 4-5. Function 34 (WRITE RANDOM) Error Codes

01 (not returned by the Random Write command)

02 No available data block - This condition is encountered
when the Write Random command attempts to allocate a new
data block to the file and no unallocated data blocks
exist on the selected disk drive.

---
_Page 048_

CP/M-86 System Guide 4.3 BDOS File Operations

Table 4-5. (continued)

=

Cannot close current extent - This error code is
returned when BDOS cannot close the current extent prior
to moving to the new extent containing the record
specified by bytes r0,rl of the FCB. This error can be
caused by an overwritten FCB or a write random operation
on an FCB that has not been opened.

(not returned by the Random Write command)

No available directory space - This condition occurs
when the write command attempts to create a new extent
that requires a new directory entry and no available
directory entries exist on the selected disk drive.

Random record number out of range - This error code is
returned whenever byte r2 of the FCB is non-zero.

Entry Return
a ——
CL: 23H FUNCTION 35 Random Record
Field Set

DX: FCB COMPUTE FILE
Offset SIZE

When computing the size of a file, the DX register addresses an
FCB in random mode format (bytes r0, rl, and r2 are present). The
FCB contains an unambiguous file name which is used in the directory
scan. Upon return, the random record bytes contain the "virtual"
file size which is, in effect, the record address of the record
following the end of the file. If, following a call to function 35,
the high record byte r2 is 01, then the file contains the maximum
record count 65536. Otherwise, bytes r0 and rl constitute a 16-bit
value (r0 is the least significant byte, as before) which is the
file size.

Data can be appended to the end of an existing file by simply
calling function 35 to set the random record position to the end of
file, then performing a sequence of random writes starting at the
preset record address.

The virtual size of a file corresponds to the physical size
when the file is written sequentially. If, instead, the file was
created in random mode and "holes" exist in the allocation, then the
file may in fact contain fewer records than the size indicates. If,
for example, a single record with record number 65535 (CP/M’s
maximum record number) is written to a file using the Write Random
function, then the virtual size of the file is 65536 records,
although only one block of data is actually allocated.

---
_Page 049_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
Cae ed ae cand
CL: 24H FUNCTION 36 Random Record
Field Set

DX: FCB SET RANDOM
Offset RECORD

The Set Random Record function causes the BDOS to automatically
produce the random record position of the next record to be accessed
from a file which has been read or written sequentially to a
particular point. The function can be useful in two ways.

First, it is often necessary to initially read and scan a
sequential file to extract the positions of various "key" fields.
As each key is encountered, function 36 is called to compute the
random record position for the data corresponding to this key. If
the data unit size is 128 bytes, the resulting record position minus
one is placed into a table with the key for later retrieval. After
scanning the entire file and tabularizing the keys and their record
numbers, you can move instantly to a particular keyed record by
performing a random read using the corresponding random record
number which was saved earlier. The scheme is easily generalized
when variable record lengths are involved since the program need
only store the buffer-relative byte position along with the key and
record number in order to find the exact starting position of the
keyed data at a later time.

A second use of function 36 occurs when switching from a
sequential read or write over to random read or write. A file is
sequentially accessed to a particular point in the file, function 36
is called which sets the record number, and subsequent random read
and write operations continue from the next record in the file.

Entry
es ae
CL: 25H FUNCTION 37
DX: Drive RESET DRIVE
Vector

The Reset Drive function is used to programmatically restore
specified drives to the reset state (a reset drive is not logged-in
and is in read/write status). The passed parameter in register DX
is a 16 bit vector of drives to be reset, where the least
significant bit corresponds to the first drive, A, and the high
order bit corresponds to the sixteenth drive, labelled P. Bit
values of "1" indicate that the specified drive is to be reset.

In order to maintain compatibility with MP/M, CP/M returns a
zero value for this function.

---
_Page 050_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
od Od
CL: 28H FUNCTION 40 AL: Return Code
DX: FCB WRITE RANDOM
Offset WITH ZERO FILL

The Write Random With Zero Fill function is similar to the
Write Random function (function 34) with the exception that a
previously unallocated data block is initialized to records filled
with zeros before the record is written. If this function has been
used to create a file, records accessed by a read random operation
that contain all zeros identify unwritten random record numbers.
Unwritten random records in allocated data blocks of files created
using the Write Random function contain uninitialized data.

Entry Return
ee a cad
CL: 32H FUNCTION 50
DX: BIOS Aj DIRECT BIOS CALL
Descriptor

Function 50 provides a direct BIOS call and transfers control
through the BDOS to the BIOS. The DX register addresses a five-byte
memory area containing the BIOS call parameters:

8-bit 16-bit 16-bit

value(CX) | value (DX)

where Func is a BIOS function number, (see Table 5-1), and value (CX)
and value(DX) are the 16-bit values which would normally be passed
directly in the CX and DX registers with the BIOS call. The CX and
DX values are loaded into the 8086 registers before the BIOS call is
initiated.

---
_Page 051_

CP/M-86 System Guide 4.3 BDOS File Operations

Entry Return
OE na ae ee

CL: 33H FUNCTION 51

DX: Base SET DMA BASE

Address

Function 51 sets the base register for subsequent DMA
transfers. The word parameter in DX is a paragraph address and is
used with the DMA offset to specify the address of a 128 byte buffer
area to be used in the disk read and write functions. Note that
upon initial program loading, the default DMA base is set to the
address of the user’s data segment (the initial value of DS) and the
DMA offset is set to 0080H, which provides access to the default
buffer in the base page.

Entry Return
SN aa ad SS
CL: 34H FUNCTION 52 BX: DMA Offset
GET DMA BASE ES: DMA Segment

Function 52 returns the current DMA Base Segment address in ES,
with the current DMA Offset in DX.

4.4 BDOS Memory Management and Load

Memory is allocated in two distinct ways under CP/M-86. The
first is through a static allocation map, Located within the BIOS,
that defines the physical memory which is available on the host
system. In this way, it is possible to operate CcP/M-86 in a memory
configuration which is a mixture of up to eight non-contiguous areas
of RAM or ROM, along with reserved, missing, or faulty memory
regions. In a simple RAM-based system with contiguous memory, the
static map defines a single region, usually starting at the end of
the BIOS and extending up to the end of available memory.

Once memory is physically mapped in this manner, CP/M-86
performs the second level of dynamic allocation to support transient
program loading and execution. CP/M-86 allows dynamic allocation of
memory into, again, eight regions. A request for allocation takes
place either implicitly, through a program load operation, or
explicitly through the BNOS calls given in this section. Programs
themselves are loaded in two ways: through a command entered at the
CCP level, or through the BDOS Program Load operation (function 59).
Multiple programs can be loaded at the CCP level, as long as each
program executes a System Reset (function 0) and remains in memory
(DL = 01H). Multiple programs of this type only receive control by
intercepting interrupts, and thus under normal circumstances there

---
_Page 052_

CP/M-86 System Guide 4.4 BDOS Memory Management and Load

is only one transient program in memory at any given time. If,
however, multiple programs are present in memory, then CONTROL-C
characters entered by the operator delete these programs in the
opposite order in which they were loaded no matter which program is
actively reading the console.

Any given program loaded through a CCP command can, itself,
load additional programs and allocate data areas. Suppose four
regions of memory are allocated in the following order: a program
is loaded at the CCP level through an operator command. The CMD
file header is read, and the entire memory image consisting of the
program and its data is loaded into region A, and execution begins.
This program, in turn, calls the BDOS Program Load function (59) to
load another program into region B, and transfers control to the
loaded program. The region B program then allocates an additional
region C, followed by a region D. The order of allocation is shown
in Figure 4-1 below:

Figure 4-1. Example Memory Allocation

There is a hierarchical ownership of these regions: the program in
A controls all memory from A through D. The program in B also
controls regions B through D. The program in A can release regions
B through D, if desired, and reload yet another program. DDT-86,
for example, operates in this manner by executing the Free Memory
call (function 57) to release the memory used by the current program
before loading another test program. Further, the program in B can
release regions C and D if required by the application. It must be
noted, however, that if either A or B terminates by a System Reset
(BDOS function 0 with DL = 00H) then all four regions A through D
are released.

---
_Page 053_

CP/M-86 System Guide 4.4 BDOS Memory Management and Load

A transient program may release a portion of a region, allowing
the released portion to be assigned on the next allocation request.
The released portion must, however, be at the beginning or end of
the region. Suppose, for example, the program in region B above
receives 800H paragraphs at paragraph location 100H following its
first allocation request as shown in Figure 4-2 below.

1000H:

Length =
8000H Region C€

Figure 4-2. Example Memory Region

Suppose further that region D is then allocated. The last 200H
paragraphs in region C can be returned without affecting region D by
releasing the 200H paragraphs beginning at paragraoh base 700H,
resulting in the memory arrangement shown in Figure 4-3.

1000H:
Length =
6000H . Region C
Length = a 70008: | ///////////
2000H IIIT IS TITS

Figure 4-3. Example Memory Regions

The region beginning at paragraph address 700H is now available for
allocation in the next request. Note that a memory request will
fail if eight memory regions have already been allocated. Normally,
if all program units can reside in a contiguous region, the system
allocates only one region.

---
_Page 054_

CP/M-86 System Guide 4.4 BDOS Memory Management and Load

Memory management functions beginning at 53 reference a Memory
Control Block (MCB), defined in the calling program, which takes the
form:

16-bit 16-bit 8-bit

where M-Base and M-Length are either input or output values
expressed in 16-byte paragraph units, and M-Ext is a returned byte
value, as defined specifically with each function code. An error
condition is normally flagged with a OFFH returned value in order to
match the file error conventions of CP/M.

Entry Return
es CE
CL: 35H FUNCTION 53 AL: Return Code
DX: Offset GET MAX MEM
of MCB

Function 53 finds the largest available memory region which is
less than or equal to M-Length paragraphs. If successful, M-Base is
set to the base paragraph address of the available area, and M-
Length to the paragraph length. AL has the value OFFH upon return
if no memory is available, and 00H if the request was successful.
M~Ext is set to 1 if there is additional memory for allocation, and
0 if no additional memory is available.

Entry Return
Ne
sd a ad
CL: 36H FUNCTION 54 AL: Return Code
DX: Offset GET ABS MAX
of MCB

Function 54 is used to find the largest possible region at the
absolute paragraph boundary given by M-Base, for a maximum of M-
Length paragraphs. M-Length is set to the actual length if
successful. AL has the value OFFH upon return if no memory is
available at the absolute address, and 00H if the request was
successful.

---
_Page 055_

CP/M-86 System Guide 4.4 BDOS Memory Management and Load

Entry Return
CL: 37H FUNCTION 55 AL: Return Code

DX: Offset ALLOC MEM
of MCB

The allocate memory function allocates a memory area according
to the MCB addressed by DX. The allocation request size is obtained
from M-Length. Function 55 returns in the user’s MCB the base
paragraph address of the allocated region. Register AL contains a
OOH if the request was successful and a OFFH if the memory could not
be allocated.

Entry Return
———_———— ———
CL: 38H FUNCTION 56 AL: Return Code
DX: Offset ALLOC ABS MEM
of MCB

The allocate absolute memory function allocates a memory area
according to the MCB addressed by NX. The allocation request size
is obtained from M-Length and the absolute base address from M-Base.
Register AL contains a 00H if the request was successful and a OFFH
if the memory could not be allocated.

Entry Return
en aad Oe ad
CL: 39H FUNCTION 57
DX: Offset FREE MEM
of MCB

Function 57 is used to release memory areas allocated to the
program. The value of the M-Ext field controls the operation of
this function: if M-Ext = OFFH then all memory areas allocated by
the calling program are released. Otherwise, the memory area of
length M-Length at location M-Base given in the MCB addressed by DX
is released (the M-Ext field-should be set to 00H in this case). As
described above, either an entire allocated region must be released,
or the end of a region must be released: the middle section cannot
be returned under CP/M-86.

---
_Page 056_

CP/M-86 System Guide 4.4 BDOS Memory Management and Load

Entry Return
od ee ad
CL: 3AH FUNCTION 58

FREE ALL MEM

Function 58 is used to release all memory in the CP/M-86
environment (normally used only by the CCP upon initialization).

Entry Return
—— ——
CL: 3BH FUNCTION 59 AX: Return Code/
Base Page Addr
NX: Offset PROGRAM LOAD BX: Base Page Addr

of FCB

Function 59 loads a CMD file. Wpon entry, register DX contains
the DS relative offset of a successfully opened FCB which names the
input CMD file. AX has the value OFFFFH if the program load was
unsuccessful. Otherwise, AX and BX both contain the paragraph
address of the base page belonging to the loaded program. The base
address and segment length of each segment is stored in the base
page. Note that upon program load at the CCP level, the DMA base
address is initialized to the base page of the loaded program, and
the DMA offset address is initialized to 0080H. However, this is a
function of the CCP, and a function 59 does not establish a default
DMA address. It is the responsibility of the program which executes
function 59 to execute function 51 to set the DMA base and function
26 to set the DMA offset before passing control to the loaded
program.

---
_Page 057_

Section 5
Basic I/O System (BIOS) Organization

The distribution version of CP/M-86 is setup for operation with
the Intel SBC 86/12 microcomputer and an Intel 204 diskette
controller. All hardware dependencies are, however, concentrated in
subroutines which are collectively referred to as the Basic I/0
System, or BIOS. A CP/M-86 system implementor can modify these
subroutines, as described below, to tailor CP/M-86 to fit nearly any
8086 or 8088 operating environment. This section describes the
actions of each BIOS entry point, and defines variables and tables
referenced within the BIOS. The discussion of Disk Definition
Tables is, however, treated separately in the next section of this
manual.

5.1 Organization of the BIOS

The BIOS portion of CP/M-86 resides in the topmost portion of
the operating system (highest addresses), and takes the general form
shown in Figure 5-1, below:

CS, DS, ES, SS:

Console
Command
Processor

and
Basic
Disk
Operating
System

cS + 25008: BIOS Jump Vector

CS + 253FH:

BIOS Entry Points

BIOS:
Disk
Parameter
Tables

Uninitialized
Scratch RAM

Figure 5-1. General CP/M-86 Organization

---
_Page 058_

CP/M-86 System Guide 5.1 Organization of the BIOS

As described in the following sections, the CCP and BDOS are
supplied with CP/M-86 in hex file form as CPM.H86. In order to
implement CP/M-86 on non-standard hardware, you must create a BIOS
which performs the functions listed below and concatenate the
resulting hex file to the end of the CPM.H86 file. The GENCMD
utility is then used to produce the CPM.SYS file for subsequent load
by the cold start loader. The cold start loader that loads the
CPM.SYS file into memory contains a simplified form of the BIOS,
called the LDBIOS (Loader BIOS). It loads CPM.SYS into memory at
the location defined in the CPM.SYS header (usually 0400H). The
procedure to follow in construction and execution of the cold start
loader and the CP/M-86 Loader is given in a later section.

Appendix D contains a listing of the standard CP/M-86 BIOS for
the Intel SBC 86/12 system using the Intel 204 Controller Board.
Appendix E shows a sample "skeletal" BIOS called CBIOS that contains
the essential elements with the device drivers removed. You may
wish to review these listings in order to determine the overall
structure of the BIOS.

5.2 The BIOS Jump Vector

Entry to the BIOS is through a "jump vector" located at offset
2500H from the base of the operating system. The jump vector is a
sequence of 21 three-byte jump instructions which transfer program
control. to the individual BIOS entry points. Although some non-
essential BIOS subroutines may contain a single return (RET)
instruction, the corresponding jump vector element must be present
in the order shown below in Table 5-1. An example of a BIOS jump
vector may be found in Appendix D, in the standard CP/M-86 BIOS
listing.

Parameters for the individual subroutines in the BIOS are
passed in the CX and DX registers, when required. CX receives the
first parameter; DX is used for a second argument. Return values
are passed in the registers acco ding to type: Byte values are
returned in AL. Word values (16 bits) are returned in BX. Specific
parameters and returned values are described with each subroutine.

---
_Page 059_

CP/M-86 System Guide 5.2 The BIOS Jump Vector

Table 5-1. BIOS Jump Vector

Offset from Suggested
Beginning Instruction |F# Description
of BIOS

Arrive Here from Cold Boot
Arrive Here for Warm Start
Check for Console Char Ready
Read Console Character In
Write Console Character Out
Write Listing Character Out
Write Char to Punch Device
Read Reader Device

Move to Track 00

Select Disk Drive

Set Track Number

SETSEC Set Sector Number

SETDMA Set DMA Offset Address

READ Read Selected Sector

WRITE Write Selected Sector
LISTST Return List Status

SECTRAN Sector Translate

SETDMAB Set DMA Segment Address
GETSEGB Get MEM DESC Table Offset
GETIOB Get I/O Mapping Byte

SETIOB Set I/O Mapping Byte

INIT
WBOOT
CONST
CONIN
CONOUT
LIST
PUNCH
READER
HOME
SELDSK
SETTRK

aed oad
NH O

There are three major divisions in the BIOS jump table: system
(re) initialization subroutines, simple character I/O subroutines,
and disk I/O subroutines.

5.3 Simple Peripheral Devices

All simple character I/O operations are assumed to be performed
in ASCII, upper and lower case, with high order (parity bit) set to
zero. An end-of-file condition for an input device is given by an
ASCII control-z (1AH). Peripheral devices are seen by CP/M-86 as
"logical" devices, and are assigned to physical devices within the
BIOS. Device characteristics are defined in Table 5-2.

---
_Page 060_

CP/M-86 System Guide 5.3 Simple Peripheral Nevices

Table 5-2. CP/M-86 Logical Device Characteristics

Device Name Characteristics

CONSOLE The principal interactive console which
communicates with the operator, accessed through
CONST, CONIN, and CONOUT. Typically, the CONSOLE
is a device such as a CRT or Teletype.

The principal listing device, if it exists on your
system, which is usually a hard-copy device, such
as a printer or Teletype.

The principal tape punching device, if it exists,
which is normally a high-speed paper tape punch or
Teletype.

READER The principal tape reading device, such as a
simple optical reader or teletype.

Note that a single peripheral can be assigned as the LIST,
PUNCH, and READER device simultaneously. If no peripheral device is
assigned as the LIST, PUNCH, or READER device, your CBIOS should
give an appropriate error message so that the svstem does not "hang"
if the device is accessed by PIP or some other transient program.
Alternately, the PUNCH and LIST subroutines can just simply return,
and the READER subroutine can return with a 1AH (ctl-%) in req A to
indicate immediate end-of-file.

For added flexibility, you can optionally implement the
"IOBYTE" function which allows reassignment of physical and logical
devices. The IOBYTE function creates a mapping of logical to
physical devices which can be altered during CP/M-86 processing (see
the STAT command). The definition of the IOBYTE function
corresponds to the Intel standard as follows: a single location in
the BIOS is maintained, called IOBYTE, which defines the logical to
physical device mapping which is in effect at a particular time.
The mapping is performed by splitting the IOBYTE into four distinct
fields of two bits each, called the CONSOLE, READER, PUNCH, and LIST
fields, as shown below:

most significant least significant

IOBYTE LIST PUNCH READER CONSOLE

bits 6,7 bits 4,5 bits 2,3 bits 0,1

---
_Page 061_

CP/M-86 System Guide 5.3 Simple Peripheral Devices

The value in each field can be in the range 0-3, defining the
assigned source or destination of each logical device. The values
which can be assigned to each field are given in Table 5-3, below.

©} Table 5-3. IOBYTE Field Definitions

CONSOLE field (bits 0,1)
0 - console is assigned to the console printer (TTY:)
1 - console is assigned to the CRT device (CRT:)
2 - batch mode: use the READER as the CONSOLE input,
and the LIST device as the CONSOLE output (BAT:)
3 user defined console device (UC1:)

READER field (bits 2,3)
0 - READER is the Teletype device (TTY:)
1 - READER is the high-speed reader device (RDR:)
2 - user defined reader # 1 (UR1:)
3 - user defined reader # 2 (UR2:)

PUNCH field (bits 4,5)
0 - PUNCH is the Teletype device (TTY:)
1 - PUNCH is the high speed punch device (PUN:)
2 - user defined punch # 1 (UP1:)
3 =- user defined punch # 2 (UP2:)

LIST field (bits 6,7)
0 - LIST is the Teletype device (TTY:)

1 - LIST is the CRT device (CRT:)
2 - LIST is the line printer device (LPT:)

3 - user defined list device (UL1:)

Note again that the implementation of the IOBYTE is optional,
and atfects only the organization of your CBIOS. No CP/M-86
utilities use the IOBYTE except for PIP which ,allows access to the
physical devices, and STAT which allows logical-physical assignments
to be made and displayed. In any case, you should omit the IOBYTE
implementation until your basic CBIOS is fully implemented and
tested, then add the IOBYTE to increase your facilities.

---
_Page 062_

CP/M-86 System Guide 5.4 BIOS Subroutine Entry Points

5.4 BIOS Subroutine Entry Points

The actions which must take place upon entry to each BIOS
subroutine are given below. It should be noted that disk I/O is
always performed through a sequence of calls on the various disk
access subroutines. These setup the disk number to access, the
track and sector on a particular disk, and the direct memory access
(DMA) offset and segment addresses involved in the 1/0 operation.
After all these parameters have been setup, a call is made to the
READ or WRITE function to perform the actual I/O operation. Note
that there is often a single call to SELDSK to select a disk drive,
followed by a number of read or write operations to the selected
disk before selecting another drive for subsequent operations.
Similarly, there may be a call to set the DMA segment base and a
call to set the DMA offset followed by several calls which read or
write from the selected DMA address before the DMA address is
changed. The track and sector subroutines are always called before
the READ or WRITE operations are performed.

The READ and WRITE subroutines should perform several retries
(10 is standard) before reporting the error condition to the BDNS.
The HOME subroutine may or may not actually perform the track 00
seek, depending upon your controller characteristics; the important
point is that track 00 has been selected for the next overation, and
is often treated in exactly the same manner as SETTRK with a
parameter of 00.

Table 5-4. BIOS Subroutine Summary

This subroutine is called directly by the CP/M-86
loader after the CPM.SYS file has been read into
memory. The procedure is responsible for any
hardware initialization not performed by the
bootstrap loader, setting initial values for BIOS
variables (including IOBYTE), printing a sign-on
message, and initializing the interrupt vector to
point to the BDOS offset (0B11H) and base. When
this routine completes, it jumps to the CCP
offset (0H). All segment registers should be
initialized at this time to contain the base of
the operating system.

This subroutine is called whenever a program
terminates by performing a BDOS function #0 call.
Some re-initialization of the hardware or
software may occur here. When this routine
completes, it jumps directly to the warm start
entry point of the CCP (06H).

Sample the status of the currently assigned
console device and return OFFH in register AL if
a character is ready to read, and 00H in register
AL if no console characters are ready.

---
_Page 063_

CP/M-86 System Guide 5.4 BIOS Subroutine Entry Points

Table 5-4. (continued)

CONIN Read the next console character into register AL,
and set the parity bit (high order bit) to zero.
If no console character is ready, wait until a
character is typed before returning.

CONOUT Send the character from register CL to the
console output device. The character is in
ASCII, with high order parity bit set to zero.
You may want to include a time-out on a line feed
Or carriage return, if your console device
requires some time interval at the end of the
line (such as a TI Silent 700 terminal). You
can, if you wish, filter out control characters
which have undesirable effects on the console
device.

Send the character from register CL to the
currently assigned listing device. The character
is in ASCII with zero parity.

Send the character from register CL to the
currently assigned punch device. The character
is in ASCII with zero parity.

READER Read the next character from the currently
assigned reader device into register AL with zero
parity (high order bit. must be zero). An end of

file condition is reported by returning an ASCII

CONTROL-2Z (1AH).

Return the disk head of the currently selected
disk to the track 00 position. If your
controller does not have a special feature for
finding track 00, you can translate the call into
a call to SETTRK with a parameter of 0.

---
_Page 064_

CP/M-86 System Guide

Subroutine

SELDSK

SETTRK

SETSEC

Table 5-4. (continued)

Select the disk drive given by register CL for
further operations, where register CL contains 0
for drive A, 1 for drive B, and so on up to 15
for drive P (the standard CP/M-86 distribution
version supports two drives). On each disk
select, SELDSK must return in BX the base address
of the selected drive’s Disk Parameter Header.
For standard floppy disk drives, the content of
the header and associated tables does not change.
The sample BIOS included with CP/M-86 called
CBIOS contains an example program segment that

performs the SELDSK function. If there is an

attempt to select a non-existent drive, SELDSK
returns BX=0000H as an error indicator. Although
SELDSK must return the header address on each
call, it is advisable to postpone the actual
physical disk select operation until an I/0
function (seek, read or write) is performed.
This is due to the fact that disk select
operations may take vlace without a subsequent
disk operation and thus disk access may be
substantially slower using some disk controllers.
On entry to SELDSK it is possible to determine
whether it is the first time the specified disk
has been selected. Register DL, bit 0 {least
significant bit) is a zero if the drive has not
been previously selected. This information is of
interest in systems which read configuration
information from the disk in order to set upd a
dynamic disk definition table.

Register CX contains the track number for
subsequent disk accesses on the currently
selected drive. You can choose to seek the
selected track at this time, or delay the seek
until the next read or write actually occurs.
Register CX can take on values in the range 0-76
corresponding to valid track numbers for standard
floopy disk drives, and 0-65535 for non-standard
disk subsystems.

Register CX contains the translated sector number
for subsequent disk accesses on the currently
selected drive (see SECTRAN, below). You can
choose to send this information to the controller
at this point, or instead delay sector selection
until. a read or write operation occurs.

5.4 BIOS Subroutine Entry Points

---
_Page 065_

CP/M-86 System Guide 5.4 BIOS Subroutine Entry Points

Table 5-4. (continued)

Description

Register CX contains the DMA (disk memory access)
offset for subsequent read or write operations.
For example, if CX = 80H when SETDMA is called,
then all subsequent read operations read their
data into 80H through OFFH offset from the
current DMA segment base, and all subsequent
write operations get their data from that
address, until the next calls to SETDMA and
SETDMAB occur. Note that the controller need not
actually support direct memory access. If, for
example, all data is received and sent through
I/O ports, the CBIOS which you construct will use
the 128 byte area starting at the selected DMA
offset and base for the memory buffer during the
following read or write operations.

Assuming the drive has been selected, the track
has been set, the sector has been set, and the
. DMA offset and segment base have been specified,
the READ subroutine attempts to read one: sector
based upon these parameters, and returns the
following error codes in register AL:

0 no errors occurred
1 non-recoverable error condition occurred

Currently, CP/M-86 responds only to a zero or
non-zero value as the return code. That is, if
the value in register AL is 0 then CP/M-86
assumes that the disk operation completed
properly. If an error occurs, however, the CBIOS
should attempt at least 10 retries to see if the
error is recoverable. When an error is reported
the BDOS will print the message "BDOS ERR ON x:
BAD SECTOR". The operator then has the option of
typing RETURN to ignore the error, or CONTROL-C
to abort.

Write the data from the currently selected DMA
buffer to the currently selected drive, track,
and sector. The data should be marked as "non-
deleted data" to maintain compatibility with
other CP/M systems. The error codes given in the
READ command are returned in register AL, with
error recovery attempts as described above.

LISTST Return the ready status of the list device. The
value 00 is returned in AL if the list device is
not ready to accept a character, and OFFH if a
character can be sent to the printer.

---
_Page 066_

CP/M-86 System Guide

SECTRAN

SETDMAB

GETSEGB

Table 5-4. (continued)

Performs logical to physical sector translation
to improve the overall response of CP/M-86.
Standard CP/M-86 systems are shipped with a "skew
factor" of 6, where five physical sectors are
skipped between sequential read or write
operations. This skew factor allows enough time
between sectors for most programs to load their
buffers without missing the next sector. In
computer systems that use fast processors, memory
and disk subsystens, the skew factor may be
changed to improve overall response. Note,
however, that you should maintain a single
density IBM compatible version of CP/M-86 for
information transfer into and out of your
computer system, using a skew factor of 6. In
general, SECTRAN receives a logical sector number
in CX. This logical sector number may range from
0 to the number of sectors -l. Sectran also
receives a translate table offset in DX. The
sector number is used as an index into the
translate table, with the resulting physical
sector number in BX. For standard systems, the
tables and indexing code is provided in the CBIOS
and need not be changed. If DX = 0000H no
translation takes place, and CX is simply copied
to BX before returning. Otherwise, SECTRAN
computes and returns the translated sector number
in BX. Note that SECTRAN is called when no
translation is specified in the Disk Parameter
Header.

Register CX contains the segment base for
subsequent DMA read or write operations. The
BIOS will use the 128 byte buffer at the memory
address determined by the DMA base and the DMA
offset during read and write operations.

Returns the address of the Memory Region Table
(MRT) in BX. The returned value is the offset of
the table relative to the start of the operating
system. The table defines the location and
extent of physical memory which is available for
transient programs.

5.4 BIOS Subroutine Entry Points

Se Se pe

SRA Rte Te erm EE tim, Stn, EE Titi ccmintoate emacs een Seger ger eg 6 a gene apse eng RE TEER

(Tete eee enemies S

wpe

Pe Pee Armen ne ine So ee rea ee

---
_Page 067_

CP/M-86 System Guide 5.4 BIOS Subroutine Entry Points [

Table 5-4. (continued) |

Description

Memory areas reserved for interrupt vectors and
the CP/M-86 operating system are not included in
the MRT. The Memory Region Table takes the form:

8-bit

R-Length
R-Length

16-bit

where R-Cnt is the number of Memory Region
Descriptors (equal to n+l in the diagram above),
while R-Base and R-Length give the paragraph base
and length of each physically contiguous area of |
memory. Again, the reserved interrupt locations,
normally 0-3FFH, and the CP/M-86 operating system
are not included in this map, because the map
contains regions available to transient programs.
If all memory is contiguous, the R-Cnt field is 1
and n = 0, with only a single Memory Region
Descriptor which defines the region.

GETIOB Returns the current value of the logical to
physical input/output device byte (IOBYTE) in AL.
This eight-bit value is used to associate
physical devices with CP/M-86\*s four logical

devices.

SETIOB Use the value in CL to set the value of the
IOBYTE stored in the BIOS.

The following section describes the exact layout and
construction of the disk parameter tables referenced by various
subroutines in the BIOS.

---
_Page 068_

Section 6
BIOS Disk Definition Tables

Similar to CP/M-80, CP/M-86 is a table-driven operating system
with a separate field-configurable Basic I/O System (BIOS). By
altering specific subroutines in the BIOS presented in the previous
section, CP/M-86 can be customized for operation on any RAM-based
8086 or 8088 microprocessor svstem.

The purpose of this section is to present the organization and
construction of tables within the BIOS that define the
characteristics of a particular disk system used with CP/M-86.
These tables can be either hand-coded or automatically generated
using the GENDEF utility provided with CP/M-86. The elements of
these tables are presented below.

6.1 Disk Parameter Table Format

In general, each disk drive has an associated (l16-byte) disk
Parameter header which both contains information about the disk
drive and provides a scratchpad area for certain BNOS operations.
The format of the disk parameter header for each drive is shown
below.

Disk Parameter Header

where each element is a word (16-bit) value. The meaning of each
Disk Parameter Header (NPH) element is given in Table 6-1.

Table 6-1. Disk Parameter Header Elements

Offset of the logical to physical translation vector,
if used for this particular drive, or the value 0000H
if no sector translation takes place (i.e, the
physical and logical sector numbers are the same).

Nisk drives with identical sector skew factors share
the same translate tables.

Scratchpad values for use within the BNOS (initial
value is unimportant).

---
_Page 069_

CP/M-86 System Guide 6.1 Disk Parameter Table Format

Table 6-1. (continued)

DIRBUF Offset of a 128 byte scratchpad area for directory
operations within BDOS. All DPH’s address the same
scratchpad area.

Offset of a disk parameter block for this drive.
Drives with identical disk characteristics address the
same disk parameter block.

Offset of a scratchpad area used for software check for
changed disks. This offset is different for each DPH.

Offset of a scratchoad area used by the BDOS to keep
disk storage allocation information. This offset is
different for each DPH.

Given n disk drives, the DPH\*s are arranged in a table whose first
row of 16 bytes corresponds to drive 0, with the last row
corresponding to drive n-l. The table thus appears as

DPBASE

(and so-forth through)

where the label DPBASE defines the offset of the NPH table relative
to the beginning of the operating system.

n-l

A responsibility of the SELDSK subroutine, defined in the
previous section, is to return the offset of the DPH from the
beginning of the operating system for the selected drive. The
following sequence of operations returns the table offset, with a
QOO0OH returned if the selected drive does not exist.

ee tN At ee RE Ne ety en nee ea ee ee nope ae ee oe eee pee

pen ate

ne en

sre rte cree pe ee
te tre eo

---
_Page 070_

CP/M-86 System Guide 6.1 Disk Parameter Table Format

NDISKS EQU 4 ;NUMBER OF DISK DRIVES

SELDSK:
;SELECT DISK N GIVEN BY CL
MOV BX,0000H ;READY FOR ERR
CPM CL,NDISKS ;N BEYOND MAX NISKS?
JNB RETURN ;RETURN IF SO
70 <= N < NDISKS
MOV CH, 0 \*DOUBLE (N)
MOV BX ,CX \*BX = N
MOV CL,4 sREADY FOR \* 16
SAL BX,CL N=N \* 16
MOV CX,OFFSET NPBASE
ADD BX ,CX sDPBASE +N \* 16
RETURN: RET sBX = .DPH (N)

The translation vectors (XLT 00 through XLTn-1) are located
elsewhere in the BIOS, and simply correspond one-for-one with the
logical sector numbers zero through the sector count-1l. The Disk
Parameter Block (DPB) for each drive is more complex. A particular

DPB, which is addressed by one or more DPH’s, takes the general
form:

Ce [bebe To Dae] oe Tor
16b 8b 8b 8b 16b 16b 8b 8b 16b

where each is a byte or word value, as shown by the "8b" or "16b"
indicator below the field. The fields are defined in Table 6-2.

SPT

Table 6-2. Disk Parameter Block Fields

is the total number of sectors per track

is the data allocation block shift factor, determined
by the data block allocation size.

is the block mask which is also determined by the data
block allocation size.

is the extent mask, determined by the data block
allocation size and the number of disk blocks.

determines the total storage capacity of the disk drive

DRM determines the total number of directory entries which
can be stored on this drive

---
_Page 071_

CP/M-86 System Guide 6.1 Disk Parameter Table Format

Table 6-2. (continued)

ALO,AL1L determine reserved directory blocks.

CKS is the size of the directory check vector

OFF is the number of reserved tracks at the beginning of
the (logical) disk.

Although these table values are produced automatically by GENDEF, it
is worthwhile reviewing the derivation of each field so that the
values may be cross-checked when necessary. The values of BSH and
BLM determine (implicitly) the data allocation size BLS, which is
not an entry in the disk parameter block. Given that you have
selected a value for BLS, the values of BSH and BLM are shown in
Table 6-3 below, where all values are in decimal.

Table 6-3. BSH and BLM Values for Selected BLS

The value of EXM depends upon both the BLS and whether the DSM value
is less than 256 or greater than 255, as shown in the following
table.

Table 6-4. Maximum EXM Values

= au > 258

The value of DSM is the maximum data block number supported by
this particular drive, measured in BLS units. The product BLS times
(DSM+1) is the total number of bytes held by the drive and, of
course, must be within the capacity of the physical disk, not
counting the reserved operating system tracks.

---
_Page 072_

CP/M-86 System Guide 6.1 Disk Parameter Table Format

The DRM entry is one less than the total number of directory
entries, which can take on a 16-bit value. The values of ALO and
AL1, however, are determined by DRM. The two values ALO and AL1 can
together be considered a string of 16-bits, as shown below.

00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15

where position 00 corresponds to the high order bit of the byte
labeled ALO, and 15 corresponds to the low order bit of the byte
labeled AL1. Each bit position reserves a data block for a number
of directory entries, thus allowing a total of 16 data blocks to be
assigned for directory entries (bits are assigned starting at 00 and
filled to the right until position 15). ach directory entry
occupies 32 bytes, as shown in Table 6-5.

Table 6-5. BLS and Number of Directory Entries

Directory Entries

1,024 times #
2,048 times #
4,096 : times #
8,192 times #
16,384 times #

Thus, if DRM = 127 (128 directory entries), and BLS = 1024, then
there are 32 directory entries per block, requiring 4 reserved
blocks. In this case, the 4 high order bits of ALO are set,
resulting in the values ALO = OFOH and ALI] = OOH.

The CKS value is determined as follows: if the disk drive
media is removable, then CKS = (DRM+1)/4, where DRM is the last
directory entry number. If the media is fixed, then set CKS = 0 (no
directory records are checked in this case).

Finally, the OFF field determines the number of tracks which
are skipped at the beginning of the physical disk. This value is
automatically added whenever SETTRK is called, and can be used as a
mechanism for skipping reserved operating system tracks, or for
partitioning a large disk into smaller segmented sections.

To complete the discussion of the DPB, recall that several
DPH\*s can address the same DPB if their drive characteristics are
identical. Further, the DPB can be dynamically changed when a new
drive is addressed by simply changing the pointer in the DNPH since
the BDOS copies the DPB values to a local area whenever the SELDSK
function is invoked.

---
_Page 073_

CP/M-86 System Guide 6.1 Disk Parameter Table Format

Returning back to the DPH for a particular drive, note that the
two address values CSV and ALV remain. Both addresses reference an
area of uninitialized memory following the BIOS. The areas must be
unique for each drive, and the size of each area is determined by
the values in the DPB.

The size of the area addressed by CSV is CKS bytes, which is
sufficient to hold the directory check information for this
particular drive. If CKS = (NRM+1)/4, then you must reserve
(DRM+1)/4 bytes for directory check use. I€ CKS = 0, then no
storage is reserved.

The size of the area addressed by ALV is determined by the
maximum number of data blocks allowed for this particular disk, and
is computed as (NDSM/8)+1l.

The BIOS shown in Appendix D demonstrates an instance of these
tables for standard 8" single density drives. It may be useful to

examine this program, and compare the tabular values with the
definitions given above.

6.2 Table Generation Using GENDEF

The GENNEF utility supplied with CP/M-86 greatly simplifies the
table construction process. GENDEF reads a file

x .DEF

containing the disk definition statements, and produces an output
file

x. LIB
containing assembly language statements which define the tables
necessary to support a particular drive configuration. The form of
the GENDEF command is:

GENDEF x parameter list
where x has an assumed (and unspecified) filetype of DEF. The

Parameter list may contain zero or more of the symbols defined in

Table 6-6. GENDEF Optional Parameters

Generate Disk Parameter Comments
Generate DPBASE OFFSET $

$Z 280, 8080, 8085 Override

$COZ (Any of the Above)

rene ne A OE A Am Ape =O pene ace a et

geet EI RT a rt

---
_Page 074_

CP/M-86 System Guide 6.2 Table Generation Using GENDEF

The C parameter causes GENDEF to produce an accompanying
comment line, similar to the output from the "STAT DSK:" utility
which describes the characteristics of each defined disk. Normally,
the DPBASE is defined as

DPBASE EQU $

which requires a MOV CX,OFFSET DPBASE in the SELDSK subroutine shown
above. For convenience, the $0 parameter produces the definition

DPBASE EQU OFFSET $

allowing a MOV CX,DPBASE in SELDSK, in order to match your
particular programming practices. The $Z parameter is included to
override the standard 8086/8088 mode in order to generate tables
acceptable for operation with 280, 8080, and 8085 assemblers.

The disk definition contained within x.DEF is composed with the
CP/M text editor, and consists of disk definition statements
identical to those accepted by the DISKDEF macro supplied with CP/M-
80 Version 2. A BIOS disk definition consists of the following
sequence of statements:

DISKS n
DISKDEF 0,...
DISKDEF 1,...
DISKDEF n-1l

ENDEF

Each statement is placed on a single line, with optional embedded
comments between the keywords, numbers, and delimiters.

The DISKS statement defines the number of drives to be
configured with your system, where n is an integer in the range l
through 16. A series of DISKDEF statements then follow which define
the characteristics of each logical disk, 0 through n-l,
corresponding to logical drives A through P. Note that the DISKS
and DISKDEF statements generate the in-line fixed data tables
described in the previous section, and thus must be placed in a non-
executable portion of your BIOS, typically at the end of your BIOS,
before the start of uninitialized RAM.

The ENDEF (End of Diskdef) statement generates the necessary
uninitialized RAM areas which are located beyond initialized RAM in
your BIOS.

---
_Page 075_

CP/M-86 System Guide 6.2 Table Generation Using GENDEF

The form of the DISKDEF statement is

DISKDEF dn,fsc,lsc, [skf],6l1s,dks,dir,cks,ofs, [0]

where
dn is the logical disk number, 0 to n-1
fse is the first physical sector number (0 or 1)
lsc is the last sector number
skf is the optional sector skew factor
bls is the data allocation block size
dks is the disk size in bls units
dir is the number of directory entries
cks is the number of "checked" directory entries
ofs is the track offset to logical track 00

[0] is an optional 1.4 compatibility flag

The value "dn" is the drive number being defined with this DISKDEF
statement. The "fsc" parameter accounts for differing sector
numbering systems, and is usually 0 or 1. The "lsc" is the last
numbered sector on a track. When present, the "skf" parameter
defines the sector skew factor which is used to create a sector
translation table according to the skew. If the number of sectors
is less than 256, a single-byte table is created, otherwise each
translation table element occupies two bytes. No translation table
is created if the skf parameter is omitted or equal to 0.

The "bls" parameter specifies the number of bytes allocated to
each data block, and takes on the values 1024, 2048, 4096, 8192, or
16384. Generally, performance increases with larger data block
sizes because there are fewer directory references. Also, logically
connected data records are physically close on the disk. Further,
each directory entry addresses more data and the amount of BIOS work
space is reduced. The "dks" specifies the total disk size in "bls"
units. That is, if the bls = 2048 and dks = 1000, then the total
disk capacity is 2,048,000 bytes. If dks is greater than 255, then
the block size parameter bls must be greater than 1024. The value
of "dir" is the total number of directory entries which may exceed
255, if desired.

The "cks" parameter determines the number of directory items to
check on each directory scan, and is used internally to detect
changed disks during system operation, where an intervening cold
Start or system reset has not occurred (when this situation is
detected, CP/M-86 automatically marks the disk read/only so that
data is not subsequently destroyed). As stated in the previous
section, the value of cks = dir when the media is easily changed, as
is the case with a floppy disk subsystem. Tf the disk is
permanently mounted, then the value of cks is typically 0, since the
probability of changing disks without a restart is quite low.

a

ees at ETT ER IE ON RTE OCR ARG ORC Pt et Meee ome RE Smt fe! en nt inn ne he Peat eS oes nee

---
_Page 076_

CP/M-86 System Guide 6.2 Table Generation Using GENDEF

The "ofs" value determines the number of tracks to skip when
this particular drive is addressed, which can be used to reserve
additional operating system space or to simulate several logical
drives on a single large capacity physical drive. Finally, the [0]
Parameter is included when file compatibility is required with
versions of CP/M-80, version 1.4 which have been modified for higher
density disks (typically double density). This parameter ensures
that no directory compression takes place, which would cause
incompatibilities with these non-standard CP/M 1.4 versions.
Normally, this parameter is not included.

For convenience and economy of table space, the special form
DISKDEF i,j

gives disk i the same characteristics as a previously defined drive
j. Astandard four-drive single density system, which is compatible
with CP/M-80 Version 1.4, and upwardly compatible with CP/M-80
Version 2 implementations, is defined using the following
statements:

DISKS 4
DISKDEF 0,1,26,6,1024,243,64,(
DISKDEF 1

DISKDEF 2
DISKDEF 3

ENDEF

with all disks having the same parameter values of 26 sectors per
track (numbered 1 through 26), with a skew of 6 between sequential
accesses, 1024 bytes per data block, 243 data blocks for a total of
243K byte disk capacity, 64 checked directory entries, and two
operating system tracks.

The DISKS statement generates n Disk Parameter Headers (DPH’s),
starting at the DPH table address DPBASE generated by the statement.
Each disk header block contains sixteen bytes, as described above,
and corresponds one-for-one to each of the defined drives. In the

four drive standard system, for example, the DISKS statement
generates a table of the form:

DPBASE EQU $

DPEO DW XLTO,0000H,0000H,0000H,DIRBUF ,DPBO,CSV0,ALVO
DPE] DW XLTO,0000H,0000H,0000H,DIRBUF,DPBO,CSV1,ALV1
DPE2 DW XLTO,0000H,0000H,0000H,DIRBUF,DPBO,CSV2,ALV2
DPE3 DW XLTO,0000H,0000H,0000H,DIRBUF,DPBO,CSV3,ALV3

where the DPH labels are included for reference purposes to show the
beginning table addresses for each drive 0 through 3. The values
contained within the disk parameter header are described in detail
earlier in this section. The check and allocation vector addresses
are generated by the ENDEF statement for inclusion in the RAM area
following the BIOS code and tables.

---
_Page 077_

CP/M-86 System Guide 6.2 Table Generation Using GENDEF

Note that if the "skf" (skew factor) parameter is omitted (or
equal to 0), the translation table is omitted, and a 0000H value is
inserted in the XLT position of the disk parameter header for the
disk. In a subsequent call to perform the logical to physical
translation, SECTRAN receives a translation table address of DX =
0000H, and simply returns the original logical sector from CX in the
BX register. A translate table is constructed when the skf
parameter is present, and the (non-zero) table address is placed
into the corresponding DPH\*s. The table shown below, for example,
is constructed when the standard skew factor skf = 6 is specified in
the DISKDEF statement call:

XLTO EQU OFFSET $
DB 1,7,13,19,25,5,11,17,23,3,9,15,21
DB 2,8,14,20,26,6,12,18,24,4,10,16,22

Following the ENDEF statement, a number of uninitialized data
areas are defined. These data areas need not be a part of the BIOS
which is loaded upon cold start, but must be available between the
BIOS and the end of operating system memory. The size of the
uninitialized RAM area is determined by EQU statements generated by
the ENDEF statement. For a standard four-drive system, the ENDEF
statement might produce

1¢72 = BEGDAT EQU OFFSET $
(data areas)
1DBO = ENDDAT EQU OFFSET $
013c = DATSIZ EQU OFFSET $-BEGDAT

which indicates that uninitialized RAM begins at offset 1C72H, ends
at 1DBOH-1, and occupies 013CH bytes. You must ensure that these
addresses are free for use after the system is loaded.

After modification, you can use the STAT program to check your
drive characteristics, since STAT uses the disk parameter block to
decode the drive information. The comment included in the LIB file
by the $C parameter to GENCMD will match the output from STAT. The
STAT command form

STAT d:DSK:

decodes the disk parameter block for drive d (d=A,...,P) and
displays the values shown below:

r: 128 Byte Record Capacity
k: Kilobyte Drive Capacity
d: 32 Byte Directory Entries
c: Checked Directory Entries
: Records/ Extent

: Records/ Block

: Sectors/ Track

: Reserved Tracks

errr pep a

ceclealats dete amenans ataerast ae I

re

---
_Page 078_

CP/M-86 System Guide 6.3 GENDEF Output

6.3 GENDEF Output

GENDEF produces a listing of the statements included in the DEF
file at the user console (CONTROL-P can be used to obtain a printed
listing, if desired). fach source Line is numbered, and any errors

i ™ are shown below the line in error, with a "?" beneath the item which
caused the condition. The source errors produced by GENCMD are
listed in Table 6-7, followed by errors that can occur when
Producing input and output files in Table 6-8.

Table 6-7. GENDEF Source Error Messages

Bad Val More than 16 disks defined in DISKS statement.

Convert Number cannot be converted, must be constant
in binary, octal, decimal, or hexadecimal as
in ASM-86.

Delimit

Missing delimiter between parameters.

Duplic Duplicate definition for a disk drive.

Extra

Extra parameters occur at the end of line.

Length Keyword or data item is too londa.

Missing

Parameter required in this position.

No Disk

Referenced disk not previously defined.

No Stmt Statement keyword not recognized.

Numeric

Number required in this position

Range

Number in this position is out of range.

Too Few Not enough parameters provided.

Quote Missing end quote on current line.

---
_Page 079_

CP/M-86 System Guide 6.3 GENDEF Output

Table 6-8. GENDEF Input and Output Error Messages

Sc

Cannot Close ".LIB" File LIB file close operation
unsuccessful, usually due
to hardware write protect.

"LIB" Disk Full No space for LIB file.

No Input File Present Specified DEF file not

found.

No ".LIB" Directory Space Cannot create LIB file due
to too many files on LIB
disk.

Premature End-of-File End of DEF file encountered
unexpectedly.

Given the file TWO.DEF containing the following statements
disks 2
diskdef 0,1,26,6,2048,256,128,128,2
diskdef 1,1,58,,2048,1024,300,0,2
endef
the command
gencemd two $c

produces the console output

DISKDEF Table Generator, Vers 1.0

1 DISKS 2

2 DISKDEF 0,1,58,,2048,256,128,128,2
3 DISKDEF 1,1,58,,2048,1024,300,0,2
4 ENDEF

No Error (s)

The resulting TWO.LIB file is brought into the following skeletal
assembly language program, using the ASM-86 INCLUDE directive. The
ASM-86 output listing is truncated on the right, but can be easily
reproduced using GENDEF and ASM-86.

---
_Page 080_

CP/M-86 System Guide

B9

000B
000F
00lF

5B
FB
5B
9B

002A
002c
002D
002E

1A
OF
FF
7F
co

003A
003E
004A

1A

DB

4c
1B

OD
0B

ll
OF
OE
0c
OA

TNM~e we

ELDSK

=e

dpbase
dped

dpel

(ig we we we te te we we we we we Ne we te

pbd

x1t0

6.3  GENDEF Output

Sample Program Including TWO.LI

MOV
INCLUDE
equ

dw

dw

dw

dw

dw

dw

dw
dw

CX,OFFSET DPBASE

TWO.LIB

DISKS 2

$ ;Base o
x1t0,0000h sTransl
0000h,0000h :Scratc
dirbuf ,dpb0 ;Dir Bu
csv0,alv0 sCheck,
x1t1,0000h :Transl
0000h,0000h \*Scratc
dirbuf ,dpbl Mir Bu
csvl,alvl :Check,

DISKDEF 0,1,26,6,2048,2

Disk 0 is CP/M 1.4 Single Densi

4096:
512:
128:
128:
2563:

16:
26:
2:

128 Byte Record Capacit
Kilobyte Drive Capacit
32 Byte Directory Entri
Checked Nirectory Entri
Records / Extent
Records / Block

Sectors / Track
Reserved Tracks

Sector Skew Factor

offset §$ sDisk P
26 sSector
4 :Block
15 sBlock
1 sExtnt
255 :Disk S
127 sDirect
192 sAllocO
0 sAllocl
32 :Check
2 :Offset
offset $ :Transl
1,7,13,19

25,5,11,17

23,3,9,15

21,2,8,14

20,26,6,12

18,24,4,10

16,22

32 sAlloca
32 sCheck

DISKDEF 1,1,58,,2048,10

Disk 1 is CP/M 1.4 Single Densi

16384:

128 Byte Record Canacit

---
_Page 081_

CP/M-86 System Guide 6.3 GENDEF Output

= ; 2048: Kilobyte Drive Capacit
= ; 300: 32 Byte Directory Entri
= ; 0: Checked Directory Entri
= : 128: Records / Extent
= : 16: Records / Block
= : 58: Sectors / Track
= ; 2: Reserved Tracks
= 004C dpb1l equ offset $ ;Disk P
= 004C 3A 00 dw 58 :Sector
= 004E 04 db 4 \*Block
= 004F OF ab 15 \*Block
= 0050 00 db 0 sExtnt
= 0051 FF 03 dw 1023 Disk S
= 0053 2B 01 dw 299 Mirect
= 0055 F8 db 248 sAllocO0
= 0056 00 db 0 sAllocl
= 0057 00 00 dw 0 Check
= 0059 02 00 dw 2 :Offset
= 0000 xltl equ 0 sNo Tra
= 0080 alsl equ 128 sAlloca
= 0000 cssl equ 0 ;Check
= : Uninitialized Scratch Memory Fo
= 005B begdat equ offset $ sStart
= 0058 dirbuf rs 128 sDirect
= 00ODB alvO rs als0 sAlloc
= O0OFB csv0 rs css0 Check
= 011B alvl rs alsl sAlloc
= 019B esvl rs cssl 3 Check
= 0198 enddat equ offset $ ;End of
= 0140 datsiz equ offset $-begdat ;Size o
= 019B 00 db 0 sMarks
END

---
_Page 082_

Section 7
CP/M-86 Bootstrap and Adaption Procedures

This section describes the components of the standard CP/M-86
distribution disk, the operation of each component, and the
procedures to follow in adapting CP/M-86 to non-standard hardware.

CP/M-86 is distributed on a single-density IBM compatible 8"
diskette using a file format which is compatible with all previous
CP/M-80 operating systems. In particular, the first two tracks are
reserved for operating system and bootstrap programs, while the
remainder of the diskette contains directory information which leads
to program and data files. CP/M-86 is distributed for operation
with the Intel SBC 86/12 single-board computer connected to floppy
disks through an Intel 204 Controller. The operation of CP/M-86 on
this configuration serves as a model for other 8086 and 8088
environments, and is presented below.

The principal components of the distribution system are listed
below:

@ The 86/12 Bootstrap ROM (BOOT ROM)
@ The Cold Start Loader (LOADER)
@ The CP/M-86 System (CPM.SYS)

(~~ When installed in the SBC 86/12, the BOOT ROM becomes a part of
the memory address space, beginning at byte location OFFOOOH, and
receives control when the system reset button is depressed. Ina
non-standard environment, the BOOT ROM is replaced by an equivalent
initial loader and, therefore, the ROM itself is not included with
CP/M-86. The BOOT ROM can be obtained from Digital Research or,
alternatively, it can be programmed from the listing given in
Appendix C or directly from the source file which is included on the
distribution disk as BOOT.A86. The responsibility of the BOOT ROM
is to read the LOADER from the first two system tracks into memory
and pass program control to the LOADER for execution.

7.1 The Cold Start Load Operation

The LOADER program is a simple version of CP/M-86 that contains
sufficient file processing capability to read CPM.SYS from the
system disk to memory. When LOADER completes its operation, the
CPM.SYS program receives control and proceeds to process operator
input commands.

Both the LOADER and CPM.SYS programs are preceded by the
{ standard CMD header record. MThe 128-byte LOADER header record
contains the following single group descriptor.

---
_Page 083_

CP/M-86 System Guide 7.1 The Cold Start Load Operation

8b 16b 16b 16b 16b

where G-Form = 1 denotes a code group, "x" fields are ignored, and
A-Base defines the paragraph address where the BOOT ROM begins
filling memory (A-Base is the word value which is offset three bytes
from the beginning of the header). Note that since only a code
group is present, an 8080 memory model is assumed. Further,
although the A-Base defines the base paragraph address for LOADER
(byte address 04000H), the LOADER can, in fact be loaded and
executed at any paragraph boundary that does not overlap CP/M-86 or
the BOOT ROM.

The LOADER itself consists of three parts: the Load CPM
program (LDCPM), the Loader Basic Nisk System (LDBDOS), and the
Loader Basic I/0 System (LDBIOS). Although the LOADER is setup to
initialize CP/M-86 using the Intel 86/12 configuration, the LDBIOS
can be field-altered to account for non-standard hardware using the
same entry points described in a previous section for BIOS
modification. The organization of LOADER is shown in Figure 7-1
below:

GD#1l 0 SSS//////////
cs 0S BS $$ 0000H:

JMPF CPM

0400H:
(LDBDOS)

1200H: JMP INIT

JMP SETIOB

INIT: .. JMP 0003H

(LDBIOS)

1700H:

Figure 7-1. LOADER Organization

Wee =

---
_Page 084_

CP/M-86 System Guide 7.1 The Cold Start Load Operation

Byte offsets from the base registers are shown at the left of the
diagram. GD#l is the Group Descriptor for the LOADER code group
described above, followed immediately by a "0" group terminator.
The entire LOADER program is read by the BOOT ROM, excluding the
header record, starting at byte location 04000H as given by the A-
Field. Upon completion of the read, the BOOT ROM passes control to
location 04000H where the LOADER program commences execution. The
JMP 1200H instruction at the base of LDCPM transfers control to the
beginning of the LDBIOS where control then transfers to the INIT
subroutine. The subroutine starting at INIT performs device
initialization, prints a sign-on message, and transfers back to the
LDCPM program at byte offset 0003H. The LDCPM module opens the
CPM.SYS file, loads the CP/M-86 system into memory and transfers
control to CP/M-86 through the JMPF CPM instruction at the end of
LDCPM execution, thus completing the'cold start sequence.

The files LDCPM.H86 and LDBDOS.H86 are inclided with CP/M-86 so
that you can append your own modified LDBIOS in the construction of
a customized loader. In fact, BIOS.A86 contains a conditional
assembly switch, called "loader_bios," which, when enabled, produces
the distributed LDBIOS. The INIT subroutine portion of LDBIOS is
listed in Appendix C for reference purposes. To construct a custom
LDBIOS, modify your standard BIOS to start the code at offset 1200H,
and change your initialization subroutine beginning at INIT to
perform disk and device initialization. Include a JMP to offset
0003H at the end of your INIT subroutine. Use ASM-86 to assemble
your LDBIOS.A86 program:

ASM86 LDBIOS

to produce the LDBIOS.H86 machine code file. Concatenate the three
LOADER modules using PIP:

PIP LOADER. H86=LDCPM.H86,LDBDOS.H86,LDBIOS.H86

to produce the machine code file for the LOADER program. Although
the standard LOADER program ends at offset 1700H, your modified
LDBIOS may differ from this last address with the restriction that
the LOADER must fit within the first two tracks and not overlap
CP/M-86 areas. Generate the command (CMD) file for LOADER using the
GENCMD utility:

GENCMD LOADER 8080 CODE[A400]

resulting in the file LOADER.CMD with a header record defining the
8080 Memory Model with an absolute paragraph address of 400H, or
byte address 4000H. Use DDT to read LOADER.CMD to location 900H in
your 8080 system. Then use the 8080 utility SYSGEN to copy the
loader to the first two tracks of a disk.

---
_Page 085_

CP/M-86 System Guide 7.1 The Cold Start Load Operation

A>DDT

- ILOADER.CMD

-R800

-\*Cc

A>SYSGEN

SOURCE DRIVE NAME (or return to skip) <cr>
DESTINATION DRIVE NAME (or return to skip) B

Alternatively, if you have access to an operational CP/M-86 system,
the command

LDCOPY LOADER

copies LOADER to the system tracks. You now have a diskette with a
LOADER program which incorporates your custom LDBIOS capable of
reading the CPM.SYS file into memory. For standardization, we
assume LOADER executes at location 4000H. LOADER is statically
relocatable, however, and its operating address is determined only
by the value of A-Base in the header record.

You must, of course, perform the same function as the BOOT ROM
to get LOADER into memory. The boot operation is usually
accomplished in one of two ways. First, you can program your own
ROM (or PROM) to perform a function similar to the BOOT ROM when
your computer’s reset button is pushed. As an alternative, most
controllers provide a power-on "boot" operation that reads the first
disk sector into memory. This one-sector program, in turn, reads
the LOADER from the remaining sectors and transfers to LOADER upon
completion, thereby performing the same actions as the BOOT ROM.
Either of these alternatives is hardware-specific, so you\*1l need to
be familiar with the operating environment.

7.2 Organization of CPM.SYS
The CPM.SYS file, read by the LOADER program, consists of the

CCP, BDOS, and BIOS in CMD file format, with a 128-byte header
record similar to the LOADER program:

G-Length A-Base G-Min
a ee
8b

16b 16b 16b

pmax
16b

where, instead, the A-Base load address is paragraph 040H, or byte
address 0400H, immediately following the 8086 interrupt locations.
The entire CPM.SYS file appears on disk as shown in Figure 7-2.

a

{i

i Senn, Y

---
_Page 086_

CP/M-86 System Guide 7.2 Organization of CPM.SYS

soet Jo V/IIIIIL,

(0040:0) CS DS ES SS 0000H:

(CCP and BDOS)

JMP INIT

JMP SETIOB

(0040:) 2500H:

(BIOS)

INIT: .. JMP 0000H

(0040:) 2A00H:

Figure 7-2. CPM.SYS File Organization

where GD#l is the Group Descriptor containing the A-Base value
followed by a "0" terminator. The distributed 86/12 BIOS is listed
in Appendix D, with an "include" statement that reads the
SINGLES.LIB file containing the disk definition tables. The
SINGLES.LIB file is created by GENDEF using the SINGLES.DEF
Statements shown below:

disks 2

diskdef 0,1,26,6,1024,243,64,64,2
diskdef 1,0

endef

The CPM.SYS file is read by the LOADER program beginning at the
address given by A~Base (byte address 0400H), and control is passed
to the INIT entry point at offset address 2500H. Any additional
initialization, not performed by LOADER, takes place in the INIT
subroutine and, upon completion, INIT executes a JMP 0000H to begin
execution of the CCP. The actual load address of CPM.SYS is
determined entirely by the address given in the A-Base field which
can be changed if you wish to execute CP/M-86 in another region of
Memory. Note that the region occupied by the operating system must
be excluded from the BIOS memory region table.

Similar to the LOADER program, you can modify the BIOS by
altering either the BIOS.A86 or skeletal CBIOS.A86 assembly language
files which are included on your source disk. In either case,
create a customized BIOS which includes your specialized 1/0
drivers, and assemble using ASM-86:

ASM86 BIOS

to produce the file BIOS.H86 containing your BIOS machine code.

---
_Page 087_

CP/M-86 System Guide 7.2 Organization of CPM.SYS

Concatenate this new BIOS to the CPM.H86 file on your distribution
disk:

PIP CPMX.H86 = CPM.H86,BI0S.H86

The resulting CPMX hex file is then converted to CMD file format by o™
executing

GENCMD CPMX 8080 CODE [A40]

in order to produce the CMD memory image with A-Base = 40H.
Finally, rename the CPMX file using the command

REN CPM.SYS = CPMX.CMD

and place this file on your 8086 system disk. Now the tailoring
process is complete: you have replaced the BOOT ROM by either your
own customized BOOT ROM, or a one-sector cold start loader which
brings the LOADER program, with your custom LDBIOS, into memory at
byte location 04000H. The LOADER program, in turn, reads the
CPM.SYS file, with your custom BIOS, into memory at byte location
0400H. Control transfers to CP/M-86, and you are up and operating.
CP/M-86 remains in memory until the next cold start operation takes
place.

You can avoid the two-step boot operation if you construct a
non-standard disk with sufficient space to hold the entire CPM.SYS
file on the system tracks. In this case, the cold start brings the {™
CP/M-86 memory image into memory at the location given by A-Base,
and control transfers to the INIT entry point at offset 2500H.
Thus, the intermediate LOADER program is eliminated entirely,
although the initialization found in the LDBIOS must, of course,
take place instead within the BIOS.

Since ASM-86, GENCMD and GENDEF are provided,in both COM and
CMD formats, either CP/M-80 or CP/M-86 can be used to aid the
customizing process. If CP/M-80 or CP/M-86 is not available, but
you have minimal editing and debugging tools, you can write
specialized disk I/O routines to read and write the system tracks,
as well as the CPM.SYS file.

The two system tracks are simple to access, but the CPM.SYS
file is somewhat more difficult to read. CPM.SYS is the first file
on the disk and thus it appears immediately following the directory
on the diskette. The directory begins on the third track, and
occupies the first sixteen logical sectors of the diskette, while
the CPM.SYS is found starting at the seventeenth sector. Sectors
are "skewed" by a factor of six beginning with the directory track
(the system tracks are sequential), so that you must load every
sixth sector in reading the CPM.SYS file. Clearly, it is worth the
time and effort to use an existing CP/M system to aid the conversion [ \
process.

. , ees ~~

---
_Page 088_

)

Appendix A
Sector Blocking and Deblocking

Upon each call to the BIOS WRITE entry point, the CP/M-86 BDOS
includes information that allows effective sector blocking and
deblocking where the host disk subsystem has a sector size which is
a multiple of the basic 128-byte unit. This appendix presents a
general-purpose algorithm that can be included within your BIOS and
that uses the BDOS information to verform the operations
automatically.

Upon each call to WRITE, the BDOS provides the following
information in register CL:

normal sector write
write to directory sector
write to the first sector
of a new data block

Condition 0 occurs whenever the next write operation is into a
previously written area, such as a random mode record update, when
the write is to other than the first sector of an unallocated block,
or when the write is not into the directorv area. Condition 1
occurs when a write into the directory area is performed. Condition
2 occurs when the first record (only) of a newly allocated data
block is written. In most cases, application programs read or write
multiple 128-byte sectors in sequence, and thus there is little
overhead involved in either operation when blocking and deblocking
records since pre-read operations can be avoided when writing
records.

This appendix lists the blocking and deblocking algorithm in
skeletal form (the file is included on your CP/M-86 disk).
Generally, the algorithms map all CP/M sector read operations onto
the host disk through an intermediate buffer which is the size of
the host disk sector. Throughout the program, values and variables
which relate to the CP/M sector involved in a seek operation are
prefixed by "sek," while those related to the host disk system are
prefixed by "hst." The equate statements beginning on line 24 of
Appendix F define the mapping between CP/M and the host system, and
must be changed if other than the sample host system is involved.

The SELDSK entry point clears the host buffer flag whenever a
new disk is logged-in. Note that although the SELDSK entry point
computes and returns the Disk Parameter Header address, it does not
physically select the host disk at this point (it is selected later
at READHST or WRITEHST). Further, SETTRK, SETSEC, and SETDMA simplv
store the values, but do not take any other action at this point.
SECTRAN performs a trivial function of returning the physical sector
number.

---
_Page 089_

CP/M-86 System Guide Appendix A Blocking and Deblocking

The principal entry points are READ and WRITE. These
subroutines take the place of your previous READ and WRITE
operations.

The actual physical read or write takes place at either
WRITEHST or REANDHST, where all values have been prepared: hstdsk is
the host disk number, hsttrk is the host track number, and hstsec is
the host sector number (which may require translation to a physical
sector number). You must insert code at this point which performs
the full host sector read or write into, or out of, the buffer at
hstbuf of length hstsiz. All other mapping functions are performed
by the algorithms.

g BREE REE REE EKER REITER REEEKREREREEKREEEREEKEEERREN

;\* Sector Blocking / Deblocking

7\* This algorithm is a direct translation of the

;\* CP/M-80 Version, and is included here for refer-
7\* ence purposes only. The file DEBLOCK.LIB is in-
7\* cluded on your CP/M-86 disk, and should be used
;\* for actual applications. You may wish to contact
:\* Digital Research for notices of updates.

ee 08 ee ce 00 co 08 20 e0 c8 cf
-

me
HFOWDDIDURWNHE
eo |

CRRREKEEEKERERERREERERREREEREERKRERREREEERERREREERRERRKREE

iy
Wh
ee

14;
15:
16:
17:
18:
19;
20:
21;
22:
23:
24;
25: blksiz equ 16384 sCP/M allocation size
26: hstsiz equ 512 shost disk sector size
27: hstspt equ 12 shost disk sectors/trk
28: hstblk equ hstsiz/128 :CP/M sects/host buff
29: 3

30: g REAR RE KERR RRR RRERRREEREKEREREKREREREREKRREREEE

PEE SSESELESESE SESE LESSEE EEE EE SESE EEE ESSE SELES ESE ES SS

CP/M to host disk constants

(This example is setup for CP/M block size of 16K
with a host sector size of 512 bytes, and 12 sec-
tors per track. Blksiz, hstsiz, hstspt, hstblk
and secshf may change for different hardware.)
REE RERERREREEREKEERERERERERRRRRRKER

equ byte ptr [BX] sname for byte at BX

\* + + eee eH
eee ete +e

oJ
»

me  ~e we we we te te we te Ne Ne

31: ;\* \*
32: ;\* secshf is log2(hstblk), and is listed below for \*
33: ;\* values of hstsiz up to 2048. \*
34; ;\* \*
35: 3\* hstsiz hstblk secshf \*
36: ;\* 256 2 1 \*
37: ;\* 512 4 2 \*
38: :\* 1024 8 3 \*
39: 3\* 2048 16 4 \*
40: ;\* \*

---
_Page 090_

CP/M-86 System Guide Appendix A Blocking and Deblocking

41: g RRR RRR RE ERERE REE EEK EEEERKEEEERREREREREKRREEREREE

42: secshf equ 2 ;log2 (hstblk)

43: cpmspt equ hstblk \* hstspt ;CP/M sectors/track
44: secmsk equ hstblk-1 ;sector mask

45: ;

46: g RRRERRRRERREREEREREERERERERERREREREREREREREEREREREREE
47: ;\* \*
48: ;\* BDOS constants on entry to write \*
49: ;\* \*
50: TIT IIT IS IT TT TiTittititittiitittititiiTitititt tlie,
51: wrall equ 0 swrite to allocated
52: wrdir equ 1 swrite to directory
53: wrual equ 2 write to unallocated
54: ;

55: WITTTTTITTITTTITTITTTiTTTTTiTT LTT LTTE ELLE ee
56: ;\* \*
57: ;\* The BIOS entry points given below show the \*
58: ;\* code which is relevant to deblocking only. \*
59: ;\* \*

60: gE RREREEEREREREREREERERRRERRERREKREKRERRRERREREREREEKER
61: seldsk:

62: sselect disk

63: zis this the first activation of the drive?
64: test DL,l sisb = 0?

65; jnz selset

66: sthis is the first activation, clear host buff
67: mov hstact,0

68: mov unacnt,0

69: selset:

70: mov al,cl ! cbw sput in AX

Tl: mov sekdsk,al sseek disk number
72: mov cl,4 ! shl al,cl stimes 16

73: add ax,offset dpbase

74: mov bx,ax

753 ret

76: 3

773: home:

78: shome the selected disk

793 mov al,hstwrt ;check for pending write
80: test al,al

81; jnz homed

82: mov hstact,0 ;clear host active flag
83: homed:

84: mov cx,0 snow, set track zero
85: ; (continue HOME routine)

86: ret

87: ;

88; settrk:

89: ;set track given by registers Cx

90; mov sektrk,CX strack to seek

91: ret

92: ;

93: setsec:

94; ;set sector given by register cl

95: mov seksec,cl ;sector to seek

---
_Page 091_

CP/M-86 System Guide Appendix A Blocking and Deblocking

96: ret

97: ;

98: setdma:

99: ;set dma address given by CX

100: mov dma off ,Cx

101: ret ~ c™
102: ;

103: setdmab:

104: sset segment address given by CX

105: mov dma_seg,CX
106: ret

107: ;

108: sectran:

109: stranslate sector number CX with table at [DX]

110: test DX,DX stest for hard skewed

lll: jz notran ;(blocked must be hard skewed)

112: mov BX,CX

113: add BX,DX

114: mov BL, [BX]

115: ret

116: no tran:

117: ~ shard skewed disk, vhysical = logical sector

118: mov BX,CX

119: ret

120: ;

121: read:

122: sread the selected CP/M sector

123: mov unacnt,0 sclear unallocated counter ;
124; mov readop,l sread operation {f
125: mov rsflag,l smust read data

126: mov wrtype,wrual ;treat as unalloc

127; jmp rwoper ;to perform the read

128: ;

129: write:

130; swrite tne selected CP/M sector

131: mov readop,0 swrite operation

132: mov wrtype,cl

133: cmp cl,wrual ;write unallocated?

134: jnz chkuna ;check for unalloc
135: ;

136: ; write to unallocated, set parameters

137: ;

138: mov unacnt, (blksiz/128) ;next unalloc recs

139; mov al,sekdsk :disk to seek

140: mov unadsk,al sunadsk = sekdsk

141; mov ax,sektrk

142: mov unatrk,ax sunatrk = sektrk

143: mov al,seksec

144; mov unasec,al sunasec = seksec
145: ; :
146: chkuna: {fn
147; scheck for write to unallocated sector

148: ;:

149; mov bx,offset unacnt spoint "UNA" at UNACNT

150: mov al,una ! test al,al ;any unalloc remain?

---
_Page 092_

CP/M-86 System Guide Appendix A Blocking and Deblocking

151: jz alloc :skip if not

152: ;

153: ; more unallocated records remain

154; dec al sunacnt = unacnt-1

155: mov una,al

156: mov al,sekdsk ssame disk?

157: mov BX,offset unadsk

158: cmp al,una ;sekdsk = unadsk?

159: jnz alloc sskip if not

160: ;

161: ; disks are the same

162: mov AX, unatrk

163: cmp AX, sektrk

164: jnz alloc :skip if not

165: ;

166: ; tracks are the same

167: mov al,seksec ssame sector?

168: ;

169: mov BX,offset unasec ;point una at unasec

170: ;:

171: cmp al,una sseksec = unasec?

172: jnz alloc sskip iff not

173: ;

174: ; match, move to next sector for future ref

175: inc una sunasec = unasectl

176: mov al,una send of track?

177: cmp al,cpmspt scount CP/M sectors

178: jb noovf ;skip if below

179: ;

180: ; overflow to next track

181: mov una,0 sunasec = 0

182: inc unatrk sunatrk=unatrk+l

183: ;

184: noovf:

185; ;match found, mark as unnecessary read

186: mov rsflag,0 srsflag = 0

187: jmps rwoper ;to perform the write

188: ;

189: alloc:

190: snot an unallocated record, requires pre-read

191: mov unacnt,0 sunacnt = 0

192: mov rsflag,1l zrsflag = 1

193: ;drop through to rwoper

194: ;

195; g RRR IRR RRR RIKER RIKER EKER KEK REE RRR KEKE KERR RE

196: ;\* \*

197: ;\* Common code for READ and WRITE follows \*
« e& \*

199: IGG IE IG ICIGI ISIC GOIGI IOI CG GIGI CIO ICO IO ICO TOIT TO Tek

200: rwoper:

201: enter here to perform the read/write

202: mov erflag,0 ;no errors (yet)

203: mov al, seksec ;compute host sector

204: mov cl, secshf

205: shr al,cl

---
_Page 093_

CP/M-86 System Guide

206:
207:
208:
209:
210:
211:
212:
213:
214:
215:
216:
217:
218:
219:
220:
221:
222:
223:
224:
226:
227:
228:
229:
230:
231:
232:
233:
234:
235:
236:
237:
238:
239:
240:
241;
242:
243:
244:
245:
246:
247:
248:
249:
250:
251:
252:
253:
254:
255:
256:
258:
259:
260:

~e te

~e we

nomatch:

PhRwe se

e
e

filhstl:

,
match:

~e sme te

Appendix A Blocking and Deblocking

mov sekhst,al shost sector to seek

active host sector?

mov al,l

xchg al,hstact salways becomes 1
test al,al swas it already?

jz filhst :fi11 host if not

host buffer active, same as seek buffer?
mov al,sekdsk

cmp al,hstdsk ssekdsk = hstdsk?
jnz nomatch

same disk, same track?

mov ax,hsttrk

cmp ax,sektrk shost track same as seek track
jnz nomatch

same disk, same track, same buffer?

mov al,sekhst

cmp al,hstsec ssekhst = hstsec?
jz match sskip if match

sproper disk, but not correct sector
mov al, hstwrt

test al,al :"dirty" buffer ?
jz filhst ;no, don’t need to write
call writehst syes, clear host buff

(check errors here)

smay have to fill the host buffer
mov al,sekdsk ! mov hstdsk,al
mov ax,sektrk ! mov hsttrk,ax
mov al,sekhst ! mov hstsec,al

mov al,rsflag

test al,al sneed to read?
jz filhstl
call readhst syes, if 1

(check errors here)

mov hstwrt,0 ;no pending write

scopy data to or from buffer depending on "readop"
mov al,seksec smask buffer number

and ax,secmsk sleast signif bits are masked
mov cl, 7 ! shl ax,cl sshift left 7 (\* 128 = 2\*\*7)

ax has relative host buffer offset

add ax,offset hstbuf sax has buffer address
mov si,ax ;put in source index register

at

---
_Page 094_

CP/M-86 System Guide Appendix A Blocking and Deblocking

261: mov di,dma_off ;user buffer is dest if readop
262: 3;
263: push DS ! push ES ;save segment registers
264: ;
265: mov ES,dma_seg ;set destseg to the users seg

-_ 266: ;SI/DI and DS/ES is swapped
2673 ;if write op
268: mov cx,128/2 slength of move in words
269: mov al,readop
270: test al,al ;which way?
271: jnz rwmove ;skip if read
272: 3
273: ; write operation, mark and switch direction
274: mov hstwrt,1l shstwrt = 1 (dirty buffer now)
275: xchg si,di ;source/dest index swap
276: mov ax,DS
2773 mov ES,ax
278: mov DS,dma_seg ;setup DS,ES for write
279: ;
280: rwmove:
281: cld ! rep movs AX,AX zmove as 16 bit words
282: pop ES ! pop DS ;restore segment registers |
283: ;
284: ; data has been moved to/from host buffer
285: cmp wrtype,wrdir write type to directory?
286: mov al,erflag sin case of errors
287: jnz return_rw sno further processing
288: ;

(~ 289: ; clear host buffer for directory write
290: test al,al serrors?
291: jnz return_rw skip if so
292: mov hstwrt,0 sbuffer written
293: call writehst
294; mov al,erflag
295: return_rw:
2963 ret
297: ;
298: g RH KRERRRRERKRRRIEKRRERRER EER EEK KERR ERERERERERREREREREEREER
299: ;\* \*
300: ;\* WRITEHST performs the physical write to the host \*
301: ;\* disk, while REANDHST reads the physical disk. \*
302: ;\* \*

303: g RARER EKER ERE RARER ERR KERRIER ERIE IKEA
304: writehst:

305: ret

306: ;

307: readhst:

308: ret

309: ;

310: g RRR KREKERE REE RRR RRR RIERA REREREKEREEEERKER
a 311: ;\* \*

312: ;\* Use the GENDEF utility to create disk def tables \*

313: ;\* \*

314: GI SG IGE IIE IGSIOGIG CIC IG IIE ICSI G IO IOS IO IO IO IOC IOI

315: dpbase equ offset $

---
_Page 095_

CP/M-86 System Guide Appendix A Blocking and Deblocking

316: ; disk parameter tables go here

317: ;

318: IESE IE IE IIE IORI IG IOI SC IOC ICCC IG IOCIOIOO OTT TTT
319: ;\* \*
320: ;\* Uninitialized RAM areas follow, including the \*
321: ;\* areas created by the GENDEF utility listed above. \*
322: ;\* \*
323: EEE IEG EIIE IO IGG SIE IORI IG OGIO IOC IO IOC IOC III IDE
324: sek_dsk rb 1 ;seek disk number

325: sek_trk rw 1 ;seek track number
326: sek_sec rb 1 ;seek sector number
327: ;

328: hst_dsk rb 1 shost disk number

329: hst_trk rw 1 ;host track number
330: hst_sec rb 1 shost sector number
331: ;

332: sek_hst rb 1 seek shr secshf

333: hst_act rb 1 shost active flag

334: hst_wrt rb 1 shost written flag
335: ;

336: una_cnt rb 1 yunalloc rec cnt

337: una_dsk rb 1 ;last unalloc disk
338: una_trk rw 1 last unalloc track
339: una_sec rb 1 last unalloc sector
340: ;

341: erflag rb 1 serror reporting

342: rsflag rb 1 sread sector flag

343: readop rb 1 :1 if read operation
344: wrtype rb 1 swrite operation type
345: dma_seg rw 1 slast dma segment

346: dma off rw 1 ;last dma offset

347: hstbuf rb hstsiz shost buffer

348: end

---
_Page 096_

Appendix B
Sample Random Access Program

This appendix contains a rather extensive and complete example
of random access operation. The program listed here performs the
simple function of reading or writing random records upon command
from the terminal. Given that the program has been created,
assembled, and placed into a file labelled RANDOM.CMD, the CCP level
command:

RANDOM X.DAT

starts the test program. The program looks for a file by the name
X.DAT (in this particular case) and, if found, proceeds to prompt
the console for input. If not found, the file is created before the
prompt is given. Each prompt takes the form

next command?

and is followed by operator input, terminated by a carriage return.
The input commands take the form

nw nR Q

where n is an integer value in the range 0 to 65535, and W, R, and 0
are simple command characters corresponding to random write, random
read, and quit processing, respectively. If the W command is
issued, the RANDOM program issues the prompt

type data:

The operator then responds by typing up to 127 characters, followed
by a carriage return. RANDOM then writes the character string into
the X.DAT file at record n. If the R command is issued, RANDOM
reads record number n and displays the string value at the console.
If the Q command is issued, the X.DAT file is closed, and the
program returns to the console command processor. The onlv error
message is

error, try again

The program begins with an initialization section where the
input file is opened or created, followed by a continuous loop at
the label "ready" where the individual commands are interpreted.
The default file control block at offset 005CH and the default
buffer at offset 0080H are used in all disk operations. The utility
subroutines then follow, which contain the principal input line
processor, called "readc." This particular program shows the
elements of random access processing, and can be used as the basis
for further program development. In fact, with some work, this
program could evolve into a simple data base management svstem.

---
_Page 097_

CP/M-86 System Guide Appendix B Sample Random Access Program

One could, for example, assume a standard record size of 128
bytes, consisting of arbitrary fields within the record. A program,
called GETKEY, could be developed which first reads a sequential
file and extracts a specific field defined by the operator. For
example, the command

GETKEY NAMES.DAT LASTNAME 10 20

would cause GETKEY to read the data base file NAMES.DAT and extract
the "LASTNAME" field from each record, starting at position 10 and
ending at character 20. GETKEY builds a table in memory consisting
of each particular LASTNAME field, along with its 16-bit record
number location within the file. The GETKEY program then sorts this
list, and writes a new file, called LASTNAME.KEY, which is an
alphabetical list of LASTNAME fields with their corresponding record
numbers. (This list is called an "inverted index" in information
retrieval parlance.)

Rename the program shown above as QUERY, and enhance it a bit
so that it reads a sorted key file into memory. The command line
might appear as:

QUERY NAMES.DAT LASTNAME. KEY

Instead of reading a number, the QUERY program reads an alphanumeric
string which is a particular key to find in the NAMES.DAT data base.
Since the LASTNAME.KEY list is sorted, you can find a particular
entry quite rapidly by performing a "binary search," similar to
looking up a name in the telephone book. That is, starting at both
ends of the list, you examine the entry halfway in between and, if
not matched, split either the upper half or the lower half for the
next search. Youll quickly reach the item you’re looking for (in
log2(n) steps) where you\*ll find the corresponding record number.
Fetch and display this record at the console, just as we have done
in the program shown above.

At this point you’re just getting started. With a little more
work, you can allow a fixed grouping size which differs from the 128
byte record shown above. This is accomplished by keeping track of
the record number as well as the byte offset within the record.
Knowing the group size, you randomly access the record containing
the proper group, offset to the beginning of the group within the
record read sequentially until the group size has been exhausted.

Finally, you can improve QUERY considerably by allowing boolean
expressions which compute the set of records which satisfy several
relationships, such as a LASTNAME between HARDY and LAUREL, and an
AGE less than 45. Display all the records which fit this
description. Finally, if your lists are getting too big to fit into
memory, randomly access your key files from the disk as well.

Some ee me

---
_Page 098_

CP/M-86 System Guide Appendix B Random Access Sample Program

PRR RPE

ee 00 08 08 00 00 ce 08 00 08 08 ef 08 08 ce se

DU BPWNHH OW DWHAU PWN FE

PH
~
ee

18;
19;
20:
21:
22:
23:
24:
26:
27:
28:
29;
30:
31:
32:
33:
34;
35:3
36:
37:
38;
39:
40:
4l:
42:
43;
44;
45;
46:
47:
48:
49;
50:
51:
52:
53:
54:

+e ee

oe =e ~e me Me we MO MO NO

‘

coninp equ
conout equ
pstring equ
rstring equ
version equ
openf equ
closef equ
makef equ
readr equ
writer equ

e
U

: Equates for
cr equ
lf equ

=e ~e te we

cseg
pushf
pop
cli
mov
mov
mov
push
popf

CP/M-86

—e se te we tO

Mov
call
cmp
jnb
,
mov
call
jmp

e

versok:

: correct
mov
mov
call

KERRIER RERREREREEREEKEERKKKKEEK

\*

Sample Random Access Program for CP/M-86 \*
\*

RARER RRR EE EEE KEEEREREEERERKEKKKKKRKKKEK

BDOS Functions

1 sconsole input function
2 ;console output function
9 sprint string until °$”
10 sread console buffer

12 sreturn version number
15 sfile open function

16 sclose function

22 smake file function

33 sread random

34 swrite random

non graphic characters
Odh scarriage return
Oah sline feed

load SP, ready file for random access

spush flags in CCP stack

ax ssave flags in AX
;disable interrupts

bx,ds sset SS register to base

ss,bx sset SS, SP with interru

sp,offset stack ; for 80888

ax srestore the flags

initial release returns the file

system version number of 2.2: check is
shown below for illustration purposes.

cl,version

bdos

al,20h sversion 2.0 or later?
versok

bad version, message and go back

dx ,offset badver

print

abort

version for random access

cl ,openf sopen default fct
dx, offset fcb

bdos

---
_Page 099_

CP/M-86 System Guide

56:
57:
58:
59:
60:
61:
62:
64:
65:
66:
67:
68;
69:
70:
71:
72:
74:
76:
78:
79:
80:
81:
82:
83:
84:
85;
86:
87;
88:
89;
90:
91:
92:
93:
94:
95;
96:
97:
98;
99;
100:
101:
102:
103:
104:
105:
106:
107:
108:
109:
110:

inc al serr 255 becomes zero
jnz ready
; cannot open file, so create it
mov cl,makef
mov dx,offset fcb
call bdos
inc al serr 255 becomes zero
jnz ready
: cannot create file, directory full
mov dx,offset nospace
call print
jmp abort back to ccp
: loop back to "ready" after each command
ready:
; file is ready for processing
,
call readcom ;read next command
mov ranrec,dx :store input record#
mov ranovf,0Oh sclear high byte if set
cmp al,\*Q\* quit?
jnz notq
‘
: quit processing, close file
mov cl,closef
mov dx, offset fcb
call bdos
inc al serr 255 becomes 0
jz error ;error message, retry
jmps abort sback to ccp
; end of quit command, process write
notq:
: not the quit command, random write?
emp al,°w\*
jnz notw
; this is a random write, fill buffer until cr
mov dx,offset datmsg
call print ;data prompt
mov cx,127 sup to 127 characters
mov bx,offset buff ;destination
rloop: :read next character to buff
push cx ssave loop conntrol.
push bx mnext destination
call getchr scharacter to AL
pop bx srestore destination
pop cx srestore counter
cmp al,cr send of line?

Appendix B Random Access Sample Program

---
_Page 100_

CP/M-86 System Guide Appendix B Random Access Sample Program

111; jz er loop

112: ; not end, store character

113: Mov byte ptr [bx],al

114: inc bx snext to fill
115: loop rloop sdecrement cx ..loop if
116: erloop:

117: ; end of read loop, store 00

118: mov byte ptr [bx],0h

119: ;

120: ; write the record to selected record number
121: mov cl,writer

122: mov dx,offset fcb

123: call bdos

124: or al,al serror code zero?
125: jz ready ;for another record

126: jmps error smessage if not

127: ;

128: ;

129: ;

130: ; end of write command, process read

131: ;

132: ;

133: notw:

134: ; not a write command, read record?

135: cmp al,“R\*

136: .. jz ranread

137: jmps error sskip if not

138: ;

139: ; read random record

140: ranread:

141; mov cl,readr

142: mov dx,offset fcb

143; call bdos

144; or al,al sreturn code 00?
145; jz readok

146: jmps error

147: ;

148: ; read was successful, write to console
149: readok:

150: call erlf snew line

151: mov cx, 128 smax 128 characters
152: mov si,offset buff ;next to get

153: wloop:

154: lods al snext character
155: and al,07fh smask parity

156: jnz wloopl

157: jmp ready sfor another command if
158: wloopl:

159: push cx ssave counter
160: push si :Save next to get
161: cmp al,” \* sgraphic?

162: jb skipw sskip output if not grav
163: call putchr soutput character
164: skipw:

165: pop si

---
_Page 101_

CP/M-86 System Guide

166:
167:
168;
169;
170:
171:
172:
173:
174:
175:
176:
177:
178:
179;
180:
181:
182;
183;
184:
185:
186:
187;
188:
189;
190:
191:
192:
193:
194;
195:
196:
197:
198;
199:
200:
201:
202:
203:
204:
205:
206:
207:
208:
209:
210;
211:
212:
213:
214:
215:
216:
217:
218;
219:
220:

(D se ~e ~e se ~e

rrors

1Q ~e we we

etchr:

putchr:

~e

crlf:

print:

°
c

readcom:

pop
loop

jmp

mov
call

jmp

cx
wloop
ready

Appendix B Random Access Sample Program

;decrement CX and check

end of read command, all errors end-up here

dx,offset errmsg

print
ready

entry subroutine

int
ret

mov
call

c1,0
bdos

7entry to BDOS if by INT

sreturn to CCP

suse function 0 to end e

utility subroutines for console i/o

;read next console character to a
cl,coninp

mov
call
ret

bdos

swrite character from a to console

mov
mov
call
ret

cl,conout
dl,al scharacter to send
bdos ssend character

;send carriage return line feed

mov
call
mov
call
ret

al,cr ;carriage return
putchr

al,lf sline feed
putchr

;print the buffer addressed by dx until $

push
call
pop
mov
call
ret

dx

erlf

ax snew line
cl,pstring

bdos ;print the string

---
_Page 102_

CP/M-86 System Guide

221:
222:
223:
224:
225:
226:
227:
228:
229:
230:
231:
232:
233:
234:
235:
236:
237:
238:
239:
240:
241:
242:
243:
244;
245:
246:
247:
248:
249:
250:
251:
252:
253:
254:
255:
256:
257:
258:
259:
260:
261:
262:
263:
264:
265:
266:
268:
269:
270:
271:
272:
273:
274:
275:

~e

read

getnum:

endr

tran

=e =e se we tO

fcb
rane

Appendix B Random Access Sample Program

sread the next command line to the conbuf

mov
call
mov
mov
call

command

mov
mov
c: mov
ine
mov
or
jnz
ret
not

sub
cmp
jnb
mov
mul
add
jmps
d:

zero

dx offset prompt

print :command?
cl,rstring

dx, offset conbuf

bdos sread command line

line is present, scan it
ax ,0 sstart with 0000
bx ,offset conlin
dl, [bx] snext command character
bx sto next command positio
dh ,0 ;zero high byte for add
al,dl scheck for end of comman
getnum

, numeric?
dl,\*0\*
d1,10 scarry if numeric
endrd
c1,10
cl smultipy accumulator by
ax ,dx stdigit

readc sfor another char

end of read, restore value in a and return value

mov
mov
cmp
jnb
ret
sl: and
ret

ax ,ax sreturn value in DX
al,-l [bx]

al,’a\* scheck for lower case
transl

al,5€H ;stranslate to upper case

Template for Page 0 of Data Group

Contains default FCB and DMA buffer

dseg

org

rb
ec rw

ranov£ rb

buff

'

rb

05ch

33 ;default file control bl
1 srandom record position
1 shigh order (overflow) b
128 :default DMA buffer

; string data area for console messages
badver
nospace
datmsg
errmsg
prompt

=e we we we

db
db
db
db
db

“sorry, you need cp/m version 2$%
“no directory space$\*

“type data: $\*

“error, try again.$”

“next command? $7”

fixed and variable data area

---
_Page 103_

CP/M-86 System Guide Appendix B- Random Access Sample Program

277:
278:
279:
280:
281:
282:
283:
284:

conbuf db
consiz rs
conlin rs
conlen equ

e
U

rs
stack rb
db
end

conlen ;length of console buffer

1 sresulting size after read
32 slength 32 buffer
offset $ - offset consiz

31 716 level stack

0 send byte for GENCMD

(.

---
_Page 104_

Appendix C
Listing of the Boot ROM

RHEE IRIE EERE IEREKEKKEKEKKEKEKKKR KKK

\*
k
\*
\*
\*
\*
k
\*
\*

\*
This is the original BOOT ROM distributed with CP/M \*
for the SBC 86/12 and 204 Controller. The listing \*
is truncated on the right, but can be reproduced by \*
assembling ROM.A86 from the distribution disk. Note \*
that the distributed source file should always be \*
referenced for the latest version \*

\*

\*

PSPS ERE RESES ESE SSELE SELES SES ELSE ESE SESE SESE EE ESE EEE SE SY

ROM bootstrap for CP/M-86 on an iSBC86/12
with the
Intel SBC 204 Floppy Disk Controller

Copyright (C) 1980,1981
Digital Research, Inc.

Box 579, Pacific Grove

California, 93950

This is the BOOT ROM which is initiated
by a system reset. First, the ROM moves
a copy of its data area to RAM at loca-

registers and the stack pointer. The
SBC 86/12 are initialized. The 8251

\*

\*

\*

\*

\*

\*

\*

ox

\*

\* baud asynchronous terminal, and the in-
\* terrupt controller is setup for inter-
\* rupts lOH-17H (vectors at 00040H-0005FH)
\* and edge-triggered auto-EOI (end of in-
\* terrupt) mode with all interrupt levels
\* masked-off. Next, the SBC 204 Diskette
\* controller is initialized, and track 1
\* sector 1 is read to determine the target
\* paragraph address for LOADER. Finallv,
\* the LOADER on track 0 sectors 2-26 and
\* track 1 sectors 1-26 is read into the

\* target address. Control then transfers
\* to LOADER. This program resides in two
\* 2716 EPROM\*’s (2K each) at location

\*
\*
\*
\*
\*
\*
\*

ROM 1 contains the odd addresses. BOOT
ROM uses RAM between 00000H and OOOFFH

the sector 1 buffer.

~e =e se we te NO Ne Me Se Ne M8 Ne Ne te Se TO TO MO NO MO MH NO ME TO NO TO NO 8 NO Te NO NO NO NO we SO “8 NO Ne NO

Lee EPEC E SESE SESE EEE ES SSE ESSE ESSE SE SER ESE SS SY

\*
\*&
\*

tion Q0000H, then initializes the segqment\*

k

various peripheral interface chips on the\*

\*

serial interface is configured for a 9600\*

\* FF FF FF FF HF eH EH HE

OFFOOOH on the SBC 86/12 CPU board. ROM \*
0 contains the even memory locations, and\*

k
\*

(absolute) for a scratch area, along with®

RRA ERE EKEEEKEKKEEKRKKKEKRKKE

---
_Page 105_

CP/M-86 System Guide

OOFF
FF00

OOFF

000D
000A

00A0
00A0
O0A0
00Al
00Al
00A2
00A4
00A5
OOA6
00A7
00A8
00A8
00A9
OOAA
OOAF

OONA
00D8

00D0
00D2
00D4
00D6

00c0
00Cc2

FE00

true equ Offh

false equ not true

debug equ true

;debug = true indicates bootstrap is in same roms

Appendix C Listing of the BOOT ROM

swith SBC 957 "Execution Vehicle" monitor
sat FEOO:0 instead of FF00:0

e
,

-_~

cr equ 13

lf equ 10

: disk ports and commands

base204 equ Oa0h

fdccom equ base204+0

fdestat equ base204+0

fdcparm equ base204+1

fderslt equ base204+1

fderst equ base204+2

dmacadr equ base204+4

dmaccont equ base204+5

dmacscan equ base204+6

dmacsadr equ base204+7

dmacmode equ base204+8

dmacstat equ base204+8

fdcsel equ base204+9

fdcsegment equ base204+10

reset204 equ base2044+15 a
actual console baud rate
baud rate equ 9600

;value for 8253 baud counter

baud equ 768/(baud_rate/100)

csts equ ODAh \*i18251 status port
cdata equ OD8h 7" data port
tch0 equ ODOh :8253 PIC channel 0
tchl equ tch0O+2 ;:ch 1 port

tch2 equ tch0+4 ;ch 2 port

temd equ tch0O+6 ;8253 command port
icpl equ 0cOh 78259a port 0

icp2 equ 0C2h

°
e
e
,

:8259a port 1

IF NOT DEBUG

ROMSEG EQU OFFOOH ;normal
ENDIF
IF DEBUG ;share prom with SB at
ROMSEG EOU OFE0O0H ‘
ENDIF

~e se

---
_Page 106_

CP/M-86 System Guide Appendix C Listing of the BOOT ROM

; This long jump prom’d in by hand
: cseg Offffh sreset goes to here
: JMPF BOTTOM sboot is at bottom
: EA 00 00 00 FF :cs = bottom of pro
; ip = 0

i ™ ; EVEN PROM ODD PROM
: 7P8 - EA 7F8 - 00
; 7F9 - 00 7F9 - 00
: TFA - FF sthis is not done i

FEOO cseg romseg

First, move our data area into RAM at 0000:0200

~e me NO

0000 8cc8 mov ax,cs
0002 8ED8 mov ds,ax spoint DS to CS for source
0004 BE3F01 mov SI,drombegin sstart of data |
0007 BFO002 mov DI,offset ram_start ;offset of destinat
000A B80000 mov ax,0
000D 8ECO mov eS,ax sdestination segment is 000
OOOF B9E600 mov CX,data length show much to move i
0012 F3A4 rep movs al,al ;move out of eprom
0014 B80000 mov ax,0
0017 8ED8 mov ds,ax ;data segment now in RAM
0019 8EDO mov ss,ax
001B BC2A03 mov sp,stack offset ;Initialize stack s
~ OO1E FC cld ~ ;clear the directio

=e

IF NOT DEBUG

Now, initialize the console USART and baud rate

mov al,0Eh

out csts,al ;give 8251 dummy mode

mov al,40h

out csts,al zreset 8251 to accept mode
mov al,4Eh

out csts,al ;normal 8 bit asynch mode,
mov al,37h

out csts,al senable Tx & Rx

mov al,0B6h

out tcmd,al 78253 ch.2 square wave mode
mov ax,baud

out tch2,al slow of the baud rate

mov al,ah

out tch2,al shigh of the baud rate
ENDIF

e
,
e
‘
e
U

OO01F B013 mov al,13h
0021 E6C0 out icpl,al 28259a ICW 1 8086 mode
0023 BO1O mov al,1l0h

>

Setup the 8259 Programmable Interrupt Controller

---
_Page 107_

CP/M-86 System Guide Appendix C Listing of the BOOT ROM

0025 E6C2 out icp2,al. :8259a ICW 2 vector @ 40-5
0027 BOI1F mov al,1Fh
0029 E6C2 out icp2,al :8259a ICW 4 auto EOI mast
002B BOFF mov al,OFFh
002D E6C2 out icp2,al 28259a OCW 1 mask all leve

Reset and initialize the iSBC 204 Diskette Interfa

e
,
e
e
e
,

r

estart: salso come back here on fatal error
002F E6AF out reset204,AL ;reset iSBC 204 logic and
0031 BOOL] mov AL,1l
0033 E6A2 out fderst,AL sgive 8271 FDC
0035 BO000 mov al,0
0037 E6A2 out fderst,AL \* a reset command
0039 BB1502 mov BX,offset specsl
003C E8E100 CALL sendcom ;program
003F BB1B02 mov BX,offset specs2
0042 E8DB00 CALL sendcom ; Shugart SA~800 drive
0045 BB2102 mov BxX,offset specs3
0048 E8D500 call sendcom ; characteristics
004B BB1002 homer: mov Bx,offset home
004E E85800 CALL execute shome drive 0
0051 BB2A03 mov bx,sectorl ;offset for first sector DM
0054 B80000 mov ax,0
0057 8ECO mov es,ax ;seqment " " " "
0059 E8A700 call setup_dma
005c BBO0202 mov bx,offset read0
005F E84700 call execute sget TO Sl
0062 8£062D03 mov es,ABS
0066 BBO0000 mov bx,0 :get loader load address
0069 £89700 call setup_dma ;setup DMA to read loader
006C BB0602 mov bx,offset readl
O06F £83700 call execute sread track 0
0072 BBOBO2 mov bx,offset read2
0075 £83100 call execute sread track 1
0078 8C06E802 mov leap_segment,ES

: setup far jump vector
007C C706E6020000 mov leap offset,0

; enter LOADER
0082 FF2EE602 jmpf dword ptr leap offset

pmsg:
0086 8A0F mov cl, [BX]
0088 84C9 test cl,cl
008A 7476 jz return
008C £80400 call conout
OO8F 43 inc BX
0090 E9SF3FF jmp pmsg

=e

ne te te enews

---
_Page 108_

CP/M-86 System Guide

009D

009E
00A0
00A2
00A4
O0A6
00A8

00A9

00AD

00B0
00B4
00B7
00B9
00BC
OOBE
00c0
00c3
00c5
00C7
00c9

00CB
00cD
00cF

00D3
00D5
00D7

00D9
0O0DB

00nD
00E0

E4DA
A801
74FA
8AC1
E6D8
C3

E4NA
A802
74FA
E4D8
247F
C3

89150002

£87000

8B1E0002
8A4701
243F
B90008
3C2C
720B
B98080
240F
3C0C
B000

E4A0
22C5
32C174F8

E4Al
241E

3c10

BB1302
E83D00

Appendix C Listing of the BOOT ROM

conout:
in al,csts
test al,l
jz conout
mov al,cl
out cdata,al
ret

°
i

conin:
in al,csts
test al,2
jz conin
in al,cdata
and al,7fh
ret

MD ~e se we

xecute: yexecute command string @ [BX]
7<BX> points to length,
;followed by Command byte
:followed by length-1l parameter bvt

=e

Mov lastcom,BX sremember what it w
retry: ;retry if not ready
call sendcom sexecute the comman

snow, let\*s see wha
;of status poll was
sfor that command t

mov BX, Lastcom spoint to command s

mov AL,1 [BX] sget command op cod

and AL, 3fh sdrop drive code bi

mov cx ,0800h smask if it will be

cmp AL, 2ch ssee if interrupt t

jb execpol1

mov CX ,8080h selse we use "not c

and AL,Ofh sunless ...

cmp AL,Och sthere isnt

mov AL,0

ja return sany result at all
execpoll: ;poll for bit in b, toggled with c

in AL,FOCSTAT

and AL,CH

xor AL,CL ! JZ execpoll

in AL, fderslt sget result registe

and AL, leh slook only at resul

jz return ;zero means it was

cmp al,l0h

jne fatal sif other than "Not
,

mov bx,offset rdstat

call sendcom sperform read statu

---
_Page 109_

CP/M-86 System Guide

00E5
00E7
00E9
00ED

0O0FO
OOF2
OOF4

OOF8
OOFB
OOFE
OOFF

010B
010D
010F
011B
011D
OllF

012B

012D
012F

E4A0
A880
75FA
8B1E0002
E9SBDFF

B400
8BD8
8B9F2702

E88BFF
E8A0FF

E92DFF

C3

B004
E6A8
B000
E6A5
B040
E6A5
8cco
E6AA
8AC4
E6AA
8BC3
E6A4
8AC4
E6A4
C3

E4A0
75FA
8A0F

8A07
E6A0

FEC9
74D1

E4A0
75FA

rd_poll:
in al,fdce_stat
test al,80h
jnz rd_poll
mov bx,last_com
jmp retry

e

fatal:

mov ah,0

mov bx,ax

mov bx,errtbl [BX]

print appropriate error
call pmsg

call conin

=e

pop
jmp

‘

return:
RET

U

setupdma:
mov
out
mov
out
mov
out
mov
out
mov
out
mov
out
mov
out
RET

M ~e ~e ~e

endcom:

and
jnz
mov
inc
Mov
out
parmloop:
dec

ax
restart

AL,04h
dmacmode , AL
al,0
damaccont ,AL
AL, 40h
dmaccont ,AL
AX,ES
fdcsegment , AL
AL, AH
fdcsegment ,AL
AX ,BX

dmacadr ,AL
AL,AH

dmacadr ,AL

Appendix C Listing of the BOOT ROM

swait for command n

srecover last attem
sand try it over ag

; fatal error

;make 16 bits
message

swait for key strik

;discard unused ite
sthen start all ove

sreturn from EXECUT

senable dmac

sset first (dummy)

sforce read data mo

;routine to send a command string t
in AL,fdcstat

AL,80h
sendcom
CL, [BX]
BX

al, [BX]
fdccom, AL

CL

jz return

inc BX
parmpoll:

in AL, fdcstat

and AL,20h

jnz parmpoll

sinsure command not busy
sget count

spoint to and fetch command
ssend command

ssee if any (more) paramete
spoint to next parameter

sloop until parm not full

---
_Page 110_

CP/M-86 System Guide

013A
013c

8A07
E6Al
E9SEEFF

013F

013F

014A
014B
014c
014D
014E

014F
015A
015D

016A
016c
016E
017A
017¢
017E

1A

026900
016C

05350D
0808E9
053510
FFFFFF
053518
FFFFFF

7F02
A202
B202
C502
D302

Appendix C Listing of the BOOT ROM

mov AL, [BX]
out fdceparm,AL
jmp parmloop

=e Case se we we

clastcom

e
creadstring

creadtrk0

;
creadtrkl

,

chome0
crdstat0
cspecsl

cspecs2

cspecs3

cerrtbl dw

ODOA4E756C6C Cer0 db

rombegin equ offset $

offset
offset
offset
offset

dw 0000h slast command
ab 3 slength
db 52h ;read function code
db 0 gtrack #
db 1 ssector #
ab 4
db 53h sread multiple i
db 0 strack 0

db 2 sectors 2 |
db 25 sthrough 26
db 4

db 53h

db 1 strack 1

ab 1 :sectors 1

db 26 sthrough 26

db 2,69h,0

db 1,6ch

db 5,35h,0dh

db 08h,08h,0e9h

dab 5,35h,10h

db 255,255,255

db 5,35h,18h

db 255,255,255

offset er0

offset erl

offset er2

offset er3

offset er4

offset er5

offset er6

offset er7
offset er8

offset er9

offset erA

offset erB

cr,lf,°“Null Error ??°,0

soutput next parameter
790 see about another

erc
erD
erE
erF

Image of data to be moved to RAM

---
_Page 111_

CP/M-86 System Guide Appendix C Listing of the BOOT ROM

204572726F72
203F3F00
0186 Cerl equ cer0
0186 Cer2 equ cer0
0186 Cer3 equ cer0
0196 ODOA436C6F63 Cer4 db cr,lf,\*Clock Error’ ,0
6B204572726F
01A4 0D0A4C617465 Cer5 db cr,lf,“Late DMA“ ,0
20444D4100
O1AF 0D0A49442043 Cer6 db cr,lf,\*ID CRC Error” ,0
524320457272
6F7200
O1BE 0D0A44617461 Cer7 db cr,lf,°Data CRC Error’ ,0
204352432045
72726F7200
OlCF 0D0A44726976 Cer8 db cr,lf,”“Drive Not Ready~,0
65204E6F7420
526561647900
01E1 0D0A57726974 Cer9 db cr,lf,’Write Protect” ,0
652050726F74
65637400
01F1 0D0A54726B20 CerA db cr,lf,°Trk 00 Not Found’ ,0
3030204E6F74
20466F756E64
0204 0D0A57726974 CerB db cr,lf,\*Write Fault” ,0
65204661756C
0212 0D0A53656374 CerC db ecr,lf,°Sector Not Found” ,0
6F72204E6F74
20466F756E64
0186 CerD equ cer0
0186 CerE equ cer0
0186 CerF equ cer0
0225 dromend equ offset $
O0E6 data_length equ dromend-drombegin
; reserve space in RAM for data area
; (no hex records generated here)
0000 dseg 0
org 0200h
0200 ram_start equ $
0200 lastcom rw 1 :last command
0202 read0 rb 4 sread track 0 secto
0206 readl rb 5 sread T0 S2-26
0208 read2 rb 5 sread Tl S1-26
0210 home rb 3 shome drive 0
0213 rdstat rb 2 sread status
0215 specsl rb 6

---
_Page 112_

CP/M-86 System Guide Appendix C Listing of the BOOT ROM

0218 specs2 rb 6
0221 specs3 rb 6
0227 errtbl rw 16
0247 er0 rb length cer0 716
0247 erl equ er0
0247 er2 equ er0
0247 er3 equ er0
0257 er4 rb length cer4 314
0265 er5 rb length cer5 s11
0270 er6 rb length cer6 715
027F er7 rb length cer? 217
0290 er8 rb length cer8 718
02A2 er9 rb length cer9 716
02B2 erA rb length cerA 719
02c5 erB rb length cerB 714
02D3 erC rb length cerC 319
0247 erD equ er0
0247 erE equ er0
0247 erF equ er0
0256 leap_offset rw 1
0258 leap_segment rw 1
0O2EA rw 32 slocal stack
032A stack offset equ offset S;stack from here do
; TO Sl read in here
032A sectorl equ offset $
032A Ty rb 1
032B Len rw 1
032D Abs rw 1 ;ABS is all we care
032F Min rw 1
0331 Max rw 1
end

---
_Page 113_

Appendix D
LDBIOS Listing

HHH HEIR IKREKEKKKEEKEEEEEEREEEEEEEKEKKK KKK

\*
\* This the the LOADER BIOS, derived from the BIOS
\* program by enabling the "loader_bios" condi-

\* tional assembly switch. The listing has been

\* edited to remove portions which are duplicated
\* in the BIOS listing which appears in Appendix D
\* where elipses "..." denote the deleted portions
\*
\*
\*
\*
\*

+e

\* +

(the listing is truncated on the right, but can \*
be reproduced by assembling the BIOS.A86 file \*
provided with CP/M-86) \*
\*
x

HR KEREREEEE RR HERREEREKEEE KR EREREEREREEKEEKEKEKEKKEK

g IIR II III III IHR KIRKE RRR REE IRRERKERREEEK

;\* Basic Input/Output System (BIOS) for
:\* CP/M-86 Configured for iSBC 86/12 with
;\* the iSBC 204 Floppy Disk Controller

ee ee

\*

;\* (Note: this file contains both embedded
;\* tabs and blanks to minimize the list file \*
;\* width for printing purposes. You may wish\*
;\* to expand the blanks before performing \*
;\* major editing.) \*
g IIHR KR IREEIRRIKREKERREEEE KR ERERRERERERE

Copyright (C) 1980,1981
Digital Research, Inc.
Box 579, Pacific Grove
California, 93950

(Permission is hereby granted to use
or abstract the following program in
the implementation of CP/M, MP/M or
CP/NET for the 8086 or 8088 Micro-
processor)

.
Ud
e
e
e
,
°
¢
°
,
e
v
e
c
e
’
e
t
e
e

FFFF true equ -l
0000 false equ not true

---
_Page 114_

CP/M-86 System Guide

FFFF
FFFF
00E0

1200 E93Cc00
1203 £96100

1239 £96400
123c £96400

g RARER KEKE HERE R KE ERERER ERE REE KEEEEREREEERE

;\* Loader _bios is true if assembling the

;\* LOADER BIOS, otherwise BIOS is for the

;\* CPM.SYS file. Blc_list is true if we

;\* have a serial printer attached to BLC8538
;\* Bdos int is interrupt used for earlier

;\* versions.

\* + te + ee eH

g RRR RRR KEE ERE HERE REE ER EREEREREEEES

loader_bios equ true

ble_list equ true

bdos_ int equ 224 ;reserved BDOS Interrupt
IF not loader_bios

ENDIF ;not loader_bios

IF loader_bios
| |
bios code equ 1200h ;start of LDBIOS
cep offset equ 0003h ;base of CPMLOADER
bdos_ofst equ 0406h ;stripped BDOS entry

ENDIF ;loader_bios

cseg

org ccpoffset
cep:

org bios_code
g RRR KKRRERKEEEEKKRERERERKEEREREREREKREKKEEREKRKEREEN
:\* \*
;\* BIOS Jump Vector for Individual Routines \*
o% \*
,

g RRR KERRIER EERREREREREEKREKKKEER

jmp INIT ;Enter from BOOT ROM or LOADER
jmp WBOOT ;Arrive here from BDOS call 0
jmp GETIOBF sreturn I/O map byte (IOBYTE)
jmp SETIOBF ;set I/O map byte (IOBYTE)

Appendix D LDBIOS Listing

we

~ ssa A:

ro sey om

ato sw ee

---
_Page 115_

CP/M-86 System Guide

123F

124A

124B
124C
124F

125B

125¢
125F

8CC8
8EDO
8ED8
8ECO

BCA916
FC

1E
B80000
8ED8

C70680030604
8C0E8203
1F

BB1514
E85A00
B100

E99CED

E99FED

Appendix D LDBIOS Listing

g III RI III RI III III R IR ERE RIERA

o\* \*
t
;\* INIT Entry Point, Differs for LDBIOS and \*
;\* BIOS, according to "Loader Bios" value \*
,

RIKI RIK RI RRR R ERR KER ERR REE REE REES

INIT: sprint signon message and initialize hardwa

mov ax,cs swe entered with a JMPF so
mov sS,ax + CS: as the initial value
mov ds,ax : DS:,

mov eS,ax : and ES:

suse local stack during initialization
mov sp,offset stkbase
cld sset forward direction

IF not loader _bios

ENDIF ;not loader_bios

IF loader_bios

:This is a BIOS for the LOADER

push ds ssave data segment
mov ax,0
mov ds,ax spoint to segment zero

;BDOS interrupt offset

mov bdos offset,bdos ofst

mov bdos segment,CS ;bdos interrupt segment
pop ds srestore data segment

ENDIF ;loader_bios

mov bx,offset signon

call pmsg ;print signon message

mov c1,0 sdefault to dr A: on coldst

jmp ccp sjump to cold start entry o
WBOOT: jmp ccp+6 sdirect entry to CCP at com

IF not loader_bios

ENDIF ;not loader bios

---
_Page 116_

CP/M-86 System Guide

126A

127D

129¢

129D
129F

E4DA

C3

E8F4FF

E4DA

E80700

C3

E441

C3

BO1A
C3

Appendix D LDBIOS Listing

go RHR RHR KREREKRKEER ERE KER ERE KERR EREKEKEERERER

3\* CP/M Character I/O Interface Routines
7\* Console is Usart (i825la) on iSBC 86/12
:\* at ports D8/DA

\* +e ee

g RARER REE KIKI KEKE RERERREREEKEREREKKEKE

CONST: sconsole status
in al,csts
const_ret:
ret sReceiver Data Available

CONIN: ;console input
call const

e ° e

CONOUT: sconsole output
in al,csts

LISTOUT: ;list device output

ENDIF ;ble_list

ret

LISTST: 7poll list status
IF ble_list

i | |
in al,lsts

ENDIF ;blc_list
ret

PUNCH: ;not implemented in this configuration
READER:

mov al,lah

ret sreturn EOF for now

---
_Page 117_

CP/M-86 System Guide

12A0
12A2

12A3

12A4
12A6

12A7

12CA

12EB

130A

131A

131B
131E

B000
C3

c3

c3

E8C9FF

BB0000

C606311500

880E3115
C3

880583215
C3

8BD9

890E2A15
C3

890E2C15
C3

BB3815
c3

Appendix D LDBIOS Listing

GETIOBF:
mov al,0 7TTY: for consistency
ret ;IOBYTE not implemented
SETIOBF:
ret siobyte not implemented
zero ret:
and al,0
ret sreturn zero in AL and flag
; Routine to get and echo a console character
; and shift it to upper case
uconecho:
call CONIN sget a console character
ITTITTITTTTITITTTTiTiTTiTitTitetiti ite t ee,
o\* \*
¢
:\* Disk Input/Output Routines \*
o® \*
,

g RRR KERR ERE RIEREREERERE KEKE ERE RE RREREREREEE
SELDSK: ;select disk given by register CL
mov bx,0000h

HOME: smove selected disk to home position (Track
mov trk,0 sset disk i/o to track zero

SETTRK: ;set track address given by CX
mov trk,cl swe only use 8 bits of trac
ret

SETSEC: ;set sector number given by cx

mov sect,cl swe only use 8 bits of sect
ret

SECTRAN: ;translate sector CX using table at [Dx]
mov bx,cx

SETDMA: ;set DMA offset given by CX
mov dma_adr,Cx
ret

SETDMAB: ;set DMA segment given by CX
mov dma_seq,CX
ret

;

GETSEGT: ;return address of physical memory table
mov bx,offset seg table
ret

---
_Page 118_

CP/M-86 System

131F B012
1321 EBO2

1323 BOOA

1325 BB2F15

1415 ODOAODOA

1419 43502F4D2D38
362056657273
696F6E20322E
320D0A00

142F ODOA486F6D65

Guide Appendix D LDBIOS Listing

g RHR RERERERREREREER EERE KEE EERIE KEK

e
\*
\*

;\* All disk I/O parameters are setup: the
;\* Read and Write entry points transfer one
;\* sector of 128 bytes to/from the current
;\* DMA address using the current disk drive

tt + +

g RARER ERE REE KEKE KERR EREREREEEEKREKKEKE

READ:
mov al,12h sbasic read sector command
jmps r_w_common

WRITE:
mov al,Qah sbasic write sector command

r_w_common:
mov bx,offset io com ;point to command stri

g RRR EERE EERE EEE RE RERRERRERERERREEREE

3% \*
o\* Data Areas \*
3\* \*
g RHR REE RRR KIK IEEE REE RRR REE EKREREEREERER
data_offset equ offset $

dseg

org data_offset ;contiguous with co

IF loader_bios
H 0 ce ame ee See SS ee ee eee ee ee ee ee ee ee ee te ee et ee ee ee ee es ss se
; |
signon db cr,lf,cr,lf ;

db “CP/M-86 Version 2.2’,cr,lf,0

ENDIF ;loader bios

IF not loader_bios

ENDIF ;not loader_bios

bad_hom db cr,lf,\*Home Error’ ,cr,lf,0
include singles.lib ;read in disk definitio
DISKS 2

~e

~

---
_Page 119_

CP/M-86 System Guide Appendix D LDBIOS Listing

= 1541 dpbase equ $ sBase of Disk Param
=1668 00 db 0 sMarks End of Modul
{ \ 1669 loc stk rw 32 ;local stack for initialization
16A9 stkbase equ offset $
16A9 00 db 0 sfill last address for GENCMD
g RR KR RIK RR IRR III IRR IIHR RTH R ERE RR EEIER
3% Dummy Data Section \*
o\* \*
EEG EI SEIU OG IE IESG IOS GIO CI IO OGIO CIAO
0000 dseg 0 sabsolute low memory
org 0 (interrupt vectors)
END

---
_Page 120_

Appendix E
BIOS Listing

HRI KEKE KEE ERE EEERREKREEEKKKKEKKKKKKKK

\*
\* This is the CP/M-86 BIOS, derived from the BIOS
\* program by disabling the "loader bios" condi-

\* tional assembly switch. The listing has been

\* truncated on the right, but can be reproduced

\* by assembling the BIOS.A86 file provided with

\* CP/M-86. This BIOS allows CP/M-86 operation

\* with the Intel SBC 86/12 with the SBC 204 con-
\* troller. Use this BIOS, or the skeletal CBIOS
\* listed in Appendix E, as the basis for a cus-

\* tomized implementation of CP/M-86.

\* provided with CP/M-86)

\*
\*

+ ee +e +e FH He HF H+ FH HF HF

RHEE REE ERE EERE EREREREKEKKEKKKKKEE

g RIKI IRI RI RRR RARER RRR ERE REE

:\* Basic Input/Output System (BIOS) for
;\* CP/M-86 Configured for iSBC 86/12 with
:\* the iSBC 204 Floppy Disk Controller

+e FF ee

;\* (Note: this file contains both embedded
;\* tabs and blanks to minimize the list file \*
;\* width for printing purposes. You may wish\*
;\* to expand the blanks before performing \*
:\* major editing.) \*
RHR KIRK RR I RIE EIR RIKER ERE RARER REE EEEER

Copyright (C) 1980,1981
Digital Research, Inc.

Box 579, Pacific Grove

California, 93950

(Permission is hereby granted to use
or abstract the following program in
the implementation of CP/M, MP/M or
CP/NET for the 8086 or 8088 Micro-
processor)

~e ~e te te we tO te NO te NO

FFFF true equ -l
0000 false equ not true

---
_Page 121_

CP/M-86 System Guide Appendix E BIOS Listing

FFFF
00E0

0B06

O0ODA
00D8

g RRR KKK KEER EKER EKER ER REE REE RERKREKERERER ERE
ok

c

;\* Loader_bios is true if assembling the

;\* LOADER BIOS, otherwise BIOS is for the

;\* CPM.SYS file. Blc_list is true if we

;\* have a serial printer attached to BLC8538
;\* Bdos_int is interrupt used for earlier

ee  & & Fe

;\* versions.

ok

IST ITI SILT TTTITTTTTTTTTTTTTTTTTTTTTeTtitiie

loader_bios equ false

ble_list equ true

bdos_int equ 224 ;reserved BDOS Interrupt
IF not loader bios

H — ee ee oe om et oe om oe oe me ee ee es oe ee ee ee ee ee ee ee a a

i | |

bios_code equ 2500h

ccp_offset equ 0000h

bdos_ofst equ 0B06h ;BDOS entry point

ENDIF ;not loader bios

IF loader_bios
; |
bios_ code equ 1200h ;start of LDBIOS
ccp_ offset equ 0003h ;base of CPMLOADER
bdos_ofst equ 0406h ;stripped BDOS entry

ENDIF ;loader_bios

csts equ ODAh ;18251 status port
cdata equ OD8h ; "data port

IF ble_list
i | |
lsts equ 4lh ;2651 No. 0 on BLC8538 stat
ldata equ 40h ; " "  " " data
ble_reset equ 60h ;reset selected USARTS on B

ENDIF ;ble_list

g RRR RRR KKK KEKE RRR ERE ERE RE RRRERERERERE
:\* \*
:\* Intel iSBC 204 Nisk Controller Ports \*
o\* \*
,

g RRR RK RERERERERERRERE KERR RK KK RERE HERE KR KEEEEE

---
_Page 122_

CP/M-86 System Guide

00A0

00A0
00A0
00A1l
00A1l
00A2
00A4
00A5
00A6
00A7
00A8

base204

fdc_com
fdc_stat
fdc_parm
fde_rslt
fde_rst
dmac_adr
dmac_cont
dmac_scan
dmac_sadr

00A8
00A9
OOAA
OOAF

000A

000D
000A

250C
250F
251B
252A
252D
253C

E93C00
E98400
E99000
£99600
E99D00
E9A500
E9B700
E9B400
E9FFOO
E9DB00
E90EO1
£91001
£91901
£92401
£92501
E99100
E90601
E90FO1
£91101
£99300
£99300

dmac_mode

dmac

stat

fdc_sel
fdce_segment
reset 204

max_retries

cr
lf

cep:

cseg
org

org

equ

equ
equ
equ
equ
equ
equ
equ
equ
equ
equ
equ
equ
equ
equ

equ

equ
equ

Appendix E BIOS Listing

0a0h

base204+0
base204+0
base204+1
base2044+1
base204+2
base204+4
base204+5
base204+6
base204+7
base204+8
base204+8
base204+9

base204+10
base204+15

Odh
Oah

ecpoffset

bios code

>SBC204 assigned ad

78271
28271
78271
78271
38271
38257
28257
38257
38257
38257

38257

FDC

out comma

in status

out

parameter

in result

out
DMA
out
out
out
out

reset
base addr
control
scan cont
scan addr
mode

in status

+FDC select port (n
ssegment address re
sreset entire inter

smax retries on dis
sbefore perm error
scarriage return

sline feed

JAG GIGIGIIICIC ISI TAI TIT IIT IITA IIIT IIE

.\*

‘\*
t

\*

++ BIOS Jump Vector for Individual Routines \*
\*

JG IIGIGIGIO ICICI GIG CCIC CCITT ITI ITAA II II

imp
Jmp
jmp
jmp
jmp
jmp
jmp
jmp
jmp
jmp
mp
Jmp
jmp
Imp
mp
jmp
jmp
imp
imp
jmp

INIT
WBOOT
CONST
CONIN
CONOUT
LISTOUT
PUNCH
READER
HOME
SELDSK
SETTRK
SETSEC
SETDMA
READ
WRITE
LISTST
SECTRAN
SETDMAB
GETSEGT
GETIOBF
SETIOBF

sEnter from BOOT ROM or LOADER
sArrive here from BDOS call 0
sreturn console keyboard status
sreturn console keyboard char
swrite char to console device
swrite character to list device
swrite character to punch device
sreturn char from reader device
smove to trk 00 on cur sel drive
sselect disk for next rd/write
sset track for next rd/write
sset sector for next rd/write
sset offset for user buff (DMA)
sread a 128 byte sector
swrite a 128 byte sector
sreturn list status
sxlate logical->physical sector
sset seg base for buff (DMA)
sreturn offset of Mem Desc Table
sreturn I/O map byte (IOBYTE)

sset I/O map byte (IOBYTE)

---
_Page 123_

CP/M-86 System Guide

253F

254A

254B
254C
254F

255D

256E

256F
257B
257D

8CC8
8ED0
8ED8
8ECO

BCE429
FC

1E
B80000
8ED8
8ECO

€70600008n25
8C0E0 200
BF0400
BE0000
B9FEO1

F3A5

C7068003060B
LF

BOFF
E660
BO4E
E642
BO3E
E642
B037
E643

Appendix E BIOS Listing

pA HRA RERRKR ERR EEREREERKRRERERERERER

ok
U

\*

;\* INIT Entry Point, Differs for LDBIOS and \*

e%

\*\* BIOS, according to "Loader Bios" value \*
- \*

‘
RRR RRER ERE REE RE REE

INIT:

;print signon message and initialize hardwa
mov ax,cs swe entered with a JMPF so
mov 8S,ax CS: as the initial value
mov ds,ax DS:,

mov eS,ax and ES:

suse local stack during initialization

mov sp,offset stkbase

cld sset forward direction

~e we te

IF not loader bios

; This is a BIOS for the CPM.SYS file.
; Setup all interrupt vectors in low

¢ memory to address trap

push ds ;save the DS register

mov ax,0

mov ds,ax

mov eS,ax sset FS and DS to zero
;setup interrupt 0 to address trap routine
mov int0O_offset,offset int_trap

mov int0_segment,CS

mov di,4
mov si,0 sthen propagate

mov cx,510 strap vector to

rep movs ax,ax ;all 256 interrupts
;BDOS offset to proper interrupt

mov bdos_offset,bdos ofst

pop ds ;restore the NS register

g RHR RII RIERA REE ERE REE REE REE
x \*
e

;\* National "BLC 8538" Channel 0 for a serial\*
:\* 9600 baud printer - this board uses 8 Sig-\*
;\* netics 2651 Usarts which have on-chip baud\*
\*\* rate generators. \*
7\* \*
g IIR IIR IRI TRIER KIRKE KERR ERR REE

mov al,OFFh
out ble reset,al ;reset all usarts on 8538
mov al,4Eh

out ldata+2,al ;set usart 0 in async 8 bit
mov al,3Eh

out ldata+2,al ;set usart 0 to 9600 baud
mov al,37h

out ldata+3,al ;enable Tx/Rx, and set up R

(

we ee te SEE Sten eee enn ne ee ne ee ee ~

---
_Page 124_

CP/M-86 System Guide Appendix FE BIOS Listing

257F

258A

258D
258E

259B
259D
259F

25Al

BB4427
E86600
B100

E976DA

E979DA

FA
8CC8
8ED8
BB7927
£85300
F4

E4DA
OCFF

c3

ENDIF ;not loader_bios

IF loader_bios

\*This is a BIOS for the LOADER

push ds ;save data seqment
mov ax,0
mov ds,ax spoint to segment zero

;BDOS interrupt offset

mov bdos_offset,bdos ofst

mov bdos_segment,CS ;bdos interrupt seqment
pop ds srestore data segment

ENDIF ;loader_bios

mov bx,offset signon

call pmsg ;print signon message

mov cl,0 :default to dr A: on coldst

jmp ccp ;jump to cold start entry o
WBOOT: jmp ccpt+é6 sdirect entry to CCP at com

IF not loader_bios
7 | |
int_trap:

cli sblock interrupts

mov ax,cs

mov ds,ax ;get our data segment

mov bx,offset int_trp

call pmsg

hlt shardstop

ENDIF ;not loader bios

gH RHR RIKI HIRI KEIR ERIE REREERE
3% \*
hal CP/M Character I/O Interface Routines \*
3% Console is Usart (i825la) on iSBC 86/12 \*
hl at ports D8/DA \*
ial \*
g RRR KRRRERE KER EERE RERRERRERRER EERE EERER

CONST: sconsole status
in al,csts
and al,2
jz const ret
or al,255 sreturn non-zero if RDA

const_ret:
ret ;Receiver Data Available

---
_Page 125_

CP/M-86 System Guide Appendix E BIOS Listing

CONIN: ;console input
25A2 E8F4FF cal]. const
25A5 74FB jz CONIN swait for RDA
25A7 E4D8 in al,cdata
25A9 247F and al,7fh sread data and remove parit
25AB C3 ret

CONOUT: sconsole output
25AC E4DA in al,csts
25AE 2401 and al,l sget console status
25B0 74FA jz CONOUT swait for TBE
25B2 8AC1 mov al,cl
25B4 E6D8 out cdata,al sTransmitter Buffer Empty
25B6 C3 ret sthen return data

LISTOUT: sList device output

IF ble_list

; |
25B7 £80700 call LISTST
25BA 74FB jz LISTOUT swait for printer not busy
25BC 8AC1 mov al,cl
25BE E640 out ldata,al ssend char to TI 810

ENDIF rblc_list

25C0 C3 ret

LISTST: ;poll list status

IF ble_list

|
25Cl E441 in al,lists
25c3 2481 and al,8lh slook at both TxRDY and DTR
25C5 3c8l cmp al,81lh
25C7 750A jnz zero_ret seither false, printer is b

25C9 OCFF or al,255 sboth true, LPT is reaay

ENDIF ;blc_list

25CB C3 ret
PUNCH: ;not implemented in this configuration
READER:

25cc BOLA mov al,lah

25CE C3 ret sreturn EOF for now
GETIOBF:

25CF B000 mov al,0 ;TTY: for consistency

25D1 C3 ret sIOBYTE not implemented

---
_Page 126_

CP/M-86 System Guide

25D2

25D3
25D5

25D6
25D9
25DA
25DC
25DF
25E0
25E2
25E4
25E6
25E8

25EA

25EB
25ED
25EF
25F1
25F3
25F6
25F7

25F9
25FC
25FF
260A

260D
260F

C3

C3

E8COFF

8AC8
E8CDFF

3c61
3C7A
2C20

C3

8A07
84C0
8AC8
E8B6FF

EBF2

BB0000
80F902
B080
80F900
B040
A26928

B500
8BD9
B104

SETIOBF:

ret

zero_ret:

ret

and

al,0

Appendix E BIOS Listing

siobyte not implemented

sreturn zero in AL and flag

+ Routine to get and echo a console character
; and shift it to upper case

uconecho:

call CONIN sget a console character

push ax

mov cl,al ;save and

call CONOUT

pop ax secho to console

cmp al,“a\*

jb uret sless than “a” is ok

emp al,~2\*

ja uret sgreater than “2° is ok

sub al,’a°-°A” ;else shift to caps
uret:

ret
: utility subroutine to print messages
pmsg:

mov al, [BX] sget next char from message

test al,al

jz return ;if zero return

mov CL,AL

call CONOUT sprint it

inc BX

jmps pmsg snext character and loop
JSG IOI IOS IIIT TIT T TTT III
o\* \*
'
3% Disk Input/Output Routines \*
\* \*
Ul

JAEGGI ICICIS ITI IOTI TTI O IIe

SELDSK:

sell:

mov
cmp
jnb
mov
cmp
jne
mov
mov

mov
mov
mov

sselect
bx ,0000h
cl,2
return
al, 80h
c1,0
sell
al, 40h
sel_mask,al

ch,0

bx ,cx
c1,4

disk given by register CL

sthis BIOS only supports 2
sreturn w/ 0000 in BX if ba

sdrive 1 if not zero

selse drive is 0

ssave drive select mask
snow, we need disk paramete

BX = word (CL)

---
_Page 127_

CP/M-86 System Guide Appendix E BIOS Listing

2613 D3E3
2615 81C37C28
2619 C3

261A C6066C2800
261F BB6E28
2622 £83500
2625 74F2

2627 BB6A27
262A E8BEFF
262D EBEB

262F 880E6C28
2633 C3

2634 880E6D28
2638 C3

2639 8BD9
263B 03DA
263D 8AlF
263F C3

2640 89086528
2644 C3

2645 8906728
2649 C3

264A BB7328
264D C3

264E B012
2650 EBO2

shl bx,cl ;multiply drive code \* 16
screate offset from Disk Parameter Base
add bx,offset dp base

return: ~
ret

HOME: ;move selected disk to home position (Track
mov trk,0 sset disk i/o to track zero
mov bx,offset hom_com
call execute

jz return shome drive and return if 0
mov bx,offset bad_hom selse print
call pmsg ;"Home Error"
jmps home sand retry

SETTRK: ;set track address given by CX
mov trk,cl swe only use 8 bits of trac
ret

SETSEC: ;set sector number given by cx
mov sect,cl swe only use 8 bits of sect
ret

SECTRAN: ;translate sector CX using table at [Dx]

mov bx,cx

add bx,dx sadd sector to tran table a
mov bl, [bx] sget logical sector

ret

SETDMA: ;set DMA offset given by CX
mov dma_adr ,CX
ret

SETDMAB: ;Set DMA segment given by CX
mov dma_seg,CX
ret

;

GETSEGT: ;return address of physical memory table
mov bx,offset seg _table
ret

g RR RRRRRRERERRER EER EREREEREEEKEERERERERERREEEE

=e
\*
\*

;\* All disk I/O parameters are setup: the
7\* Read and Write entry points transfer one
:\* sector of 128 bytes to/from the current
;\* DMA address using the current disk drive

+e et +

’
gp RRR RRR REE EERE REE REE RER REE HEREREREREREEES

READ:
mov al,1l2h sbasic read sector command
jmps r_w_common

WRITE:

---
_Page 128_

CP/M-86 System Guide

265A

265E

266A
266E
267B
267D
267F

268B
268D
268F

269B

BOOA

BB6A28
884701

891E6328

C60662280A

8B1E6328
E88900

8B1E6328
8A4701
B90008
3C2C
720B
B98080
240F
3c0c
B000

E4A0
22C5
32C1
74F8

E4Al
241E

3C10

FE0E6228
75C8

B400

Appendix & BIOS Listing

mov al,Oah sbasic write sector command
r_w_common:
mov bx,offset io_com ;point to command stri
mov byte ptr 1[BX],al ;put command into str
: fall into execute and return
execute: ;sexecute command string.
{BX] points to length,
followed by Command byte,
followed by length-1l parameter byte

=e se se

mov last_com,BX ;save command address for r
outer retry:

zallow some retrying

mov rtry_cnt,max_retries

retry:

mov BX,last_com

call send _com stransmit command to i8271
; check status poll

mov BX,last com

mov al,1[bx] sget command op code

mov cx,0800h smask if it will be "int re

cmp al,2ch

jb exec_poll sok if it is an interrupt t

mov cx,8080h selse we use "not command b

and al,0fh

cmp al,0ch sunless there isn“t

mov al,0

ja exec_exit any result

‘poll for bits in CH,

exec_poll: ; toggled with bits in CL

in al,fdc_stat
and al,ch

xor al,cl ;
jz exec_poll

sread status

isolate what we want to
sand loop until it is done

;Operation complete,
in al,fdc_rslt ; see if result code indica
and al,leh
jz exec_exit sno error, then exit
;some type of error occurre
emp al,10h
je dr_nrdy swas it a not ready drive ?
7no,
dr_rdy: ; then we just retry read or write

dec rtry_cnt

jnz retry + up to 10 times
: retries do not recover from the
: hard error

mov ah,0

---
_Page 129_

CP/M-86 System Guide

269D
269F
26A3
26A6
26A8
26AB
26AD
26AF
26Bl
26B3
26B5
26B7

26B9

26BA
26BD
26BF
26C2
26C4
26C7

26CA
26CD
26CF

26D1
26D3

26D4

26D7
26D9
26DE
26E0

26E2
26E5

26EA
26EC
26EE
26F0

8BD8
8B9F9127
E845FF
E4D8
E82BFF
3C43
3C52
74AB
3c49
741A
OCFF

C3

E81A00
75A4
£81500
759F
BB0228
E821FF

E80A00
74FB
EB92

C3

E9B3FE

B640
F606692880
B604

BB7128
E80B00

E4A0
A880
75FA
E4Al
8 4C6

exec_exi

dr_nrdy:

Appendix E BIOS Listing

mov bx,ax smake error code 16 bits
mov bx,errtbl [BX]

call pmsg sprint appropriate message
in al,cdata eflush usart receiver buffe

call uconecho yread upper case console ch
cmp al,“C\*

je wboot 1 cancel

cmp al,\*R\*

je outer_retry ;retry 10 more times

cmp al,“I\*

je z_ret signore error
or al,255 ;set code for permanent err
ts
ret
shere to wait for drive ready
call test_ready
jnz retry ;if it\*’s ready now we are d
call test_ready
jnz retry sif not ready twice in row,

mov bx,offset nrdymsg
call pmsg ;"Drive Not Ready"

nrdy0l:
call test_ready
jz nrdy0l snow loop until drive ready
jmps retry sthen go retry without decr
zret:
and al,0
ret ;return with no error code
wboot_1: ;can\*t make it w/ a short 1
jmp WBOOT
g RARER EERE EEERER ER EKER EEE EREER EKER E
o\* \*
;\* The i8271 requires a read status command \*
;\* to reset a drive-not-ready after the \*
;\* drive becomes ready \*
o\* \*
‘

g KKK KKK RHEE ERK EKER RK REKRKEEKEREEKEKEEREREKREERE

test_ready:

nrdy2:

dr_poll:

mov dh, 40h sproper mask if dr l

test sel mask,80h

jnz nrdy2

mov dh, 04h smask for dr 0 status bit

mov bx,offset rds_com
call send_com

in al,fdce_stat ;get status word
test al,80h

jnz dr_poll swait for not command busy

in al,fdc_rslt ;get "special result"

test al,dh slook at bit for this drive

~ oF

as , iain. ae

“~ ?

~~ Ty _— wr oo!

oe tee ee

ow eee

—~ eS ome

---
_Page 130_

CP/M-86 System Guide

26F2

26F3
26F5
26F7

26F9
26FC
26FE

270A
270C
270E
271D
271F

272A
| 272D
: 2731

CS an33

C3

E4A0
A880
75FA

8A4701
3C12
B140
EB06

3C0A
B180

B004
E6A8
B000
E6A5
8AC1
E6A5
A16528
E6A4
8AC4
E6A4
A16728
E6AA
8AC4
E6AA

8A0F

8A07
0A066928
E6A0

FEC9

ret

Appendix E BIOS Listing

sreturn status of ready

JARS III IIIT ITI TTT TTT TITS IIE

\*

;\* Send_com sends a command and parameters
BX addresses parameters.
:\* The DMA controller is also initialized
>\* if this is a read or write

>\* to the i8271:

o%

\* +e ee

'
ECGS CGICIGI SOIC GICIOII III IIIT IT IIs

send_com:

in al,fde_stat
test al,80h

jnz

send_com

ssee if we have

mov
cmp
jne
MOV

al,l{bx]
al,1l2h
write maybe
c1,40h

jmps init _dma

write maybe:
cmp
jne
mov

init_dma:

al, Oah
dma_exit
cl,80h

sinsure command not busy
sloop until ready

to initialize for a DMA ope
;get command byte

:if not a read it could be

s;is a read command, go set

sleave DMA alone if not rea
swe have write, not read

swe have a read or write operation, setup DMA contr
contains proper direction bit)

; (CL
mov
out
mov
out
mov
out
mov
out
mov
out
mov
out
mov
out

dma_exit:
mov
inc
mov

al,04h
dmac_mode,al
al ,00
dmac_cont,al
al,cl
dmac_cont,al
ax,dma_adr
dmac_adr,al
al,ah
dmac_adr,al
ax,dma_seg
fdc_segment,
al,ah
fde_segment,

cl, [BX]
BX
al, [BX]

or al,sel_mask

out
parm_loop:
dec

fdc_com,al

cl

jz exec_exit

inc
parm poll:

BX

;enable dmac
;send first byte to con
sload direction register
ssend low byte of DMA
s;send high byte
al ;send low byte of segmen
al :then high segment addre
sget count
sget command

smerge command and drive co
ssend command byte

sno (more) parameters, retu
;point to (next) parameter

---
_Page 131_

CP/M-86 System Guide Appendix E BIOS Listing

2738 E4A0
273A A820
273C 75FA
273E 8A07
2740 E6Al1
2742 EBEF

2744 ODOAODOA

2748 202053797374
656D2047656E
657261746564
20202D203131
204A616E2038
310D0A00

276A ODOA486F6D65
204572726F72
ODOA00

2779 OD0A496E7465
727275707420
547261702048
616C740D0A00

2791 B127B127B127
B127

2799 C127D127DE27
EF27

27Al 022816282828
3D28

27A9 4D28B127B127

in al,fdc_stat

test al,20h stest "parameter register f
jnz parm_poll sidle until parm reg not fu
mov al, [BX]

out fde_parm,al ;send next parameter

jmps parm_loop ;go see if there are more p

g RRR RRR KEE REE ERE KEE ERE EREKEREKKES
al \*
;\* Data Areas \*
3% \*
go RRR ERERERRERKEKKEREREKERERERKEEEEEEKEEERE
data_offset equ offset $

dseg

org data_offset scontiguous with co

IF loader_bios
H em ee em oe ee we oe oe wr ee oe ee ee em ee ee ee te ee et oe 8 Oe ee oe we oe oe ee oe oe oe oe oe oe oe oe os oe ee
i | |
signon db cr,lf,cr,lf

db “CP/M-86 Version 22rd Ero

ENDIF ;loader_bios

IF not loader bios
7 | |
signon db cr,lf,cr,lf

db \* System Generated - 11 Jan 81’,c

ENDIF ;not loader bios

bad_hom db cr,1lf,“Home Error’ ,cr,1lf,0

int_trp db cr,lf,“Interrupt Trap Halt’,cr,1f,0

errtbl dw er0,erl,er2,er3
dw er4,er5,er6,er7
dw er8,er9,erA,erB

dw erC,erD,erE,erF

tow Se ee eek

tim ee ee

ae we we ee me ee ee

---
_Page 132_

CP/M-86 System Guide Appendix E BIOS Listing

B127
27Bl1 ODOA4E756C6C er0 db cr,lf,\*Null]l Error ??7,0
204572726F72
203F3F00
f™ 27B1 erl equ er0
27B1 er2 equ er0
27Bl er3 equ er0
27Cl ODOA436C6F63 er4 db cr,lf,\*Clock Error :\*,0
6B204572726F
72203A00
27D1 OD0A4C617465 er5 db cr,lf,°Late DMA :\*,0
20444D41203A
27DE 0D0A49442043 er6 db cr,1lf,°ID CRC Error :\*,0
524320457272
6F72203A00
27EF 0D0A44617461 er7 db cr,lf,\*Data CRC Error :\*,0
204352432045
72726F72203A
2802 0D0A44726976 er8 db cr,lf,°Drive Not Ready :\*,0
65204E6F7420
526561647920
3A00
2816 0D0A57726974 er9 db cr,lf,°“Write Protect :\*,0
652050726F74
656374203A00
{> 2828 0D0A54726B20 erA ab cr,lf,\*Trk 00 Not Found :\*,0
3030204E6F74
20466F756E64
203A00
283D 0D0A57726974 erB db cr,lf,\*Write Fault :\*,0
65204661756C
74203A00
284D 0D0A53656374 erC db cr,lf,°Sector Not Found :\*,0
6F72204E6F74
20466F756E64
203A00
27B1 erD equ er0
27B1 erE equ er0
27B1l erF equ er0
2802 nrdymsg equ er8
2862 00 rtry_ cnt db 0 sdisk error retry counter
2863 0000 last com dw 0 saddress of last command string
2865 0000 dma_adr dw 0 ;dma offset stored here
2867 0000 dma_seg dw 0 ;dma segment stored here
2869 40 sel_mask db 40h ;select mask, 40h or 80h
{ \ ; Various command strings for i8271
286A 03 io_com db 3 slength
286B 00 rd_wr db 0 sread/write function code

286c 00 trk db 0 strack #

---
_Page 133_

CP/M-86 System Guide

286D 00 sect

286E

022900
012¢

e
,

2873 02

2874 DFO2
2876 2105
2878 0020
287A 0020

1ouoa ou

dpbase
dped

287C

AB280000
00000000
C5289C28
64294529
AB280000
00000000
C5289C28
93297429

dpel

289 dpbo

=289C
=289E
=289F
=28A0
=28Al

1A00
F200

=28A3 3F00
=28A5 CO

=28A6 00

=28A7 1000
=28A9 0200

= 28AB

=28AB 01070D13
=28AF 19050B11
=28B3 1703090F
=28B7 1502080E
=28BB 141A060C
=28BF 1218040A
=28C3 1016
001F

x1t0

als0
css0

289C
0O1F
28AB

dpbl
alsl
cssl
xltl

e
c

noi ut bob tot od tea

28C5 begdat

db

hom_com db 2,29h,0
rds_com db 1,2ch

Appendix E BIOS Listing

ssector #

shome drive command
sread status command

System Memory Segment Table

dw
dw
dw
dw

equ
dw
dw
aw
dw
dw
dw
dw

segtable db 2

tpa_s
tpa_l
2000h
2000h

;2 segments
eg slst seg starts after BIOS
en sand extends to 08000

ssecond is 20000 -
:3FFFF (128k)
include singles.lib ;read in disk definitio

DISKS 2
$ :Base of Disk Param
x1t0,0000h :Translate Table
0000h,0000h :Scratch Area
dirbuf,dpb0 \*Dir Buff, Parm Blo
csv0,alv0 :Check, Alloc Vecto
xl1t1,0000h sTranslate Table
0000h,0000h :Scratch Area
dirbuf,dpbl :Dir Buff, Parm Blo
csvl,alvl :Check, Alloc Vecto

dw

equ
dw
db
db
db
dw
dw
db
ab
dw
dw
equ
db
db
ab
db
ab
ab
db
equ
equ

equ
equ
equ
equ

DISKDEF 0,1,26,6,1024,243,64,64,2

offset $ :Disk Parameter Blo
26 ;Sectors Per Track
3 sBlock Shift

7 sBlock Mask

0 sExtnt Mask

242 sDisk Size - 1

63 sDirectory Max

192 sAllocO

0 sAllocl

16 :Check Size

2 ;Offset

offset $ sTranslate Table
1,7,13,19

25,5,11,17

23,3,9,15

21,2,8,14

20,26,6,12

18,24,4,10

16,22

31 sAllocation Vector
16 sCheck Vector Size
DISKDEF 1,0

dpb0d sEquivalent Paramet
als0 :Same Allocation Ve
css0 \*Same Checksum Vect
x1t0d :Same Translate Tab
ENDEF

Uninitialized Scratch Memory Follows:

equ

offset $ \*Start of Scratch A

(

a ee rr

we ey

we ee

~

li ee ee

---
_Page 134_

CP/M-86 System Guide

28C5

29E4
02DF
29E4 00

dirbuf rs
alvO rs
csv0 rs
alvl rs
csvl rs
enddat equ
datsiz equ

db

loc_stk rw
stkbase equ

lastoff equ
tpa_seg equ
tpa len equ

~ db 0

ee RREKKKKKEKK

o\*

e

:\*

g RERKEKKKEKRE
dseg
org

int0O_offset

int0_ segment

; pad
rw

bdos_offset
bdos_segment
END

Appendix E BIOS Listing

128 \*Directory Buffer
als0 sAlloc Vector

css0 sCheck Vector

alsl sAlloc Vector

cssl sCheck Vector
offset $ sEnd of Scratch Are
offset $-begdat ;Size of Scratch Ar
0 sMarks End of Modul

32 slocal stack for initialization
offset $

offset $:
(lastof£+0400h+15) / 16
0800h - tpa_seg
;f€i11 last address for GENCMD

REAR EEKERKEKKKKKKEKK
bd

Dummy Data Section \*
\*
KHER KERR KKK
0 sabsolute low memory
0 : (interrupt vectors)
rw 1
rw 1

to system call vector
2\* (bdos_int-1)

rw 1
rw 1

---
_Page 135_

Appendix F
CBIOS Listing

RHEE RUE ERERREREERREERERKEKK

\*

\* This is the listing of the skeletal CBIOS which
\* you can use as the basis for a customized BIOS

\* for non-standard hardware.

\*
\*
\*
\*

FFFF
000D
000A

00E0

0B06

tions of the BIOS remain, with "rs" statements

®

®

\*

The essential por- \*
\*

marking the routines to be inserted. \*
x

\*

RRR RARE RE REE IKKE KEKE

g RRR EKER ERERRE ERR ERR ERE ERE

;\* This Customized BIOS adapts CP/M-86 to
7\* the following hardware configuration

:\* Processor:

el Brand:

3% Controller:
o%

,

\*®

c

3\* Programmer:
:\* Revisions :
ok

tf

et + Fe He + HH HF FH

g RRR REE RE RKEKEREREREREREKEREREKEEKRKKKKE

true equ
false equ
cr equ
lf equ

not true

Odh ;carriage return
Oah ;line feed

g RRR KERIKERI KREKRRKERERERERRERREERRERREREREE

o%
’

+\* Loader bios is true if assembling the
;\* LOADER BIOS, otherwise BIOS is for the

;\* CPM.SYS file.

o%
c

+ ee eH

g RRR RR IRRE RARER EERE RRR RK HER ERER ERE HERE KEEKERE

loader bios equ

bdos_int equ
IF not

|

bios code equ

cep offset equ

bdos_ofst equ

false
224 sreserved BDOS interrupt

loader bios

2500h
0000h
OBO6h ;BDOS entry point

---
_Page 136_

CP/M~-86 System Guide

250C
250F
251B
252A
252D
253C

E93C00
E97900
E98500
E98D00
E99A00
E9A200
E9B500
E9BD00
E9F600
E9D900
E90101
E90301
E90C01
E91701
£94701
E98F00
E9F900
E90201
E90401
E9A400
E9A500

253F 8CC8

ENDIF  ;not loader bios

IF loader_bios
| |
bios code equ 1200h :start of LOBIOS
cep offset equ 0003h ;base of CPMLOADER
bdos_ofst equ 0406h ;stripped BNOS entry

ENDIF ;loader bios

cseg

org ccpoffset
ccp:

org bios code

g RRR RRR KE

o%\* \*
;\* BIOS Jump Vector for Individual Routines \*
o\* \*

,
g RHR RIERA ERE KEE RRR RRA KE

jmp INIT ;Enter from BOOT ROM or LOADER
jmp WBOOT Arrive here from BDOS call 0
jmp CONST ;return console keyboard status
jmp CONIN ;return console keyboard char
jmp CONOUT ;write char to console device
jmp LISTOUT write character to list device
jmp PUNCH write character to punch device
jmp READER ;return char from reader device
jmp HOME ;move to trk 00 on cur sel drive
jmp SELDSK ;select disk for next rd/write
jmp SETTRK ;set track for next rd/write

jmp SETSEC ;set sector for next rd/write
jmp SETDMA ;set offset for user buff (DMA)
jmp READ ;read a 128 byte sector

jmp WRITE ;write a 128 byte sector

jmp LISTST ;return list status

jmp SECTRAN ;Xlate logical->physical sector
jmp SETNDMAB ;set seg base for buff (NMA)

jmp GETSEGT ;return offset of Mem Desc Table

jmp GETIOBF ;return I/O map byte (IOBYTR)
jmp SETIOBF ;set I/O map byte (IOBYTE)

RRR KKH KERR KK RRR R IRIE KER IKKERRARAKRKEK

ox &
;\* INIT Entry Point, Differs for LDBIOS and \*
;\* BIOS, according to "Loader Bios" value \*
o\* ~ \*

g RRR KIRKE KHER EKER ERE RRR RRR IKKE

INIT: ;print signon message and initialize hardwa
mov ax,cs swe entered with a JMPF so

Appendix F CBIOS Listing

(

we eR Re

Te ene

i i ae

ee a. i a os

wee Se

Me a

---
_Page 137_

CP/M-86 System Guide

254A

254B
254C

255E
256B

256D

8EDO
8ED8
8ECO

BC5928
FC

1E
C606A72600
B80000
8ED8

8ECO

C70600008225
8C0E0200
BF0400
BE0000
B9FEO1]

F3A5

C7068003060B
1F

BBB126
E86F00

257A B100
257C E981DA

Appendix F CBIOS Listing

mov ss,ax \*CS: as the initial value o
mov ds,ax \*DS:,
mov es,ax sand ES:

;use local stack during initialization
mov sp,offset stkbase
clad sset forward direction

IF not loader_bios

This is a BIOS for the CPM.SYS file.
Setup all interrupt vectors in low
memory to address trap

=e se se

push ds ;save the DS register
mov IOBYTE,0 sclear IOBYTE

mov ax,0

mov ds,ax

mov eS,ax sset ES and NS to zero

;setup interrupt 0 to address trap routine
mov int0 offset,offset int_trap
mov int0_segment,CS

mov di,4
mov si,0 ;then propagate
mov cx,510 ;trap vector to

rep movs ax,ax ;all 256 interrupts
:BDOS offset to proper interrupt

mov bdos offset,bdos ofst

pop ds _ srestore the NS register

(additional CP/M-86 initialization)

ENDIF j;not loader bios

IF loader bios

:This is a BIOS for the LOADER

push ds ;save data segment
mov ax,0
mov ds,ax spoint to seqment zero

;BDOS interrupt offset

mov bdos_ offset,bdos ofst

mov bdos segment,CS ;bdos interrupt segment
(additional LOADER initialization)

pop ds srestore data segment

ENDIF ;loader_bios

mov bx,offset signon

call pmsg sprint signon message

mov cl1,0 sdefault to dr A: on coldst

jmp ccp ;jump to cold start entry o

---
_Page 138_

CP/M-86 System Guide

257F

258A
258D

258E

259C
259E
25A8

25A9
25B3

25B4
25BE

25BF
25C9

25CA
25D4

25D5
25DF

25E0

E984DA

FA
8CC8
8ED8
BBD126
E85c00
F4

C3

E8F2FF
74FB

C3

C3

C3

C3

C3

c3

A0A726

WBOOT: jmp ccp+6

e
;
int_trap:

Appendix F CBIOS Listing

not loader_bios

cli sblock interrupts
mov ax,cs
mov ds,ax :get our data segment
mov bx,offset int_trp
call pmsg
hlt shardstop
; |
ENDIF ;not loader bios

g RRR KRERER ERE REE EEREE KEE KEERKREEKE EERE

o\*
c

\*

3\* CP/M Character I/O Interface Routines \*

o%
c

\*

g RRR RHEE KREUK REEEREKEREREE

CONST: sconsole status
rs 10 (fill-in)
ret
CONIN: ;console input
call CONST
jz CONIN swait for RDA
rs 10 + (fill-in)
ret
CONOUT: sconsole output
rs 10 :(fill-in)
ret sthen return data
LISTOUT: slist device output
rs 10 : (fill-in)
ret
LISTST: ;poll list status
rs 10 s(fill-in)
ret
PUNCH: swrite punch device
rs 10 3 (fill-in)
ret
READER:
rs 10 + (fill-in)
ret
GETIOBF:

mov al, IOBYTE

sdirect entry to CCP at com

we ee ee

i a a

ee

---
_Page 139_

CP/M-86 System Guide

25E3

25E4
25E8

25E9
25EB
25ED
25EF
25F1
25F4
25F5

c3

880EA7 26
C3

8A07
84C0
8AC8
E8B5FF
EBF2

25F7
25FB
25FE
260B
260E

262B

262C
262E

880EA8 26
BB0000
80F902
730D
B500
8BD9
B104
D3E3
B9OF126
03D9
c3

C706A9260000

C3

890EA926
c3

890EAB26
C3

8BD9
03DA
8AlF
C3

Appendix F CBIOS Listing

ret
SETIOBF:

mov IOBYTE,cl :set iobyte

ret siobyte not implemented
pmsg:

mov al, [BX] :get next char from message

test al,al

jz return ;if zero return

mov CL,AL

call CONOUT sprint it

inc BX

jmps pmsg ;next character and loop
g RRR ERR HR EKEREEKHRRIE KEE KEREREEEKEEE KES
o\* \*
c
:\* Disk Input/Output Routines \*
o\* \*

g RRR RHR HERRERA EREREKREES

SELDSK: sselect disk given by register CL
ndisks equ 2 s;number of disks (up to 16)
mov disk,cl ssave disk number
mov bx,0000h sready for error return
cmp cl,ndisks sn beyond max disks?
jnb return sreturn if so
mov ch,0 :double(n)
mov bx,cx sbx =n
mov cl,4 sready for \*16
shl bx,cl sn =n \* 16
mov cx,offset dpbase
add bx,cx ;dpbase + n \* 16
return: ret sbx = .dph
HOME: ;move selected disk to home position (Track

mov trk,0 :set disk i/o to track zero
rs 10 > (fill-in)
ret
SETTRK: ;set track address given by Cx
mov trk,CX
ret
SETSEC: ;set sector number given by cx
mov sect,CX
ret
SECTRAN: ;translate sector CX using table at [DX]
mov bx,cx
add bx,dx sadd sector to tran table a
mov bl, [bx] :get logical]. sector
ret

SETDMA: ;set DMA offset given by CX

---
_Page 140_

CP/M-86 System Guide Appendix F CBIOS Listing

263C

263D

26A6

890EAD26
C3

890EAF26
C3

BBE826
C3

C3

C3

26A7

26A7
26A8
26A9
26AB
26AD
26AF

mov dma_adr,Cx
ret

SETDMAB: ;set DMA segment given by CX
mov dma_seg,CX
ret

GETSEGT: ;return address of physical memory table
mov bx,offset seg_ table
ret

g RRR ERR RHE REE IKK KERIKERI KKK ERR EKER EREEKEER

\* \*
,

;\* All disk I/O parameters are setup: \*
hal DISK is disk number (SELDSK) \*
:\* TRK is track number (SETTRK) \*
al SECT is sector number (SETSEC) \*
;\* DMA_ADR is the DMA offset (SETDMA) \*
:\* DMA SEG is the DMA segment (SETDMAB) \*

7\* READ reads the selected sector to the NMA\*
7\* address, and WRITE writes the data from \*
;\* the DMA address to the selected sector \*
7\* (return 00 if successful, 01 if perm err)\*
\*

g RRR RRR RRR EEE KERIKERI KIER KERR ERER ER EKER

READ:
rs 50 :fill-in
ret

WRITE:
rs 50 : (fill-in)
ret

g RRR REE RRR EERE EERIE KEKE KEE ERK EERE RES

3\* \*
;\* Data Areas \*
3\* \*
g RRR RRR REE REE KKK KE REREREKREERKEEERER
data_offset equ offset $

dseg

org data_offset scontiguous with co
IOBYTE db 0
disk ab 0 sdisk number
trk dw 0 strack number
sect dw 0 ;sector number
dma_adr dw 0 ;DMA offset from DS
dma_seg dw 0 sDMA Base Segment

IF loader_bios
a ee
signon db cr,lf,cr,lf

---
_Page 141_

CP/M-86 System Guide

26B1
26B5

26CE

26D1
26D3

26E6

26E8
26E9
26EB
26ED
26EF

ODOAODOA
53797374656D
2047656E6572
617465642030
302F30302F30

ODOA00

ODOA
496E74657272
757074205472
61702048616C

ODOA

C602
3A05

26F1

20270000
00000000
3A271127
D927BA27
20270000
00000000
3A271127
08285927

°
c

segtable

dpbase
ape

dpel

dpbo

Appendix F CBIOS Listing

“CP/M-86 Version 1.0’,cr,1f,0

;loader_ bios

not loader bios

db

cr,lf,cr,lf

“System Generated 00/00/00”

;not loader bios

cr,lf

“Interrupt Trap Halt”

cr,lf

System Memory Segment Table

db 2 72 segments
dw tpa_seg
dw tpa_len
dw 2000h
aw 2000h
include singles.
DISKS 2
equ s
dw x1t0,0000h
dw 0000h,0000h
dw dirbuf ,dpb0
dw esv0,alv0
dw x1t1,0000h
dw 0000h,0000h
dw dirbuf,dpbl
dw esvl,alvl
DISKDEF 0,1,26,6
equ offset $
dw 26
db 3
db 7
db 0
dw 242
dw 63
db 192
db 0

;lst seg starts after BIOS
sand extends to 08000

ssecond is 20000 -
¢3FFFF (128k)

lib ;read in disk definitio

sBase of Disk Param
:Translate Table
sScratch Area

:Dir Buff, Parm Blo
;Check, Alloc Vecto
sTranslate Table
:Scratch Area

:Dir Buff, Parm Blo
:Check, Alloc Vecto
,1024,243,64,64,2
:Disk Parameter Blo
sSectors Per Track
\*Block Shift

sBlock Mask

sExtnt Mask

\*Disk Size - l
sDirectory Max
sAllocO

sAllocl

---
_Page 142_

CP/M-86 System Guide

=271C 1000
=271E 0200

= 2720

=2720 01070D13
=2724 19050B11
=2728 1703090F
=272C 1502080E
=2730 141A060C
=2734 1218040A
=2738 1016
001F

001F

053A
2859 00

x1t0

enddat
datsiz

loc stk
stkbase

lastoff
tpa_seg
tpa_len

dw
dw
equ
db
db
db
db
db
db
db
equ
equ

equ
equ
equ
equ

Appendix F CBIOS Listing

offset $
1,7,13,19
25,5,11,17
23,3,9,15
21,2,8,14
20,26,6,12
18,24,4,10
16,22

DISKDEF 1,0
dpb0

als0O

css0

x1t0d

ENDEF

:Check Size
:Offset
sTranslate Table

sAllocation Vector
:Check Vector Size

sEquivalent Paramet
:Same Allocation Ve
;Same Checksum Vect
;Same Translate Tab

Uninitialized Scratch Memory Follows:

equ
rs
rs
rs
rs
rs
equ
equ
ab

rw
equ

equ
equ
equ
db 0

offset $

als0d

css0

alsl

cssl

offset $

offset $-begdat

\*Start of Scratch A
;Mirectory Buffer
sAlloc Vector
:Check Vector
sAlloc Vector
sCheck Vector

sEnd of Scratch Are
:Size of Scratch Ar
sMarks End of Modul

32 ;local stack for initialization

offset $

offset $

(lastoff+0400h+15) / 16

0800h - tpa_seg

sfill last address for GENCMD

ge RRR EKER REE ERE EREEREEEREREREEERIKER

ok
‘
o%
,
e\*
Ul

,

dseg
org

intO offset

int0_segment

e
o

Dummy Data Section

®
\*

\*
PEPE REESE LES EEE ELSES EE SESE E SELES EEE EST SESE ES Se
0 sabsolute low memory
0 ; (interrupt vectors)
rw 1
rw 1

pad to system call vector

Iw

bdos_offset

bdos_segment

END

2\* (bdos_int-1)

rw 1
rw 1

ee a

ngs mee Se Ne re, eee ee ee

---
_Page 143_

Index

A

allocate absolute memory, 52
allocate memory, 52

base page, l
BIOS, 121
bootstrap, 4
bootstrap ROM, 81

Cc

CBIOS, 56, 137
close file, 34
CMD, 1, 15
cold start loader, 1, 56, 81
compact memory model, 11, 21
compute file size, 45
CONIN, 61
CONOUT, 61
console input, 25
console output, 25
console status, 30
CONST, 60
converting 8080 programs
to CP/M-86, 3, 17, 23
cross development tools, 2

D

data block, 72, 74

delete file, 36

direct BIOS call, 47

direct console 1/0, 27

directory entries, 7l

disk definition tables, 4, 67

disk parameter block, 69

disk parameter header, 62,
67, 75

DMA buffer, 14, 39, 60, 63

PF

far call, ll, 14

file control block, 30
file structure, 1

free all memory, 53

G

GENCMD, 2, 3, 15, 17

GENDEF, 2

get address of disk parameter
block, 41

get allocation vector
address, 39

get DMA base, 48

get I/O byte, 27

get maximum memory, 51

get or set user code, 41

get read/only vector, 40

GETIOB, 65

GETSEGB, 65

group, 2

header record, 20
HOME, 61

I

INIT , 4, 60
Intel utilities, 17
IOBYTE, 58

L

L-module format, 19

LDCOPY, 2

LIST, 61

list output, 26

LISTST, 63

LMCMD, 19

logical to physical sector
translation, 64

make file, 37

memory, 14

memory region table, 65
memory regions, 1

oO

offset, 2
open file, 33

---
_Page 144_

Index

P

print string, 28
Program load, 53
PUNCH, 61

Punch output, 26

R

random access, 95

READ, 63

read buffer, 29

read random, 42

read sequential, 36

READER, 61

reader input, 26

release all memory, 53

release memory, 52

rename, 38

reserved software interrupt,
l, 23

reset disk, 33

reset drive, 46

return current disk, 38

return login vector, 38

return version number, 30

Ss

search for first, 35

search for next, 35

sector blocking and
deblocking, 87

SECTRAN, 64

segment, 2

segment group memory
requirements, 17

segment register change, 11

segment register
initialization, 8

SELDSK, 62

select disk, 33

set DMA address, 39

set DMA base, 48

set file attributes, 41

set I/O byte, 28

set random record, 46

SETDMA, 63

SETDMAB, 64

SETIOB, 65

SETSEC, 62

SETTRK, 62

small memory model, 10, 21

system reset, 4, 7, 14, 25
49, 60, 74

T

translation vectors, 69

U

utility program operation, 2
W

WBOOT, 60

WRITE, 63

write protect disk, 39

write random, 44

write random with zero
fill, 47

8080 memory model, 3, 10,
14, 21

lan

we eet

a a a

---
_Page 145_

Reader Comment Card

x We welcome your comments and suggestions. They help us provide you with better !
product documentation.

Date ____———__- Second Edition: June 1983 |

1. What sections of this manual are especially helpful?

2. What suggestions do you have for improving this manual? What information
(> is missing or incomplete? Where are examples needed?

3. Did you find errors in this manual? (Specify section and page number.)

CP/M-86® Operating System System Guide

COMMENTS AND SUGGESTIONS BECOME THE PROPERTY OF DIGITAL RESEARCH.

---
_Page 146_

& a . ,

wee ee

ee ee te nts - en cece at ane staan Ale pat i et a ee NN NET He ae ee

Eo

From: ————___————————
NO POSTAGE
oOo aoa NECESSARY

IF MAILED IN THE

UNITED STATES
Ee
Ee
eeC—C“C;isCtés
ee
ne
ee
ee
Cis
ee
Bee
ee
ee
BUSINESS REPLY MAIL ———
FIRST CLASS / PERMITNO.182 / PACIFIC GROVE, CA Pe
a Fe
POSTAGE WILL BE PAID BY ADDRESSEE oo
ee
™ Be
0) DIGITAL RESEARCH a
Be
Pacific Grove, California
93950

Attn: Publications Production

