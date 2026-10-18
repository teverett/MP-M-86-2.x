# MPM86 pgmr Sep81

> OCR text extracted from `MPM86_pgmr_Sep81.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `MPM86_pgmr_Sep81.pdf`.

---

---
_Page 001_

ENB Se

co

eg ance ee eas
Cr Cia
See

Cee

MP/M-86
OPERATING SYSTEM
PROGRAMMER’S GUIDE

---
_Page 002_

MP/M-86'"™-
Operating System

PROGRAMMER'S GUIDE

Copyright © 1981

Digital Research
P.O. Box 579
167 Central
Pacific Grove, CA 93950
(408) 649-3896
TWX 910 360 5001

All Rights Reserved

---
_Page 003_

COPYRIGHT

Copyright © 1981 by Digital Research. All rights
reserved. No part of this publication may he
reproduced, transmitted, transcribed, stored ina
retrieval system, or translated into any language or
computer language, in any form or by any means,
electronic, mechanical, magnetic, optical, chemical,
Manual or otherwise, without the prior written
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

CP/M is a registered trademark of Digital Research.
ASM-86, CP/M-86, DDT-86 and MP/M-86 are trademarks
of Digital Research.

The "MP/M-86 Programmer’s Guide" was prepared using
the Digital. Research TEX Text Formatter and printed
in the United States of America by Commercial Press
/ Monterey.

KKK KEKE RRR KER EKER KEKE KERR REE KKK

\* First Printing: September 1981 \*
KKK KKK KEKE KK KEK REE KR EKRKKRKERAERKEKEKRKEKKER

---
_Page 004_

FOREWORD

MP/M-86'™ is a multi-user operating system for microcomputers
that use the Intel 8086, 8088, or compatible microprocessor. It
will support multi-terminal access with multi-programming at each
terminal. The minimum hardware environment for MP/M-86 must include
an 8086 or similar processor, 64K bytes of random access memory
(RAM), a syStem console, and a real-time clock. A typical MP/M-86
kernel occupies less than 32K bytes.

This manual describes the programming interface to MP/M-86.
Sections 1 through 6 describe the modules that comprise the
Operating system, the manner in which MP/M-86 monitors running
processes, as well as detailed descriptions of all the system entry
points.

Section 7 contains a complete description of the Digital
Research assembler ASM-86'™ and the various options that can be
invoked with it. One of these options controls the hexadecimal
Output format. ASM-86 can generate 8086 machine code in either
Intel or Digital Research format. Appendix A describes these
formats.

Section 8 discusses the elements of ASM-86 assembly language.
It defines ASM-86%s character set, constants, variables,
identifiers, operators, expressions, and statements. ;

Section 9 discusses the ASM-86 housekeeping functions such as
requesting conditional assembly, including multiple source files,
and controlling the format of the listing printout.

Section 10 summarizes the 8086 instruction mnemonics accepted
by ASM-86. These mnemonics are the same as those used by the Intel
assembler except for four instructions: the intra-segment short
jump, and inter-segment jump, return and call instructions.
Appendix B summarizes these differences.

Section 11 discusses the code-macro facilities of ASM-86,
including code-macro definition, specifiers and modifiers as well as
nine special code-macro directives. This information is also
Summarized in Appendix H.

Section 12 discusses DDT-86'™, the interactive debugging
Program, which allows the user to test and debug programs in the
8086 environment. The section includes a sample debugging session.

This manual is not intended as a tutorial. Therefore,

familiarity with the material covered in the User’s Guide and with
processor architecture and assembly language in general is assumed.

iii

---
_Page 005_

---
_Page 006_

TABLE OF CONTENTS

1 MP/M-86 System Overview

1.1
1.2

1.3

1.10
1.11
1.12

Introduction ...... 2.6268.

Supervisor .......6464888

Real-Time Monitor .........
1.3.1 Process Dispatching ....
1.3.2 Queue Management ......
1.3.3 System Timing Functions ..

Memory Module .......+e....
Character I/O Module .......
Basic Disk Operating System... .
Extended I/O System ........
Resident System Processes .....
Transient Programs .......
Resident Procedure Library

System Function Calling Conventions

Error handling .........-.

The MP/M-86 File System

File System Overview .......
File Naming Conventions ......
Disk Drive and File Organization

File Control Block Definition...
User Number Conventions ......
Directory Labels and XFCBs ....
File Passwords .......e8-s
File Date and Time Stamps .....

File Open Modes .........-.

---
_Page 007_

TABLE OF CONTENTS

(continued)

2.10 File Security .....+. ses
2.11 Concurrent File Access... .

2.12 Multi-Sector I/O .......-.
2.13 xXIOS Blocking and Deblocking,. .
2.14 Reset, Access and Free Drive. .

2.15 BDOS Error Handling .....

Transient Commands

3.1 Transient Process Load and Exit .
3.2 Command File Format ..... .
3.3 Base Page Initialization .....

3.4 Parent/Child Relationships ...

Command File Generation
4.1 Transient Execution Models ...
The 8080 Memory Model. .

The Small Memory Model .
The Compact Memory Model

b b

-l.
el.
el.

WN FF

4.2 GENCMD .... 2. «© © © © © © ©

4.3 Intel HEX File Format ..... .

RSP Generation
5.1 Introduction . ...... «© « -«
5.2 RSP Memory Models .......-

5.2.1 8080 Model RSP ......
5.2.2 Small Model RSP .....

vi

---
_Page 008_

TABLE OF CONTENTS

(continued)

5.3 Multiple Copies of RSPs .......+4 624640. 60
5.3.1 8080 Model... 1... 1. we ew ew ew ew ee eee) 61
5.3.2 Small Model ... rs 0 8
5.3.3 Small Model with Shared “Code a oO

5.4 Creating and Initializing an RSP .......... 621

1 The RSP Header . ... 2... 1 ee ee we ew ew ee) «663

2 The RSP Process Descriptor .......... 63

3. The RSP User Data Area. ........ 2... 64

-4 The RSP Stack .......- 64.2424 2664+ «© © « 64

5 The RSP Command Queue .. ee ee ew we ew we ew) C65

6 Multiple Processes within an RSP... ... . 66

5.5 Developing and Debugging an RSP ........... «66
System Function Calls

Introduction to ASM
7.1 Assembler Operation . . .. 4... 1 ee © ee ee ee 207
7.2 Optional Run-time Parameters ......... +. . 209

7.3 Aborting ASM-86 ..... 2.2. ee we ew ew ew ew ee 210

Elements of ASM-86 Assembly Language

8.1 ASM-86 Character Set .......2..2464 624640468 e-. 211
8.2 Tokens and Separators . . 1... 2 ee ee ew ew ew ew we 211
8.3 Delimiters . 2... 1 1 we we ee ee ee ee ew ee 211
8.4 Constants . . . 2. 2. 1 1 ew we we ee ww we ew ew 213

8.4.1 Numeric Constants ....... 6.664.246... 213
8.4.2 Character Strings .......... 6+... 214

8.5 Identifiers ........... 6.6.6... 2 6 « « 214

8.5.1 Keywords ..... oe ea i)
8.5.2 Symbols and Their Attributes oe ee ew ew ww ew 216

vii

---
_Page 009_

TABLE OF CONTENTS
(continued)

8.6 Operators . 1. 1 1 ee ee ee ee ee ew we ew we 218

8.6.1 Operator Examples ..... 2. ee ee © ee + 221
8.6.2 Operator Precedence ..... +. +s ee 2 «© © 223
8.7 Expressions . . . 6. 6 6 6 6 © © 6 8 8 ee ee ee 224

8.8 Statements ..... ee ee © © ew ew ew ee ew ew ww 225

Assembler Directives
9.1 Introduction .... 2. 2 ee ee ee ee ee we ew ee 227

9.2 Segment Start Directives . 2... 1 1 ee ee ew ew ee 227

9.2.1 The CSEG Directive . .... 2 «© © © © © @ © © 228
9.2.2 The DSEG Directive ..... +. +e «© © «© © © «© © 228
9.2.3 The SSEG Directive . .... «2. e © © «© © © © © 228
9.2.4 The ESEG Directive . .... 2 «© «© © © © © © © 229

9.3 The ORG Directive... . 2. 6 6 ee ee ee ee et we 229
9.4 The IF and ENDIF Directives . .... s+ 6 © «© «© «© + 230
9.5 The INCLUDE Directive... 1... 6 6 © ee ee we ee 230
9.6 The END Directive... . 2. 6. 6 ee ee ew ee ew ew ee 230
9.7 The EQU Directive . .. 2. 2 ee ee ee ee ew ee ee 231
9.8 The DB Directive ..... ee ee ee ee ee ew ee 231
9.9 DW Directive ..... ee eee ee ee ee ee ee 232
9.10 The DD Directive ... 2. 2. ee ee ee ee ew ee ee 232
9.11 The RS Directive .... 2. 6 ee ee ee ew ew ee ee 233
9.12 The RB Directive . . . ee ee ee et ee ee ee ee 233
9.13 The RW Directive . 2... ee ee ee ee ew ew ew ee 233
9.14 The TITLE Directive 2
9.15 The PAGESIZE Directive . we ee ew ee ww we 233

9.16 The PAGEWIDTH Directive se ee ew ew ee we ew ww ew 234

viii

---
_Page 010_

TABLE OF CONTENTS

(continued)

9.17 The EJECT Directive - 8 ee we lw

9.19 The NOLIST and LIST Directive .

The ASM-86 Instruction Set
10.1 Introduction ..........~.

10.2 Data Transfer Instructions ....

10.3 Arithmetic, Logical, and Shift Instructions

10.4 String Instructions .......
10.5 Control Transfer Instructions ..

10.6 Processor Control Instructions ..

Code-Macro Facilities

11.1 Introduction to Code-macros ...
11.2 Specifiers .......0..2..
11.3 Modifiers .........204.2..
11.4 Range Specifiers.......4..

11.5 Code-macro Directives ......

11.5.1 SEGFIX ..........

11.5.2 NOSEGFIX ........

11.5.3 MODRM...........

11.5.4 RELB and RELW.......

11.5.5 DB, DW and DD. ......,

11.5.6 DBIT .........20424
DDT-86

12.1 DDT-86 Operation .......4..

12.1.1 Invoking DDT-86 ......
-2 DDT-86 Command Conventions

12.1.3 Specifying a 20-Bit Address
-4 Terminating DDT-86 ....

ix

DDT-86 Operation with Interrupts

---
_Page 011_

12.2

No
.

G2

12.4

12.5

TABLE OF CONTENTS

(continued)

DDT-86 Commands . oe ee lel lel lll

12.2.1 The A (Assemble) Command ..... -
12.2.2 The D (Display) Command. . ce ee
12.2.3 The E (Load for Execution) Command .
12.2.4 The F (Fill) Command .....-.-.
12.2.5 The G (Go) Command... oe
12.2.6 The H (Hexadecimal Math) Command 7
12.2.7 The I (Input Command Tail) Command.
12.2.8 The L (List) Command .....+-.-e. >
12.2.9 The M (Move) Command .....+-.-s. +
12.2.10 The R (Read) Command ....-+-+-. -
12.2.11 The S (Set) Command ce ee ew ew
12.2.12 The T (Trace) Command - ee ew ew
12.2.13 The U (Untrace) Command - ee ee
12.2.14 The V (Value) Command - 8 ew ee ew
12.2.15 The W (Write) Command - ee ee
12.2.16 The X (Examine CPU State) Command °
3 Default Segment Values . . . »- + - «© s+ =
Assembly Language Syntax for A and L Commands

DDT-86 Sample Session . .. + + + + © + © =

---
_Page 012_

APPENDIXES

ASM-86 Invocation .........40 0 2 we ew ee ew ee 277

Mnemonic Differences from the Intel Assembler ... » 6 « 279

exe BF © 3 mw oA we Dp

2 £S= FP AR &

ASM~86 Hexadecimal Output Format .. 281
Reserved Words .........4.-. 285
ASM-86 Instruction Summary ..... 287
Sample Program. .......ee. 291
Code-macro Definition Syntax .... 297
ASM-86 Error Messages ....... 299
DDT-86 Error Messages ....... 301
TMP Listing ........88. . 303
ECHO Listing ........4..6.46-4 315
System Function Summary ...... 319
Glossary ...........2. 2. 331
ASCII and Hexadecimal Conversions . 329

xi

---
_Page 013_

SECTION 1

MP/M-86 SYSTEM OVERVIEW

1.1 Introduction

MP/M-86 is a microcomputer operating system that supports
multiple terminals with multi-programming at each terminal. MP/M-86
is compatible with the single-user operating system, CP/M-86™ In
addition, the system functions used by MP/M-86 to control the multi-
programming environment are available to application programs. Asa
result, MP/M-86 supports extended features such as communication
between and synchronization of independently running processes.

Under MP/M-86, there is an important distinction between a
program and a process. A program is simply a block of code residing
somewhere in memory or on disk; it is essentially static. A
process on the other hand, is dynamic, and can be thought of as a
“logical machine" that not only executes the program\*s code, but
also executes code in the operating system. When MP/M-86 loads a
Program, it also creates a process that is associated with the
loaded program. Subsequently, it is the process, rather than the
program that controls all access to the system’s resources. Thus,
MP/M-86 monitors the process, not the program. This distinction is
a subtle one, but vital to understanding the operation of the system
as a whole,

Processes running under MP/M-86 fall into two categories:
transient processes and MP/M-86 system processes (including Resident
System Processes). The first category consists processes that run
absolute memory images of programs the system loads from disk into
available memory partitions.

The second category consists of MP/M-86 system processes that
perform operating system tasks. For example, the IDLE process is a
pre-defined process that does not perform any task but gives the
system a process to execute when there are no other processes ready
to run.

Resident System Processes (RSPs) are those processes that can
be integrated into MP/M-86 during system generation, thus becoming a
part of the system. For example, the TERMINAL MESSAGE PROCESS
(TMP), is the system process that provides command line support for
system consoles under MP/M-86. With RSPs, users can write custom
processes and include them in the system along with those supplied
with MP/M-86 (see Section 1.8 and Section 5). Note: All processes
running under MP/M-86 compete for the CPU and other system resources
on a priority basis under control of the Real-Time Monitor.

---
_Page 014_

MP/M-86 Programmer®s Guide 1.1 Introduction

The following list briefly summarizes MP/M-86°s capabilities.

e Multi-terminal support. MP/M-86 supports up to 254
character I/O devices. These include consoles and list
devices. Although there is no set restriction on the
number of devices specified during system generation, a
typical number of system consoles would be 4 to 16. Also,
under MP/M-86 a single process can access multiple
terminals.

e Multi-programming at each terminal. Any system console can
initiate multiple programs. In addition, once a process is
initiated, it can can generate subprocesses.

e Inter-process communication, synchronization, and mutual
exclusion. These functions are provided by system queues.

e Logical interrupt mechanism using flags. This allows MP /M-
86 to interface with any physical interrupt structure.

e System timing functions. These functions enable processes
running under MP/M-86 to compute elapsed times, delay
execution for specified intervals, and to access and set
the current date and time.

e User-selected options at system generation time. The
available options include the number of system consoles and
list devices, the number, size, and location of memory
partitions, and the maximum number of locked files that can
be opened on the system at one time. Also, the user can
select which RSPs to include with MP/M-86 during system
generation.

Functionally, MP/M-86 is composed of several distinct modules.
They are: the Supervisor (SUP), the Real-Time Monitor (RTM), the
Memory Management module (MEM), the Character I/O module (CIO), the
Basic Disk Operating System (BDOS), and the Extended I/O System
(XIOS). The SUP module handles miscellaneous system functions such
as returning the version number or the address of the System Data
Area, and also calls other system functions when necessary. The RTM
module monitors the execution of running processes and arbitrates
conflicts for the system’s resources. The MEM module allocates and
frees memory upon demand from executing processes. The CIO module
handles all character I/O for console and list devices in the
system. The (BDOS) is the hardware-independent module that contains
the logically invariant portion of the file system for MP/M-86. The
BDOS file system is explained in detail in Section 2. The XIOS is
the hardware-dependent module that defines MP/M-86°s interface to a
particular hardware environment. Although a sample XIOS is supplied
by Digital Research, the XIOS is usually customized by an OEM or
distributer of MP/M-86 to support the user\*s local hardware
environment.

---
_Page 015_

MP/M-86 Programmer\*s Guide 1.1 Introduction

When MP/M-86 is configured for a single console and is
executing a single program, its speed approximates that of CP/M-86.
In environments where either multiple processes and/or users are
running, the speed of each individual process is degraded in
proportion to the amount of I/O and compute resources required. A
process that performs a large amount of I/O in proportion to
computing exhibits only minor speed degradation. This also applies
to a process that performs a large amount of computing, but is
running concurrently with other processes that are largely I/o-
bound. On the other hand, significant speed degradation occurs in
those environments where more than one compute-bound process is
running.

1.2 Supervisor (SUP)

The Supervisor module (SUP) Manages the interaction between
transient processes and the other System modules, including future
networking interfaces, All system function calls, whether they
originate from a transient process or internally from another system
module, go through a common table-driven function interface. The
SUP module handles all system functions that call other system
functions, such as the PROGRAM LOAD and CLI (COMMAND LINE
INTERPRETER) functions.

1.3 Real-Time Monitor (RTM)

MP/M-86 is controlled by a real-time multi-tasking nucleus
called the Real-Time Monitor (RTM). The RTM performs process
dispatching, queue management, flag management, device polling, and
system timing tasks. Many of the system functions used to perform
these tasks can also be called by user programs.

1.3.1 Process Dispatching

Although MP/M-86 is a multi-processing operating system, at any
given point in time, only one process has access to the CPU
resource. Unless it is specifically written to communicate or
Synchronize execution with other processes, it runs unaware that
other processes may be competing for the system\*’s resources.
Eventually, the system suspends the process from execution and
allows another process to run.

The primary task of the RT is transferring the CPU resource
from One process to another. This task is called dispatching and is
performed by a part of the RTM called the Dispatcher. Each process
running under MP/M-86 is associated with two data structures called
the Process Descriptor (PD) and the User Data Area (UDA). The
Dispatcher uses these data structures to save and restore the
current state of a running process. Each process in the system
resides in one of three states: ready, running, and suspended. A
ready process is one that is waiting for the CPU resource only. A
running process is one that the CPU is currently executing. A
Suspended process is one that is waiting for some other system

---
_Page 016_

MP/M-86 Programmer’s Guide 1.3 Real-Time Monitor

resource or a defined event.

A dispatch operation can be summarized as follows:

1) The Dispatcher suspends the process from execution and
stores the current state in the Process Descriptor and
UDA.

2) The Dispatcher scans all of the suspended processes on
the Ready List and selects the one with the highest
priority.

3) The Dispatcher restores the state of the selected
process from its Process Descriptor and UDA and gives it
the CPU resource.

4) The process executes until a resource is needed, a
resource is freed, or an interrupt occurs. At this
point, a dispatch occurs, allowing another process to
run. The system clock generates interrupts once every
clock tick (approximately 16ms) thereby generating time
slices for CPU-bound processes.

Only processes that are placed on the Ready List are eligible
for selection during dispatch. By definition, a process is on the
Ready List if it is waiting for the CPU resource only. Processes
waiting for other system resources cannot execute until their
resource requirements are satisfied. Under MP/M-86, a process is
blocked from execution if it is waiting for:

@ a queve message so it can complete a read queue operation.

@ space to become available in a queue so that it can
complete a queve write operation.

@e a system flag to be set.
e a console or list device to become available.

e a specified number of system clock ticks before it can be
removed from the system Delay List.

@ an I/O event to complete.

These situations are discussed in more detail in the following
sections.

MP/M-86 is a priority-driven system. This means that the
Dispatcher selects the highest priority ready process and gives it
the CPU resource. Processes with the same priority are "round-
robin" scheduled. That is, they are given equal shares of the

---
_Page 017_

MP/M-86 Programmer“s Guide 1.3 Real-Time Monitor

system’s resources. With priority dispatching, control is never
passed to a lower priority process if there is a higher priority
process on the Ready List. Since high priority compute-bound
processes tend to monopolize the CPU resource, it is advisable to
lower their priority to avoid degrading overall system performance.

MP/M-86 requires at least one process run at all times. To
ensure this, the system maintains the IDLE process on the Ready List
so it can be dispatched if there are no other processes available.
The IDLE process runs at a very low priority and is never blocked
from execution. It does not perform any useful task, but simply
gives the system a process to run when no other ready processes
exist.

1.3.2 Queue Management

Queues perform several critical functions for processes running
under MP/M-86. They are used for communicating messages between
processes, for synchronizing process execution, and for mutual
exclusion. Each system queue is composed of two parts: the Queue
Descriptor, and the Queue Buffer. These are special data structures
implemented in MP/M-86 as "memory files" that contain room for a
specified number of fixed length messages. Like files, queues are
made, opened, deleted, read from, and written to with appropriate
system function calls. When a queue is created by the MAKE QUEUE
function call, it is assigned an 8-character name that identifies
the queue in all the other function calls. As the name implies,
messages are read from a queue on a first-in, first-out basis.

A process can read messages from a queue or write messages toa
queue in two ways: conditionally or unconditionally. If no messages
exist in the queue when a conditional read is performed, or the
queue is full when a conditional write is performed, the system
returns an Error Code to the calling process. On the other hand, if
a process performs an unconditional read operation from an empty
queue, the system suspends the process from execution until another
process writes a message to the queue.

When more than one process is waiting for a message, preference
is given to the higher priority process. Conflicts involving
processes with the same priority are resolved on a first-come first-—
serve basis.

Mutual exclusion queues are a special type of queue under MP/M-
86. They contain one message of zero length and are typically
assigned a name beginning with the upper-case letters, MX. In
effect, a mutual exclusion queve is a binary semaphore. Mutual
exclusion queues ensure that only one process has access to a
resource at a time.

---
_Page 018_

MP/M-86 Programmer’s Guide 1.3 Real-Time Monitor

Access to a process protected by a mutual exclusion queue takes
place as follows:

1) The process issues an unconditional READ QUEUE call from
the queue protecting the resource, thereby suspending
itself until the message is available.

2) The process accesses the protected resource.

3) The process writes the message back to the queue when it
has finished using the protected resource, thus freeing
the resource for other processes.

As an example, the system mutual exclusion queue, MXdisk, ensures
that processes serially access the file system.

Mutual exclusion queues have one other feature that is
different from normal queues. When a process reads a message froma
mutual exclusion queue, the RTM saves queue and the address of the
Process Descriptor for the process reading the message. If the
process is aborted while it owns the mutual exclusion message, the
RTM automatically writes the message back to the queue for the
aborted process, thus enabling other processes to gain access to the
protected resource.

1.3.3 System Timing Functions

MP/M-86°s system timing functions include keeping the time of
day, and delaying the execution of a process for a specified period
of time. An internal process called CLOCK, provides the time of day
for the system. This process issues FLAG WAIT calls on the systems
"one second" flag, Flag 2. When the XIOS Interrupt Handler sets
this flag, it initiates the CLOCK process which then increments the
internal time and date. Subsequently, the CLOCK process makes
another FLAG WAIT call and suspends itself until the flag is set
again. MP/M-86 provides functions that allow the user to set and
access the internal date and time. In addition, the file system
uses the internal time and date to record when a file is updated,
created, or last accessed.

The DELAY function replaces the typical programmed delay loop
for delaying process execution. The DELAY function requires that
Flag 1, the system tick flag, be set approximately every 16
milliseconds (usually 60 times a second). When a process makes a
DELAY call, it specifies the number of ticks it is to be suspended
from execution. The system maintains the address of the Process
Descriptor for the process on an internal Delay List along with its
current delay tick count. Another system process, TICK, waits on
the tick flag and decrements this delay count on each system tick.
When the delay count goes to zero, the system removes the process
from the Delay List and places it on the Ready List.

---
_Page 019_

MP/M-86 Programmer’s Guide 1.4 Memory Module

1.4 Memory Module (MEM)

The Memory Module handles all memory management functions.
MP/M-86 2.0 supports an extended, fixed partition model of memory
management. In practice, the exact method that the operating system
uses to allocate and free memory is transparent to the programmer.
In fact, the programmer should take care to write code that is
independent of the memory management model by using only the MP/M-86
system functions as described in Section 6. If the system functions
are not used, incompatibilty may result since future versions of
MP/M-86 may support different versions of the Memory module
depending on the classes of memory Management hardware that are
available.

1.5 Character I/O module (CIO)

The Character I/O module handles all console and list T/o.
Under MP/M-86, every character I/O device is associated with a data
Structure called a Character Control Block (CCB). The CCB contains
the current owner, the root of a linked list of Process Descriptors
(PDs) that are waiting for access, line editing variables, and
status information. CCBs reside in the CCB Table of the System Data
Area. Each Process Descriptor contains the CCB Index of its default
console and list device. Consoles are mapped such that CCB Index 0
corresponds to console 0. List device CCBs start after the console
CCBs. The number of CCBs in the CCB Table is a system generation
option, and must be large enough to include all the console and list
devices supported in the xIOs.

1.6 Basic Disk Operating System (BDOS)

The MP/M-86 BDOS is an upward-compatible version of the single-
user CP/M-86 BDOS. It handles file creation and deletion, file
access, either sequential or random, and allocates and frees disk
Space. In most cases, CP/M-86 programs that make BDOS calls for I/o
can run under MP/M-86 without modification. MP/M-86°s BDOS is
extended to provide support for multiple console and list devices.
In addition, the file system is extended to provide services
required in multi-user environments. Two major extensions to the
file system are:

@ File locking. Normally, files opened under MP/M-86 cannot
be opened or deleted by other users. This feature prevents
accidental conflicts with other users.

e@ Shared access to files. As a special option, independent
users can open the same file in shared or unlocked mode.
MP/M-86 supports record locking and unlocking commands for
files opened in this mode, and protects files opened in
shared mode from deletion by other users.

---
_Page 020_

MP /M-86 Programmer’s Guide 1.7 Extended Input/Output System

1.7. Extended Input/Output System (XIOS)

The XIOS module is similar to the CP/M-86 Basic Input/Output
System (BIOS) module but is extended in several ways. Primitive
functions such as console I/O are modified to support multiple
consoles. Several new primitive functions support MP /M-86°s
additional features. Also, new facilities are added to eliminate
wait loops. Refer to the MP/M-86 System Guide for a detailed
description of the XIOS.

1.8 Resident System Processes

Resident System Processes are considered part of the operating
system. The system generation utility, GENSYS, prompts the user to
select which RSPs to include in the system. All RSPs selected are
placed next to each other immediately following the System Data Area
(SYSDAT). The MP/M-86 System Guide describes in greater detail the
manner in which the operating system modules reside in memory.

RSPs are permanently system resident, residing within the
Operating System area. Thus, if an RSP creates a queue or a
subprocess, the Process Descriptor, Queue Descriptor, and Queue
Buffer areas are usually used directly by the Operating System
instead of copying them into system tables. The only time these
areas are copied is when the data structures are actually outside
the 64K address space of the SYSDAT module. This is because all
pointers to these structures are relative to the SYSDAT segment
address.

1.9 Transient Programs

Under MP/M-86, a transient program is one that is not system
resident. That is, the system must load it from disk into an
available memory partition every time it executes. The command file
of a transient program is identified by a file type of CMD. When a
user enters a command at the console, the operating system searches
on disk for the appropriate CMD file which it then loads and
initiates. MP/M-86 supports three different execution models for
transient programs. These models are explained in detail in Section

1.10 Resident Procedure Library (RPL)

MP/M-86 supports a special type of RSP called a Resident
Procedure Library (RPL). RPLs provide a method of utilizing a block
of code as a system resource. A Resident Procedure Library is set
up by an RSP. For each library procedure, the process creates a
queue with the name of the RPL and sends it a single 4-byte message
containing the double-word address of the procedure (code) to be
accessed. Once this is accomplished, the RSP terminates itself.

---
_Page 021_

MP/M-86 Programmer’s Guide 1.10 Resident Procedure Library

The RPL is accessed by through the Function 151, CALL RPL.
This function opens the queue and reads the message to obtain the
actual memory address of the procedure. It then executes a Far Call
instruction to this address. Because only one message can reside in
the queue, only one process can gain access to the procedure until
the message is written back to the queue. Thus a process can
determine whether or not the procedure is used concurrently or
serially, by writing the message back to the queue just after entry,
or just prior to return. Once the procedure completes its intended
function, it executes a Far Return instruction back to the CALL RPL
routine, and finally back to the calling process.

1.11 System Function Calling Conventions

Under MP/M-86, when a process makes a system function call, it
uses the protocol shown in Table 1-1.

Table 1-1. Register Usage For System Function Calls

ENTRY PARAMETERS

Register CL: Function Number
DL: Byte Parameter
or
DX: Word Parameter
or
DX: Address - Offset
DS: Address — Segment

Register AL: Byte Return
or
AX: Word Return
or
AX: Address - Offset
ES: Address - Segment

BX: Same as AX
CX: Error Code

---
_Page 022_

MP/M-86 Programmers Guide 1.12 Error Handling

1.12 Error Handling

Most system functions return an Error Code to the calling
process. Under MP/M-86, the CX register is reserved as the Error
Code return register. Also under MP/M-86, there is one set of Error
Codes common to all functions except those in the BDOS module. The
BDOS functions have their own Error Codes which are explained in
Section 2.15. The Error Codes for the non-BDOS MP/M-86 system
functions are shown in Table 1-2.

---
_Page 023_

MP/M-86 Programmer’s Guide 1.12

Table 1-2. MP/M-86 Error Codes

0 NO ERROR

1 FUNCTION NOT IMPLEMENTED

2 ILLEGAL FUNCTION NUMBER

3 CAN“T FIND MEMORY

4 ILLEGAL SYSTEM FLAG NUMBER

5 FLAG OVERRUN

6 FLAG UNDERRUN

7 NO UNUSED QUEUE DESCRIPTORS
LEFT IN QD TABLE

8 NO UNUSED QUEUE BUFFER AREA LEFT

9 CAN\*T FIND QUEUE

10 QUEUE IN USE

1l QUEUE NOT ACTIVE

12 NO UNUSED PROCESS DESCRIPTORS
LEFT IN PD TABLE

13 QUEUE ACCESS DENIED

14 EMPTY QUEUE

15 FULL QUEUE

16 CLI QUEUE MISSING

17 NO QUEUE BUFFER SPACE

18 NO UNUSED MEMORY DESCRIPTORS
LEFT IN MD TABLE

19 ILLEGAL CONSOLE NUMBER

20 CAN°T FIND PD BY NAME

21 CONSOLE DOES NOT MATCH

22 NO CLI PROCESS

23 ILLEGAL DISK NUMBER

24 ILLEGAL FILE NAME

25 ILLEGAL FILE TYPE

26 CHARACTER NOT READY

27 ILLEGAL MEMORY DESCRIPTOR

28 BAD LOAD

29 BAD READ

30 BAD OPEN

31 NULL COMMAND

32 NOT OWNER

33 NO CODE SEGMENT IN LOAD FILE

34 ACTIVE PD

35 CAN\*T TERMINATE

36 CAN\*T ATTACH

37 ILLEGAL LIST DEVICE NUMBER

38 ILLEGAL PASSWORD

1l

Error Handling

---
_Page 024_

---
_Page 025_

SECTION 2

THE MP/M-86 FILE SYSTEM

2.1 File System Overview

The Basic Disk Operating System (BDOS) supports a named file
System on one to sixteen logical drives. Each logical drive is
divided into two regions: a directory area and a data area. The
directory area defines the files that exist on the drive and
identifies the data area space that belongs to each file. The data
area contains the file data defined by the directory. The directory
area is subdivided into sixteen logically independent directories,
which are identified by user numbers 0 through 15 respectively. In
general, only files belonging to the current user number are
"visible" in the directory. For example, the MP/M-86 DIR utility
only displays files belonging to the current user number.

The BDOS file system automatically allocates directory and
data area space when a file is created or extended and returns
previously allocated space to free space when a file is deleted. If
no directory or data space is available for a requested operation,
the BDOS returns an Error Code to the calling process. The
allocation and retrieval of directory and data Space is transparent
to the calling process. As a result, the user does not need to be
concerned with directory and drive organization when using the file
system functions.

An eight-character filename field and a three-character file
type field identifies each file ina directory. An eight-character
Password can also be assigned to a file to protect it from
unauthorized access. All system functions that involve file
Operations specify the requested file by the filename and type
fields. Multiple files can be specified by an ambiguous reference.
An ambiguous reference uses one or more "?" marks in the name or
type field to indicate that any character matches that position.
Thus, a name and type specification of all "?"~s (equivalent to a
command line file specification of "\*.\*") matches all files in the
directory that belong to the current user number.

The BDOS file system supports four categories of functions:
file access functions, directory functions, drive related functions,
and miscellaneous functions. The file access category includes
functions to make (create) a new file, open an existing file and
close an existing file. Both the MAKE FILE and OPEN FILE functions
activate the file for subsequent access by read and write functions.
After a file has been opened, subsequent BDOS functions can read or
write to the file, either sequentially or randomly by record
position. BDOS read and write commands transfer data in 128-byte
logical units, which is the basic record size of the file system.
The CLOSE FILE function performs two steps to terminate access to a
file, First, it indicates to the file system that the calling
process has finished accessing the file. The file then becomes

---
_Page 026_

MP/M-86 Programmer’s Guide 2.1 File System Overview

available to other processes. In addition, the function makes any
necessary updates to the directory to permanently record the current
status of the file.

BDOS directory functions operate on existing file entries in
a a@rive’s directory. This category includes functions to search for
one or more files, delete one or more files, rename a file, set file
attributes, assign a password to a file, and compute the size of a
file. The BDOS search and delete functions are the only functions
that allow ambiguous file references. All other directory and file
related functions require a specific file reference. The BDOS file
system does not allow a process to delete, rename, or set the
attributes of a file that is currently opened by another process.

BDOS drive-related functions include those which select a
drive as the default drive, compute a drive’s free space,
interrogate drive status and assign a Directory Label to a drive.
The Directory Label for a drive controls whether file passwords are
to be honored, and the type of date and time stamping to be
performed for files on the drive. Also included in this category
are functions to reset specified drives and to control whether other
processes can reset particular drives. When a drive is reset, the
next operation on the drive reactivates it by logging it in. The
function of the log-in operation is to initialize the drive for file
and directory operations. Under MP/M-86, a successful drive reset
operation must be performed on drives that support removeable media
before changing disks.

Miscellaneous functions include those that set the current
DMA address, access and update the current user number, chain to a
new program, and flush the internal blocking/deblocking buffer.
Also included are functions to set the BDOS Multi-Sector Count and
the BDOS Error Mode. The BDOS Multi-Sector Count determines the
number of 128-byte records to be processed by BDOS read, write,
record lock, and record unlock functions. It can range from one to
sixteen 128-byte records; the default value is one. The BDOS Error
Mode determines whether the BDOS file system intercepts errors or
returns all errors to the calling process.

The following list summarizes the operations performed by the
BDOS file system:

Disk System Reset

Drive Selection

File Creation

File Open

File Close

Directory Search

File Delete

File Rename

Random or Sequential Read

Random or Sequential Write
Interrogate Selected Disks
Set DMA Address

Set/Reset File Indicators

---
_Page 027_

MP/M-86 Programmer’s Guide 2.1 File System Overview

Reset Drive

Access/Free Drive

Random Write With Zero Fill
Lock and Unlock Record
Set Multi-Sector Count
Set BDOS Error Mode

Get Disk Free Space

Chain To Program

Flush Buffers

Set Directory Label
Return Directory Label
Read and Write File XFCB
Set/Get Date and Time

Set Default Password
Return BDOS Serial Number

The following sections contain information on important
topics related to the BDOS file system. The reader should be
familiar with the content of these sections before attempting to use
the system functions described individually in Section 6.

2.2 File Naming Conventions

Under MP/M-86, filenames consist of four parts: the drive
select code (d), the filename field, the file type field, and the
file password field. The general format for a command line file
specification is shown below:

{d:}f£ilename{.typ}{;password}

The drive select code field specifies the drive where the file is
located. The filename and type fields identify the file. The
password field specifies the password if a file is password
protected.

The drive, type, and password fields are optional and the
delimiters ":.;" are required only when specifying their associated
field. The drive select code can be assigned a value from "A" to
"P" where the actual drive codes supported on a given system are
determined by the XIOS implementation. When the drive code is not
Specified, the current default drive is indicated. The filename
field can contain one to eight non-delimiter characters, the file
type field, one to three non-delimiter characters, and the password
field, one to eight non-delimiter characters. All alphabetic
characters must be in upper-case. In addition, the PARSE FILENAME
function pads all three fields with blanks, if necessary. Omitting
the optional type or password fields implies a field specification
Of all blanks.

The PARSE FILENAME function recognizes certain ASCII

characters as valid delimiters when it parses a file from a command
line. The valid characters are shown in Table 2-1.

a

---
_Page 028_

MP/M-86 Programmer’s Guide 2.2 File Naming Conventions

Table 2-1. Valid Filename Delimiters

The PARSE FILENAME function also excludes all control characters
from the file fields and translates all lower-case letters to upper-
case.

The characters "(" and ")" should be avoided in filename and
type fields because they are commonly used delimiters. The
characters "\*" and "?" must not be used in filename and type fields
unless they are used to make an ambiguous reference. If the PARSE
FILENAME function encounters a "\*" in a file name or type fieid, it
pads the remainder of the field with "2" marks. For example, a
filename of "X\*.\*" is parsed to "X???????.2???". The BDOS search and
delete functions treat a "?" in the filename and type fields as
follows: A "?" in any position matches the corresponding field of
any directory entry belonging to the current user number. Thus, a
search operation for "X???????.?2??" finds all the current user files
on the directory beginning in "X". Most other file related BDOS
functions treat the presence of a"?" in the filename or type field
aS an error.

It is not mandatory to follow the file naming conventions of
MP/M-86 when creating or renaming a file with BDOS functions.
However, the conventions must be used if the file is to be accessed
from a command line. For example, the CLI function cannot locate a
command file in the directory if its filename or type field contains
a lower-case letter.

As a general rule, the file type field names the generic
category of a particular file, while the filename distinguishes
individual files in each category. Although they are generally
arbitrary, the file types listed below name some of the generic
categories that have been established.

---
_Page 029_

MP/M-86 Programmer\*s Guide 2.2 File Naming Conventions

ASM Assembler Source LIB Library File

BAK ED Source Backup LST List File

BAS Basic Source File PLI PL/I Source File

BRS 8080 Banked RSP File PRL Page Relocatable

CMD 8086 Command File REL Relocatable Module

COM 8080 Command File RSP Resident System Process
DAT Data File SPR Syetem Page Relocatable
HEX HEX Machine Code SYM SID Symbol File

H86 ASM-86 HEX File SYS System File

INT Intermediate File $$$ Temporary File

2.3 Disk Drive and File Organization

The BDOS file system can support from one to sixteen logical
drives, The maximum file size supported on a drive is 32 megabytes.
The maximum capacity of a drive is determined by the data block size
specified for the drive in the XIOS. The data block size is the
basic unit in which the BDOS allocates disk Space to files. Table
2-2 displays the relationship between data block size and drive
Capacity.

Table 2-2. Logical Drive Capacity

+------------------ +--+ ee +
| Data Block Size | Maximum Drive Capacity |
+------------------ +--------------~-------+- ++ +
1K 256 Kilobytes
2K 64 Megabytes
4k 128 Megabytes
8K 256 Megabytes
16K 512 Megabytes
tan n-ne +

Logical drives are divided into two regions: a directory
area and a data area. The directory area contains from one to
sixteen blocks located at the beginning of the drive. The actual
number is set in the XIOS. This area contains entries that define
which files exist on the drive. The directory entries corresponding
to a particular file define which data blocks in the drive’s data
area belong to the file. These data blocks contain the files
records. The directory area is logically subdivided into sixteen
independent directories identified as user 0 through 15. Each
independent directory shares the actual directory area on the drive.
However, a file\*s directory entries cannot exist under more than one
user number. In general, only files belonging to the current user
number are visible in the directory.

Each disk file consists of a set of up to 242,144 128-byte
records. Each record in a file is identified by its position in the
file. This position is called the record’s Random Record Number.
If a file is created sequentially, the first record has a position

---
_Page 030_

MP/M-86 Programmer’s Guide 2.3 Disk Drive and Organization

of zero, while the last record has a position one less than the
number of records in the file. Such a file can be read sequentially
in record position order beginning at record zero, or randomly by
record position. Conversely, if a file is created randomly, records
are added to the file by specified position. A file created in
this way is called "sparse" if positions exist within the file where
a record has not been written.

The BDOS automatically allocates data blocks to a file to
contain its records on the basis of the record positions consumed.
Thus, a sparse file that contains two records, one at position zero,
the other at position 242,143, would consume only two data blocks in
the data area. Sparse files can only be created and accessed
randomly, not sequentially. Note that any data block allocated toa
file is permanently allocated to the file until the file is deleted.
There is no other mechanism supported by the BDOS for releasing data
blocks belonging to a file.

Source files under MP/M-86 are treated as a sequence of ASCII
characters, where each "line" of the source file is followed by a
carriage-return line-feed sequence (0DH followed by OAH). Thus a
single 128-byte record could contain several lines of source text.
The end of an ASCII file is denoted by a 7Z (1AH) or a real end-of-
file, returned by the BDOS read operation. tZ characters embedded
within machine code CMD files are ignored. The end of file
condition returned by BDOS is used to terminate read operations.

2.4 File Control Block Definition

The File Control Block (FCB) is a data structure used with the
BDOS file access and directory functions. All of these functions
reference an FCB to determine the file or files to be operated on.
Certain fields in the FCB are also used for invoking special options
associated with some functions. Other functions use the FCB to
return data to the calling process. Most importantly, when a
process opens a file and subsequently accesses it with read, write,
lock, and unlock record functions, the BDOS file system maintains
the current file state and position within the user’s FCB. In
addition, all BDOS random I/O functions specify the Random Record
Number with a 3-byte field at the end of the FCB.

When making a file access or directory BDOS function call, a
process passes an FCB address. The address is composed of two
parts: register DX contains the offset, and DS contains the segment.
The length of the FCB data area depends on the BDOS function. For
most functions, the required length is 33 bytes. For random 1/0
functions and the COMPUTE FILE SIZE function, the FCB length must be
36 bytes. When either the BDOS OPEN or MAKE FILE functions specify
a file is to be opened in Unlocked Mode, the FCB must be 35 bytes in
length. The FCB format is shown below.

---
_Page 031_

MP/M-86\*Programmer’s Guide 2.4 File Control Block Definition

TT rr ee +
[ar|f1/£2|...
+--—-~—

£8/t1]t2|t3lex|sl|s2|rcldo|...

dnj[er|rO|rl]r2

were ee ee ee ee wee ee ne et

00 01 02 ... 08 09 10 11 12 13 14 15 16 ... 31 32 33 34 35

Figure 2-1. File Control Block Format

The fields in the FCB are defined as follows:

dr

fl...£8

tl,t2,t3

ex

cs

rs

re

do...dn

cr

r0,r1l,r2

Note: The 2-byte File ID is returned in bytes r0 and rl when a file

drive code (0 - 16).

0 => use default drive for file
1 => auto disk select drive A,
2 => auto disk select drive B,

16=> auto disk select drive P.

contain the filename in ASCII upper-case, with high bit
= 0. £1°, ..., £8° denote the high-order bit of these
positions, and are file attribute bits.

contain the file type in ASCII upper-case, with high
bit = 0. t1°, t2\*, and t3\* denote the high bit of
these positions, and are file attribute bits.

tl” = 1 => Read/Only file,
t2° = 1 => System file,
t3\* = 1 => File has been archived.

contains the current extent number, normally set to 0
by the calling process, but can range 0 - 31 during
file I/O.

contains the FCB checksum value for open FCBs.

reserved for internal system use, set to zero on call
to OPEN, MAKE, SEARCH.

record count for extent "ex" takes on values from 0 -
128,

filled-in by MP/M-86, reserved for system use.

current record to read or write in a sequential file
operation, normally set to zero by the calling process
when a file is opened or created.

optional Random Record Number in the range 0-242,143 (0
- 3FFFFH). ro,rl,r2 constitute a 18-bit value with
low byte r0, middle byte rl, and high byte r2.

1s successfully opened in Unlocked Mode (see Section 2.9)

---
_Page 032_

MP/M-86 Programmer”s Guide 2.4 File Control Block Definition

For BDOS directory functions, the calling process must
initialize bytes 0 through ll of the FCB before issuing the function
call. The SET DIRECTORY LABEL and WRITE FILE XFCB functions also
require the calling process to initialize byte 12. The BDOS RENAME
FILE function requires the calling process to place the new file
name and type in bytes 17 through 27.

BDOS OPEN or MAKE FILE function calls require the calling
process to initialize bytes 0 through 12 of the FCB before issuing
an OPEN FILE or MAKE FILE function call. Normally, byte 12 is set
to zero, In addition, if the file is to be processed from the
beginning using sequential read or write functions, byte 32 (cr)
must be zeroed. After an FCB is activated by an open or make
operation, the FCB should not be modified by the user. Open FCBs
are checksum verified to protect the integrity of the file system.
In general, if a process modifies an open FCB, the next read, write,
or close function call will return with a checksum error (see
Section 2.9 for more on FCB checksums). Normally, sequential read
or write functions do not require initialization of an open FCB.
However, random I/O functions require that a process set bytes 33
through 35 to the requested Random Record Number prior to making the
function call.

File directory elements maintained in the directory area of
each disk drive have the same format as FCBs (excluding bytes 32
through 35), except for byte 0 which contains the file”’s user
number. Both the OPEN FILE and MAKE FILE functions bring these
elements (excluding byte 0) into memory in the FCB specified by the
calling process. All read and write operations on a file must
specify an FCB activated in this manner. Otherwise, a checksum
error is returned. The BDOS updates the memory copy of the FCB
during file processing to maintain the current position within the
file. During file write operations, the BDOS updates the memory
copy of the FCB to record the allocation of data to the file, and at
the termination of file processing, the CLOSE FILE function
permanently records this information on disk. Note that data
allocated to a file during file write operations is not completely
recorded in the directory until the the calling process issues a
CLOSE FILE call. Therefore, it is mandatory that a process which
creates or modifies files, close the files at the termination of any
write processing. Otherwise, data may be lost.

As a general rule under MP/M-86, a process should close files
as soon as they are no longer needed, even if they have not been
modified. The BDOS file system maintains an entry in the system
Lock List for each file opened by each process on the system. This
entry is not removed from the system Lock List until the file is
closed or the process owning the entry terminates. The BDOS file
system uses this entry to prevent other processes from accessing the
file unless the file was opened in a mode that supports shared
access. Normally, a process must close a file before other
processes on the system can access the file.

---
_Page 033_

MP/M-86 Programmer”s Guide 2.4 File Control Block Definition

Keep in mind that the space in the system Lock List is a
limited resource under MP/M-86. If a process attempts to open a
file and no space exists in the system Lock List, or the process
exceeds the process open file limit (specified during system
generation), the BDOS denies the open operation and usually aborts
the calling process.

The high-order bits of the FCB filename (f1\*,...,£8°) and type
(t1\*,t2\*,t3°) fields are called attribute bits. Attributes bits are
l-bit boolean fields where 1 indicates on or true, and 0 indicates
off or false. Attribute bits have two functions within the file
System: as file attributes and interface attributes.

The file attributes (f1°,...,f4° and t1\*,t2\*,t3\*) are used to
indicate that a file has a defined attribute. These bits are
recorded in a file\*s directory FCBs. File Attributes can only be
set or reset by the BDOS SET FILE ATTRIBUTES function. When the
BDOS MAKE FILE function creates a file, it initializes all file
attributes to zero. A process can interrogate file attributes in an
FCB activated by the BDOS OPEN FILE function or in directory FCBs
returned by the BDOS SEARCH FOR FIRST and SEARCH FOR NEXT functions.
Note: the BDOS file system ignores the file attribute bits when it
attempts to locate a file in the directory.

The file attributes (t1\*,t2\*,t3\*) are defined by the file
system as follows:

tl1\*: Read/Only attribute
This attribute, if set, prevents write operations to a file.
t2°: System Attribute

This attribute, if set, identifies the file as a MP/M-86 system
file. System files are not normally displayed by the MP/M-86 DIR
utility. In addition, user-zero system files can be accessed on a
read/only basis from other user numbers (see Section 2.5).

t3°: Archive Attribute

This attribute is designed for user-written archive programs.
When a archive program copies a file to backup storage, it sets the
archive attribute of the copied files. The file system
automatically resets the archive attribute of a directory FCB that
has been issued a write command. The archive program can test this
attribute in each of the file\*s directory FCBs via the BDOS SEARCH
FOR FIRST and SEARCH FOR NEXT functions. If all directory FCBs have
the archive attribute set, it indicates that the file has not been
modified since the previous archive. Note that the MP/M-86 PIP
utility supports file archival.

Attributes f£1° through f£4° are available for definition by the user.

---
_Page 034_

MP/M~86 Programmer®s Guide 2.4 File Control Block Definition

The interface attributes are £5° through f8°. These attributes
cannot be used as file attributes. Interface attributes £5 and £6\*
are used to request options for BDOS calls requiring an FCB address
in register DX. They are used by the BDOS OPEN, MAKE, CLOSE, and
DELETE FILE functions. Table 2-3 shows the £5\* and f£6\* interface
attribute definitions for these functions.

Table 2-3. BDOS Interface Attributes

foo --------------- po--- = -- - ne +
| Function | Attribute |
po----------- = -- 5 - ee rrr +
OPEN FILE £5° = 1 : Open in Unlocked Mode
£6° = 1 : Open in Read/only Mode
MAKE FILE £5° = 1 : Open in Unlocked Mode
£6° = 1: Assign password to file
CLOSE FILE f£5° = 1: Partial Close
DELETE FILE £5° = 1: Delete file XFCBs only
poa---------- nn nent +

The interface attributes are discussed in detail for each of the
above functions in Section 6. Attributes f£5°\* and f£6° are always
reset when control is returned to the calling process. Interface
attributes £7\* and £8° are reserved for internal use by the BDOS

file system. |

The BDOS search and delete functions allow multiple file
(ambiguous) reference. In general, a ? mark in the filename, type,
or extent field matches any value in the corresponding positions of
directory FCBs during a directory search operation. The BDOS search
functions also recognize a ? mark in the drive code field, and if
specified, they return all directory entries on the disk regardless
of user number including empty entries. A directory FCB beginning
with E5H is an empty directory entry.

2.5 User Number Conventions

The MP/M-86 User facility divides each drive directory into
sixteen logically independent directories, designated as user 0
through user 15. Physically, all user directories share the
directory area of a drive. In most other aspects, however, they are
independent. For example, files with the same name can exist on
different user numbers of the same drive with no conflict. However,
a single file cannot reside under more than one user number.

Only one user number is active for a process at one time, and
the current user number applies to all drives on the system.
Furthermore, the FCB format does not contain any field that can be
used to override the current user number. As a result, all file and
directory operations reference directories associated with the
Current user number. However, it is possible for a process to

---
_Page 035_

MP/M-86 Programmer’s Guide 2.5 User Number Conventions

access files on different user numbers by setting the user number to
the file”s user number with the SET/GET USER function prior to
issuing the desired BDOS function call for the file. Note that this
technique must be used carefully. If a process attempts to read or
write to a file under a user number that is not the same as the user
number that was active when the file was opened, the BDOS file
system returns a FCB checksum error.

When the CLI function initiates a transient process or RSP, its
user number is set to the default value established by the process
issuing the CLI function call. Normally, the sending process is the
TMP. However, the sending process may be another process such as a
transient program that makes a BDOS CHAIN TO PROGRAM call. A
transient program can change its user number by making a SET/GET
USER function call. Changing the user number in this way does not
affect the command line user number displayed by the TMP. Thus,
when a transient process that has changed its user number
terminates, the original user number for the console is restored
when the TMP regains control.

User 0 has special properties under MP/M-86. With some
restrictions, the file system automatically opens a file under user
zero, if it is not present under the current user number. of
course, this action is only performed when the current user number
is not zero. In addition, a file on user zero must have the system
attribute (t2°) set to be eligible for this operation. This
procedure allows utilities that may include overlays and any other
commonly accessed files to be placed on user zero, but be available
for access from other user numbers. As a result, it eliminates the
need for copying commonly needed utilities to all user numbers ona
directory, and gives the MP/M-86 manager control over which user-
zero files are directly accessible from other user numbers. Refer
to Section 2.8 for more information on this topic.

2.6 Directory Labels and XFCBs

The BDOS file system includes two special types of FCBs, the
XFCB and the Directory Label. The XFCB is an "extended" FCB that
can optionally be associated with a file in the directory. If

present, it contains the file\*s password field and date and time
stamp information. The format of the XFCB is shown below:

ee ew re sn en re re ee ee re ne ne es se re ee ee ee ee ee ee re ee ee eee we es oe

Figure 2-2. XFCB Format

---
_Page 036_

MP/M-86 Programmer’s Guide 2.6 Directory Labels and XFCBs

The fields in the XFCB are defined as follows:

dr - drive code (0 - 16)
file ~ filename field
type - file type field

pm - password mode

bit 7 - Read Mode
bit 6 - Write Mode
bit 5 - Delete Mode

(bit references are right to left, relative to 0)

$1,s2,rc - reserved for system use

password - 8-byte password field (encrypted)

tsl - 4-byte creation or access time stamp field
ts2 - 4-byte update time stamp field

An XFCB can be created for a file in two ways: automatically,
as part of the BDOS MAKE FILE function or explicitly, by the BDOS
function, WRITE FILE XFCB. The BDOS file system does not
automatically create an XFCB for a file unless a Directory Label is
present on the file\*s drive. The BDOS READ FILE XFCB function
returns a file’s XFCB if it exists in the directory. Note that in
the directory, an XFCB is identified by a drive byte value (byte 0
in the FCB) equal to 16 + N, where N equals the user number.

The Directory Label specifies for a drive if passwords for
password protected files are to be required, if date and time
stamping for files is to be performed, and if XPCBs are to be
created automatically for files by the MAKE FILE function. The
format of the Directory Label is similar to that of the XFCB as
shown below:

Figure 2-3. Directory Label Format

dr - drive code (0 - 16)

name - Directory Label name

type - Directory Label type

dl - Directory Label data byte

bit 7 - require passwords for files
bit 6 - perform access time stamping
bit 5 - perform update time stamping
bit 4 - Make creates XFCBsS

bit 0 - Directory Label exists

(bit references are right to left, relative to 0)

sl,s2,re - nfa

password - 8-byte password field (encrypted)
tsl - 4-byte creation time stamp field
ts2 - 4-byte update time stamp field

---
_Page 037_

MP/M-86 Programmer”s Guide 2.6 Directory Labels and XFCBs

Only one Directory Label can exist in a drive’s directory. The
Directory Label name and type fields are not used to search for a
Directory Label in the directory; they can be used to identify a
diskette or a drive. A Directory Label can be created or its fields
can be updated by the BDOS function, SET DIRECTORY LABEL. This
function can also assign a Directory Label a password. The
Directory Label password, if assigned, cannot be circumvented,
whereas file password protection is an option controlled by the
Directory Label. Thus, access to the Directory Label password
provides a kind of super-user status for that drive.

Note: The BDOS file system provides no function to read the
Directory Label FCB directly. However, the Directory Label data
byte can be read directly with the BDOS function, RETURN DIRECTORY
LABEL. In addition, the BDOS search functions (\*?\* in FCB drive
byte) can be used to find the Directory Label on the default drive.
In the directory, the Directory Label is identified by a drive byte
value (byte 0 in the FCB) equal to 32 (20H).

2.7 File Passwords

Files may be assigned passwords in two ways: by the MAKE FILE
function if the Directory Label specifies automatic creation of
XFCBs or by the WRITE FILE XFCB function. A file\*s password can
also be changed by the WRITE FILE XFCB function if the original
password is supplied. However, a file\*s password cannot be changed
without the original password even when password protection for the
drive is disabled by the Directory Label.

Password protection is provided in one of three modes. Table

2-4 shows the difference in access level allowed to BDOS functions
when the password is not supplied.

Table 2-4. Password Protection Modes

Password Access level allowed when the password
Mode is not supplied.
“l. Read | The file cannot be read, modified, or deleted.
2. Write The file can be read but not modified, or deleted.
3. Delete The file can be read and modified, but not deleted.

If a file is password protected in Read Mode, the password must be
supplied to open the file. A file protected in Write Mode cannot be
written to without the password. A file protected in Delete Mode
allows read and write access, but the user must specify the password
to delete the file, rename the file, or to modify the file’s
attributes. Thus, password protection in mode 1 implies mode 2 and

---
_Page 038_

MP/M-86 Programmer\*’s Guide 2.7 File Passwords

3 protection, and mode 2 protection implies mode 3 protection. All
three modes require the user to specify the password to delete the
file, rename the file, or to modify the file”s attributes.

If the correct password is supplied, or if password protection
is disabled by the Directory Label, then access to the BDOS
functions is the same as for a file that is not password protected.
In addition, the SEARCH FOR FIRST and SEARCH FOR NEXT functions are
not affected by file passwords. Table 2-5 lists the BDOS functions
that test for password.

Table 2-5. BDOS Functions That Test For Password

15. OPEN FILE

19. DELETE FILE

23. RENAME FILE

30. SET FILE ATTRIBUTES
100. SET DIRECTORY LABEL
103. WRITE FILE XFCB

File passwords are eight bytes in length. They are maintained
in the XFCB and Directory Label in encrypted form. To make a BDOS
function call for a file that requires a password, a process must
place the password in the first eight bytes of the current DMA or
specify it with the BDOS function, SET DEFAULT PASSWORD, prior to
making the function call. Note: the BDOS maintains the assigned
default password on a system console basis and retains it across
process termination.

2.8 File Date and Time Stamps

The BDOS file system can record when a file was created or last
accessed, and/or last updated. It records the creation stamp only
when an XFCB is automatically created by the MAKE FILE function. If
an XFCB is created by the MAKE FILE XFCB function, the creation
stamp is set to zero. The CLOSE FILE function makes the update
stamp if a write operation is made to the file while the file is
open. The OPEN FILE function makes the access stamp if the file is
successfully opened. The creation date stamp is overwritten when
access stamping is performed because only two date and time fields
reside in the XFCB and the access and creation time stamps share the
same field.

The drive’s Directory Label determines the type of date and
time stamping supported for files on a drive. If a drive does not
have a Directory Label, or if it is read/only, or if the drive’s
Directory Label does not specify date and time stamping, then no
date and time stamping for files is performed. In addition, a file
must have an XFCB to be eligible for date and time stamping. For
the Directory Label itself, time stamps record when it was created
and last updated. No access stamping for Directory Labels is

---
_Page 039_

MP/M-86 Programmer’s Guide 2.8 File Date and Time Stamps

supported.

A process can directly access the date and time stamps for a
file by using the READ FILE XFCB function. No mechanism is provided
to directly update XFCB date and time fields.

The BDOS file system uses the MP/M-86 internal date and time
when it records a date and time stamp. The MP/M-86 TOD utility can
be used to set the system date and time.

2.9 Pile Open Modes

The BDOS file system provides three different modes of opening
files. They are defined as follows:

Locked Mode:

A process can open a file in Locked Mode only if the file is
not currently opened by another process. Once open in Locked Mode,
no other process can open the file until it is closed. Thus, if a
process successfully opens a file in Locked Mode, that process in
effect owns the file until the file is closed or the process
terminates. Files opened in Locked Mode support read and write
operations unless the file is a read/only file (attribute tl’ set)
or the file is password protected in Write mode and the password is
not supplied with the BDOS OPEN FILE call. In both of these cases,
only read operations to the file are allowed. Note: Locked Mode is
the default mode for opening files under MP/M-86.

Unlocked Mode:

A process can open a file in Unlocked Mode if the file is not
currently open, or if the file has been opened by another process in
Unlocked Mode. This mode allows more than one process to open the
same file. Files opened in Unlocked Mode support read and write
Operations unless the file is a read/only file (attribute tl’ set)
or the file is password protected in Write mode and the password is
not supplied with the BDOS OPEN FILE call. However, when a file
opened in Unlocked Mode is extended by a write operation, the BDOS
allocates space to the file in data block units, not in 128-byte
record units as is normally the case. The BDOS record locking and
unlocking functions are only supported for files opened in Unlocked
Mode.

When opening a file in Unlocked Mode, a process must reserve 36
bytes in the FCB, because the OPEN FILE function returns a 2-byte
value called the File ID in the r0 and rl bytes of the FCB. The
File ID is a required parameter for the BDOS record lock and record
unlock commands.

---
_Page 040_

MP/M-86 Programmer’s Guide 2.9 File Open Modes

Read/only Mode:

A process can open a file in Read/only Mode if the file is not
currently opened by another process, or the file has been opened by
another process in Read/only Mode. This mode allows more than one
process to open the same file for read/fonly access.

The OPEN FILE function performs the following steps for files
opened in Locked or Read/only Mode. If the current user is non-
zero, and the file to be opened does not exist under the current
user number, the OPEN FILE function searches user zero for the file.
If the file exist, under user zero and the file has the system
attribute (t2\*) set, the file is opened under user zero. The open
mode is automatically forced to Read/only when this is done. For
more information on this, refer to Section 2.5.

The OPEN FILE function also performs the following action for
files opened in Locked Mode when the current user number is zero.
If the file exists under user zero and has the system (t2%) and
read/fonly (tl\*) attributes set, the open mode is automatically set
to Read/only. Thus, the read/only attribute controls whether a
user-zero system file can be concurrently opened by a uSer-zero
process and processes on other uSer numbers when each process opens
the file in the default Locked Mode. If the read/only attribute is
set, all processes open the file in Read/only Mode and concurrent
access of the file is allowed. However, if the read/only attribute
is reset, the user-zero process opens the file in Locked Mode. Tf
it successfully opens the file, no other process can open it. If
another process has the file open, its open operation is denied.

Table 2-6 shows the definition of the FCB interface attributes
£5° and f6\* for the BDOS OPEN FILE function.

Table 2-6. FCB Interface Attributes F5° F6\*
OPEN FILE Function

po—-- + err
£5°\* = 0, £6° = 0 - open in Locked Mode (default mode)
f£5\* =1, f6\* = 0 - open in Unlocked Mode
£5° = 0 or 1, £6\* = 1 - open in Read/only Mode

poi ee ee ee +

Interface attribute £5\* designates the open mode for the BDOS MAKE
FILE function. Table 2-7 shows the definition of the FCB interface
attribute £5° for the MAKE FILE function.

---
_Page 041_

MP/M-86 Programmer’s Guide 2.9 File Open Modes

Table 2-7. FCB Interface Attribute F6”
MAKE FILE Function

£5° = 0 - open in Locked Mode (default mode)
= 1 - open in Unlocked Mode

Note: the MAKE FILE function does not allow opening the file in
Read/only Mode.

2.10 File Security

In general, the security measures implemented in the BDOS file
system are intended to prevent accidental collisions between running
processes, It is not possible to provide total security under MP/M-
86 because the BDOS file system maintains file allocation
information in open FCBs in the user’s memory region, and MP/M-86
does not support memory protection. In the worst case, a program
that "crashes" on MP/M-86 can take down the entire system.
Therefore, MP/M-86 requires that all processes running on the system
be "friendly." However, the BDOS file system is designed to ensure
that multiple processes can share the same file system without
interfering with each other. It does this in two ways:

e@e it performs checksum verification of open FCBs.

@ it monitors all open files and locked records via the
system Lock List.

User FCBs are checksum validated before I/O operations to
protect the integrity of the file system from corrupted FCBs. The
OPEN FILE and MAKE FILE functions compute and assign checksums to
FCBs. The READ, WRITE, LOCK RECORD, UNLOCK RECORD and CLOSE FILE
functions subsequently verify and recompute the checksums when the
FCB changes, If the BDOS detects an FCB checksum error, it does not
perform the requested command. Instead, it either terminates the
calling process with an error, or if the process is in BDOS Return
Error Mode (see Section 2.15), it returns to the process with an
Error Code.

The system Lock List is established during the system
generation process at which time the user can establish the size of
the list and also define limits for the number of files a single
process can open and the number of records a single process can
lock. Each time a process opens a file or locks a record
successfully, the BDOS file system allocates an entry in the system
Lock List to record the fact. The file system uses this information
to:

Oo prevent a process from deleting, renaming, or updating the
attributes of another process’s open file.

---
_Page 042_

MP/M-86 Programmer’s Guide 2.10 File Security

@ prevent a process from opening a file currently opened by
another process unless both processes open the file in
Locked or Read/only Mode.

@ prevent a process from resetting a drive on which another
process has an open file.

@ prevent a process from locking or updating a record
currently locked by another process. Refer to Section 2.11
for more information on record locking and unlocking.

For reasons of efficiency, the file system verifies only for certain
functions whether another process has the FCB specified file open.
These functions are: OPEN FILE, MAKE FILE, DELETE FILE, RENAME
FILE, and SET FILE ATTRIBUTES. For open FCBs, the FCB checksum
controls whether the process can use the FCB. By definition, a
valid FCB checksum implies that the file has been successfully
opened and an entry for the file resides in the system Lock List.
When a process closes a file permanently, the file system removes
the file from the system Lock List and invalidates its FCB checksum
field.

There are several other situations where the file system
removes open file entries from the system Lock List for a process.
For example, if a process makes a delete call fora file that it has
open in Locked Mode, the file system deletes the file and also
removes the file’s entry from the system Lock List. Deleting an
open file is not recommended practice under MP/M-86 but is supported
for files opened in Locked Mode (the default open mode), to provide
compatibility with software written under earlier releases of MP/M
and CP/M. Note that the file system does not delete a file opened
in Unlocked or Read/only Mode.

To ensure that the process does not use the FCB corresponding
to the deleted file, the file system subsequently checks all open
FCBs for the process to ensure that a Lock List item exists for the
FCB. Each open FCB is checked the next time it is used. If a Lock
List entry exists for the file, the operation is allowed to proceed.
Otherwise, a FCB checksum error is returned.

The file system performs this verification of open FCBs for all
situations where it purges an open file entry from the system Lock
List. The following list describes these situations:

e@ A process deletes a file it has open in Locked Mode.

e A process renames a file it has open in Locked Mode.

e A process updates the attributes via the BDOS SET FILE
ATTRIBUTES command of a file it has open in Locked Mode.

e A process issues a FREE DRIVE call for a drive on which it
has an open file.

---
_Page 043_

MP/M-86 Programmer”s Guide 2.10 File Security

@ A change in media is detected on a drive that has open
files. This situation is a special case because a process
Cannot control whether it occurs and it can impact more
than one process. Refer to Section 2.13 for more
information on this situation.

The automatic verification of open FCBs by the file system
after it purges a file entry from the system Lock List can affect
performance. Each verification requires a directory search
operation. Therefore, it is strongly recommended that these
Situations be avoided in new programs developed for MP/M-86,

2.11 Concurrent File Access

More than one process can access the same file if each process
opens the file in the same shared access mode. BDOS supports two
Shared access modes, Unlocked and Read/only. Read/only Mode is
functionally identical to the default Locked Mode except that more
than one process can access the file and no process can change it.
Files opened in Unlocked Mode present a more complex situation
because a file opened in this mode can be modified by multiple
processes concurrently. As a result, Unlocked Mode differs in some
important ways from the other open modes,

When a process opens a file in Unlocked Mode, the file system
returns a 2-byte field called the File ID in the rO and rl bytes of
the FCB. The File ID is a required parameter of the BDOS LOCK
RECORD and UNLOCK RECORD functions.

The file system supports two mechanisms that allow processes to
coordinate update operations on files open in Unlocked Mode. The
record locking and unlocking functions allow a process to establish
and relinquish temporary ownership of particular records. A record
lock does not prevent another process from reading the locked
record; only write and lock operations for other processes are
intercepted. As an alternative, the TEST AND WRITE RECORD function
verifies the current contents of a record before allowing the write
Operation to proceed.

The record locking and unlocking functions and the TEST AND
WRITE RECORD function provide two fundamentally different approaches
to record update coordination. When a record is locked, the file
system allocates an entry in the system Lock List, identifying the
locked record and associating it with the calling process. The
UNLOCK RECORD function removes the locked entry from the list.
While the locked record’s entry exists in the system Lock List, no
other process can lock or write to that record. Because the system
Lock List is a limited resource under MP/M, a process is restricted
regarding the number of records it can lock.

The TEST AND WRITE RECORD function, on the other hand, performs

its verification at the I/O level. In a single indivisible
operation, it verifies that the user’s current version of the record

---
_Page 044_

MP /M-86 Programmer’s Guide 2.11 Concurrent File Access

matches the version on disk before allowing the write operation to
proceed. As a result, it is not restricted like the LOCK RECORD
function. However, record update coordination can usually be
performed more efficiently with the lock functions.

The BDOS file system performs additional steps for read and
write operations to a file open in Unlocked Mode. These added steps
are required because the BDOS file system maintains the current
state of an open file in the user’s FCB. When multiple processes
have the same file open, FCBs for the same file exist in each
processes” memory. To ensure that all processes have current
information, the file system updates the directory immediately when
an FCB for an unlocked file is changed. In addition, the file
system verifies error situations such as end-of-file or reading
unwritten data with the directory before returning an error. As a
result, read and write operations are less efficient for files open
in Unlocked Mode when compared to equivalent operations for files
opened in the default Locked Mode.

Extending a file is also a special situation for files opened
in Unlocked Mode. Normally, when a file is extended, the size of
the file is set to the Random Record Number of the last record + l.
However, when a file opened in Unlocked Mode is extended, the size
of the file is set to the Random Record Number + 1 of the last 128-
byte record in the file\*’s last data block. A process must keep
track of the actual last record of a file extended while open in
Unlocked Mode, if that is required.

2.12 Multi-Sector I/O

The BDOS file system provides the capability to read or write
multiple 128-byte records in a single BDOS function call. This
multi-sector facility can be visualized as a BDOS "burst" mode,
enabling a process to complete multiple 1/0 operations without
interference from other running processes. The use of this facility
in an application program can improve its performance, and also
enhance overall system throughput. For example, the PIP utility
performs its sequential I/O with a Multi-Sector Count of 8. Multi-
sector I/O has its greatest impact, however, in the performance of
sequential I/O processing on MP/M-86 systems that support record
blocking/deblocking in their XIOS. Improved performance is achieved
by eliminating the need for a large percentage of XIOS physical
record pre-read operations.

The number of records that can be supported with multi-sector
I/O ranges from one to sixteen. For transient programs, the default
value is one because the CLI function initializes the Multi-Sector
Count of a transient program to one when it initiates the program.
The BDOS SET MULTI-SECTOR COUNT function can be used to set the
count to another value.

The Multi-Sector Count determines the number of operations to
be performed by the following BDOS functions:

---
_Page 045_

MP/M-86 Programmer’s Guide 2.12 Multi-Sector I/0

@ Sequential Read and Write functions

@ Random Read and Write functions including WRITE WITH ZERO
FILL and TEST AND WRITE RECORD

@ LOCK RECORD and UNLOCK RECORD

If the Multi-Sector Count is N, calling one of the above functions
is equivalent to making N function calls. If a multi-sector I/O
operation is interrupted with an error, the file system returns the
number of 128-byte records successfully processed in the high-order
nibble of register BH.

2.13 XIOS Blocking and Deblocking

An optional physical record blocking and deblocking facility
can be implemented as part of the XIOS when it is necessary to
maintain physical records on disk in units greater than 128-bytes.
In general, record blocking and deblocking in the XIOS is
transparent to the BDOS file system as well as to programs that make
BDOS file system calls.

If this facility is implemented, then the XIOS sends data toor
receives data from the BDOS file system in logical 128-byte records,
but accesses the disk with a larger physical record size. The XIOS
uses an internal physical record buffer equal in size to the
physical record size to buffer logical records. The process of
building up physical records from 128-byte logical records is called
blocking, and it is required for BDOS write operations. The reverse
Process is called deblocking and it is required for BDOS read
operations. For BDOS write operations, the XIOS postpones the
physical write operation for permanent drives (see Section 2.14) if
the write operation is not to the directory. For BDOS read
operations, the XIOS performs a physical read only if the current
physical record buffer does not contain the requested logical
record. In addition, if the physical record is "pending" as the
result of a previous write operation, the XIOS performs a physical
write operation prior to the read operation.

Postponing physical record write operations has implications
for some application programs. For those programs that involve file
updating, it is often critical to guarantee that the state of a file
on disk parallels the state of the file in memory after updating the
file. This is only an issue for systems that implement blocking and
deblocking because of the postponement of physical write operations.
If the system should crash while the physical buffer is pending,
data would be lost. To prevent this, the BDOS FLUSH BUFFERS
function can be invoked to force the write of any pending physical
buffers in the XIOS.

Note: The system automatically calls this function when a process

terminates. In addition, the BDOS file system automatically makes a
FLUSH BUFFERS call in the CLOSE FILE function.

---
_Page 046_

MP/M-86 Programmer’s Guide 2.14 Reset, Access and Free Drive

2.14 Reset, Access and Free Drive

The BDOS functions DISK SYSTEM RESET, RESET DRIVE, ACCESS
DRIVE, and FREE DRIVE allow a process to control when a drive's
directory is to be reinitialized for file operations. When MP /M-86
is initiated by MPMLDR, all drives are initialized to the reset
state. Subsequently, as drives are referenced, they are
automatically logged-in by the file system. The log-in operation
initializes the drive for BDOS file operations. In general, once a
drive is logged-in, it is not necessary to relog the drive unless a
disk media change is to be made. However, MP/M-86 requires that a
successful drive reset be performed for a drive before a media
change. If a drive is in the reset state when the media is changed,
the next access to the drive logs in the drive. Note that the DISK
SYSTEM RESET and RESET DRIVE functions have similar effects except
that the DISK SYSTEM RESET function is directed to all drives on the
system. The user can specify any combination of drives to be reset
with the RESET DRIVE function.

Under MP/M-86, the drive reset operation is conditional in
nature. Generally speaking, the file system cannot reset a drive
for a process if another process has an open file on the drive.
However, the exact action taken by a drive reset operation depends
on whether the drive to be reset is permanent or removeable. MP/M-
86 determines whether a drive is permanent or removeable by
interrogating a bit in the drive’s Disk Parameter Block (DPB) in the
XIOS (refer to the MP/M-86 System’s Guide for a detailed discussion
of the DPB). A high-order bit of 1 in the DPB checksum vector size
field designates the drive as permanent. Under MP/M-86, a drive’s
designation is critical to the reset operation, which is described
below.

The BDOS first determines if there are any files currently open
on the drive to be reset. If there are none, the reset takes place.
Otherwise, if the drive is a permanent drive and if the drive is not
read/fonly, the reset operation is not performed but a successful
result is returned to the calling process. However, if the drive is
removeable or read/only, the file system determines whether other
processes have open files on the drive. If they do, the drive reset
operation is denied and an Error Code is returned to the calling
process. If all the files open on the drive belong to the calling
process, the file system performs a "qualified" reset operation for
the drive and returns a successful result to the calling process.
This means that the next time the drive is accessed, the log-in
operation is only performed if a media change is detected on the
drive. The logic flow of the drive reset operation is shown in
Figure 2-4.

---
_Page 047_

MP/M-86 Programmer’s Guide 2.14 Reset, Access and Free Drive

Open files
on drive ?
+-~--~-----~---+-- +

no t-------------- +
| Drive yes
removeable ?
+-------------+- +
| no
+-------------~ + yes
| Drive R/O ? |
+-------------- +
no
+---------- + +-------------- t+ 4+----~----------- +
Reset | | Don\*t reset | Open files yes
drive drive belong to
+---------- + 9 $-------------- + another a
process ?
+----------~+---- +
| no
+---~---~-----+-- +
Qualified
reset
performed
t-----------~+--- +
+---------~- + +------------
Disk Disk
Reset =| ~---~-------~---------------- Reset
Success Denied
+---------- + +------------

Figure 2-4. Disk System Reset

If the file system detects a media change on a drive after a
qualified reset, it purges all open files on the drive from the
System Lock List and subsequently verifies all open FCBs in file
Operations for the owning process (see Section 2.9). The drive is
also relogged-in. In all other cases where a media change is
detected on a drive, the file system performs the following steps:
All open files on the drive are purged from the system Lock List,
and all process owning a purged file are flagged for automatic open
FCB verification. The drive is then placed in read/fonly status. It
is not relogged-in until a drive reset is issued for the drive.
Note: If a process references a file purged from the system Lock
List in a BDOS command that requires an open FCB, it is returned an

---
_Page 048_

MP/M-86 Programmer®s Guide 2.14 Reset, Access and Free Drive

FCB checksum error by the BDOS file system.

The ACCESS DRIVE and FREE DRIVE functions perform special
actions under MP/M-86. The ACCESS DRIVE function inserts a "dummy"
open file item into the system Lock List for each specified drive.
While that item exists in the system Lock List, the drive cannot be
reset by another process. The FREE DRIVE function purges the Lock
List of all items including open file items belonging to the calling
process on the specified drives. Any subsequent reference to those
files by a BDOS function call requiring an open FCB results in a FCB

checksum error return.

The WRITE PROTECT DISK function has special properties under
MP/M-86. This function can be used to set the specified drive to
read/only. However, MP/M-86 does not allow a process to set a drive
read/only if another process has an open file on the drive. This
applies to both removeable and permanent drives. If a process has
successfully set a drive read/fonly, it can prevent other processes
from resetting the drive by either opening a file on the drive or
issuing an ACCESS DRIVE call for the drive. While the open file or
"dummy" item belonging to the process resides in the system Lock
List, no other process can reset the drive to take it out of
read/fonly status.

2.15 BDOS Error Handling

The BDOS file system has an extensive error handling
capability. When it detects an error, it can respond in three ways:

1) It can return to the calling process with return codes
in AX register identifying the error.

2) It can display an error message on the console and abort
the process.

3) It can display an error message on the console and
return to the calling process as in method l.

The file system handles the majority of errors it detects via method
1. The kinds of errors the file system handles via methods 2 and 3
are called "physical" and "extended" errors. The BDOS SET ERROR
MODE function determines how the file system handles physical and
extended errors. The BDOS Error Mode can exist in three states. In
the default mode, the BDOS displays the error message and terminates
the calling process (method 2). In Return Error Mode, the BDOS
returns control to the calling process with the error identified in
the AX register (method 1). In Return and Display Mode, the BDOS
returns control to the calling process with the error identified in
the AX register, and also displays the error message at the console
(method 3). Both of the return modes ensure that MP/M-86 does not
terminate the process because of a physical or extended error. The
Return and Display Mode also allows the calling process to take
advantage of the built-in error reporting of the BDOS file system.
Physical and extended errors are displayed on the console in the

---
_Page 049_

MP/M II Programmer’s Guide 2.15 BDOS Error Handling

following format:

BDOS Err on d: error message
BDOS function: nn File: filename.type

where "d" is the name of the drive selected when the error condition
is detected; "error message" identifies the error; "nn" is the BDOS
function number, and "filename.type" identifies the file specified
by the BDOS function. If the BDOS function did not involve a FCB,
the file information is omitted.

The BDOS physical errors are identified by the following error
messages:

@ Bad Sector

@ Select

@e File R/O

® R/O
The "Bad Sector" error results from an error condition returned to
the BDOS from the XIOS module. The file system makes XIOS read and
write calls to execute file related BDOS calls. If the XIOS read or
write routine detects an error, it returns an Error Code to the BDOS
resulting in this error.

The "Select" error also results from an error condition
returned to the BDOS from the XIOS module. The BDOS makes an XIOS
SELECT DISK call prior to accessing a drive to perform a requested
BDOS function. If the XIOS does not support the selected disk, it
returns an Error Code resulting in this error.

The BDOS returns the "File R/O" error whenever a process makes
a write operation to a file with the R/O attribute set.

The BDOS returns the "R/O" error whenever a process makes a
write operation to a disk that is in read/only status. A drive can
be placed in read/only status explicitly with the BDOS WRITE PROTECT
DISK function, or implicitly if the file system detects a change in
media on the drive.

The BDOS extended errors are identified by the following error
messages:

@ File Opened in Read/Only Mode
@ File Currently Opened

e@ Close Checksum Error

@ Password Error

@ File Already Exists

---
_Page 050_

MP/M II Programmer“s Guide 2.15 BDOS Error Handling

e Illegal ? in FCB
@ Open File Limit Exceeded
@ No Room in System Lock List

The BDOS returns the "File Opened in Read/Only Mode" error when a
process attempts to write to a file opened in Read/only Mode. A
file can be opened in Read/only Mode explicitly, or opened in
Read/only Mode implicitly in two ways. If a file is opened from
user zero when the current user number is non-zero, the file is
opened in Read/only Mode. In addition, if a file is password
protected in Write Mode and the password is not supplied with the
open call, the BDOS returns this error if an attempt is made to
write to the file.

The BDOS returns the "File Currently Open" error when a process
attempts to delete, rename, or modify the attributes of a file
opened by another process. The BDOS also returns this error when a
process attempts to open a file in a mode incompatible with the mode
in which the file was opened by another process.

The BDOS returns the "Close Checksum Error" message when the
BDOS detects a checksum error in the FCB passed to the file system
with a BDOS CLOSE FILE call.

The BDOS returns the "File Password" error when the file
password is not supplied, or it is incorrect.

The BDOS returns the "File Already Exists" error for the BDOS
MAKE FILE and RENAME FILE functions when the BDOS detects a conflict
on filename and type.

The BDOS returns the "Illegal ? in FCB" error whenever the BDOS
detects a "?" in the filename or type field of the passed FCB for
the BDOS RENAME FILE, SET FILE ATTRIBUTES, OPEN FILE and MAKE FILE
functions.

The BDOS returns the "Open File Limit Exceeded" error when a
process exceeds the file lock limit specified in the system Lock
List during system generation. The OPEN FILE, MAKE FILE, and ACCESS
DRIVE functions can return this error.

The BDOS returns the "No Room in System Lock List" error when
no room for new entries exists within the system Lock List. The
capacity of the system Lock List is a system generation parameter.
The OPEN FILE, MAKE FILE, and ACCESS DRIVE functions can return this
error.

The following paragraphs describe the error return code
conventions of the BDOS file system functions. Most BDOS file
system functions fall into three categories in regard to return
codes; they return an Error Code, a Directory Code, or an Error
Flag. The error conventions are designed to allow programs written

---
_Page 051_

MP/M II Programmer's Guide 2.15 BDOS Error Handling

for CP/M~86 to run without modification.

The following BDOS functions return an Error Code in register
AL.

20. READ SEQUENTIAL

21. WRITE SEQUENTIAL

33. READ RANDOM

34. WRITE RANDOM

40. WRITE RANDOM WITH ZERO FILL
41. TEST AND WRITE RECORD

42. LOCK RECORD

43. UNLOCK RECORD

The Error Code definitions for register AL are shown in Table 2-8.
Table 2-8. BDOS Error Codes

| 00 : Function successful |
| 255 : Physical error : refer to register AH |
| O01 : Reading unwritten data |
| No available directory space (Write Sequential) |
| 02 : No available data block |
| 03 : Cannot close current extent |
| 04 : Seek to unwritten extent |
| 05 : No available directory space |
| 06 : Random record number out of range
| 07 : Record match error (Test and Write) |
| \* 08 : Record locked by another process |
| (restricted to files opened in unlocked mode) |
| 09 : Invalid FCB (previous BDOS read or write call |
| returned an error code and invalidated the FCB) |
| 10 : FCB checksum error |
| \* 11 : Unlocked file unallocated block verify error |
| \*\* 12 : Process record lock limit exceeded |
| \*\* 13 : Invalid File ID |
| \*\* 14 : No room in System Lock List |

\* - returned only for files opened in Unlocked Mode
\*\* — returned only by the LOCK RECORD function
for files opened in Unlocked Mode

The following BDOS functions return a Directory Code in
register AL:

15. OPEN FILE

16. CLOSE FILE

17. SEARCH FOR FIRST
18. SEARCH FOR NEXT
19. DELETE FILE

22. MAKE FILE

---
_Page 052_

MP/M II Programmer”s Guide 2.15 BDOS Error Handling

23. RENAME FILE

30. SET FILE ATTRIBUTES
100. SET DIRECTORY LABEL
101. READ FILE XFCB

102. WRITE FILE XFCB

The Directory Code definitions for register AL are shown in Table 2-

Table 2-9. BDOS Directory Codes

successful function
unsuccessful function

With the exception of the BDOS search functions, Directory Code
values (0-3) have no significance other than to indicate a
successful result. However, for the search functions, a successful
Directory Code identifies the relative starting position of the
directory element in the calling process\* current DMA buffer.

If the SET BDOS ERROR MODE function is used to place the BDOS
in Return Error Mode, the following functions return an Error Flag
in register AL on physical errors:

14. SELECT DISK

35. COMPUTE FILE SIZE

38. ACCESS DRIVE

46. GET DISK FREE SPACE

48. FLUSH BUFFERS

101. RETURN DIRECTORY LABEL DATA

The Error Flag definition for register AL is shown in Table 2-9.
Table 2-10. BDOS Error Flags

: successful function
255 : physical error : refer to register AH |

The BDOS returns register AH values for all three of the above
categories in the following format:

Figure 2-5. Return Values - Register AH

---
_Page 053_

MP/M II Programmer\*s Guide 2.15 BDOS Error Handling

where Nl denotes the high-order nibble and N2 denotes the low-order
nibble. The following rules govern the assignment of values to Nl
and N2.

N1 For functions that return Error Codes, the BDOS sets Nl to
the number of sectors successfully read or written before
the error is encountered. This information is returned
Only when a process uses the Set MULTI-SECTOR COUNT
function to set the BDOS Multi-Sector Count to a value
other than one; otherwise the BDOS sets Nl to zero.
Successful read and write functions also set Nl to zero.

Nl Functions that return a Directory Code or an Error Flag set
Nl to zero,

N2 The values contained in N2 identify BDOS physical and
extended errors. The BDOS returns values in N2 only if it
is in one of the Return Error Modes; otherwise, it sets N2
to zero. Table 2-10 lists the physical and extended error
codes returned in N2.

Table 2-11. BDOS Physical and Extended Errors

00 - no error or not a register AH error
01 - Bad Sector : permanent error
02 - R/O : read/only disk
03 - R/O File : read/only file

- File Opened in Read/Only Mode
04 - Select : drive select error
05 - File Currently Open
06 - Close Checksum Error
07 - Password Error
08 - File Already Exists
09 - Illegal ? in FCB
10 - Open File Limit Exceeded
11 - No Room in System Lock List

Note: Register AH is equal to zero if the called function is
successful. In addition, the BDOS sets N2 to zero when register AL
returns a value other than 255. Except for functions that return
Directory Codes, if register AL contains a value of 255 upon return,
N2 identifies the error when the BDOS is in Return Error Mode.

The following two functions represent a special case because
they return an address in register AX.

27. GET ADDR(ALLOC)
31. GET ADDR(DISK PARMS)

When the BDOS is in Return Error Mode and it detects a physical
error for these functions, it returns to the calling process with

---
_Page 054_

MP/M II Programmer”s Guide 2.15 BDOS Error Handling

registers AX, and BX set to 255. Otherwise, they return no error
code.

Under MP/M~86, the following functions also represent a special
case.

13. RESET DISK SYSTEM
28. WRITE PROTECT DISK
37. RESET DRIVE

These functions return to the calling process with registers AL, and
BL set to 255 if another process has an open file or has made a BDOS
ACCESS DRIVE call that prevents the reset or write protect operation
(see Section 2.14). If the BDOS is not in Return Error Mode, these
functions also display an error message identifying the process that
prevented the requested operation.

---
_Page 055_

SECTION 3

TRANSIENT COMMANDS

3.1 Transient Process Load and Exit

A user can initiate a transient process by entering a command
at a system console. The console's TMP then calls the CLI function,
and passes to it the command line entered by the user. If the
command is not resident, then the CLI function locates and then
loads the proper CMD file (see the CLI function). The CLI function
calls the PARSE FILENAME function which parses up to two filenames
following the command and places the properly formatted FCBs at
locations 005CH and OO6CH in the Base Page of the initial Data
Segment. The CLI function initializes memory, the Process
Descriptor, and the User Data Area (UDA), and allocates a 96-byte
stack area independent of the program, to contain the process's
initial stack. MP/M-86 divides the DMA address into two parts: the
DMA segment address, and the DMA offset. The CLI function
initializes the default DMA base to the value of the initial Data
Segment, and the default DMA offset to O0O80H.

The CLI function creates the new process with a CREATE
PROCESS call (Function 144), and sets the initial stack such that
the process can execute a Far Return call to terminate. A process
can also terminate by calling SYSTEM RESET (Function 0), or by
calling TERMINATE (Function 143). A user may terminate a process by
typing a single TC during line edited input. This has the same
effect as the process calling Function 0.

3.2 Command File Format

A CMD file consists of a 128-byte Header Record followed
immediately by the memory image. The command file Header Record is
composed of 8 Group Descriptors (GDs), each 9 bytes long. Each
Group Descriptor describes a portion of the program to be loaded.
The format of the Header Record is shown in Figure 3-l.

Figure 3-1. CMD File Header Format

In Figure 3-1, GD 1 through GD 8 represent "Group Descriptors."
Currently only the first 72 bytes of the Header Record are used.
The remaining bytes are reserved for future facilities.

---
_Page 056_

MP/M-86 Programmer's Guide 3.2 Command File Format

In Figure 3-l, each Group Descriptor corresponds to an
independently loaded program unit and has the format shown in Figure
3-2.

8-bit 16-bit 16-bit lo-bit 16-bit

Figure 3-2. Group Descriptor Format

where G-Form describes the group format, or has the value zero if no
more descriptors follow. If G-Form is non-zero, then the 8-bit
value is parsed as two fields shown in Figure 3-3.

G-Forms:
$+-------- $--------- +
| 4-bit | 4-bit |
+--------- | -----3---- +
| x x x x | G-Type |
fore +—-------+-- +

Figure 3-3. G-~-Form Format

The G-Type field determines the Group Descriptor type. The valid

Group Descriptors have a G-Type in the range 1 through 9, as shown
in Table 3-l.

---
_Page 057_

MP/M-86 Programmer's Guide 3.2 Command File Format

Table 3-1. Group Descriptors

1 Code Group |
2 Data Group |
3 Extra Group |
4 Stack Group |
5 Auxiliary Group #1 |
6 Auxiliary Group #2 |
7 Auxiliary Group #3 |
8 Auxiliary Group #4 |
9 Shared Code Group |
10 Unused, but Reserved |
uw |

|

|

|

|

All remaining values in the Group Descriptor are given in
increments of 16-byte paragraph units with an assumed low-order 0
nibble to complete the 20-bit address.

G-Length gives the number of paragraphs in the group.
Given a G-length of 0080H, for example, the size
of the group is 00800H = 2048D bytes.

A-Base defines the base paragraph address for a non-
relocatable group.

G-Min/G-Max define the minimum and maximum size of the memory
area to allocate to the group.

The memory model described by a Header Record is implicitly
determined by the Group Descriptors (see Section 4.1). The 8080
Model is assumed when only a Code Group is present, since no
independent Data Group is named. The Small Model is assumed when
both a Code and Data Group are present, but no additional Group
Descriptors occur. Otherwise, the Compact Model is assumed when the
CMD file is loaded.

3.3 Base Page Initialization
The MP/M-86 Base Page contains default values and locations

initialized by the CLI and PROGRAM LOAD functions, and used by the
transient process,

---
_Page 058_

MP/M-86 Programmer's Guide 3.3 Base Page Initialization

The Base Page occupies the regions from offset O000H through OOFFH
relative to the initial Data Segment, and contains the values shown
in Figure 3-4.

L M H L H
0 1 2 3 4 5 6
t--------- fa--------- $--------- +--------- $--------- $—--------- +
0. (| Code Length | Code Base | M80 |
t—--------- +--------- te-------- ta-------- +--------- $o-------- +
6 | Data Length | Data base |Reserved |
to-------- +—--------- +--------- $—--------- fo-------- $o-------- +
Cc | Extra Length | Extra base |Reserved |
f--------- $--------- +--------- $--------- $--------- $—--------- +
12 | Stack Length | Stack Base |Reserved |
4+--------- $o-------- to-------- +—--------- a $--------- +
18 | Aux 1 | Aux 1 [Reserved |
t—-------- +--------- f--------- $--------- to--------- $--------- +
1E | Aux 2 | Aux 2 |Reserved |
t—-------- $—--------- +--------- fo-------- ta-------- $--------- +
24 | Aux 3 | Aux 3 [Reserved |
$--------- $--------- $--------- ta-------- +--------- ta--------- +
2A | Aux 4 | Aux 4 |Reserved |
taee------ +--------- fe-------- +--------- t—-------- $--------- +
30 | Bytes 30 through 4F are currently not used but are |
| reserved for use by MP/M-86. |
| . |
| .
| . |
| |
4+—--------- +--------- $--------- +--------- to-------- foo HHH +
50 | Drive | Password 1 Addr | Pl Len | Password 2 Addr |
+--------- $--------- +--------- t—-------- +—--------- $--------- +
56 | P2 Len | Currently not used but reserved |
+--------- +--------- +--------- $--------- t--------- $--------- +
5c | |
| Default FCB Area l |
| . |
| e |
ne penne ---- 4—-—-------- freee ta----- HHH t--------- +
6c | |
| Default FCB Area 2 |
| . |
| . |
+--------- t--------- 4--------- $o------ =H fo-- = --+--- +
7c | CR | Random Record Number (opt) | MIA
4--------- t--------- +--------- $---------4--------- + --- = +
80 | |
| |
| Default 128-byte DMA Buffer |
| |
| |
fo--------+------- --- +--+ - - - - - - - - - - 5 enn +

Figure 3-4. MP/M-86 Base Page Values

---
_Page 059_

MP/M-86 Programmer's Guide 3.3 Base Page Initialization

The various fields within the Base Page are defined as follows:

@ The M80 byte is a flag indicating whether the 8080 memory
model was used during load. The values of the flag are
defined as:

8080 Model
not 8080 Model

If the 8080 Model is used, the code length never exceeds
OFFFFH.

e The bytes marked Aux 1 through Aux 4 correspond to a set of
four optional independent groups which may be required for
programs which execute using the Compact Memory Model. The
initial values for these descriptors are derived from the
Header Record in the memory image file.

e Length is stored using the Intel convention (i.e. low,
middle, and high bytes).

e Base refers to the address of the beginning of the segment,

e The Drive byte identifies the drive from which the
transient program was read. 0 designates the default
drive, while a value of 1 through 16 identifies drives A
through P.,

@ Password 1 Addr (bytes 0051H-0052H) contains the address of
the password field of the first command-tail operand in the
default DMA buffer at 0080H. The CLI function sets this
field to 0 if no password is specified.

e Pl Len (byte 0053H) contains the length of the password
field for the first command-tail operand. The CLI function
sets this to 0 if no password is specified.

e Password 2 Addr (bytes 0054H-0055H) contains the address
of the password field of the second command-tail operand in
the default DMA buffer at 0080H. The CLI function sets
this field to 0 if no password is specified.

e P2 Len (byte 0056H) contains the length of the password
field for the second command-tail operand. The CLI
function sets this field to 0 if no password is specified,

e FCB Area 1 (bytes 005CH-007CH) is initialized by the CLI
function for a transient program from the first command-
tail operand of the command line (if it exists).

e FCB Area 2 (bytes O06CH-007CH) is initialized by the CLI

function for a transient program from the second command-
tail operand of the command line (if it exists). Note:

A7

---
_Page 060_

MP/M-86 Programmer's Guide 3.3 Base Page Initialization

this area overlays the last 16 bytes of FCB Areal. To use
information in this area, the transient process must copy
it to another location before using Area l.

e The CR field (byte 007CH) contains the current record
position used in sequential file operations with FCB area

e The optional Random Record Number (bytes 007DH-007FH) is an
extension of FCB Area 1 used in random record processing.

e The Default DMA buffer (bytes OO80H-OOFFH) contains the
command tail when the CLI function loads a transient
program.

3.4 Parent/Child Reltionships

Under MP/M-86, when one process creates another process, there
is a parent/child relationship between them. That is, the child
process inherits all the default values of the parent process. This
includes the default disk, user number, console, list device, and
password. The child process will also inherit any Interrupt Vectors
that the parent process has initialized.

---
_Page 061_

SECTION 4

COMMAND FILE GENERATION

4.1 Transient Execution Models

The initial values of the segment registers are determined by
which one of the three "memory models" is used by the transient
process. The specific memory model is indicated in the CMD file
Header Record. The three memory models are summarized in Table 4-1
below.

Table 4-1. MP/M-86 Memory Models

ton ---- 3 ---~ + ee eee ee +
| Model Group Relationships |
+ +
ton ne ee 5 +
| 8080 Model | Code and Data Groups Overlap |
| -----~--~----~--~~--~------------~----------~--------- |
| Small Model | Independent Code and Data Groups |
| ~---~----~--------------~------------------------------ |
| Compact Model | Three or More Independent Groups |
t--------~---- -- = -- --- -- - -- - 5 + ee +

The 8080 Model supports programs which are directly translated
from an 8080 environment where code and data are intermixed. The
8080 Model consists of one group which contains all the code, data,
and stack areas. Segment registers are initialized to the starting
address of the region containing this group. The segment registers
can, however, be managed by the application program during execution
so that multiple segments within the Code Group can be addressed.

The Small Model is similar to that defined by Intel, where the
program consists of an independent Code Group and a Data Group. The
Code and Data Groups often consist of, but are not restricted to,
single 64K-byte segments.

The Compact Model occurs when any of the Extra, Stack, or
Auxiliary Groups are present in program. Each group may consist of
one or more segments, but if any group exceeds one segment in size,
or if Auxiliary Groups are present, then the application program
must manage its own segment registers during execution in order to
address all code and data areas.

The three models differ primarily in the manner in which the
Operating System initializes the segment registers when it loads a
transient process. The PROGRAM LOAD function determines the memory
model used by a transient program by examining the program group

---
_Page 062_

MP/M-86 Programmer's Guide 4.1 Transient Execution Models

usage, as described in the following sections.

4.1.1 The 8080 Memory Model

The 8080 Model is assumed when the transient program contains
only a Code Group. In this case, the CLI function initializes the
CS, DS, and ES registers to the beginning of the Code Group, and
sets the SS and SP registers to a 96-byte initial stack area that it
allocates. Note: the CLI function initializes the stack such that
if the process executes a Far Return instruction, it will terminate.
The CLI function sets the Instruction Pointer Register (IP) to 100H,
thus allowing Base Page values at the beginning of the code group.
Following program load, the 8080 Model appears as shown in Figure 4-
l.

4--------- +
SS SP ----> | 96-BYTE STACK AREA |
4-------------- +

fee -- ee +

| |

| CODE/DATA |

| |

. |

| . |

| . |

| |

| CODE/DATA |

| |

Q100H +---------------- +

| (IP = O100H) |

| |

| BASE PAGE |

| |

0 +---------------- +

cs DS ES

Figure 4-1. MP/M-86 8080 Memory Model

The intermixed code and data areas are indistinguishable. The Base
Page values are described in Section 3-3. The following ASM-86
example shows how to code an 8080 Model transient program,

---
_Page 063_

MP/M-86 Programmer's Guide 4.1 Transient Execution Models

cseg
org 100h
: (code)
endcs equ $
dseg
org offset endcecs
. (data)
end

4.1.2 The Small Memory Model

The Small Model is assumed when the transient program contains
both a Code and Data Group. (In ASM-86, all code is generated
following a CSEG directive, while data is defined following a DSEG
directive with the origin of the Data Segment independent of the
Code Segment.) In this model, the CLI function sets the CS register
to the beginning of the Code Group, the DS and ES registers to the
beginning of the Data Group, and the SS and SP registers to a 96-
byte initial stack area that it initializes. Following program
load, the Small Model appears as shown in Figure 4-2,

+----~---~----~-------~-+-- +
SS SP ----> | 96-BYTE STACK AREA |
tae----------------------- +

+------~--------- + +---------------- +
| | | |
| | | |
| | | . |
| | | . |
| | | . |
| | | |
| . | | DATA |
| . | | |
| . | | |
| | 100H+-----------~----- +
| CODE | | |
| | | BASE PAGE |
| | | |
| (IP = 0000H) | | |

Q +----------~------ + Q +---------------- +

cs DS ES

Figure 4-2. MP/M-86 Small Memory Model

The machine code begins at CS+0000H, the Base Page values begin at
DS+0000H, and the data area starts at DS+0100H. The following ASM-
86 example shows how to code a Small Model transient program.

---
_Page 064_

MP/M-86 Programmer's Guide 4.1 Transient Execution Models

cseg
. (code)
dseg
org 100h
. (data)
end

4.1.3 The Compact Memory Model

The Compact Model is assumed when Code and Data Groups are
present, along with one or more of the remaining Stack, Extra, OF
Auxiliary Groups. In this case, the CLI function sets the CS, DS,
and ES registers to the base addresses of their respective areas,
and the SS and SP registers to a 96-byte stack area it allocates.
Figure 4-3 shows the initial configuration of the segments in the
Compact Model. The values of the various segment registers can be
programmatically changed during execution by loading from the
initial values placed in Base Page, thus allowing access to the
entire memory space.

+------------- nnn +
SS SP ----> | 96-BYTE STACK AREA |
f----------- rn +
a ataa + t—--------- + -- + fo--- 7 oer
| | | | |
| | | | |
| | | | |
| | | : | |
| | | . | |
| . | | . | |
| : | | | | .
| : | | DATA | | :
| | | | | .
| CODE | 10Q0H+---------------- + |
| | | | | DATA
| (IP=0000H) | | BASE PAGE | |
Q +---------------- + 0 te--------------- + 0 t----------------
cs DS ES

Figure 4-3. MP/M-86 Compact Memory Model
If the transient program intends to use the Stack Group as a

stack area, the SS and SP registers must be set upon entry. The SS
and SP registers remain in the initial stack area, even if a Stack

---
_Page 065_

MP/M-86 Programmer's Guide 4.1 Transient Execution Models

Group is defined.

Although it may appear that the SS and SP registers should be
set to address the Stack Group, there are two contradictions.
First, the transient program may be using the stack group as a data
area. In that case, the Far Call instruction used by the CLI
function to transfer control to the transient program could
overwrite data in the stack area. Second, the SS register would
logically be set to the base of the group, while the SP would be set
to the offset of the end of the group. However, if the Stack Group
exceeds 64K the address range from the base to the end of the group
exceeds a 16-bit offset value.

The following ASM-86 example shows how to code a Compact Model
transient program,

cseg

. (code)

dseg

org 100h

. (data)

eseg

. (more data)
sseg

. (Stack area)
end

4.2 GENCMD

The GENCMD utility creates a CMD file from an input HEX file.
GENCMD is non-destructive. That is, it does not alter the original
HEX file. The user invokes the GENCMD utility by typing

OA>GENCMD filename {parameter-list}

where the filename corresponds to the HEX input file with an assumed
(and unspecified) file type of H86. GENCMD accepts optional
parameters to specifically identify the 8080 Model and to describe
memory requirements of each segment group. The GENCMD parameters
are listed following the filename, as shown in the command line
above where the parameter-list consists of a sequence of keywords
and values separated by commas or blanks. The keywords are:

8080 CODE DATA EXTRA STACK Xl xX2 X3 x4
The 8080 keyword forces a single Code Group so that the PROGRAM LOAD

function sets up the 8080 Model for execution, thus allowing
intermixed code and data within a single segment. The form of this

---
_Page 066_

MP/M-86 Programmer's Guide 4.2 GENCMD

command is
OA>GENCMD filename 8080

The remaining keywords follow the filename or the 8080 option and
define specific memory requirements for each segment group,
corresponding one-to-one with the segment groups defined in the
previous section. In each case, the values corresponding to each
group are enclosed in square brackets and separated by commas. Each
value is a hexadecimal number representing a paragraph address or
segment length in paragraph units denoted by hhhh, prefixed by a
single letter which defines the meaning of each value:

Ahhhh Load the group at absolute location hhhh

Bhhhh The group starts at hhhh in the hex file

Mhhhh The group requires a minimum of hhhh \* 16 bytes
Xhhhh The group can address a maximum of hhhh \* 16 bytes

Generally, the CMD file Header Record values ere derived directly
from the HEX file and the parameters shown above need not be
included. The following situations, however, require the use of
GENCMD parameters.

e The 8080 keyword is included whenever ASM-86 is used in the
conversion of 8080 programs to the 8086/8088 environment
when code and data are intermixed within a single 64K
segment, regardless of the use of CSEG and DSEG directives
in the source program.

e An absolute address (A value) must be given for any group
which must be located at an absolute location. Normally,
this value is not specified since MP/M-86 cannot generally
ensure that the required memory region is available, in
which case the CMD file cannot be loaded.

e The B value is used when GENCMD processes a HEX file
produced by Intel's OH86, or similar utility program that
contains more than one group. The output from OH86 consists
of a sequence of data records with no information to
identify Code, Data, Extra, Stack, or Auxiliary groups. In
this case, the B value marks the beginning address of the
group named by the keyword, causing GENCMD to load data
following this address to the named group (see the examples
below). Thus, the B value is normally used to mark the
boundary between Code and Data Segments when no segment
information is included in the HEX file. Files produced by
ASM-86 do not require the use of the B value since segment
information is included in the HEX file.

e The minimum memory value (M value) is included only when
the HEX records do not define the minimum memory
requirements for the named group. Generally, the Code
Group size is determined precisely by the data records
loaded into the area. That is, the total space required

---
_Page 067_

MP/M-86 Programmer's Guide 4.2 GENCMD

for the group is defined by the range between the lowest
and highest data byte addresses. The Data Group, however,
may contain uninitialized storage at the end of the group
and thus no data records are present in the HEX file which
define the highest referenced data item. The highest
address in the data group can be defined within the source
program by including a "DB 0" as the last data item.
Alternatively, the M value can be included to allocate the
additional space at the end of the group. Similarly, the
Stack, Extra, and Auxiliary Group sizes must be defined
using the M value unless the highest addresses within the
groups are implicitly defined by data records in the HEX
file.

e The maximum memory size, given by the X value, is generally
used when additional free memory may be needed for such
purposes as I/O buffers or symbol tables. If the data area
size is fixed, then the X parameter need not be included.
In this case, the X value is assumed to be the same as the
M value. The value XFFFF allocates the largest memory
region available but, if used, the transient program must
be aware that a three-byte length field is produced in the
Base Page for this group where the high-order byte may be
non-zero, Programs converted directly from an 8080
environment or programs that use a 2-byte pointer to
address buffers should restrict this value to XFFF or less,
producing a maximum allocation length of OFFFOH bytes,

The following GENCMD command line transforms the file X.H86
into the file X.CMD with the proper Header Record:

OA>gencmd x code[a40] data[m30,xfff]

In this case, the Code Group is forced to paragraph address 40H, or
equivalently, byte address 400H. The Data Group requires a minimum
of 300H bytes, but can use up to OFFFOH bytes, if available.

Assuming a file Y.H86 exists on drive B containing Intel HEX
records with no interspersed segment information, the commana

0A>gencmd b:y data[b30,m20] extra[b50] stack[m40] x1l[m40]

produces the file Y.CMD on drive B by selecting records beginning at
address 0000H for the Code Segment, with records Starting at 300H
allocated to the Data Segment. The Extra Segment is filled from
records beginning at 500H, while the Stack and Auxiliary Segment #1
are uninitialized areas requiring a minimum of 400H bytes each. In
this example, the data area requires a minimum of 200H bytes. Note
again, that the B value need not be included if the Digital Research
ASM-86 assembler is used,

---
_Page 068_

MP/M-86 Programmer's Guide 4.3 Intel HEX File Format

4.3 Intel HEX File Format

GENCMD input is in Intel HEX format produced by both the
Digital Research ASM-86 assembler and the standard Intel OH86
utility program (see Intel document #9800639-03 entitled "MCS-86
Software Development Utitities Operating Instructions for ISIS-II
Users"). The CMD file produced by GENCMD contains a Header Record
which defines the memory model and memory size requirements for
loading and executing the CMD file.

An Intel HEX file consists of the traditional sequence of ASCII
records in the following format:

where the beginning of the record is marked by an ASCII colon, and
each subsequent digit position contains an ASCII hexadecimal digit

in the range 0-9 or A-F. The fields are defined in Table 4-l.

Table 4-1. Intel Hex Field Definitions

Field Contents
Li Record Length OO-FF (0-255 in decimal)

aaaa Load Address

tt Record Type:

00 data record, loaded starting at offset
aaaa from current base paragraph

01 end of file, cc = FF

02 extended address, aaaa is paragraph
base for subsequent data records

03 start address is aaaa (ignored, IP set
according to memory model in use)

The following are output from ASM-86 only:

81 same as 00, data belongs to Code Segment

82 same as 00, data belongs to Data Segment

83 same as 00, data belongs to Stack Segment

84 same as 00, data belongs to Extra Segment

85 paragraph address for absolute Code Segment

86 paragraph address for absolute Data Segment

87 paragraph address for absolute Stack Segment
88 paragraph address for absolute Extra Segment

---
_Page 069_

MP/M-86 Programmer's Guide 4.3 Intel HEX File Format

Table 4-1. (continued)

Field Contents
d Data Byte
cc Check Sum (00 - Sum of Previous Digits)

All characters preceding the colon for each record are ignored.
(Additional HEX file format information is included in the ASM-86
User's Guide, and in Intel's document #9800821A entitled "MCS-86
Absolute Object File Formats.")

---
_Page 070_

---
_Page 071_

SECTION 5

RSP GENERATION

5.1 RSP Introduction

Resident System Processes are programs that can optionally
become part of the MP/M-86 Operating System. They can be useful in
several ways including creating a "turn key" system, autoloading
programs when MP/M-86 is booted, creating customized user interfaces
or "shells" at the consoles, monitoring hardware not supported in
the XIOS, and avoiding disk loading time for often used commands.

The source code for the TMP (TERMINAL MESSAGE PROCESS) and ECHO
RSPs is included in Appendices J and K, respectively. The reader
should study these carefully while reading this section. The
discussion of the CREATE PROCESS function (Function 144) in Section
6 is also helpful in understanding RSPs.

Resident System Processes are included with MP/M-86 during
system generation. GENSYS searches the directory for all files with
the file type .RSP and prompts the user to choose whether it will be
included in the generated system file, MPM.SYS. An RSP file is
created by generating a CMD file and renaming it. The GENSYS
program is documented in the MP/M-86 System Guide.

5.2 RSP Memory Models

Under MP/M-86, there are two basic memory models for RSPs.
They are similar to the 8080 and Small Models of transient programs.
However, several important distinctions exist between the transient
program and RSP memory models. The RSP has no equivalent to the
Base Page of the transient program's Data Segment. The RSP is
responsible for its own Process Descriptor (PD) and User Data Area
(UDA). The system creates and initializes these data structures for
the transient programs automatically at load time. RSPs, on the
other hand, must have these structures initialized within their own
Data Segments.

5.2.1 8080 Model RSP

The 8080 Model implies mixed code and data. When the system
gives control of the CPU to an 8080 Model RSP, the Code, Data, Extra
and Stack Segment registers are initialized to the same value. An
8080 Model RSP is generated by GENCMD with the 8080 option. GENSYS
assumes the 8080 Model if the CMD file Header Record of the RSP has
a Single Code Group Descriptor and no other Group Descriptors (see
Section 3.2). Throughout this section, when discussing an 8080
Model RSP, any reference to the Data Segment also refers to the Code
Segment.

---
_Page 072_

MP/M-86 Programmer's Guide 5.2 RSP Memory Models

5.2.2 Small Model RSP

The Small Model RSP implies separate Code and Data Segments.
When the system gives control of the CPU to a Small Model RSP, the
Data, Extra and Stack Segment registers are initialized to the Data
Segment while the Code Segment register is initialized to the Code
Segment. There is no guarantee where GENSYS will place the Code
Segment in memory relative to the Data Segment. The CMD Header
Record for this kind of RSP must have both Data and Code Group
Descriptors.

5.3 Multiple Copies of RSPs

At system generation, GENSYS can make up to 255 extra copies of
an RSP such that each copy generates a separate process running
under MP/M-86. GENSYS accomplishes this by making multiple copies
of the RSP, and initializing each to be a separate RSP. The number
of copies made by GENSYS can be fixed or dependent on a byte value
in the System Data Area. To determine the number of copies to make,
GENSYS looks at two fields in the RSP Header. The format of the RSP
Header is shown in Figure 5-1.

byte: 0 1 2 3 4 5 oa 8
fa---- $----- +----- +----- +----- $o--------------- +
| LINK | SDATVAR | NCP | RESERVED |
+----- +----- +----- $----- es $---------------- +

Figure 5-1. RSP Header Format

If the SDATVAR field is non-zero, it is used as an offset of a byte
value in the System Data Area which contains the number number of
copies to be generated. The offset should indicate a value that is
set by the user during GENSYS. The TMP RSP uses this feature by
placing the offset of the Number Of System Consoles field into the
SDATVAR field. This way, a TMP is generated for each System Console
specified by the user. If SDATVAR is 0 then the NCP byte in the RSP
header is used as the number or extra copies to make. If both of
these fields in the RSP Header are 0 then no extra copies are made
and only a single RSP is created. The ECHO RSP is an example of the
latter.

If the number of extra copies is determinted by GENSYS to be
greater than 0, each copy of the RSP is given a unique copy number,
The copy number is placed in the NCP field and the ASCII equivalent
is appended to the end of the Process Descriptor NAME field of each
copy. If there is not enough space for the number in the PD NAME,
part of the PD NAME will be over written. For the example TMP RSP,
GENSYS makes the specified number of copies and changes the NAME
field in each copy to be "TMPO, TMPl, TMP2,...", and sets the NCP
field to 0, 1, 2, ..., respectively.

---
_Page 073_

MP/M-86 Programmer's Guide 5.3 Multiple Copies of RSPs

5.3.1 8080 Model

When GENSYS makes copies of an 8080 Model RSP, the CS, DS, ES
and SS fields in each copy's User Data Area are set to the paragraph
address where the RSP will be in memory after loading.

5.3.2 Small Model

If multiple copies of a Small Model RSP are to be generated,
GENSYS copies both the Code and Data Groups of the RSP, if the MEM
field of the Process Descriptor is 0. See the CREATE PROCESS
function for a description of the Process Descriptor format. GENSYS
sets the UDA fields CS to the Code Segment of the RSP and DS, ES and
SS to the Data Segment of the RSP.

5.3.3 Small Model with Shared Code

If a Small Model RSP has a non-zero MEM field in its Process
Descriptor, the Code Segment is assumed to be reentrant. When
copies are made of this type of RSP only the Data Group is copied,
GENSYS sets the UDA CS field for each copy to the paragraph address
of the one Code Segment for the RSP's. The DS, ES and SS, in each
copied Data Segment, are set by GENSYS to the paragraph address of
the Data Segment for that particular copy.

5.4 Creating and Initializing an RSP

An RSP that is to be invoked from a console, or through the CLI
function (Function 150), must create a special queue called an RSP
Command Queue. Such an RSP is called a Command RSP. This type of
RSP usually performs some initialization routine and then goes into
aloop. The initialization routine consists of creating and opening
an RSP Command Queue as well as changing the priority to the default
transient process priority. (Priority values with regard to RSPs
are discussed below).

The first step of the loop is to read a message from the RSP
Command Queue. The process that writes the message to the RSP
Command Queue essentially activates the associated RSP. After the
RSP returns from the READ QUEUE function call, it obtains the system
resources it needs, such as the calling process's console.
Typically, the RSP is assigned the console resource before a message
is written to the RSP Command Queue. This is true however, only if
the Process Descriptor name matches the queue name.

When the RSP completes its activities for the given command, it
releases any system resources it has acquired, including the
console, and re-starts the loop by reading from its RSP Command
Queue. A Command RSP is a single process and is a serially re-
usable resource; i.e., the RSP acts on one message at a time. When

---
_Page 074_

MP/M-86 Programmer's Guide 5.4 Creating and Initializing a RSP

several processes attempt to invoke a single Command RSP, they will
wait as described in the READ QUEUE and CONDITIONAL READ QUEUE
function calls in Section 6. Note: it is certainly possible to
create RSPs that are invoked differently and function differently
than an Command RSP.

The format of the RSP Command Queue Message is shown in Figure

5-2.
byte: 0 1 2 3 4 see 130
+------ f----- +----- +----- +------~-------------- +
| PDADDRESS | COMMAND TAIL (129 bytes) |
f----- t----- +----- +----- $------- == +

Figure 5-2. RSP Command Queue Message

The PDADDRESS is the offset relative to the System Data Area
segment of the Process Descriptor of the process calling the RSP. A
program that wants to invoke an RSP and is forming an RSP Command
Queue Message, can find its Process Descriptor address by calling
RETURN PD ADDRESS (Function 156). The COMMAND TAIL usually contains
what the TMP sends to the CLI minus the command name, and is
terminated with a zero byte.

When a command is entered at a console, the TMP performs a CLI
function call. The CLI function attempts to open a Queue that has
the RSP Flag on and has the same name as the command sent to the
CLI. If the queue open is successful, the CLI function attempts to
assign the calling process's console to a process with the same name
as the command. If this step is also successful, the CLI function
creates an RSP Command Queue Message with the command tail sent to
the CLI from the TMP, and writes it to the RSP Command Queue (see
the discussion of the CLI function in Section 6). A transient
program can use a Command RSP in the same manner by writing directly
to the appropriate RSP Command Queue. An advantage of using the CLI
function is that it looks for an RSP first, and only searches on
disk for a CMD file if the the RSP is not found.

When an RSP reads a RSP Command Queue Message, it will often
need information about the calling process such as which console,
list device, drive or user number to use. If an RSP is invoked
through the CLI function, the RSP will have been assigned the
calling process's console, but if the RSP Command Queue was written
to directly, the calling process may or may not have assigned its
console to the RSP. A Command RSP can use the PD address in the
Command RSP Message to find out what the default devices of the
calling process are. The RSP should release any resources it
assigns to itself when it is finished.

The beginning of the RSP Data Segment has a fixed format

starting at offset 0. This data structure is the RSP Header. Note
that in the 8080 Model, the RSP Header is also in the Code Segment.

---
_Page 075_

MP/M-86 Programmer's Guide 5-4 Creating and Initializing a RSP

After the RSP Header is a Process Descriptor starting at offset
O10H. A User Data Area and a stack must also be within the Data
Segment, with the UDA placed at a paragraph boundary relative to the
beginning of the Data Segment. If system functions assuming a
default DMA buffer are used, a 128-byte DMA Buffer must also exist.
The offset of this buffer is put in the DMA OFFSET field in the User
Data Area. The DMA OFFSET can also be set by calling Function 26,
SET DMA ADDRESS once the RSP is running. The DMA SEGMENT field in
the UDA is set to the value in the DS field when a process is
created. The beginning of the RSP Data Segment is shown in Figure
5-3.

tron ~~ - - -- - ee +

OH | RSP HEADER |

ton -- --- ~~~ -- - + +

10H | PROCESS DESCRIPTOR |
trem ene e+ + ee +

40H | USER DATA AREA |
tren nen nnn nnn ee +

140H_ | STACK |
tote ee ee eee +

Figure 5-3. Beginning of RSP Data Segment

The RSP Header must be located at offset zero in the RSP Data
Segment, the RSP Process Descriptor must be at offset O10H. The
RSP User Data Area must be on a even paragraph boundary.

5.4.1 The RSP Header

As discussed in Section 5.2, the number of copies made of an
RSP is dependent on the values of the SDATVAR and NCP fields in the
RSP Header. If no copies are desired, these fields must be zero,
AS a convienence, when MP/M-86 creates the RSP process, the LINK
field in the RSP Header is set to the paragraph address of the
System Data Area. The System Data Area can always be obtained by an
RSP or transient program with the GET SYSTEM DATA ADDRESS function.

5.4.2 The RSP Process Descriptor

The RSP Process Descriptor should be initialed to zeros except
for the PRIORITY, FLAGS, NAME, and UDA SEGMENT fields. The
PRIORITY field is usually initialized to 190. This is higher than
transient programs and TMPs, (200 and 198 respectively), but lower
than the INIT process, which has one of the best possible
priorities. The description of SET PRIORITY (Function 145) in
Section 6 contains more information about system priority
asssignments. Starting an RSP ata priority of 190 ensures that the
RSP will be able to create and open an RSP Command Queue before it
can be invoked through a TMP. RSPs such as ECHO, usually set their
priority to 200 after creating and opening their RSP Command Queue

A2

---
_Page 076_

MP/M-86 Programmer's Guide 5.4 Creating and Initializing a RSP

and before attempting to read from the Queue. Note there are no
guarantees about the order in which the RSP processes are created by
the MP/M-86 Operating System. If one RSP must run before another,
it must have a higher priority. Such is the case when one RSP uses
a resource created by a second RSP; the second must run with a
priority higher than the first.

The Process Descriptor SYS and KEEP Flags can be initialized in
the RSP Data Segment. The SYS Flag allows a process to read and
write to restricted system queues. This is discussed below with
regard to RSP Command Queues. The KEEP flag signals to the
Operating System that this process cannot be terminated. This flag
is necessary if a RSP is not to be terminated when a Tc is typed on
a console being used by the RSP.

The NAME field of the RSPs Process Descriptor is 8 bytes long.
It is assumed to be left justified and padded with blanks on the
right. If an RSP Command Queue is going to be used to invoke the
RSP through the CLI, the PD must have the same upper-case name as
the Command Queue. The UDA field in the Process Descriptor must be
the offset in paragraphs of the UDA relative to the RSP data
segment.

5.4.3 The RSP User Data Area

The User Data Area must have the SP field set to the offset of
a three-word "IRET structure", in the RSP's Data Segment. The
offset is relative to the beginning of the Data Segment. The first
of the three words is the offset of the code entry point for the
RSP, relative to the beginning of the RSP Code Segment. MP/M-86
executes an IRET instruction to start the RSP using these three
words for the IP, CS and Flag registers respectively. The CS value
on the stack is initialized to be the CS field of the UDA while the
Flag value is set to 0200H (interrupts on). The RSP stack must come
immediately before these three words.

The initial values of the AX,BX,CX,DX,DI,SI and BP registers
are taken from the appropriate fields in the UDA.

The DMA OFFSET field should be set to the offset of the DMA
Buffer in the RSP's Data Segment. Except for the SP and DMA OFFSET
fields, and possibly the AX,BX,CX,DX,DI,SI, and BP fields, the
remainder of the UDA fields should be initialized to 0. The CS DS,
ES and SS fields are set by GENSYS as discussed above.

5.4.4 The RSP Stack

The RSP must manage its own stack, which is assumed to lie
within the RSP's Data Segment. This stack must be large enough to
accommodate what the RSP code will need, plus four levels (eight
bytes) to handle possible hardware interrupts. The three-word "IRET
structure" pointed to by the SP field in the RSPs UDA, is considered
part of the stack, since the 8086 Interrupt Return Instruction

---
_Page 077_

MP/M-86 Programmer's Guide 9.4 Creating and Initializing a RSP

(IRET) pops three words when the RSP starts execution.

5.4.5 The RSP Command Queue

The RSP's Command Queue contains information that determines
when it will begin execution, and which console it will be attached
to. If an RSP is to be accessable from a console via the TMP, the
Command Queue name must be in upper-case. However the command tail
put in an RSP Command Queue Message by the CLI, is not translated to
upper-case. The FLAGS field in the RSP Command Queue Descriptor
must have the RSP bit on. If this flag is not on, the CLI will not
write a message to the RSP Command Queue, and will instead attempt
to load a transient program. The KEEP flag should be set on to
protect the RSP queue from inadvertent use of the DELETE QUEUE
function.

The RESTRICTED flag makes a queue accessable only by privileged
processes. Privileged processes have the SYS Flag on in their
Process Descriptor. If the RESTRICTED Flag is on in an RSP Command
Queue, then only privileged processes can invoke the related RSP. A
lower-case letter in the RSP Command Queue name and the RESTRICTED
Flag provide two methods of filtering access to an RSP.

The Queue Descriptor of the RSP Command Queue must have have a
message length 131 bytes. The format of this message is shown
above. The number of messages will usually be 1. If the Queue
Descriptor is within 64K bytes of the beginning of the System Data
Area, buffer space for the Queue Descriptor must be allocated in the
RSP. The QBUFPTR field in the Queue Descriptor must be the offset
of this buffer, relative to the beginning of the RSPs Data Segment.
Also the Queue Buffer must be before the Queue Descriptor within the
RSP Data Segment. The buffer size is the message length times the
number of messages, usually 131 bytes.

An RSP can certainly create other queues besides the RSP
Command Queue used with Command RSPs, However, any queue an RSP
Creates that lies within 64K of the System Data Area, must have a
buffer area pointed to by the QBUFPTR field in its Queue Descriptor.
To be safe, the buffer should come before the Queue Descriptor in
the RSP's Data segment. It is assumed the QBUFPTR field points to a
buffer that is also within 64K of the System Data Area. If the
Queue Descriptor is farther than 64K from then System Data Area,
MP/M-86 will use buffer space in the System Data Area. See the
discussion of the MAKE QUEUE function call in Section 6 for more
detail.

In order to open the RSP Command Queue and subsequently read
from it, a Queue Parameter Block and its associated buffer must be
allocated in the RSP's Data Segment. These structures are treated
just as in a transient process. For any queues created by an RSP,
it is stressed that the Queue Buffer areas associated with the Queue
Descriptor and the Queue Parameter Block are separate, distinct
areas of storage.

---
_Page 078_

MP/M-86 Programmer's Guide 5.4 Creating and Initializing a RSP

5.4.6 Multiple Processes within an RSP

An RSP can create child processes by calling CREATE PROCESS
(function 144). Note that if the Process Descriptor of the process
being created is within 64K bytes of the beginning of the System
Data Area, the PD structure is used directly by MP/M-86. Otherwise
the PD structure is copied into the PD table in the System Data
Area.

5.5 Developing and Debugging an RSP

New RSPs should be debugged to as large extent as possible as
transient, CMD type programs. The first RSP that the user attempts
should be very simple, on the order of ECHO.

An RSP can be debugged in a similar manner as the XIOS, by
running MP/M-86 under DDT86 which was loaded under a CP/M-86 system.
Refer the MP/M-86 System Guide for more information about running
MP/M-86 under CP/M-86. After reading the MPM.SY5 file in under
DDT86, the RSPSEG field of the System Data Area should be found.
The paragraph address of the System Data Area is found in the ABS
field of the Data Group Descriptor in the MPM.SYS command file
Header. The CMD Header is described in Section 3.2 and the System
Data Area is described in the MP/M-86 System Guide. The RSPSEG
field contains the paragraph address of the Data Segment of the
first RSP in a linked list cof the RSPs included by GENSYS.

By using the Display Memory ("D") command of DDT86 to show
memory at the segment RSPSEG, the name of the first RSP can be
identified in the RSP's Process Descriptor. The LINK field in the
RSP Header, which will be the first word in the RSPSEG segment, is
the paragraph value of the next RSP's Data Segment. A zero in the
LINK field means the end of the list of RSPsS. Note that linkage
information is lost once MP/M-86 is initialized. The LINK field of
the RSP Header contains the System Data Segment once an RSP begins
execution.

Once the RSP to be debugged is located, the initial code entry
point may also be found. As discussed previously, the SP field in
the RSP's UDA, is the offset from the beginning of the RSP's Data
Segment, of the three-word "IRET Structure". The first word of the
"TRET structure" contains the initial value of the IP register when
MP/M-86 creates the RSP process. The initial value of the CS
register is in the CS field also in the RSP's UDA. Break points can
now be set in the RSP, similar to break points set in XIOS
functions.

---
_Page 079_

SECTION 6

SYSTEM FUNCTION CALLS

This section contains a description of each of the MP/M-86
system functions, including the parameters a process must pass when
calling the function, and the values the function returns to the
process. The reader should be familiar with the material in
Sections 1 through 5 before proceeding.

RRR KKK KKK KKK KKK KKK ARK R ERK RARE KK ERK KKK KKEREKEKEKE

\* \*
\* FUNCTION 0: SYSTEM RESET \*
\*« \*
KKRKEREKKEKKK ERE KK KKK KKK EKER RK AK KKK KKK KKK EKRKEKRKKKKKE
\* \*
\* System Reset \*
\* \*
KEK ERE RK KEKE KKK KER KKK KERR KEK KEK KK ERK EAKEKEKK KS
\* x
\* Entry Parameters: \*
\* Register CL: OOH \*
\* \*
\* Return Values: \*
\* Register CX: Error Code \*
\* \*
RRR RR KK KEK KKK KKK KKK KKK EKER ERK KAKA EE KR RKKRKEEKKKAKEKE

The SYSTEM RESET function terminates the calling process,
releasing all system resources owned by the process. In general, a
Process can own one or more of the following resources: memory
segments, consoles, printers, mutual exclusion messages, and system
Lock List entries that record open files and locked records. When a
process terminates and releases its resources, they become available
to other processes on the system. For example, if a terminating
process releases a system console, the console is usually given back
to the console's TMP, This occurs when the TMP is the highest
priority process waiting for the console.

The SYSTEM RESET function is implemented internally by
calling the TERMINATE PROCESS function (Function 143) with the
Termination Code set to zero.

Under CP/M~86, the SYSTEM RESET function has a further
argument which allows a process not to release its memory. This is
necessary to place a piece of code into memory that becomes an
interface for later programs. This option is not included under
MP/M-86. Memory segments are not recovered by the system until all
processes that own the memory segment have released it.

---
_Page 080_

MP/M-86 Programmer's Guide 6 System Calls : Function 1

KKK KKK KKK KKK RK KR KKK KEK ERK KEKE KKK KEKE KEKE KKEKEKEKEKER

\* \*
\* FUNCTION 1: CONSOLE INPUT \*
\* \*
KA KKK KKK KKK ERR KKK ERK KERR KEK KR ERK KKK K KKK EKKKEE
\* \*
\* Read a character from the default console \*
\* \*
KKK IKI KKK ERK KEKE KEKE RRR KEKE ER EKER KEKE KKK KKK
\* \*

\* Entry Parameters: \*
\* Register CL: O1H \*
\* \*
\* Return Values: \*
\* Register AL: Character \*
\* BL: Same as AL \*
\* \*
\* \*

KKK KKK KE KKK EKER RRR EERE RE KRERERKKEKE EKER EREK

The CONSOLE INPUT function reads a character from the default
console of the calling process. Before attempting the read, MP/M-86
internally calls the ATTACH CONSOLE function (Function 146) to
verify ownership of the console. If the calling process does not
own the console, it relinquishes the CPU resource until the attach
operation is successful. Typically, a process that is created
through the CLI function (Function 150) owns its default console
when it begins execution.

MP/M-86 verifies ownership of the console resource in all
console functions. This allows a user to type a TD character to
detach a process. The detached process continues execution until it
needs subsequent console I/O. It then waits until the console
becomes available before continuing.

Function 1 echoes graphic characters read from the console.
This includes the carriage return, line feed and backspace
characters. It expands tab characters (TI) in columns of eight
characters, and checks for start/stop scroll (TS/TQ) and start/stop
printer echo (TP). It also checks for the terminate character (TC)
and the detach character (TD). The terminate character causes the
system to call the TERMINATE function with the termination code set
to zero. Function 1 ignores the detach character if the calling
process cannot be terminated (see Function 143). Function 1 does
not return until a character is typed on the console. The system
suspends the calling process until a character is ready.

---
_Page 081_

MP/M-86 Programmer's Guide 6 System Calls : Function 2

RKRKKEK KEKE RK KK ERK KKK RK KKK KKK KER KRKRR ER KE KKK KK KKK

k \*
\* FUNCTION 2: CONSOLE OUTPUT \*
\* \*
RRA KKK ERK KEK KK KEE RRR KKK RE KEKE KER KEK RE KER AK KKK KK
\* \*
\* Write a character to the default console \*
\* \*
KRRKKK KK KKK EK KEK KKK KERR KKK KKK KKK ERK KK EKKRKKKKEKEK
\* \*
\* Entry Parameters: \*
\* Register CL: 02H \*
\* DL: ASCII character \*
\* x
RKKEKKKKKKKEKKKE KEE RERK KKK KKK RKERKEEKRREKREKKK KKK KKK

The CONSOLE OUTPUT function writes the specified character to
the calling process' default console. As in the CONSOLE INPUT
function (Function 1), MP/M-86 verifies that the calling process
owns it default console before actually performing the operation.
On output, Function 2 expands tabs in columns of eight characters
and checks for start/stop scroll (TS/TQ) and Start/stop printer echo
(TP). It also checks for the terminate character (TC) and the
detach character (TD).

---
_Page 082_

MP/M-86 Programmer's Guide 6 System Calls : Function 3

KKK KAI K KKK KEKE KE KKK RR KEKE KKK ERK KEKE KEKE EKE

\* \*
\* FUNCTION 3: RAW CONSOLE INPUT \*
\* \*
KKK KKK KEKE KEK IRE KEE KERR ERR KEK RRR RE KRKKKKEAKKKEK
\* \*
\* Read a character from the default console \*
\* in Raw Mode \*
\* \*
KKK KK KKK KEK KEKE KKK REE KEKE KKK RRR KER EKKKEKKKKKKK
\* \*

Entry Parameters:
Register CL: 03H

\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AL: Character \*
\* BL: Same as AL \*
\* \*
\* \*

KEKE KEK KKK KKK KERR REE KEKE RRR KERR KERR RRR KERERE

The RAW CONSOLE INPUT function reads a character from the
default console of the calling process. As in the CONSOLE INPUT
function (Function 1), MP/M-86 verifies ownership of the console
before performing the operation. Calling Function 3 places the
process in Raw Mode which means that no checking is done for special
characters such as terminate or detach. Note: The process is taken
out of Raw Mode as soon as a it calls a non-raw console function.
Calling RAW CONSOLE INPUT will force the process to relinquish the
CPU resource until a character is actually typed at the console.

MP/M-86 does not support the READER INPUT function because it
treats all character 1/0 devices such as the Reader and the Punch as
consoles. MP/M-86 places no practical limit to the number of
Character I/O devices allowed to be configured with a system.
(There is an absolute limit of 255 character I/O devices actually
allowed).

---
_Page 083_

MP/M-86 Programmer's Guide 6 System Calls : Function 4

RRR KEKE KKK KEKE KKK KEKKKKRKKRRKEKKEKEKKKKK RAK KAKA

\* \*
\* FUNCTION 4: RAW CONSOLE OUTPUT \*
\* \*
KEKE KKK KKK KK ERE KKK RAK KERR KKK RAE KKEKREK KK KKKKEK
\* \*
\* Write a character to the default console \*
\* in Raw Mode \*
\* \*
HK KREEK KEKE KK KEKE KK AEK AKER RE RRR RK EKER REKREEKEKEKER
\* \*
\* Entry Parameters; \*
\* Register CL: 04H \*
\* DL: Character \*
\* \*
RRA KKK RE REE KE RE KERRIER KEKE RE REARKERERREK

The RAW CONSOLE OUTPUT function writes a character to the
default console of the calling process. MP/M-86 verifies ownership
of the console before permitting the operation. Calling Function 4
places the process in Raw Mode which means that no checking is done
for special characters such as terminate or detach.

MP/M-86 does not support the PUNCH OUTPUT function (see
Function 3).

---
_Page 084_

MP/M-86 Programmer's Guide 6 System Calls : Function 5

KKK KKK KERR KEKE KEKE KERR EEK EKA EKER EEK KK KEKEKKKEKEEKE

\* \*
\* FUNCTION 5: LIST OUTPUT \*
\* \*
KKK KKK KKK KKK KERR RRR KRKEEKKEKRKRKEREEKRKEKKKKKKKEKKEK
\* \*
\* Write a character to the default List device \*
\* k
KK KKK KKK KKK KR RIKKI KEKE KR RRR KERR EKER ERE KK RK KRKEKREKKEE
\* \*
\* Entry Parameters: \*
\* Register CL: O5H \*
\* DL: Character \*
\* k
KKK KKK KK KIRKE KKK KK KEK KKK KER KEKE KERR RK REAR EKEKEKKEKE

The LIST OUTPUT function writes the specified character to
the default list device of the calling process. Before writing the
character, the system internally calls ATTACH LIST, (Function 158)
to verify that the calling process owns its default list device.

---
_Page 085_

MP/M-86 Programmer's Guide 6 System Calls : Function

KREKKEKKKKEKKKEKE KE KEKE KEK KEKE ERE KEKE KEK RKRKKKKKRKKKE KEK

\* \*
\* FUNCTION 6: DIRECT CONSOLE I/O \*
\* \*
KREKREKEKKKEKREREEKKEKEKEKRKKK KERR KKK KE RKEKKKKRKRREKEKREKRKEK
\* \*
\* Perform Direct Console I/0 \*
\* with default console \*
\* \*
KREKKKKEKKEKKK EKER EKER EERE KK EKER KEK REE KKREK
\* \*
\* Entry Parameters: \*
\* Register CL: 06H \*
\* DL: OFFH (Input/ \*
\* Status) or \*
\* OFEH (Status) or \*
\* OFDH (Input) or \*
\* Character (Output) \*
\* \*
\* Return Values: \*
\* Register AL: (Input/Status:) \*
\* = OH -No Character \*
\* = Character \*
\* (Status:) \*
\* = OH —- No Character \*
\* = OFFH - Ready \*
\* (Input: ) \*
\* = Character \*
\* (Output: ) \*
\* No return value \*
\* BL: Same as AL \*
\* \*
KEKEKKEKKKEKEKKEKKEKEEK EKER EEK REKEEKEEKKKKEKRKREKE

The DIRECT CONSOLE I/O function allows the calling process to
nsole I/O to its default console. MP/M-86 verifies that

do Raw co

the calling process owns its default console before allowing any

I/O.

A process calls the DIRECT CONSOLE I/O function by passing

one of th

OFFH

OFEH

OFDH

ASCII
character

ree different values shown below.

console input command (If no character if ready,
a OH is returned).

console status command (On return, register AL
contains 00 if no character is ready; otherwise
it contains FFH.)

console input command (If no character is ready,
the calling process waits until one is typed),

Function 6 assumes register DLeontains a valid

---
_Page 086_

MP/M-86 Programmer's Guide 6 System Calls : Function 6

ASCII character and sends it to the console.

There are two main differences between the DIRECT CONSOLE 1/0
function and the RAW CONSOLE functions (Function 3 and Function 4).
First, CP/M-86 does not support the RAW CONSOLE functions but does
support the DIRECT CONSOLE I/O function. Secondly, the DIRECT
CONSOLE I/O does not allow totally transparent I/O because the
calling process cannot output characters OFFH, OFEH cr OFDH. The
RAW CONSOLE functions do allow totally transparent I/0 when used in
conjunction with the console status option in the DIRECT CONSOLE I/O
function.

As with the RAW CONSOLE functions, the DIRECT CONSOLE I/0
function places the calling process in Raw Mode, and special
characters such as terminate and detach are not intercepted.

MP/M-86 performs a dispatch if the process sends a direct

console input command (OFFH), and the function returns a 0
indicating that a character is not ready.

---
_Page 087_

MP/M-86 Programmer's Guide 6 System Calls : Function 7

KHKKEKEKEKKEKKEEK ERE KEKE KER KEKE KEE KEE KEE KR RKKKEKEKKERKEKREKRAK

\* \*
\* FUNCTION 7: GET I/O BYTE \*
\* FUNCTION 8: SET I/O BYTE \*
\* \*

KEKKKKKEKRKKKKKEKEREKKEEKRKEKEKEKEKRKRERRKEKREKKKKKEKKKKKKEE

MP/M-86 does not support the GET I/O BYTE and SET I/O BYTE
functions.

REKKKKEKKKEKKEKEKEKE KEKE KEK KKEKKEKEKEKERKKEKEKE KKK KK

\* \*
\* FUNCTION 9: PRINT STRING \*
\* \*
KEKKEKEKEKEKKEKKEEK KEKE KK EERE KEKE EKKKKREK
\* \*
\* Print an ASCII String to the default console \*
\* \*
KRKEKKKEKEKEKKKKEK KKK EKER EKER ERK EERE KRKEEKEKEKKKEEK
\* \*

\* Entry Parameters; \*
\* Register CL: 09H \*
\* DX: STRING Address - Offset \*
\* DS: STRING Address - Segment \*
\* \*
\* \*

KREKEKKKEKKEKKKKEEKKEK KEKE ERE RK EKKEKREKEEKKKEEKRKEKRKKES

The PRINT STRING function prints an ASCII string starting at
the indicated STRING address, and continuing until it reaches a
dollar '$' character. Function 9 writes the string to the calling
process's default console. MP/M-86 verifies that the calling
process owns the console before writing the string. Function 9
recognizes any special characters such as terminate, detach or
Start/stop scroll. It also expands tabs in columns of eight
characters as in the CONSOLE OUTPUT function (Function 2).

---
_Page 088_

MP/M~86 Programmer's Guide 6 System Calls : Function 10

KKKKKEEKKEKKE KEKE KKK EKER KEKKERKEKEREKRKEKEKKKKEKKEKKKEKE

\* \*
\* FUNCTION 10: READ CONSOLE BUFFER \*
\* \*
KK KEKE KKK ERE KKK KEKE KKK KEKE KERR KKK ERK KKKKEKEKEKRKRKKEK
\* \*
\* Read an edited line from the default console \*
\* \*
KKK KKK KKK KK KK KKK KEKE ERK KERR RRR KE KKKKKKKKKREREKRE
\* \*

\* Entry Parameters: \*
\* Register CL: OAH \*
\* DX: BUFFER Address - Offset \*
\* DS: BUFFER Address - Segment \*
\* \*
\* \*

KKREKKEKKEEKKEKEKKKREKRER KKK KEKRKKE KEKE RKEKKKKEKKEEER

The READ CONSOLE BUFFER function reads characters from the
calling process's default console and places them into the specified
buffer. The format of the buffer is shown in Figure 6-1. Function
10 performs line editing functions on the line as it is read from
the console. The READ CONSOLE BUFFER function completes a line and
returns whenever it receives a terminator character from the
console, or the maximum number of characters is reached, As in
Function 1, the READ CONSOLE BUFFER function echoes all graphic

maha varbae ewe annA OF th + +
characters read from the console. Note: MP/M-86 verifies that the

calling process owns the default console before allowing I/O to
begin.

0 1 MAX + 2
$----- +------ fo---- +---~- +------ +------ t-f oo ft----- +
| MAX |NCHAR| CHARACTERS ... \ NI |
+----- te---- +----- +----- t----- +----- t-f 0 f/ter---- +

Figure 6-1. Console Buffer Format

MAX Maximum number of characters that can be read
into the buffer, This value must be
initialized before calling the READ CONSOLE
BUFFER function.

NCHAR Actual number of characters read into the
buffer as filled in by the READ CONSOLE BUFFER
function.

CHARACTERS Actual characters read from the console as
filled in by the READ CONSOLE BUFFER function.

---
_Page 089_

MP/M-86 Programmer's Guide 6 System Calls : Function 10

The READ CONSOLE BUFFER recognizes a number of Special
characters used in editing the input line as well as a set of
special characters that actually control the calling process.

Line Editing Characters:

RUB/DEL Removes the last character from the line
and echoes it.

<TE> Echoes new line (a Carriage Return <TM> and
a Linefeed <TJ>) to the screen but does
not affect the line buffer.

BACKSPACE <TH> Removes the last character from the line
and backspaces over that character.

TAB <TI> Echoes enough spaces to place the next
character position at a tab stop. Tab
stops are fixed at every eighth character
of the physical line.

LINE FEED <TJ> Terminates the input line. The READ
CONSOLE BUFFER function does not echo a
terminating character nor does it place
the character in the line buffer.

RETURN <TM> Terminates the input line.

REDRAW <TR> Retypes the current line after echoing a
new line.

<Tu> Removes all of the current line from the
line buffer, echoes a new line, and starts
all over again.

<TX> Removes all of the current line from the

line buffer and echoes enough backspaces
to return to the beginning of the line.

---
_Page 090_

MP/M-86 Programmer's Guide 6 System Calls : Function 10

TERMINATE (TC)

DETACH (TD)

Process Control Characters:

Attempts to terminate the calling process with
the TERMINATE function (FUNCTION 143). The

Termination Code is set to zero. If the
calling process does not terminate, the
character is ignored. Function 10 only

recognizes the detach character if it is the
first character in the line.

Detaches the calling process from its default
console. If there are any processes waiting to
attach to the console, the process with the
highest priority will then get the console. At
this point, the system sends a message
indicating which process now owns the console.
The calling process can immediately recover the
console only if no other processes are waiting.
If the DETACH character is typed during the
READ CONSOLE BUFFER function, the calling
process effectively releases the CPU resource
until the next detach character is typed. If
the detach character is typed at other times,
the process continues to execute in the
background until console I/O is performed. At
that time, the system internally calls ATTACH
CONSOLE, and the process waits until a
subsequent detach character allows the process
to own the console again.

---
_Page 091_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 11

RREKKKKKKEKRKKKERRE RRR KERR EK KKK KKK KKK EKER EEK KK KKK

\*k \*
\* FUNCTION 11: CONSOLE STATUS \*
k \*
RRR RK KK KEKE KERR KKK KR EK KK RR KR EKER RRR KEKE RRR KEKE
\* \*
\* Obtain the status of the default console \*
\* \*
RRR KEK KERR RK RR RRR KEKE RRR ERE KKK ERE KKK KKK KEKEK

\*

Entry Parameters:
Register CL: OCH

OOH not ready

k
\*
\*
\*
\* Return Values:
%
\*
\* BL: Same as AL
%
\*

x
\*
\*
x
Register AL: 01H character ready \*
\*
\*
\*
\*

RRR KKK KEE KER KEK REE KERR RRR ERE RAKE REE

The CONSOLE STATUS function checks to see if a character has
been typed at the default console of the calling process. If the
calling process is not attached to its default console, the CONSOLE
STATUS function will cause a dispatch to occur and return 00H (the
not ready condition).

---
_Page 092_

MP/M-86 Programmer’s Guide 6 System Calls : Function 12

KKK KKK KR KR KKK KR RRR RRR KEK KKK RRR KERR KEKE KHKKKEER

\* \*
\* FUNCTION 12: RETURN VERSION NUMBER \*
\*x \*k
KKK KKK KKK IKK RRR KEK KERR RK RRR KK KKKEKKEKKKK KK KKK KKK
\*k \*
\* Return BDOS Version Number \*
\* \*
KKK KKK KKK KKK KEK KKK KR KEKE KEK KKK KKK KKKRKKEKRKKEKKKKE
\* \*

Entry Parameters:
Register CL: OCH

\* \*
\* \*&
\* \*
\* Return Values: \*
\* Register AL: 30 (BDOS Version 3.0) \*
\* AH: 1l (MP/M-86) \*
\* BX: Same as AX \*
\* \*
\* \*

KK RK KKK KEK KK KERR RR KR RRR KKK RR KERR KKK KERKR KK KERR

The RETURN VERSION NUMBER function returns the BDOS file
system version number, thereby allowing version independent
programming.

The RETURN MPM VERSION function (Function 16
to obtain the MP/M version number. Function 12 in
of Operating System but not which version.

) can be called
3 ca Bhan fterna
L Ll LIS Lu

ces ype

---
_Page 093_

MP/M-86 Programmer’s Guide 6 System Calls : Function 13

RRR KKK RAKE RRR RR RK RK RK RK KEK KKK RIKER EKER EKEKK KAKA RAK

\* \*
\* FUNCTION 13: RESET DISK SYSTEM \*
\* \*
RR KKK KKK RRR KKK RR KKK KKK RK KKK KKK KERR RRR ERK KEK KKK KEK
\* \*
\* Restore all File Systems to Reset State \*
\* \*
RRR KK KKK KKK RR KK RIK KEK KERR RIKER KK RRR EKER KEE EKK KK RAKE
\* \*

Entry Parameters:
Register CL: ODH

\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AL: 0 if successful \*
\* OffH on error \*
\* BX: Same as AX \*
\* k
\* \*

RRKKK KKK KKK KER ERK RRR KKK KR RK KEK KKK KKK KEK KK KKK KKK

The RESET DISK SYSTEM function restores the file system to a
reset State where all the disk drives are set to read/write (see
Functions 28 and 29), the default disk is set to drive A, and the
default DMA address is reset to offset 080H relative to the current
DMA segment address. This function can be used, for example, by an
application program that requires disk changes during operation.
RESET DRIVE (Function 37) can also be used for this purpose,

This function is conditional under MP/M-86. If another
Process has an open file on a removeable or read/only drive, the
disk reset is denied and no drives are reset.

Upon return, if the reset operation is successful, the
function returns a0. Otherwise, it returns OFFH (255 decimal). If
the BDOS is not in the Return Error mode when an error occurs, (see
Function 45), then the system displays an error message at the
console, identifying the process owning an open file.

---
_Page 094_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 14

KKK KK KKK IKK II KKK KKK KK KKK KKK ERK ER KERR KEK KEK

\* \*
\* FUNCTION 14: SELECT DISK \*
\* \*
KOK KK IIR RK KR RK KR KKK KK KK ERK KEK KERR KKK KEK KEK REE K KKK
\* \*
\* Set calling process’s default disk \*
\* \*
KR KKK KKK KKK KIRK KKK RR KKK RRR KEK KER KERR EK EKK KKK
\* \*k
\* Entry Parameters: \*
\* Register CL: OEH \*
\* DL: Selected Disk \*
\* \*
\* Return Values: \*
\* Register AL: Error Flag \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
KKK KKK KK KKK KKK KK RR RE KKK ERK RRR REE EREREKRKEEKEE

The SELECT DISK function designates the specified disk drive
as the default disk for subsequent BDOS file operations. The
specified drive is set to 0 for drive A, l for drive B, and so-forth
through 15 for drive P in a full 16-drive system. In addition,
function 14 logs-in the designated drive if it is currently in the
reset state. Logging-in a drive activates the drive’s directory
until the next RESET DISK SYSTEM or RESET DRIVE function call.

FCBs that specify drive code zero (dr = 00H) automatically
reference the currently selected default drive. FCBs with drive
code values between 1 and 16, however, ignore the selected default
drive and directly reference drives A through P.

Upon return, register AL equal to 0 indicates the select
operation was successful. If a physical error was encountered, the
SELECT DISK function performs different actions depending on the
BDOS Error Mode (see Function 45). If the BDOS Error mode is in the
default mode, the system displays a message at the console
identifying the error, and terminates the calling process.
Otherwise, the SELECT DISK function returns to the calling process
with register AL set to OFFH and register AH set to one of the
following physical Error Codes:

O01 : Permanent error
04 : Select error

---
_Page 095_

MP/M-86 Programmer~\*s Guide 6 System Calls : Function 15

RRR RRR KKK KER KKK KR EKER ERR KEKE RAK KERR KARR KK RRR

\* \*
\* FUNCTION 15: OPEN FILE \*
\* \*
RRR RRR KER KER KKK KARE RRR KERRIER EKER RK RK KKK KK
\* \*
\* Open a Disk File \*
\* \*
RRR EKER KK RR EK RR KER KK RR ERE RKRE KKK KEK KKK KKK
\* \*

Entry Parameters:

Register CL: OFH
DX: FCB Address - Offset
DS: FCB Address - Segment

+ Fe ee FH

Register AL: Directory Code
AH: Physical or Extended Error \*

BX: Same as AX \*
\*

\*
\*
\*
\*
\*
\* Return Values:
\*
\*
\*
x
BRK KK KKK ERK KKK ARK KK RRR KR KEKE RE KR KEKE KERR ARK KEKE

The OPEN FILE function activates the indicated FCB for a file
that exists in the disk directory under the currently active user
number, or user zero. The calling process passes the address of the
FCB, with byte 0 of the FCB specifying the drive, bytes 1 through 11
Specifying the file name and type, and byte 12 specifying the
extent. Normally, the process initilaizes byte 12 to zero.
Interface attributes £5\* and f£6\* of the FCB specify the mode in
which the file is to be opened as shown below:

f5° = 0, £6" = 0 - Open in Locked Mode (default mode)
£5° = 1, f6° = 0 - Open in Unlocked Mode
£5° = 0 or 1, £6\* = 1 - Open in Read/only Mode

If the file is password protected in Read/only Mode, the correct
Password must be placed in the first eight bytes of the current DMA
Or have been previously established as the default password (see
Function 106). Note: the calling process must zero the Current
Record field of the FCB ("cr") if the file is to be accessed
sequentially from the first record.

The OPEN FILE function performs the following steps for files
opened in Locked or Read/only Mode. If the current user is non-
zero, and the file to be opened does not exist under the current
user number, the OPEN FILE function searches user zero for the file,
If the file exists under user zero, and has the system attribute
(t2°) set, the file is opened under user zero. The Open Mode is
automatically set to Read/only when this is done.

The OPEN FILE function also performs the Following action for
files opened in Locked Mode when the current user number is zero.
If the file exists in the directory under user zero, and has both
the system attribute (t2\*) set and the read/fonly attribute (tl)

---
_Page 096_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 15

set, the Open Mode is automatically set to Read/only. Note that
Read/only Mode implies the file can be concurrently accessed by
other processes if they open the file in Read/only Mode.

If the open operation is successful, Function 15 activates
the user’s FCB for read and write operations as follows: It copies
the relevant directory information from the matching directory FCB
into bytes dO through dn of the FCB. It also computes a checksum
and assigns it to the FCB. All BDOS functions that require an open
FCB (e.g. READ SEQUENTIAL) verify that the FCB checksum is valid
before performing their operation.

If the file is opened in Unlocked Mode, Function 15 sets
bytes r0 and rl of the FCB to a two-byte value called the File ID.
The File ID is a required parameter for the BDOS LOCK RECORD and
UNLOCK RECORD functions. If the Open Mode is forced to Read/only,
Function 15 sets interface attribute £8° to 1 in the user’s FCB. In
addition, the function sets attribute f7\* to 1 iff the referenced
file is password protected in Write mode and the correct password
was not passed in the DMA or did not match the default password.
The BDOS does not support write operations for an activated FCB if
interface attribute f7\* or f£8° is set tol.

The BDOS file system also creates an open file item in the
system Lock List to record a successful open file operation. While
this item exists, no other process can delete, rename, or modify the
file’s attributes. In addition, this item prevents other processes
from opening the file if the file was opened in Locked Mode. [It
also requires that other processes match the file\*s Open Mode if the
file was opened in Unlocked or Read/only Mode. Normally, this item
remains in the system Lock List until the file is permanently closed
or the process that opened the file terminates.

When the open operation is successful, the OPEN FILE function
also makes an Access date and time stamp for the opened file under
the following conditions: the referenced drive has a directory
label that requests Access date and time stamping, the opened file
has an XFCB, and the referenced drive is read/write.

Upon return, the OPEN FILE function returns a Directory Code
in register AL with the value 0 through 3 if the open was
successful, or OFFH (255 decimal) if the file was not found.
Register AH is set to 0 in both of these cases. If a physical or
extended error was encountered, the OPEN FILE function performs
different actions depending on the BDOS Error Mode (see Function
45). If the BDOS Error Mode is in the default mode, the system
displays a message identifying the error at the console and the
terminates the process. Otherwise, the OPEN FILE function returns
to the calling process with register AL set to OFFH and register AH
set to one of the following physical or extended Error Codes:

01 : Permanent error

04 : Select error

05 : File is open by another process or by the
current process in an incompatible mode

---
_Page 097_

MP/M-86 Programmer’s Guide 6 System Calls : Function 15

07 : File password error

09 : ? in the FCB file name or type field
10 : Process open file limit exceeded
11 : No room in the system Lock List

---
_Page 098_

MP/M-86 Programmer\*®s Guide 6 System Calls : Function 16

KKK IK KKK IKK KKK KKK KI KEIR RE KK EKREKKE KKK KKK KKK KR ERK

\* \*
\* FUNCTION 16: CLOSE FILE \*
\* \*
KKK KK KKK KKK KKK KKK RK KR KEKE RK KEKE KEKRKEKKEE KEKE
\* \*k
\* Close a Disk File \*
\* \*
KEK KI KKK EK KKK KK ERR KR KKK KER KEKE KERR KR KEK KKKEEKKKKRKKER
k \*
\* Entry Parameters: \*
\* Register CL: 10H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* \*
KKK KKK KE KERR RHR EK KERR RR KKK KERR ERR RE KEKEERKKE

The CLOSE FILE function performs the inverse of the OPEN FILE
function. The calling process passes the address of an FCB. The
referenced FCB must have been previously activated by a successful

Annm ~ 7 + i
OPEN or MAKE FILE function call (see Functions 15 and 22).

Interface attribute £5° specifies how the file is to be closed as
shown below:

f£5°\* = 0 - Permanent close (default mode)
£5° = 1 - Partial close

The CLOSE FILE function first verifies that the referenced
FCB has a valid checksum. If the checksum is valid and the
referenced FCB contains new information because of write operations
to the FCB, the CLOSE FILE function permanently records the new
information in the referenced disk directory. Note that the FCB
does not contain new information and the directory update step is
bypassed if only read and/or update operations have been made to the
referenced FCB. However, the CLOSE FILE function always attempts to
locate the FCB’s corresponding entry in the directory, and returns
an Error Code if the directory entry is not found.

If the CLOSE FILE function successfully performs the above
steps, and if interface attribute f5° indicates that the close is
permanent, it removes the file’s item from the system Lock List. If
the FCB was opened in Unlocked Mode, it also purges all record lock
items belonging to the file from the system Lock List. By removing
the file’s Lock List item, the CLOSE FILE function invalidates the
FCB’s checksum to ensure the referenced FCB is not subsequently used
with BDOS functions that require an open FCB (e.g. WRITE
SEQUENTIAL).

---
_Page 099_

MP/M-86 Programmer’s Guide 6 System Calls : Function 16

The CLOSE FILE function makes an Update date and time stamp
for the closed file under the following conditions: the referenced
drive has a Directory Label that requests Update date and time
stamping, the referenced file has an XFCB, the referenced drive is
read/write, and a write operation to the file was made since the FCB
was opened. None of these steps are performed for partial close
Operations (£5° = 1),

Upon return, the CLOSE FILE function returns a Directory Code
in register AL with the value 0 to 3 if the close was successful, or
OFFH (255 Decimal) if the file was not found. Register AH is set to
0 in both of these cases. If a physical or extended error was
encountered, the CLOSE FILE function performs different actions
depending on the BDOS Error Mode (see Function 45). If the BDOS
Error Mode is in the default mode, the system displays a message
identifying the error at the console and terminates the calling
process. Otherwise the CLOSE FILE function returns to the calling
process with register AL set to OFFH and register AH set to one of
the following physical or extended Error Codes:

O01 : Permanent error
02 : Read/only disk
04 : Select error
: FCB checksum error

---
_Page 100_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 17

FOR KR KR KOKI OR RK ITI KK KR RIK RR KK REAR KERR ERE KKK EK

\* \*
\* PUNCTION 17: SEARCH FOR FIRST \*
\* \*
ORK KK KK KR IKK KR IR KK RK KIRKE RRR KEKE KEK KEK KK KK KER KEK ER
\* \*
\* Find the first file that matches \*
\* the specified FCB \*
\*k \*
KOKI KK KKK KKK KEK RR KK IIR RRR KK EKER RR KEK EKER KKEEKE
\* \*

Entry Parameters:

Register CL: 11H
DX: FCB Address - Offset
DS: FCB Address - Segment

e+ FF FF

Register AL: Directory Code
AH: Physical or Extended Error
BX: Same as AX

\* +

\*

k
\*
\*
x
\*
\* Return Values:
\*
\*
\*
\*
KKK KK KR KKK IKK RI KI KERR RRR RARE KEK KK KERR ERK KEK

The SEARCH FOR FIRST function scans the directory for a match
with the specified FCB. Two types of searches can be performed.
For standard searches, the calling process initializes bytes 0
through 12 of the referenced FCB, with byte 0 specifying the drive
directory to be searched, bytes 1 through ll specifying the file or
files to be searched for, and byte 12 specifying the extent.
Normally byte 12 is set to zero. An ASCII question mark (63
decimal, 3F hex) in any of the bytes 1 through 12 matches all
entries on the directory in the corresponding position. This
facility, called ambiguous reference, can be used to search for
multiple files on the directory. When called in the standard mode,
the search function scans for the first file entry in the specified
directory that matches the FCB and belongs to the current user
number.

The SEARCH FOR FIRST function also initializes the SEARCH FOR
NEXT function. After the search function has located the first
directory entry matching the referenced FCB, the SEARCH FOR NEXT
function can be called repeatedly to locate all remaining matching
entries. In terms of execution sequence, however, the SEARCH FOR
NEXT call must either follow a SEARCH FOR FIRST or SEARCH FOR NEXT
call with no other intervening BDOS disk related function calls.

If byte 0 of the referenced FCB is set to a question mark,
Function 17 ignores the remainder of the referenced FCB and locates
the first directory entry residing on the current default drive.
All remaining directory entries can be located by making multiple
SEARCH FOR NEXT calls. This type of search operation is not
normally made by application programs, but it does provide complete
flexibility to scan all current directory values. Note that this
type of search operation must be performed to access a drive’s

---
_Page 101_

MP/M-86 Programmer’s Guide 6 System Calls : Function 17

Directory Label (see Section 2.2.5).

Upon return, the SEARCH FOR FIRST function returns a
Directory Code in register AL with the value 0 to 3 if the search
was successful, or OFFH (255 Decimal) if a matching directory entry
was not found. Register AH is set to zero in both of these cases.
For successful searches, the current DMA is also filled with the
directory record containing the matching entry, and the relative
starting position is AL \* 32 (i.e. rotate the AL register left 5
bits). Although not normally required for application programs, the
directory information can be extracted from the buffer at this
position.

If a physical error was encountered, the SEARCH FOR FIRST
function performs different actions depending on the BDOS Error Mode
(see Function 45). If the BDOS Error Mode is in the default mode,
the system displays a message identifying the error at the console
and terminates the calling process. Otherwise, it returns to the
calling process with register AL set to OFFH and register AH set to
one of the following physical Error Codes:

O01 : Permanent error
04 : Select error

---
_Page 102_

MP/M-86 Programmer~s Guide 6 System Calls : Function 18

KKK KKK KKK KK KKK KKK ERK KKK KEKE EERE KKEKEKKKEKEKKKKEE

\* \*
\* FUNCTION 18: SEARCH FOR NEXT \*
\* \*
KKK KKK KKK KKK KKK KKK REE KKKKKRKKKKKKEKEKRKEKRKKKKRKKRKKKKEK
\* \*
\* Find a subsequent file that matches the \*
\* specified FCB of a previous Search for First \*
\* \*
KKK RRR RK EKER KKK RRR KK KEKE KKK KKK KKKKKKKKAK KKK KKKEEE
\* \*
\* Entry Parameters: \*
\* Register CL: 12H \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* \*
KKK RRR KK RIK KER KR KEK KEK RKEK RRR KEKE RE KREREKRKREKK ER

The SEARCH FOR NEXY function is identical to the SEARCH FOR
FIRST function, except that the directory scan continues from the
last entry that was matched. Function 18 returns a Directory code
in register A, analogous to Function 17. Note: In execution
sequence, a Function i8 calli must follow either a Function 17 or
another Function 18 call with no other intervening BDOS disk-related
function calls.

---
_Page 103_

MP/M-86 Programmer’s Guide 6 System Calls : Function 19

RRKKKEKKKEEKKEKRERKR RRR EREEKRKEKEKKKKEKEEKREKKKRKRKKKREKE

\* \*
\* FUNCTION 19: DELETE FILE \*
\* \*
RRKKKKEKKEKKEKRKKEKKEKEEEKEKEKEKEKKKKKEKKRKEKRRKKKKKRKK KEKE
\* \*
\* Delete a Disk File \*
\* \*
KREKKKKEREKEKRKEKKEKKKEEEKERKERKEKKEREKKKRKRRKEKKRKKKKKEKKRRKRKRKREE
\* \*
\* Entry Parameters: \*
\* Register CL: 13H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* \*
KREKKEKKEKEK KEE KEK KEKE KERR ERK KEKE RKKEKKKAEKK

The DELETE FILE function removes files and/or XFCBs that
match the FCB addressed in register DX. The filename and type may
contain ambiguous references (i.e., question marks in bytes fl
through t3), but the "dr" byte cannot be ambiguous, as it can in the
SEARCH FOR FIRST and SEARCH FOR NEXT functions. Interface attribute
£5° specifies the type of delete operation to be performed as shown
below:

£5°
£57

0 - Standard Delete (default mode)
1 - Delete only XFCBs

If any of the files specified by the referenced FCB are password
protected, the correct password must be placed in the first eight
bytes of the current DMA buffer, or have been previously established
as the default password (see Function 106).

For standard delete operations, the DELETE FILE function
removes all directory entries belonging to files that match the
referenced FCB. All disk directory and data space owned by the
deleted files is returned to free space, and becomes available for
allocation to other files. Directory XFCBs that were owned by the
deleted files are also removed from the directory. If interface
attribute £5° of the FCB is set to 1, Function 19 deletes only the
directory XFCBs matching the referenced FCB. Note: If any of the
files matching the input FCB specification fail the password check,
are read/only, or are currently open by another process, then the
DELETE FILE function deletes no files or XFCB’s. This applies to
both types of delete operations.

A process can delete a file that it currently has open if the
file was opened in Locked Mode. However, the BDOS returns a
checksum error if the process makes a subsequent reference to the

---
_Page 104_

MP/M-86 Programmer’s Guide 6 System Calls : Function 19

file with a BDOS function requiring an open FCB. Files open in
Read/only or Unlocked Mode cannot be deleted by any process.

Upon return, the DELETE FILE function returns a Directory
Code in register AL with the value 0 to 3 if the delete was
successful, or OFFH (255 Decimal) if no file matching the referenced
FCB was found. Register AH is set to 0 in both of these cases. If
a physical or extended error was encountered, Function 19 performs
different actions depending on the BDOS Error Mode (see Function
45). If the BDOS Error Mode is the default mode, the system
displays a message identifying the error at the console and
terminates the calling process. Otherwise, it returns to the
calling process with register AL set to OFFH and register AH set to
one of the following physical or extended Error Codes:

Ol : Permanent error

02 : Read/fonly disk

03 : Read/fonly file

04 : Select Error

05 : File open by another process or open
in Read/only or Unlocked Mode

07 : File password error

---
_Page 105_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 20

KRREREKEK KEKE ERR KEKE KKK RK K ERR EER K RE KEK KKK RK KR KRK RR EK

\* &
\* FUNCTION 20: READ SEQUENTIAL \*
\* \*
RRR EKER KEK KKK KKK KEKE IKK RRR KR KEK KKK ERK ERE KEKE ER
\* x
\* Sequentially Read Records from a Disk File \*
\* \*
KERR KEK KEKE RRR KEE RE KKK KK KER KERR ERR RRR KKKEKREKEEER
\* \*
\* Entry Parameters: \*
\* Register CL: 14H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* k
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
HR KKK KE KK KERR KR KEKE KKK KEKE KEE KE KK ERR RRR EK KKK

The READ SEQUENTIAL function reads the next one to sixteen
128-byte records from a file into memory beginning at the current
DMA address. The BDOS Multi-Sector Count (see Function 44)
determines the number of records to be read. The default is one
record. The addressed FCB must have been previously activated by an
OPEN or MAKE FILE function call.

Function 20 reads each record from byte "cr" of the extent,
then automatically increments the "cr" field to the next record
position. If the "cr" field overflows then the function
automatically opens the next logical extent and resets the "cr"
field to 0 in preparation for the next read operation. The calling
Process must set the "cr" field to 0 following the open call if the
intent is to read sequentially from the beginning of the file.

Upon return, the READ SEQUENTIAL function sets register AL to
zero if the read operation was successful. Otherwise, register AL
contains an error code identifying the error as shown below:

Ol : Reading unwritten data (end of file)
09 : Invalid FCB

10 : FCB checksum error

ll : Unlocked file verification error
255 : Physical error; refer to register H

The function returns Error Code 01 if no data exists at the
next record position of the file. Normally, the no data situation
is encountered at the end of a file. However, it can also occur if
an attempt is made to read a data block that has not been previously
written, or an extent that has not been created. These situations
are usually restricted to files created or appended with the BDOS
random write functions (Functions 34 and 40).

---
_Page 106_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 20

The function returns Error Code 09 if the FCB was invalidated
by a previous BDOS random read or write call that returned an error.
A READ RANDOM call (Function 33) for an existing record in the file,
can be made to revalidate the FCB.

The function returns Error Code 10 if the referenced FCB
failed the FCB checksum test.

The function returns Error Code 11 if the BDOS cannot locate
the FCB’s directory entry when attempting to verify that the
referenced FCB contains current information. The function only
returns this error for files opened in Unlocked Mode.

The function returns Error Code 255 if a physical error was
encountered and the BDOS is in Return Error mode or Return and
Display Error mode (See Function 45). If the Error Mode is the
default mode, the system displays a message at the console
identifying the physical error, and terminates the calling process.
When the function returns a physical error to the calling process,
it is identified by the four low-order bits of register AH as shown
below:

O01

Permanent error
Select error

The READ SEQUENTIAL function also sets the four high-order
bits of register AH on all error returns when the BDOS Multi-Sector
Count is greater than one. In this case, the four bits contain an
integer set to the number of records successfully read before the
error waS encountered. This value can range from 0 to 15. The four
high-order bits of register AH are always zeroed when the Multi-
Sector Count is equal to one.

---
_Page 107_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 21

RRR KERR KKK KKK RRR KKK KKK EKER KKK KAKA KKK RRR KK

\* \*
\* FUNCTION 21: WRITE SEQUENTIAL \*
\* \*
RREKKEKKE EEK KR KER KKK KEK KERR RRR RKRKERREKKREKRE KK KKK KEK
\* \*
\* Sequentially Write Records to a Disk File \*
\* \*
RRR KK KKK IKK RR EKER KKK KKREKR KEKE RE KKK KK KKK
\* \*
\* Entry Parameters: \*
\* Register CL: 15H \*
\* DX: FCB Address —- Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* x
RRKKKKKEKEKEKKR KEE KEKE KEKEKEKEKREKKK KKK KKK KR KEKE KKK KKK

The WRITE SEQUENTIAL function writes one to sixteen 128-byte
data records beginning at the current DMA address into the file
named by the specified FCB. The BDOS Multi-Sector Count (see
Function 44) determines the number of 128-byte records that are
written. The default is one record. The referenced FCB must have
been previously activated by a BDOS OPEN or MAKE FILE function call.

Function 21 places the record into the file at the position
indicated by the "cr" byte of the FCB, and then automatically
increments the "cr" byte to the next record position. If the "cr"
field overflows, the function automatically opens or creates the
next logical extent and resets the "cr" field to 0 in preparation
for the next write operation. If Function 21 is used to write to an
existing file, then the newly-written records overlay those already
existing in the file. The calling process must set the "cr" field
to 0 following an OPEN or MAKE FILE Function call if the intent is
to write sequentially from the beginning of the file.

Upon return, the WRITE SEQUENTIAL function sets register AL
to zero if the write operation was successful. Otherwise, register
AL contains an error code identifying the error as shown below:

01 : No available directory space

02 : No availabel data block

08 : Record locked by another process
09 : Invalid FCB

10 : FCB checksum error
11 : Unlocked file verification error

255 : Physical error : refer to register AH

---
_Page 108_

MP/M-86 Programmer’s Guide 6 System Calls : Function 21

The function returns Error Code 01 when it atempts to create
a new extent that requires a new directory entry and no available
directory entries exist on the selected disk drive.

The function returns Error Code 02 when it attempts to
allocate a new data block to the file and no unallocated data blocks
exist on the selected disk drive.

The function returns Error Code 08 if it attempts to write to
a record locked by another process. The function only returns this
error for files open in Unlocked Mode.

The function returns Error Code 09 if the FCB was invalidated
by a previous BDOS random read or write call that returned an error.
A READ RANDOM call (Function 33) for an existing record in the file
can be made to revalidate the FCB.

The function returns Error Code 10 if the referenced FCB
failed the FCB checksum test.

The function returns Error Code 11 if the BDOS cannot locate
the FCB’s directory entry when attempting to verify that the
referenced FCB contains current information. The function only
returns this error for files open in Unlocked Mode.

The function returns Error Code 255 if a physical error was
encountered and the BDOS is in Return Error mode or Return and
Display Error mode (See Function 45). If the Error Mode is the
default mode, the system displays a message at the console
identifying the physical error and terminates the calling process.
When the function returns a physical error to the calling process,
it is identified by the four low-order bits of register AH as shown
below:

01: Permanent error
02 : Read/only disk
03 : Read/fonly file or
File open in Read/only Mode or
File password protected in Write mode
04 : Select error

The WRITE SEQUENTIAL function also sets the four high-order bits of
register AH on all error returns when the BDOS Multi-Sector Count is
greater than one. In this case, the four bits contain an integer
set to the number of records successfully written before the error
waS encountered. This value can range from zero to 15. The four
high-order bits of register AH are always zeroed when the Multi-
Sector Count is equal to one.

---
_Page 109_

MP/M-86 Programmer’s Guide 6 System Calls : Function 22

RRR KKK KR RARER KKK KKK RRR KEK K RRR RR KEK KKK EEK AKER KK

\* \*
\* FUNCTION 22: MAKE FILE \*
\* \*
KEKE KER KEK KEKE KEK KR KKK KER KEK KERR KEE KEKE KE KKK KKK
\* \*
\* Create a Disk File \*
\* \*
RRKKK EK KEKE RRR KR KR KE RK RRR KKK KR KERR RE KEKE KKERRE
\* \*
\* Entry Parameters: \*
\* Register CL: 16H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* \*
RRR KKK KEK KK KER ERE KER RAE KKE KEKE EKER KEKE

The MAKE FILE function creates a new directory entry for a
file under the current user number. It also creates an XFCB for the
File if the referenced drive has a Directory Label that invokes
automatic creation of XFCBs. The calling process passes the address
of the FCB with byte 0 of the FCB specifying the drive, bytes 1
through 11 specfying the file name and type, and byte 12 set to the
extent number. Normally, byte 12 is set to zero. Byte 32 of the
FCB (the "cr" field) must be initialized to zero (before or after
the MAKE FILE call) if the intent is to write sequentially from the
beginning of the file.

Interface attribute £5° specifies the mode in which the file
is to be opened. Interface attribute f£6° specifies whether a
password is to be assigned to the created file. The interface
attributes are summarized below:

£5° = 0 - Open in Locked Mode (default mode)
£5° = 1 - Open in Unlocked Mode

£6° = 0 - Don’t assign password (default)
f£6° = 1 - Assign password to created file

When attribute £6° is set to 1, the calling process must place the
password in the first 8 bytes of the current DMA buffer and set byte
9 of the DMA buffer to the password mode (See Function 102).

The MAKE FILE function returns with an Error Code if the
referenced FCB names a file that currently exists in the directory
under the current user number. If there is any possibility of
duplication, a DELETE FILE call should precede the MAKE FILE call.

---
_Page 110_

MP/M-86 Programmer’s Guide 6 System Calls : Function 22

If the make operation is successful, it activates the
referenced FCB for file operations (opens the FCB) and initializes
both the directory entry and the referenced FCB to an empty file.
It also computes a checksum and assigns it to the FCB. BDOS
functions that require an open FCB (e.g. WRITE RANDOM) verify that
the FCB checksum is valid before performing their operation. If the
file is opened in Unlocked Mode, the function sets bytes r0 and rl
in the FCB to a two-byte value called the File ID. The File ID isa
required parameter for the BDOS LOCK RECORD and UNLOCK RECORD
functions. Note that the MAKE FILE function intializes all file
attributes to zero.

The BDOS file system also creates an open file item in the
system Lock List to record a successful make file operation. While
this item exists, no other process can delete, rename, or modify the
file’s attributes.

If the referenced drive contains a Directory Label that
invokes automatic creation of XFCBs, the MAKE FILE function creates
an XFCB and makes a Creation date and time stamp for the created
file. Note: the Creation time stamp is not made (the XFCB Creation
time stamp field is set to zeros) if an XFCB is assigned to a file
by the WRITE FILE XFCB function. If interface attribute f£6° of the
FCB is 1, the MAKE FILE function also assigns the password passed in
the DMA to the file.

Upon return, the MAKE FILE function returns a Directory Code
in register AL with the value 0 through 3 if the make operation was
successful, or OFFH (255 decimal) if no directory space was
available. Register AH is set to zero in both of these cases. Ifa
physical or extended error was encountered, the MAKE FILE function
performs different actions depending on the BDOS Error Mode (see
Function 45). If the BDOS Error Mode is the default mode, the
system displays a message at the console identifying the error and
terminates the calling process. Otherwise, it returns to the
calling process with register AL set to OFFH and register AH set to
one of the following physical or extended Error Codes:

Ol : Permanent error

02 : Read/only disk

04 : Select error

08 : File already exists

09 : ? in file name or type field

10 : Process open file limit exceeded
11 : No room in the system Lock List

---
_Page 111_

MP/M-86 Programmer’s Guide 6 System Calls : Function 23

RRR KK ERK KKK KER KKK RR E KK E RK KKRRKERKKRKRKERE KKK KKK

k \*
\* FUNCTION 23: RENAME FILE \*
\* \*
RRR KEKE ERK KE REKRKE KKK RIE KKK KR KERR KKK KEKE ARK ERK KKK
\* \*
\* Rename a Disk File \*
\* k
HRRE RRR KKK KKK KEK ERR KERR KKK ARERR KEEKEKR KAR KKK KKK ERE KK
\* \*
\* Entry Parameters: \*
\* Register CL: 17H \*
\* DX: FCB Address — Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* \*
RR KKK KH KERR KEK KK KE RRR KK IKE KERR KKK EKER ERK RIK

THe RENAME FILE function uses the indicated FCB to change all
directory entries of the file specified by the filename in the first
16 bytes of the FCB to the filename in the second 16 bytes. If the
file specified by the first filename is password protected, the
correct password must be placed in the first eight bytes of the
current DMA buffer, or have been previously established as the
default password (See Function 106). The calling process must also
ensure that the filenames specified in the FCB are valid and
unambiguous, and that the new filename does not already exist on the
drive. Function 23 uses the "dr" code at byte 0 of the FCB to
Select the drive. The drive code at byte 16 of the FCB is ignored.

A process can rename a file that it has open if the file was
Opened in Locked Mode. However, the BDOS will return a checksum
error if the process subsequently references the file with a
function requiring an open FCB. A file open in Read/only or
Unlocked Mode cannot be renamed by any process.

Upon return, the RENAME FILE function returns a Directory
Code in register AL with the value 0 to 3 if the rename was
successful, or OFFH (255 Decimal) if the file named by the first
file name in the FCB was not found. Register AH is set to zero in
both of these cases. If a physical or extended error was
encountered, the RENAME FILE function performs different actions
depending on the BDOS Error Mode (see Function 45). If the BDOS
Error Mode is the default mode, the system displays a message at the
console identifying the error, and terminates the process.
Otherwise, it returns to the calling process with register AL set to
OFFH and

---
_Page 112_

MP/M-86 Programmer’s Guide 6 System Calls : Function 23

register AH set to

Codes:

O01

one of the following physical or extended Error

Permanent error
Read/fonly disk
Read/fonly file
: Select error
: File open by another process
: File password error
: File already exists
? in filename or type field

---
_Page 113_

MP/M-86 Programmer’s Guide 6 System Calls : Function 24

RRR KK KK KK EKER EKER KK KEKE KEK ERK KKK RRR KKK KEKE KKEKE

\* \*
\* FUNCTION 24: RETURN LOGIN VECTOR \*
\* \*
REKKKKKKEE KEKE KER KR RE KK KEKE KEK RRR KKKKKKKRKR KKK K
\* \*
\* Return Bit Map of Logged in Disk Drives \*
\* \*
RREKKKKEK KERR KEKE KR KKK KEKE KEKE EKER RR RRR KKK ERK EKER KE
\* \*

\* Entry Parameters: \*
\* Register CL: 18H \*
\* \*
\* Return Values: \*
\* Register AX: Login Vector \*
\* BX: Same as AX \*
\* \*
\* \*

RREKKEKKKEKKEK KERR KERR RR KERR KEK KER EEKKEEREKRKK KEKE

The RETURN LOGIN VECTOR function returns a bit map of
currently logged-in disk drives. The login vector is a 16-bit value
with the least significant bit corresponding to drive A, and the
high-order bit corresponding to the 16th drive (drive P). A "O" bit
indicates that the drive is not on-line, while a "1" bit indicates
the drive is active. A drive is made active by either an explicit
BDOS SELECT DISK call (Function 14), or an implicit selection when a
BDOS file operation specifies a non-zero "dr" byte in the FCB.

---
_Page 114_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 24

KKK KKK KKK KKK KKK KIRKE KR RRR KKK KKEKREKEKKKKEKKKKKKKKEKK

\* \*
\* FUNCTION 25: RETURN CURRENT DISK \*
\* \*
KKK KKK IKK KKK RRR RK KKK RRR EKER KEKE KEKKKKKKKKKKKRKREE
\* \*
\* Return the Calling Process\*s Default Disk \*
\* \*
KKK KKK KK KKK RR RRR KKK RE KKEKKEKEKKERKKRRKKKKKKKEKEK
\* &

Entry Parameters:
Register CL: 19H

k
\*
\*
\* Return Values:
\*
\* BL: Same as AL
\*
\*

\*

\*

\*

\*

Register AL: Login Vector \*
\*

\*

\*

KREKKKEKKKKKEKR KERR KEKE KEK KEK KERR KKEEKKEKEREKKRKKKEKE

The RETURN CURRENT DISK function returns the calling
process’s currently selected default disk. The disk numbers range
from 0 through 15 corresponding to drives A through P.

---
_Page 115_

MP/M-86 Programmer’s Guide 6 System Calls : Function 26

RRR KEKE ERK KK KERR KEK EERE KKKKK RK KAKA K KEK KK

\* \*
\* FUNCTION 26: SET DMA OFFSET \*
\* \*
KR KEKE KKK KKK KR KER KKK KEKE KERR RE KREKKKEE KR RKKKEKKREK
\* \*
\* Set the Direct Memory Address Offset \*
\* \*
RRKKEEKEK EKER EKER KEK KERR EKER EKER RKEKRKKEKKEKK KKK KKK KEK
\* \*
\* Entry Parameters: \*
\* Register CL: 1AH \*
\* DX: DMA Address - Offset \*
\* \*
HR KKK KER ERK KERRI KEK KEK RR KARR RK KEK ERK RIKER ERE KERRIER

"DMA" is an acronym for Direct Memory Address, which is often
used in connection with disk controllers that directly access the
memory of the computer to transfer data to and from the disk
subsystem. Under MP/M-86, the current DMA is usually defined as the
buffer in memory where a record resides before a disk write and
after a disk read operation. If the BDOS Multi-Sector Count is
equal to one (see Function 44), the size of the buffer is 128 bytes.
However, if the BDOS Multi-Sector Count is greater than one, the
size of the buffer must equal N \* 128, where N equals the Multi-
Sector Count.

Some BDOS functions also use the current DMA to pass
parameters and to return values. For example, BDOS functions that
check and assign file passwords, require that the password be placed
in the current DMA. As another example, GET DISK FREE SPACE
(Function 46) returns its results in the first 3 bytes of the
current DMA. When the current DMA is used in this context, the size
of the buffer in memory is determined by the specific requirements
of the called function.

When the CLI function initiates a transient vrogram, it sets
the DMA offset to 080H and the DMA Segment or Base to its initial
Data Segment. RESET DISK SYSTEM (Function 13) also sets the DMA
offset to 080H. The SET DMA OFFSET function can change this default
value to another memory address. The DMA address remains at its
current value until it is changed by a SET DMA OFFSET, Set DMA BASE
Or RESET DISK SYSTEM call.

---
_Page 116_

MP/M-86 Programmer’s Guide 6 System Calls : Function 27

KK IKK KKK IKK KKK KKK KEE KKK KKK KEK KER KKK KKRKK KKK KKK KEK

k x
\* FUNCTION 27: GET ADDR (ALLOC) \*
\* x
KOK KKK KK RRR RK KKK KKK KR RRR KKK RK KK EK KKK KEKE RRR KAR KK
k \*
\* Get Allocation Vector Address \*
\* \*
KKK KK KK RR KKK KKK KER KKK IK RR KKK RRR KER KK KERR KERR RE KK
\* \*

Entry Parameters:
Register CL: 1BH

\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: ALLOC Address - Offset \*
\* BX: Same as AX \*
\* ES: ALLOC Address - Segment \*
\* \*
\* \*

KKK KK RR RRR RRR RK KK KR RRR KKK KK RRR KE KREKEK KK KEREKEK

MP/M-86 maintains an “allocation vector" in main memory for
each active disk drive. Many programs commonly use the information
provided by the allocation vector to determine the amount of free
data space on a drive. Note, however, that the allocation
information may be inaccurate if the drive has been marked
read/only.

Function 27 returns the base address of the allocation vector

for the currently selected drive. If a physical error is
encountered when the BDOS Error Mode is one of the return modes (see
Function 45), Function 27 returns the value OFFFFH in AX.

GET DISK FREE SPACE (Function 46), can be used to directly
return the number of free 128-byte records on a drive. In fact, the
MP/M-86 utilities that display a drive’s free space (STAT,SDIR, and
SHOW) use Function 46 for that purpose.

---
_Page 117_

MP/M-86 Programmer’s Guide 6 System Calls : Function 28

KEKRKKEEKEKKEKRKRER EKER EEKKEKEKEKEKEKRKEKRKRKERRERKERKKEKKEKRKEK

\* x
\* FUNCTION 28: WRITE PROTECT DISK \*
\* \*
KHEKKEKKEKKKKRKREKRKEE KKK KEKE RRR EKER ERE KEKEKKKEKKKEKKEKEE
\* \*
\* Set Default Disk to Read Only \*
\* \*&
KEKE KKK RRR EK KEKKKERKKKKKEKEEKERKKKEKRKEKKEKRKEKKEKK
\* : \*

Entry Parameters:
Register CL: 1CH

Register AL: Return Code

\*
\*
\*
\*
\*
BL: Same as AL \*
\*
\*

k
\*
\*
\* Return Values:
\*
\*
\*
k

REKRKEKKEEKREKREKRKREKEKREREEKREKREKREKRKEKRRKERKEKERERRRKRKKKEE

The WRITE PROTECT DISK function provides temporary write
protection for the currently selected disk by marking the drive as
read/only. No process can write to a disk that is in the read/only
state. A successful drive reset operation must be performed for a
read/only drive to restore it to the read/write state (see Functions
13 and 37).

The WRITE PROTECT DISK function is conditional under MP/M-86.
If another process has an open file on the drive, the operation is
denied and the function returns the value OFFH to the calling
process. Otherwise, it returns a 0. Note that a drive in the
read/only state cannot be reset by a process if another process has
an open file on the drive.

---
_Page 118_

MP/M~86 Programmer~s Guide 6 System Calls : Function 29

KREEEKKEKEKKKKEKKKKEKKKKKKKEKKKKRKKRKRKEKR KKK KK KKK KKK KKK K

\* \*
\* FUNCTION 29: GET READ/ONLY VECTOR \*
\* \*
KRKREKKEKEKRKEKEKEKRKEKKEE KEKE KKKKKKKRKKKRKRKKKRKKKKRKaKKK KKK
\* \*
\* Return Bit Map of Read Only Disks \*
\* \*
KKK KRKEKKEKKKEEKEKEKKEKKKKK KAKA KKKKKKKKKK KKK KKK KEKE
\* \*

Entry Parameters:
Register CL: 1DH

\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: R/O Vector \*
\* BX: Same as AX \*
\* \*
\* \*

KRREKEKEKKEKKEEKEKEKKEKREKEKKKRKRKKKEKKRKKR KKK RRA KKK

Function 29 returns a bit vector indicating which drives have
the temporary read/only bit set. The read/only bit is set either by
a BDOS WRITE PROTECT DISK call, or by the automatic software
mechanisms within MP/M-86 that detect changed disk media.

The format of the bit vector is analagous to that of the
login vector returned by Function 24. The least significant bit
corresponds to drive A, while the most significant bit corresponds
to drive P.

---
_Page 119_

MP/M-86 Programmer”~s Guide 6 System Calls : Function 30

RREKEKEKKKEKEKRKEKKEKERKEKRKEKKEEKRKEKKEKKKEEKRKKKKKRKRKKKKKKKKKKKEKEE

\* \*
\* FUNCTION 30: SET FILE ATTRIBUTES \*
k \*
KKK KEKREKEKRRKEKEKEKREKRERKKEKEKRKEEKEREKRKEKKKKEKKEKKKEKKKKEKE
\* \*
\* Set the Attributes of a Disk File \*
\* \*
KEKE KREKEKRKEKRREEEREKKEEKREKRKEKRKKRKKKKKKKRKRKKRKRKRKKEK
x \*
\* Entry Parameters: \*
\* Register CL: 1EH \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
x \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* BL: Same as AL \*
k k
RRKKEKKEKEKEKEREKRREEKEEKRKEKEER ERR RKEKRKEKREKEEKKKRKKEKEKRKKKKKKEK

The SET FILE ATTRIBUTES function is the only BDOS function
that allows a program to manipulate file attributes. Other BDOS
functions can interrogate these file attributes but cannot change
them. The file attributes that can be set or reset by Function 30
are: f1° through £4°\*, R/O (tl\*), System (t2°), and Archive (t37%).
The specified FCB contains a filename with the appropriate
attributes set or reset. The calling process must ensure that it
does not specify an ambiguous filename. In addition, if the
specified file is password protected, the correct password must be
placed in the first eight bytes of the current DMA buffer, or have
been previously established as the default password (See Function
106).

Function 30 searches the FCB specified directory for an entry
belonging to the current user number that matches the FCB specified
mame and type fields. The function then updates the directory to
contain the selected indicators. File attributes tl”, t2°\*, and t3”
are defined by MP/M-86. They are described in Section 2.2.4.
Attributes f1° through f4° are not presently used, but may be useful
for application programs, because they are not involved in the
matching process used by the BDOS during OPEN FILE and CLOSE FILE
operations. Indicators £5° through £8° are reserved for use as
interface attributes.

This function is not performed if the file specified by the
referenced FCB is currently open for another process. It is
performed, however, if the referenced file is open for the calling
process in Locked Mode. After successfully setting the attributes
Of a file opened by the calling process, the BDOS will return a
checksum error on any subsequent file reference requiring an open
FCB. Function 30 does not set the attributes of a file currently

---
_Page 120_

MP/M-86 Programmer’s Guide 6 System Calls : Function 30

open in Read/only or Unlocked Mode for any process.

Upon return, Function 30 returns a Directory Code in register
AL with the value 0 to 3 if the function was successful, or OFFH
(255 Decimal) if the file specified by the referenced FCB was not
found. Register AH is set to zero in both of these cases. If a
physical or extended error was encountered, the SET FILE ATTRIBUTES
function performs different actions depending on the BDOS Error Mode
(see Function 45). If the BDOS Error Mode is the default mode, the
system displays a message at the console identifying the error and
terminates the process. Otherwise, it returns to the calling
process with register AL set to OFFH and register AH set to one of
the following physical or extended Error Codes:

O01 : Permanent error

02 : Read/only disk

04 : Select error

05 : File open by another process
07 : File password error

09 : ? in file name or type field

---
_Page 121_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 31

KKK RRR KR KKK KEKE KK RE RRR KR KEKE KEKE KERR KEKE KKK KKK

\* \*
\* FUNCTION 31: GET ADDR (DISK PARMS) \*
\* \*
RRKER KEKE KER EKER KERRIER RK KKK KKKKEKKK KKK KEKE
& \*
\* Return Address of Disk Parameter Block \*
\* for Calling Process\*s Default Disk \*
\*® \*®
KRREKRKEK KERR KERR RE KRRKRKEKRHEKEEKKKEREKKEKR KKK KKK KKK KEK KKK
\* k
\* Entry Parameters: \*
\* Register CL: 1FH \*
\* \*
\* Return Values: \*
\* Register AX: DPB Address - Offset \*
\* OFFFFH - on Physical Error \*
\* BX: Same as AX \*
\* ES: DPB Address — Segment \*
\* \*
RRR KER KKK KERR ER KEK KEK KR KERR RK RR KR KR RK EKER KR KAR

Function 31 returns the address of the xlIOS-resident Disk
Parameter Block (DPB) for the currently selected drive. (Refer to
the MP/M-86 System Guide for the format of the DPB). The calling
process can use this address to extract the disk parameter values
for display or to compute the space on a drive.

If a physical error is encountered when the BDOS Error Mode is

one of the return modes (See Function 45), Function 31 returns the
value OFFFFH.

---
_Page 122_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 32

PKK IKK KKK KK KK IKK IKE KEKE KEKE KER KEKRKEKKEKEEKKKKKKEKKE

\* \*
\* FUNCTION 32: SET/GET USER CODE \*
® \*&
KKK IKK IKK RK KEKE KKK RE KERR RK KEKEKKKKKEKRKEKER
x k
\* Set of Return the Calling Process’s \*
\* Default User Code \*
\* \*
KKK KKK KK KEKE KEK KERR KKK ERR RRR EKRKEKERKKKKKKKKEKK
k \*
\* Entry Parameters: \*
\* Register CL: 20H \*
\* DL: OFFH to GET USER CODE \*
\* User Code to SET \*
\* \*
\* Return Values: \*
\* Register AL: Current User Code if SET \*
\* BL: Same as AL \*
\* \*
KKK KKK KKK KHER REE KER KEKE KERR EKER ERE RK KE

A process can change or interrogate the currently active user
number by calling Function 32. If register DL = OFFH, then the
function returns the value of the current user number in register
AL. The value can range of 0 to 15. If register DL is not OFFH,
then the function changes the current user number to the value of DL

(modulo 16).

---
_Page 123_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 33

RKEK KKK ERE KEKE RE KEE RRR EER ERE KEKKKKRRAKKE KKK KKK RK

\* \*
\* FUNCTION 33: READ RANDOM \*
\* \*
KRKEKKEE KK KEKE RK ERE KEKE KERR KK ERR KEKE KERR KKK KEK KHAKI K
\* \*
\* Read Random Records from a Disk File \*
\* \*
KRKKRKEKEKE KKK KEKE KER KKK KKK KKK KKK RK KK
\* \*
\* Entry Parameters: \*
\* Register CL: 21H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
RRR KKK KKK KE KK RR KKK KEK ERIK KEKE REE KER EERE RRR KEK

The READ RANDOM function is similar to the READ SEQUENTIAL
function except that the read operation takes place at a particular
Random Record Number, selected by the 24-bit value constructed from
the three-byte (r0, rl, r2) field beginning at postion 33 of the
FCB. Note that the sequence of 24 bits is stored with the least
significant byte first (r0), the middle byte next (rl), and the high
byte last (r2). The Random Record Number can range from 0 to
242,143. This corresponds to a maximum value of 3 in byte r2.

In order to read a file with Function 33, the calling process
must first open the base extent (extent 0). This ensures that the
FCB is properly initialized for subsequent random access operations.
(The base extent may or may not contain any allocated data).
Function 33 places the specified record number in the random record
field, and then BDOS reads the record into the current DMA address.
The function automatically sets the logical extent and current
record values, but unlike the READ SEQUENTIAL function, it does not
advance the record number. Thus a subsequent READ RANDOM call will
re-read the same record. After a random read operation, a file can
be accessed sequentially, starting from the current randomly
accessed position. However, the last randomly accessed record will
be re-read or re-written when switching from random to sequential
mode,

If the BDOS Multi-Sector count is greater than one (See
Function 44), the READ RANDOM function reads multiple consecutive
records into memory beginning at the current DMA. Function 33
automatically increments the r0,rl, and r2 field of the FCB to read
each record. However, it restores the FCB’s Random Record Number to
the first record\*s value upon return to the calling process. Upon

---
_Page 124_

MP/M-86 Programmer”’s Guide 6 System Calls : Function 33

return, the READ RANDOM function sets register AL to zero if the
read operation was successful. Otherwise, register AL contains one
of the following error codes:

Ol : Reading unwritten data

03 : Cannot Close current extent

04 : Seek to unwritten extent

06 : Random record number out of range

10 : FCB checksum error

1l : Unlocked file verification error
255 : Physical error : refer to register H

The function returns Error Code 01 when the it accesses a data
block that has not been previously written.

The function returns Error Code 03 when it cannot close the
current extent prior to moving to a new extent.

The function returns Error Code 04 when a read random operation
accesses an extent that has not been created.

The function returns Error Code 06 when byte 35 (r2) of the
referenced FCB is greater than 3.

The function returns Error Code 10 if the referenced FCB failed
the FCB checksum test.

The function returns Error Code 11 if the BDOS cannot locate
the FCB’ s directory entry when attempting to verify that the
referenced FCB contains current information. The function only
returns this error for files open in Unlocked Mode.

The function returns Error Code 255 if a physical error was
encountered and the BDOS Error Mode is one of the return modes (see
Function 45). If the error mode is the default mode, the system
displays a message at the console identifying the physical error and
terminates the calling process. When a physical error is returned
to the calling process, it is identified by the four low-order bits
of register AH as shown below:

Permanent Error
Select Error

The READ RANDOM function also sets the four high-order bits of
register AH on all error returns when the BDOS Multi-Sector Count is
greater than one. In this case, the four bits contain an integer
set to the number of records successfully read before the error was
encountered. This value can range from 0 to 15. The four high-
order bits of register AH are always zeroed when the Multi-Sector
Count is equal to one.

---
_Page 125_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 34

KERR KKK EK KR KKK KEKE RR RRR EKER KKK RK RRR RRR KKK RAK

\* \*
\* FUNCTION 34: WRITE RANDOM \*
\* k
KR ERK KR ERE RE KR KKK KIKI KR RIK ERE RE KKEK EKER KKEKR KKK AKK
\* \*
\* Write Random Records from a Disk File \*
k x
RRR KKK KR KERR RRR EK KK RRR KKK RR KEK ERE KEKE KK KAKI K
\* \*
\* Entry Parameters: \*
\* Register CL: 22H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* k
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
RRR KR EKER K KERR KERR E KERR KKK RRR KERR RRR KKK

The WRITE RANDOM function is analagous to the Read Random
Function, except that data is written to the disk from the current
DMA address. If the disk extent and/or data block where the data is
to be written is not already allocated, the BDOS automatically
performs the allocation before the write operation continues.

In order to write to a file using the WRITE RANDOM function,
the calling process must first open the base extent (extent 0).
This ensures that the FCB is properly initialized for subsequent
random access operations. The base extent may or may not contain
any allocated data, but opening extent 0 records the file in the
directory so that it is can be displayed by the DIR utility. If a
process does not open extent 0 and allocates data to some other
extent, the file will be invisible to the DIR utility.

The WRITE RANDOM function sets the logical extent and current
record positions to correspond with the random record being written,
but does not change the Random Record Number. Thus sequential read
Or write operations can follow a random write, with the current
record being re-read or re-written as the calling process switches
from random to sequential mode.

Tf the BDOS Multi-Sector count is greater than one (see
Function 44), the WRITE RANDOM function reads multiple consecutive
records into memory beginning at the current DMA. The function
automatically increments the r0,rl, and r2 field of the FCB to write
each record. However, it restores the FCB’s Random Record Number to
the first record’s value upon return to the calling process. Upon
return, the WRITE RANDOM function sets register AL to zero if the
write operation was successful.

---
_Page 126_

MP/M-86 Programmer”s Guide 6 System Calls : Function 34
Otherwise, register AL contains one of the following Error Codes:

02 : No available data block

03 : Cannot Close current extent

05 : No available directory space

06 : Random record number out of range

08 : Record locked by another process

10 : FCB checksum error

11 : Unlocked file verification error
255 : Physical error : refer to register H

The function returns Error Code 02 when it attempts to allocate
a new data block to the file and no unallocated data blocks exist on
the selected disk drive.

The function returns Error Code 03 when it cannot close the
current extent prior to moving to a new extent.

The function returns Error Code 05 when it attempts to create a
new extent that requires a new directory entry and no available
directory entries exist on the selected disk drive.

The function returns Error Code 06 when byte 35 (r2) of the
referenced FCB is greater than 3.

The function returns Error Code 08 when it attempts to write to
a record locked by another process. The function only returns this
error is only returned for files open in Unlocked Mode.

The function returns Error Code 10 if the referenced FCB failed
the FCB checksum test.

The function returns Error Code 11 if the BDOS cannot locate
the FCB’s directory entry when attempting to verify that the
referenced FCB contains current information. The function only
returns this error for files open in Unlocked Mode.

The function returns Error Code 255 if a physical error was
encountered and the BDOS Error Mode is one of the return modes (see
Function 45). If the Error Mode is the default mode, the system
displays a message at the console identifying the physical error and
terminates the calling process. When a physical error is returned
to the calling process, it is identified by the four low-order bits
of register AH as shown below:

---
_Page 127_

MP/M-86 Programmer’s Guide 6 System Calls : Function 34

O01 : Permanent error
02 : Read/only disk
03 : Read/only file

File open in Read/only Mode
File password protected in Write mode
04 : Select Error

The WRITE RANDOM function also sets the four high-order bits of
register AH on all error returns when the BDOS Multi-Sector Count is
greater than one. In this case, the four bits contain an integer
set to the number of records successfully read before the error was
encountered. This value can range from 0 to 15. The four high-
order bits of register AH are always zeroed when the Multi-Sector
Count is equal to one.

---
_Page 128_

MP/M-86 Programmer’s Guide 6 System Calls : Function 35

KK RK I IT I KK RK KKK KKK KR EKER RRR ERE KEKE KK KER EKER

x x
\* FUNCTION 35: COMPUTE FILE SIZE \*
k \*
KOK KK KKK I KK RIK KK KKK RIK RIK KR AEKR ERK KKK KR KKK ERK KK KEKE
\* \*
\* Compute the size of a Disk File \*
x \*
KOR KK IKK KKK RK KKK KKK KERR RRR KKK KKK RRR KER KEKKKEEREEKEE
\* \*&
\* Entry Parameters: \*
\* Register CL: 23H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* k
\* Return Values: \*
\* Register AL: Error Flag \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* Random Record Field of FCB Set \*
\* \*
KIKI IKK KKK RR KKK RK KR RHR EKER KE KKK KKK KER ERE EREKK

The COMPUTE FILE SIZE function determines the "virtual" file

size, which is, in effect, the address of the record immediately
following the end of the file. The "virtual" size of a file
corresponds to the physical size if the file is written
sequentially. If the file is written in random mode, gaps may exist
in the allocation, and the file may contain fewer records than the
indicated size. For example, if a single record with record number
262,143 (the MP/M-86 maximum) is written to a file using the WRITE
RANDOM function, then the "virtual" size of the file is 262,144
records even though only 1 data block is actually allocated.

To compute file size, the calling process passes the address of
a FCB in random mode format (bytes r0, rl and r2 present). Note
that the FCB must contain an unambiguous filename and type.
Function 35 sets the random record field of the FCB to the Random
Record Number + 1 of the last record in the file. If the r2 byte is
set to 04, then the file contains the maximum record count 262,144.

A process can append data to the end of an existing file by
calling Function 35 to set the random record position to the end of
file, then performing a sequence of random writes starting at the
preset record address.

Note: the BDOS does not require the file to be open in order to use
Function 35.

Upon return, Function 35 returns a zero in register AL if the

file specified by the referenced FCB was found, or a OFFH in
register AL if the file was not found. Register AH is set to zero

---
_Page 129_

MP/M-86 Programmer’s Guide 6 System Calls : Function 35

in both of these cases. I€ a physical or extended error was
encountered, Function 35 performs different actions depending on the
BDOS Error Mode (see Function 45). If the BDOS Error Mode is the
default mode, the system displays a message at the console
identifying the error and terminates the process. Otherwise,
Function 35 returns to the calling process with register AL set to
OFFH and register AH set to one of the following physical or
extended Errors Codes:

Ol : Permanent error
04 : Select error
09 : ? in filename or type field

---
_Page 130_

MP/M-86 Programmer~s Guide 6 System Calls : Function 36

KKK KKK HK KEKE KEKE KEK IKE KKK REE KKKKEKKEKKKEKKKKEKE

\* \*
\* FUNCTION 36: SET RANDOM RECORD \*
\* \*
KKK KEKE KKK KKK K KEK KEKE KEKE KKKKRKKKKKEKEKK
\* \*
\* Return the Random Record Number of the \*
\* Next Record to Access in a Disk File \*
\* \*
KKK KKK KKK KKK RRR KKK RIKER REE KEKREKKRERKE KKK KE
k \*
\* Entry Parameters: \*
\* Register CL: 24H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Random Record Field of FCB Set \*
\* \*
KAKI K KEKE KKK RE KR ERK ERK ERK EKER ER KEKEEKEEKKKRKEEKEK

The SET RANDOM RECORD function returns the Random Record Number
of the next record to be accessed from a file that has been read or
written sequentially to a particular point. The function returns
this value in the random record field (bytes r0, rl, and r2) of the
addressed FCB. Function 36 can be useful in two ways.

First, it is often necessary to initially read and scan a
sequential file to extract the positions of various "key" fields.
As each key is encountered, Function 36 is called to compute the
random record position for the data corresponding to this key. If
the data unit size is 128 bytes, the resulting record number minus
one is placed into a table with the key for later retrieval. After
scanning the entire file and tabularizing the keys and their record
numbers, you can move directly to a particular record by performing
a random read using the corresponding Random Record Number that was
saved earlier. The scheme is easily generalized when variable
record lengths are involved since the program need only store the
buffer-relative byte position along with the key and record number
in order to find the exact starting position of the keyed data at a
later time.

A second use of Function 36 occurs when switching from a
sequential read or write over to random read or write. A file is
sequentially accessed to a particular point in the file, Function 36
is called which sets the record number, and subsequent random read
and write operations continue from the next record in the file.

---
_Page 131_

MP/M-86 Programmer\*s Guide 6 System Calls : Function 37

KRKEKKKE EK EKER EKER EKER EKER ERK KERR KKK KKK KK

\* \*
\* FUNCTION 37: RESET DRIVE \*
k \*
KKK KKKKK EKER KK KEKE KRKR KEK K ERK RRR EKER RRR KKK KKK KKK KKK
\* \*
\* Reset Specified Disk Drives \*
\* \*
RRR KKK KKK RK KK KEKE KKK ERK KR REE KK ERR
\* \*
\* Entry Parameters: \*
\* Register Ch: 25H \*
\* DX: Drive Vector \*
\* \*®
\* Return Values: \*
\* AL: Return Code \*
\* BL: Same as AL \*
\* \*
RRR KK EKER EERE RR KEKE KKK IKKE EKER KEKE KKK KKK

The RESET DRIVE function is used to programmatically restore
specified drives to the reset state (a reset drive is not logged-in
and is in read/write status). The passed parameter in register DX
is a 16-bit vector of drives to be reset, where the least
significant bit corresponds to the first drive A, and the high-order
bit corresponds to the sixteenth drive, labelled P. Bit values of
"1" indicate that the specified drive is to be reset.

This function is conditional under MP/M-86. If another process
has a file open on a drive to be reset, and the drive is removeable
or read/only, the DRIVE RESET function is denied and no drives are
reset,

Upon return, if the reset operation is successful, Function 37
Sets register AL to 0. Otherwise, it sets register AL to OFFH (255
decimal). If the BDOS is not in Return Error mode (see Function
45), then the system displays an error message at the console
identifying the process owning an open file.

---
_Page 132_

MP/M-86 Programmer’s Guide 6 System Calls : Function 38

KK IKK KK KK IRE KER RK KR RE KR KKK KKK KEK KKK KEKE KKKKKEER KE

\* \*
\* FUNCTION 38: ACCESS DRIVE \*
\* \*
KK KK IKK KK IK KKK KKK ERK KER EK KKK REK ERK ERK KAR KKAEK
& \*
\* Access Specified Disk Drives \*
\* x
KKK KKK RRR KKK KKK KKK ERR KKK KK KER EKER KKK KEK KKKE KER
\* ®

Entry Parameters:
Register CL: 26H
DX: Drive Vector

AH: Extended Error

k
\*
\*
\*
\* Return Values:
\*
\*
\* BL: Same as AL
\*
\*

x

k

\*

\*

\*

AL: Return Code \*
\*

\*

\*

\*

KKK KKK KKK RK KR KER EKER KERR KK KERR KKEE KEKE EKEEKKER

The ACCESS DRIVE function inserts a special open file item into
the system Lock List for each specified drive. While the item
exists in the Lock List, the drive cannot be reset by another
process. As in Function 37, the calling process passes the drive
vector in register DX. The format of the drive vector is the same
as that used in Function 37.

The ACCESS DRIVE function inserts no items if insufficient free
space exists in the Lock List to support all the new items or if the
number of items to be inserted puts the calling process over the
Lock List open file maximum. This maximum is a MP/M-86 GENSYS
option. If the BDOS Error Mode is the default mode (see Function
45), the system displays a message at the console identifying the
error and terminates the calling process. Otherwise, the ACCESS
DRIVE function returns to the calling process with register AL set
to OFFH and register AH set to one of the following values.

Process Open File limit exceeded
No room in the system Lock List

oe ef

If the ACCESS DRIVE function is successful, it sets register AL
to 0.

---
_Page 133_

MP/M-86 Programmer’s Guide 6 System Calls : Function 39

RRKEKKRKEEKKEKRRKKEKEREKEEKRKKKEEKRKEEKRKEKKEKKEEKRKEKKKKKKKKKER

\* \*
\* FUNCTION 39: FREE DRIVE \*
\* \*
KRREKEKEKEKR EKER KRKEKKEKEKEKRKEKE KKK KEKKKKKEKKREKEKKRKEKKKEKKEKKEKER
\* k
\* Free Specified Disk Drives \*
\* k
KRREKEKEKKEKEKEEKKERKEKKRKEKKEKEKRRKERKEREKREKKKEKKEKEKEKEKKKKKEKKEKE
\* \*
\* Entry Parameters: \*
\* Register CL: 27H \*
\* DX: Drive Vector \*
\* \*
REKKEKRKEKRKEKEKRKKEREERERERRRERKREREKRKEKRKERKERRKKKKKRKE KKK

The FREE DRIVE function purges the System Lock List of all file
and locked record items that belong to the calling process on the
specified drives. As in Function 38, the calling process passes the
drive vector in register DX.

Function 39 does not close files associated with purged open
file Lock List items. In addition, if a process references a
"purged" file with a BDOS function requiring an open FCB, the
Function returns a checksum error. A file that has been written to
should be closed before making a FREE DRIVE call to the file’s
drive. Otherwise data may be lost.

---
_Page 134_

MP/M-86 Programmer~s Guide 6 System Calls : Function 40

KKKKEKKKKEKKK EKER KKKE KR EKKR ERE KK KEKKKEKKKKKEKKKKKKKEKKE

\* k
\* FUNCTION 40: WRITE RANDOM WITH ZERO FILL \*
& \*
RRR KKKHKEKEKK KKK KKEKKKEKEKKREKKEEKKEKKKKKKKKRKKEKKK KK KKK
\* k
\* Write a Random Record to a Disk File \*
\* and Pre-Fill New Data Blocks With Zeros \*
\* \*
KRREKKKKKEKEKKRRE EEK KERR RRR KKK EEKREKKKRKKKKKEKEKEKEKRRREES
\* \*
\* Entry Parameters: \*
\* Register CL: 28H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
k \*
KAREKKKKKKEKKEKKEKKKRE KEKE RRR KRKRKKKREKRKRKKKKKKKKRKEKRKKKEKKEEK

The WRITE RANDOM WITH ZERO FILL function is similar to the
WRITE RANDOM function (Function 34) with the exception that it fills
a previously unallocated data block with zeros before writing the
record. If this function has been used to create a file, records
accessed by a READ RANDOM function that contain all zeros identify
unwritten Random Record Numbers. Unwritten random records in
allocated data blocks of files created using the WRITE RANDOM
function contain uninitialized data.

---
_Page 135_

MP/M-86 Programmer's Guide 6 System Calls : Function 41

REKEKEKKEKREKEKKEKEKEKKEKKKKRKRKKKKKKEKKRKEKEREKRKRKKKKEKKEKKEK

\* \*
\* PUNCTION 41: TEST AND WRITE RECORD \*
\* \*
BREE KKEKKEKEKEKEEKREKREKKREKEKEKKEKKRKEKEKREEEKREKEKKAEKEKKKAKSE
\* \*
\* Verify Contents of Current Record Before Write\*
\* \*
KRKKEKKEKEKKKEKEREKERKEKKKREKERKKEKEKEKEKKEKEKEKKEKEKEEKKKKEKKAKE
\* \*
\* Entry Parameters: \*
\* Register CL: 29H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same aS AX \*
\* \*
KRKEKREKKEKEKKEKRKRKKREKEKEKREKKEKEKEERKEKKEEKEEKEKKRKEKEKEKEKRKEKE

The TEST AND WRITE RECORD function provides a means of
verifying the current contents of a record on disk before updating
it. The calling process must set bytes r0O, rl, and r2 of the FCB
addressed by register DX to the Random Record Number of the record
to be tested. The original version of the record (i.e. the record
to be tested) must reside at the current DMA address, followed
immediately by the new version of the record. The record size can
range from 128 bytes to sixteen times that value depending on the
BDOS Multi-Sector Count (see Function 44).

Function 41 verifies that the first record is identical to the
record on disk before replacing it with the new version of the
record. If the record on disk does not match, the record on disk is
not changed and the function returns an Error Code to the calling
process.

The TEST AND WRITE RECORD function is intended for use in
Situations where more than one process has read/write access to a
common file. This situation is supported under MP/M-86, when more
than one process opens the same file in unlocked mode. Function 41
is a logical replacement for the record lock/unlock sequence of
operations because it prevents two processes from simultaneously
updating the same record. Note that this function is also supported
for files open in Locked Mode to provide compatibility between MP/M-
86 and CP/M-86.

Upon return, the TEST AND WRITE RECORD function sets register
AL to zero if the function was successful.

---
_Page 136_

MP/M~86 Programmer's Guide 6 System Calls : Function 41

Otherwise, register AL contains one of the following Error Codes:

Ol : Reading unwritten data

03 : Cannot Close current extent

04 : Seek to unwritten extent

06 : Random record number out of range

07 : Records did not match

08 : Record locked by another process

10 : FCB checksum error

11 : Unlocked file verification error
255 : Physical error : refer to register AH

The function returns Error Code 01 when it accesses a data
block which has not been previously written.

The function returns Error Code 03 when it cannot close the
current extent prior to moving to a new extent.

The function returns Error Code 04 when a read operation
accesses an extent that has not been created.

The function returns Error Code 06 when byte 35 (r2) of the
referenced FCB is greater than 3.

mk Leeneerinnk eatery ee Teva AA AR AF? shan hha ewanne kA Wr
iii Luimevusvyil CeEreurlilo ou LUL wVYUUS Ue WiliGid elie borviu tu we
updated does not match the record on disk.

The function returns Error Code 08 if the specified record is
locked by another process. The function only returns this error for
files opened in Unlocked Mode,

The function returns Error Code 10 if the referenced FCB failed
the FCB checksum test.

The function returns Error Code ll if the BDOS cannot locate
the FCB's directory entry when attempting to verify that the
referenced FCB contains current information, The function only
returns this error for files opened in Unlocked Mode.

The function returns Error Code 255 if a physical error was
encountered and the BDOS Error Mode is one of the return modes (see
Function 45). If the Error Mode is the default mode, the system
displays a message at the console identifying the physical error and
terminates the calling process. When the function returns a
physical error to the calling process, it is identified by the four
low-order bits of register AH as shown below:

---
_Page 137_

MP/M-86 Programmer's Guide 6 System Calls : Function 41

Ol : Permanent error
O02 : Read/only disk
03 : Read/fonly file or
File open in Read/only Mode
File password protected in Write mode
04 : Select Error

The TEST AND WRITE RECORD function also sets the four high-
order bits of register AH on all error returns when the BDOS Multi-
Sector Count is greater than one. In this case, the four bits
contain an integer set to the number of records successfully tested
or written before the error was encountered. This value can range
from 0 to 15. The four high-order bits of register AH are always
zeroed when the Multi-Sector Count is equal to one.

---
_Page 138_

MP/M-86 Programmer's Guide G System Calls : Function 42

KEKE KKK KEKE KEK KKK KK KKK KKK KERR EKKKKEKKKKKKKKKESE

\* \*
\* FUNCTION 42: LOCK RECORD \*
\* \*
KK KKK KEKE EEK KEKE RKKEKEEKEKKKKKKKKKKKRKKKK
\* \*
\* Lock Records in a Disk File \*
\* \*
KKK KK EEE KKK KEKE KEKE KEKE KEE KKK KKK EEKKEKKEREKKKKKKEKE
\* \*
\* Entry Parameters: \*
\* Register CL: 2AH \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
KKK KK IKKE ERE KEKE REAR EKER EKER KKKREEK

The LOCK RECORD function locks one or more consecutive records
so that no other program with access to the records can
simultaneously lock or update them. This function is only supported
for files open in Unlocked Mode. If it is called for a file open in
Locked or Read/only Mode, no locking action is performed and a
successful result is returned. This is done to provide
compatibility between MP/M-86 and CP/M-86.

The calling process passes the address of an FCB in which the
Random Record Field is filled with the Random Record Number of the
first record to be locked. The number of records to be locked is
determined by the BDOS Multi-Sector Count (see Function 44). The
current DMA must contain the 2-byte File ID returned by the OPEN
FILE function when the referenced FCB was opened. Note that the
File ID is only returned by the OPEN FILE function when the open
mode is Unlocked.

The LOCK RECORD function requires that each record number to be
locked reside in an allocated block for the file. In addition,
Function 42 verifies that none of the records to be locked are
currently locked by another process. Both of these tests are made
before any records are locked.

A MP/M-86 system generation parameter specifies the maximum
number of records that may be locked by a single process. Each
locked record consumes an entry in the BDOS system Lock List which
is shared by locked record and open file entries. Another MP/M-86
system generation parameter sets the size of this table. If there
is not sufficient space in the system Lock List to lock all the
specified records, or the process record lock limit is exceeded,

---
_Page 139_

MP/M-86 Programmer's Guide 6 System Calls : Function 42

then the LOCK RECORD function locks no records and returns en Error
Code to the calling process.

Upon return, the LOCK RECORD function sets register AL to zero
if the lock operation was successful. Otherwise, register AL
contains one of the following Error Codes:

Ol : Reading unwritten data

03 : Cannot Close current extent

04 : Seek to unwritten extent

06 : Random Record Number out of range
08 : Record locked by another process
10 : FCB checksum error

ll : Unlocked file verification error
12 : Process record lock limit exceeded
13 : Invalid File ID

14 : No room in the system Lock List
255 : Physical error : refer to register AH

The function returns Error Code 01 when it accesses a data
block that has not been previously written.

The function returns Error Code 03 when it cannot close the
Current extent prior to moving to a new extent.

The function returns Error Code 04 when it accesses an extent
that has not been created.

The function returns Error Code 06 when byte 35 (r2) of the
referenced FCB is greater than 3.

The function returns Error Code 08 if the specified record is
locked by another process.

The function returns Error Code 10 if the referenced FCB failed
the FCB checksum test.

The function returns Error Code 1l if the BDOS cannot locate
the referenced FCB's directory entry when attempting to verify that
the FCB contains current information.

The function returns Error Code 12 when the sum of the number
of records currently locked by the calling process and the number of
records to be locked by the LOCK RECORD call, exceeds the maximum
allowed value. This value is an MP/M-86 GENSYS parameter,

The function returns Error Code 13 when an invalid File ID is
placed in the current DMA.

The function returns Error Code 255 if a physical error was
encountered and the BDOS Error Mode is one of the return modes (see
Function 45). If the Error Mode is the default mode, the system
displays a message at the console identifying the physical error and

---
_Page 140_

MP/M-86 Programmer's Guide 6 System Calls : Function 42

terminates the calling process. When the function returns a
physical error to the calling process, it is identified by the four
low-order bits of register AH as shown below:

Ol : Permanent error
04 : Select Error

The LOCK RECORD function also sets the four high-order bits of
register AH on all error returns when the BDOS Multi-Sector Count is
greater than one. In this case, the four bits contain an integer
set to the number of records successfully locked before the error
was encountered. This value can range from 0 to 15. The four high-
order bits of register AH are always zeroed when the Multi-Sector
Count is equal to one.

---
_Page 141_

MP/M-86 Programmer's Guide 6 System Calls : Function 43

KKK KEK KEKE EKER KKK EKER KARR EKA KKK KEK

\* \*
\* FUNCTION 43: UNLOCK RECORD \*
\* \*
KKK KKK KKK KEKE KER KK EKER KERR KKK EKER EKER EKEKRKEKRK RHE
\* \*
\* Unlock Records in a Disk File \*
\* \*
RRKK KERR KERR ERK KR KK KEK KKK REE KE KK REE RRA
\* \*
\* Entry Parameters: \*
\* Register CL: 2BH \*
\* DX: FCB Address — Offset \*
\* DS: FCB Address —- Segment \*
\* \*
\* Return Values: \*
\* Register AL: Error Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
x \*
RRKEKKKKKEKKE KKK ERK RKKEKEK RRR KR KERR KR KKK

The UNLOCK RECORD function unlocks one or more consecutive
records previously locked by the LOCK RECORD function. This
function is only supported for files open in Unlocked Mode. If it
is called for a file open in Locked or Read/only Mode, no locking
action is performed and a successful result is returned.

The calling process passes the address of an FCB in which the
Random Record Field is filled with the Random Record Number of the
first record to be unlocked. The number of records to be unlocked
is determined by the BDOS Multi-Sector Count (see Function 44). The
Current DMA must contain the 2-byte File ID returned by the OPEN
FILE function when the referenced FCB was opened. Note that the
File ID is only returned by the OPEN FILE function when the open
mode is Unlocked.

The UNLOCK RECORD function will not unlock a record that is
currently locked by another process. However, the function does not
return an error if a process attempts to do that. Thus, if the
Multi-Sector Count is greater than one, the UNLOCK RECORD function
will unlock all records locked by the calling process, while
skipping those records locked by other processes.

Upon return, the UNLOCK RECORD function sets register AL to
zero if the unlock operation was successful.

---
_Page 142_

MP/M~86 Programmer's Guide 6 System Calls : Function 43

Otherwise, register AL contains one of the following Error Codes:

Reading unwritten data

Cannot Close current extent

Seek to unwritten extent

Random Record Number out of range

FCB checkSum error

Unlocked file verification error
Invalid File ID

255 : Physical error : refer to register AH

oO
Ww

i
i
eo oe

The function returns Error Code 01 when it accesses a data
block which has not been previously written.

The function returns Error Code 03 when it cannot close the
current extent prior to moving to a new extent.

The function returns Error Code 04 when it accesses an extent
that has not been created.

The function returns Error Code 06 when byte 35 (r2) of the
referenced FCB is greater than 3.

The function returns Error Code 10 if the referenced FCB failed

FCR checksum test

wee e ate Soe

The function returns Error Code 1l if the BDOS cannot locate
the referenced FCB's directory entry when attempting to verify that
the FCB contains current information.

The functions return Error Code 13 when an invalid File ID is
placed in the current DMA.

The function returns Error Code 255 if a physical error was
encountered and the BDOS Error Mode is one of the return modes (see
Function 45). If the Error Mode is the default mode, the system
displays a message at the console identifying the physical error and
terminates the calling process. When the function returns a
physical error to the calling process, it is identified by the four
low-order bits of register AH as shown below:

Ol : Permanent error
04 : Select Error

The UNLOCK RECORD function also sets the four high-order bits
of register AH on all error returns when the BDOS Multi-Sector Count
is greater than one. In this case, the four bits contain an integer
set to the number of records successfully unlocked before the error
was encountered. This value can range from 0 to 15. The four high-
order bits of register AH are always zeroed when the Multi-Sector
Count is equal to one.

---
_Page 143_

MP/M-86 Programmer's Guide 6 System Calls : Function 44

RRR KKK EK KEK RE KKK KEE KKEEREKKEKREKERKEAKE KKK KKK KKK

\* \*
\* FUNCTION 44: SET MULTI-SECTOR COUNT \*
\* \*
BRAKE KKK ERK KERR KEK IRE RRR KEKE KERR KREKEKKE KRENEK RK KE
\* \*
\* Set Number of Records for Subsequent \*
\* Disk Reads and Writes \*
\* \*
RRR KK KKK KERR ERE KRK IKE KR ERE KEIR ERR KEE KERR KK
\* \*
\* Entry Parameters: \*
\* Register CL: 2CH \*
\* DL: Number of Sectors \*
\* \*
\* Return Values: \*
\* Register AL: Return Code \*
\* BL: Same as AL \*
\* \*
HRKKKKE KEKE KEK KEKE KKK AKER EEE K RK KKK

The SET MULTI-SECTOR COUNT function Provides logical record
blocking under MP/M-86. It enables a process to read and write from
1 to 16 "physical" records of 128 bytes at a time during subsequent
BDOS read and write functions. It also specifies the number of 128-
byte records to be locked or unlocked by the BDOS LOCK RECORD and
UNLOCK RECORD functions.

Function 44 sets the Multi-Sector Count value for the calling
process to the value passed in register DL. Once set, the specified
Multi-Sector Count remains in effect until the calling process makes
another SET MULTI-SECTOR COUNT function call and changes the value.
Note that the CLI function sets the Multi-Sector Count to one when
it initiates a transient program.

The Multi-Sector count affects BDOS error reporting for the
BDOS read, write, lock and unlock functions. If an error interrupts
these functions when the Multi-Sector is greater than one, they
return the number of records successfully processed in the four
high-order bits of register AH.

Upon return, the function sets register AL to 0 if the

Specified value is in the range of 1 to 16. Otherwise, it sets
register AL to OFFH.

---
_Page 144_

MP/M-86 Programmer's Guide 6 System Calls : Function 45

KKK KK IK IKI KKK IKKE RIKER KKK KEKE KE KKK KKK KEKE

\* \*
\* FUNCTION 45: SET BDOS ERROR MODE \*
\* \*
KKK IK KK KKK KKK KKK KER KR KKK KKK KEKE EKER KK KK KKK
\* \*
\* Set BDOS Error Mode for types of Error Returns\*
\* \*
ee ee ee ee eee ee eee ee ee
\* \*
\* Entry Parameters: \*
\* Register CL: 2DH \*
\* DL: BDOS Error Mode \*
\* \*
KKK KK KKK KKK RK RR KERR EKER KK ARE KK EERE RRR KEEKKKEKEKEK

The BDOS Error Mode determines how physical and extended errors
(see Section 2.2.13) are handled for a process. The Error Mode can
exist in three modes: the default mode, Return Error Mode and Return
and Display Error Mode. In the default mode, BDOS displays a system
message at the console identifying the error and terminates the
calling process. In the return modes, BDOS sets register AL to OFFH
(255 Decimal), places an Error Code identifying the physical or
extended error in the four low-order bits of register AH, and
returns to the calling process. In Return and Display Mode, the
BDOS displays the system message before returning to the calling
process. However,when the BDOS is in Return Error Mode, it does not
display any system messages.

Function 45 sets the BDOS Error Mode for the calling process to
the mode specified in register DL. If register DL is set to OFFH
(255 Decimal), the Error Mode is set to Return Error Mode. Tf
register DL is set to OFEH (254 Decimal), the Error Mode is set to
Return and Display Mode. If register DL is set to any other value,
the Error Mode is set to the default mode.

---
_Page 145_

MP/M-86 Programmer's Guide 6 System Calls : Function 46

KEKKEKEKEKEKKKEK EKER KR ERE KEKKEKEKEER EKER KEKE KE KEKE KEKE

\* \*
\* FUNCTION 46: GET FREE DISK SPACE \*
\* \*
KAEKKKKKKEREK KEKE EEKKER KEKE RK ERK EERE KER KKK KERKEE
\* \*
\* Return Free Disk Space on Specified Drive \*
\* \*
KEKEKEKKEKEKEK ERE REE EEK EKER KREKREKKEKEKKR RRR KEKE
\* \*

Entry Parameters:
Register CL: 2EH
DL: Drive

\* \*
\* \*
x \*
\* \*
\* Return Values: \*
\* Register AL: Error Flag \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* First 3 bytes of DMA buffer \*
\* \*
\* \*

KEEKRKEKKKKKKRKKKKEKEK KER EKER KKK KEK KKRKEREEKREKEKRKKRKEKEE

The GET DISK FREE SPACE function determines the number of free
sectors (128-byte records) on the specified drive. The calling
process passes the drive number in register DL, with 0 for drive A,
1 for B, etc., through 15 for drive P in a full 16-drive system.
Function 46 returns a binary number in the first 3 bytes of the
current DMA buffer. This number is returned in the format shown in
Figure 6-2.

Figure 6-2. Disk Free Space Field Format

fsO = low byte
fsl middle byte
fs2 high byte

Upon return, the function sets register AL to 0 if the BDOS Error
Mode is the default mode. However, if the BDOS Error Mode is one of
the return modes (see Function 45) and a physical error was
encountered, it sets register AL to OFFH (255 Decimal), and register
AH to one of the following values:

Ol - Permanent error
04 - Select error

---
_Page 146_

MP/M-86 Programmer's Guide 6 System Calls : Function 47

KKK KAKI KERR KEKE KK KKK KK KKK KEKE KKKEKKKKKKKKKEKKEKEK

\* \*
\* FUNCTION 47: CHAIN TO PROGRAM \*
\* \*
KKK KKK KIKI KIRKE KERR RK KIKI KEIRA ERR KKK EEK KKK KEKE
\* \*

\* Load, Initialize and Jump to specified Program\*
\* \*

KKK KKK KK KER EKER KEKE KKK KEKE REKKERKEKRKKEKKKKKKEKKEKKE

\* \*
\* Entry Parameters: \*
\* Register CL: 2FH \*
\* DMA buffer: Command Line \*
\* \*

\* Return Values: \*
\* Register AX: OFFFFH - Could not find \*
\* Command \*
\* \*
KKK KKK KEK REE RE KEKKEEKK RRR KRKKEKREKEKKKKEKKE

The CHAIN TO PROGRAM function provides a means of chaining from
one program to the next without operator intervention. Although
there is no passed parameter for this call, the calling process must
place a command line terminated by a null byte in the default DMA

buffer.

Under MP/M-86, the CHAIN TO PROGRAM function releases the
memory of the calling function before executing the command. The
command is processed in the same manner as the CLI function
(Function 150). If the command warrants the loading of a CMD file
and the memory released is large enough for the new program, MP/M-86
loads the new program into the same memory area as the old program.

Except in the case of passing the command to an RSP, the new
program is run by the same process that ran the old program. The
name of the process is changed to reflect the new program being run.
If the command invokes an RSP, the calling process terminates upon
successfully writing the command to the RSP queue.

Parameter passing between the old and new programs is
accomplished through the use of disk files, queues or the command
line. The command line is parsed and placed in the Base Page of the
new program in the manner documented in the CLI function (Function
150).

The CHAIN TO PROGRAM function returns an error if there is no
RSP with the same name as the command and no CMD file is found. If
a CMD file is found and an error occurs after it is successfully
opened, the calling process is terminated since its memory has been
released.

---
_Page 147_

MP/M-86 Programmer's Guide 6 System Calls : Function 48

KEEKKKEKKKEKEEKEKE KEKE KEKE REE KEKE RKEREKKEKKKEKESK

\* \*
\* FUNCTION 48: FLUSH BUFFERS \*
\* \*
RRKEKKEKKREKEKEKEEREKREKRKEKREKKEKEKKEKRKEKREKKERKKRKRKK KK KKK
\* \*k
\* Flush a Write-Deferred Buffers \*
\* \*
KRKKREEKEKEKEKKEKEEK EKER ERE EK KK RRR EEK KEK KKKREKE
\* \*
\* Entry Parameters: \*
\* Register CL: 30H \*
\* \*
\* Return Values: \*
\* Register AL: Error Flag \*
\* AH: Permanent Error \*
\* BX: Same aS AX \*
\* \*
KREKKEKEKKKKEKEREKKREKRKREREEKKE RR EKREERKRERKR EKER KKRKKKEE

The FLUSH BUFFERS function forces the write of any write-
pending records contained in internal blocking/deblocking buffers.
This function only affects those systems that have implemented a
write-deferring blocking/deblocking algorithm in their XIOS (see
Section 2.2.12).

Upon return, the function sets register AL to 0 if the flush
operation was successful. If a physical error was encountered, the
FLUSH BUFFERS function performs different actions depending on the
BDOS Error Mode (see Function 45). If the BDOS Error Mode is in the
default mode, the system displays a message at the console
identifying the error and terminates the calling process.
Otherwise, it returns to the calling process with register AL set to
OFFH and register AH set to the following physical Error Code:

Ol : Permanent error

---
_Page 148_

MP/M-86 Programmer's Guide 6 System Calls : Function 50

KK KKK KKK KKK KKK KKK KKK ERK KKK KEKE KKEKKEKKKKRKKKKKKEKEE

\* \*
\* FUNCTION 50: DIRECT BIOS CALL \*
\* \*
KKK KKK KKK KEKE ERK EKER KEK RKRRERKRKEKEKKR KKK KKKKKKEKKK
\* \*
\* Call BIOS character routine \*
x \*
KR KKK KKK KKK KKK KEKE KKK RRR KK KEK KEE KR ERK KEKE KREKKEKEER
\* \*
\* Entry Parameters: \*
\* Register CL: 32H \*
\* DX: BIOS Desc. Addr. - Offset \*
\* DS: BIOS Desc. Addr. - Segment \*
\* \*
\* Return Values: \*
\* Register AX: BIOS Return \*
\* BX: Same as AX \*
\* \*
KEK KKK KKK KEKE KEKE RE KEE EKER RRKEKKEKREKKKKKEKEKE

BIOS Descriptor:

+----- +----- te---- +----- +----- +
|FUNC | CX | DX
+—----- 4+----- 4+----- +—----- +----- +

Figure 6-3. BIOS Descriptor Format

The DIRECT BIOS CALL function is provided under MP/M-86 for
compatibility with programs generated under CP/M-86 that uses this
function. Under MP/M-86, only routines that interface with
character devices are supported. The arguments to character routines
such as CONIN and LIST are converted to those appropriate for the
MP/M-86 XIOS. Where console or list device numbers are needed by

the XIOS, default values of the calling process are sent to the XIOS

The FUNC,CX and DX fields of the BIOS Descriptor explained in
the Digital Research CP/M-86 System Guide.

---
_Page 149_

MP/M-86 Programmer's Guide 6 System Calls : Function 51

KRREKEKKKKERKEKEKRKRKK ERE KEEKREKRKERKEKKEKKEKKRKEKEKRKKKKKKRKEKRKRKRKEE

\* k
\* FUNCTION 51: SET DMA BASE \*
\* \*
KREKKEKKRKEEKKKEKE KEKE EEE EREKEER KEKE KE REKKEKKERKKKEKKEKE
k \*
\* Set Direct Memory Access Segment Address \*
\* \*
KAKEKEKKKEKEK KEE KEKE REKKEKEKRKERERKEEEKERKEEKKKKKKRKKRKEE
\* \*
\* Entry Parameters: \*
\* Register CL: 33H \*
\* DX: DMA Segment Address \*
\* \*
KRKEKEKKKEKKKKKEKR KERR RE KKK EE RRR EEE ERREKERKKEREKE

Function 51 sets the base register for subsequent DMA
transfers. The word parameter in DX is a paragraph address and is
used with the DMA offset to specify the address of a 128-byte buffer
area to be used in the disk read and write functions. Note that
upon initial program loading, the default DMA base is set to the
address of the user's data segment (the initial value of DS) and the
DMA offset is set to 0080H, which provides access to the default
buffer in the Base Page.

---
_Page 150_

MP/M-86 Programmer's Guide 6 System Calls : Function 52

KRKKKKEKEKKEEKKEKEKKEKREKEEKRKKKEKKEKKEKRKKKEKRKKKEKKKRRKKKRKS

\* \*
\* FUNCTION 52: GET DMA ADDRESS \*
\* \*
KRREKEKKKKEKEKEEKKKEEKKEEKKKEKKKEKKEKKRRKKEKKEKEKKEKKEKKKKKEKKKKEKEK
\* \*
\* Return Address of Direct Memory Access Buffer \*
\* \*
KRAEKREKKEKKEKEKKEKEEEEKEKKKEEKRKEKKRKRKEKKKKKKKEKEKKKEKKKKEKKKEKK
\* \*x

Entry Parameters:
Register CL: 34H

\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: DMA Offset \*
\* BX: Same as AX \*
\* ES: DMA Segment \*
\*x \*
\* \*

RKEKRKEKEKEEKEKREKKKREREKEKRKEKEKEREKREKRKREKREKKKEKKEKKKKKEK

Function 52 returns the current DMA Base Segment address in ES,
with the current DMA Offset in Dx.

---
_Page 151_

MP/M-86 Programmer's Guide 6 System Calls : Function 53

KEKEKKKEEKKKKREKKKEKKEKEREKRKEKEKKRKEKKEKKEEKKKEKEEKKKKKRKKEEK

\* \*
\* FUNCTION 53: GET MAX MEM \*
\* \*
KRARKKEKKKEKKKEKEEKEKKKKRE REE KEK KKEREKEKRKKKKREKKERAEKKKKKKEEK
\* \*
\* Allocate Maximum Memory Available \*
\* \*
REKKKEKKEKEKKEKKAREKEEEKERERKREKEKREKKEK KEKE RRR KKEKEEEKKE
\* \*

Entry Parameters:
Register CL: 35H
DX: MCB Address - Offset
DS: MCB Address - Segment

\* \*
\* \*k
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AL: 0 if successful \*
\* OFFH on failure \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* MCB filled in \*
\* \*
\* \*

REKEKRKEKEKREKEKRKKEEEKKKEEKEEKREKEKRKEKRKEKKEKKEKEKKKEKRKKER

Memory Control Block (MCB):

+-~--- +----- +----- +----- +----- +
| BASE | LENGTH | EXT |
+----- +----- +----- +----- +----- +

Figure 6-4. Memory Control Block Format

BASE The Segment Address of the beginning of the allocated
memory. The function fills in this field on a successful
allocation.

LENGTH Length of the Memory Segment in paragraphs. The LENGTH
field is set to the maximum number of paragraphs wanted.
The function sets this field to the actual number of
paragraphs obtained on a successful allocation.

EXT The function fills in the EXT byte on a successful
allocation and always sets it to one.

Function 53 allocates the largest available memory region which
is less than or equal to the LENGTH field of the MCB in paragraphs.
If the allocation is successful, the function sets the BASE to the
base paragraph address of the available area, and LENGTH to the
Paragraph length. Upon return, register AL has the value OFFH if no
memory is available, and OOH if the request was successful. The
function sets the EXT to 1 if there is additional memory for
allocation, and 0 if no additional memory is available.

---
_Page 152_

MP/M-86 Programmer's Guide 6 System Calls : Function 54

BASE

LENGTH

EXT

KKKKKEKEKKKEKKKKEKEEKRKKKEKRKEKKKKEKKKRKEKKEKKKRKKKKKKKKKKKKKEK

\* k
\* FUNCTION 54: GET ABS MAX \*
\* \*«
KKEKKKKEKKKKEKEKKKEKKRKEKKEKE KEK KKK KEE KKEKKEKKKKKKEKEKEEE
\* k
\* Allocate Maximum Memory Available \*
\* at a Specified Address \*
\* \*
KREKKKKKKEKEKEKEKKEKEKEKEEKEKEKREKEKRKEREKKKEKEEEKKKEKEKKKKKKER
k \*

Entry Parameters:
Register CL: 36H
DX: MCB Address - Offset
DS: MCB Address - Segment

\* \*
\* \*
\* \*k
\* \*
\* \*
\* Return Values: \*
\* Register AL: 0 if successful \*
\* OFFH on failure \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* MCB filled in \*
\* \*
\* \*

RRKKEKKEKKKKEEKRKEEKEKREKKRRKRKERKREKREEREKKKRKEKEKERKEKKKRES

Memory Control Block (MCB):

+-~---- +----- +----- +----- +----- +
| BASE | LENGTH | EXT |
+----- +----- +----- +----- +----- +

Figure 6-4. Memory Control Block Format

The Segment Address of the beginning of the memory segment
wanted. This field is maintained on a _ successful
allocation.

Length of the Memory Segment in paragraphs. The LENGTH
field is set to the maximum number of paragraphs wanted.
On a successful allocation, the function sets this field
to the actual number of paragraphs obtained.

The EXT field is unused but must be available.

Function 54 is used to allocate the largest possible region at
the absolute paragraph boundary given by the BASE field of the MCB,
for a maximum of LENGTH paragraphs. If the allocation is
successful, the function sets the LENGTH to the actual length. Upon
return, register AL has the value OFFH if no memory is available at
the absolute address, and OOH if the request was successful.

---
_Page 153_

MP/M-86 Programmer's Guide 6 System Calls : Function 55

BASE

LENGTH

EXT

REKKKEKKEK EKER EKER ERE KEKE EK EK EKA KEKKEKERERKEKEREKKKEKRE

\* \*
\* FUNCTION 55: ALLOC MEM \*
\* \*
KEKEKKERKEKKKKEKREKKEK EKER KK KEKE KREKEKKEKEKKKKEKKKKKKEE
\* \*
\* Allocate a Memory Segment \*
\* \*
KEKKKK EE KEE KEKE REE EKKREKKE KKK EKER KEKE AEKEEKKREEKEEEKK
\* \*

Entry Parameters:
Register CL: 37H
DX: MCB Address - Offset
DS: MCB Address - Segment

\* \*
\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AL: 0 if successful \*
\* OFFH on failure \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* MCB filled in \*
\* \*
\* \*

KHAKKEKEKEKEKEEKEKKKEEEKEKEKREKEK KEKE KEKE KKERKREKRRKKKKKEK

Memory Control Block (MCB):

+----- +----- +----- +----- +----- +
| BASE | LENGTH | EXT |
+----- +----- +----- +----- +----- +

Figure 6-5. Memory Control Block Format

The Segment Address of the beginning of the memory segment
allocated. The function fills in this field on a
successful allocation.

Length of the Memory Segment in paragraphs. The LENGTH
field is set to the number of paragraphs wanted. On a
successful allocation, this field is maintained.

The EXT field is unused but must be available.

The ALLOCATE MEMORY function allocates a memory area whose size
is the LENGTH field of the MCB. Function 55 returns the base
Paragraph address of the allocated region in the user's MCB. Upon
return, register AL contains a 00H if the request was successful and
a OFFH if the memory could not be allocated.

---
_Page 154_

MP/M-86 Programmer's Guide 6 System Calls : Function 56

BASE

LENGTH

EXT

KREKKEKKKKKRKKKEKEKKKKEKAEKKEKKKRKERKKKKKKKRKRKKKKKK KEKE

\* \*
\* FUNCTION 56: ALLOC ABS MAX \*
\* x
KEKKKEKKEKKEKKEEKEKRKKREEKKRKKKEKKKEKKKEKKKKKKKKKKKEKKKE
\* \*
\* Allocate a Memory Segment \*
\* at a Specified Address \*
\* \*
KKEKKKKKKEK KKK KEKE KEKE KERR KEK KKEKEKKKEKKKKKKKKKKEKE
\* \*

Entry Parameters:
Register CL: 38H
DX: MCB Address - Offset
DS: MCB Address — Segment

\* \*
\* \*
\* \*
\*k \*
\* \*
\* Return Values: \*
\* Register AL: 0 if successful \*
x OFFH on failure \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* MCB filled in \*
\* \*k
\* \*

KKEKKKEKEK KKK KARE REKEKEKKEEKEKKKEKKKESK

Memory Control Block (MCB):

+----- +----- +----- +----- +----- +
| BASE | LENGTH | EXT |
+----- +----- +----- +----- +----- +

Figure 6-6. Memory Control Block Format

The Segment Address of the beginning of the memory segment
wanted. This field is maintained on a successful
allocation.

Length of the Memory Segment in paragraphs. The LENGTH
field is set to the number of paragraphs wanted. This
field is maintained on a successful allocation.

The EXT field is unused but must be available.

The ALLOCATE ABSOLUTE MEMORY function allocates a memory area
which starts at the address specified by the BASE field and whose
length is specified by the LENGTH field of the MCB. Upon return,
register AL contains a 00H if the request was successful and a OFFH
if the memory could not be allocated.

---
_Page 155_

MP/M-86 Programmer's Guide 6 System Calls : Function 57

RREEKKEKEKKKEKKEKE KEKE KEKE KK ERE KKK EKKKKEKKKRKKRKEK

\* \*
\* FUNCTION 57: FREE MEM \*
\* \*
KRKEKKKEKEKEKE KEKE KEKE KER KK KERR EKER KEKE KEEK
\* \*
\* Free a specified Memory Segment \*
\* \*
KEK KEKE KKEKAKRKE RE RK ERE RRR KKK KEKRERKKEEKEKKKAKK KEKE
\* \*

Entry Parameters:
Register CL: 39H
DX: MCB Address - Offset
DS: MCB Address - Segment

\* \*
\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AL: 0 if successful \*
\* OFFH on failure \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* MCB filled in \*
\* \*
\* \*

KKKEKKEKEEKEKKKEEKKEKKKKREKRERKKEREEEKEKEKKEKKKKRKKEK

Memory Control Block (MCB):

+----- +----- +----- +----- +----- +
| BASE | LENGTH | EXT |
+----- +----- +----- +----- +----- +

Figure 6-7. Memory Control Block Format

BASE A Segment Address in the memory segment which begins the
area to be freed.

LENGTH Length of the Memory Segment in paragraphs. This field is
not used. The memory area freed always goes to the end
of the previously allocated memory segment.

EXT If the EXT field is 00H, the memory segment specified by
the BASE field is freed. If the value is OFFH, all
memory except memory allocated at load time is freed.

The FREE MEMORY function is used to release memory areas
allocated to the program. The value of the EXT field of the MCB
controls the operation of this function. If EXT = OFFH then the
function releases all memory areas allocated by the calling program.
If the EXT field is zero, the function releases the memory area
beginning at the specified BASE and ending at the end of the
previously allocated memory segment.

---
_Page 156_

MP/M-86 Programmer's Guide 6 System Calls : Function 58

KKK KEKE RK KKK KEKE KKK RRR KKREKRKKEKRKKERRKKKKKKKKK KKK

\* \*
\* PUNCTION 58: FREE ALL MEM \*
\* \*
KKK EK KEKE KKK KKK KEKE KEKE KERR ER REE KER KKKKKKEKEKKKKEKEKE
\* \*
\* Terminate Calling Process \*
\* \*
KKK KKK EK KKK KERIKERI RK KEKE EE KEE KKKKKKKEEE
\* \*
\* Entry Parameters: \*
\* Register CL: 3AH \*
\*& \*

KKK KKK KKK ERE KE KEKE KR KEKE KR KEKE ER KERR KKEKKKEKER

In the CP/M-86 environment, the FREE ALL MEMORY function
releases all memory in order to release memory allocated by

interface type programs before returning to the CCP.

Under MP/M-86,

the equivalent action is to terminate the calling process.

---
_Page 157_

MP/M-86 Programmer's Guide 6 System Calls : Function 59

KRKKKKREKK KKK ERK RKEKKEEK KERR ERE KEKE KR KKK KKK KKK

\* \*
\* FUNCTION 59: PROGRAM LOAD \*
\* \*
KRKKKKEEKK KKK KEKE ER ER KEKE KKK KEKE REKEEKRRKRRRREKEEEK
\* \*&
\* Load a Progam into Memory \*
\* From a CMD type file \*
\* \*
KKK KEK KKK KR KKK KEKE KKEKKEK EK KREEKEEKEK KKK KEKE
& \*
\* Entry Parameters: \*
\* Register CL: 3BH \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AX: Base Page Segment \*
\* OFFFFH on error \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
KREKKEKKKEEKEKERKEK KEK EKER KREKEKRKERREKE KKK

The PROGRAM LOAD function loads a CMD type disk file into
Memory. Upon entry, register DX contains the DS-relative offset of
a successfully opened FCB that names the input CMD file. Upon
return, register AX has the value OFFFFH if the program load was
unsuccessful. Otherwise, AX contains the paragraph address of the
Base Page belonging to the loaded program. The base address and
segment length of each segment is stored in the Base Page. Upon
program load, the CLI function initializes the DMA base address to
the Base Page of the loaded rogram, and the DMA offset address to
0080H. Note: the CLI function performs this initialization. The
PROGRAM LOAD function does not establish a default DMA address. A
Program must execute Function 51 (SET DMA BASE) and Function 26 (SET
DMA OFFSET) before executing the PROGRAM LOAD function. If a new
Process is to run the loaded program, it must initialize a User Data
Area (UDA) and a Process Descriptor (PD) and then call the CREATE
PROCESS function. It is recommended that the SEND CLI COMMAND
function be used in the case of creating a new process.

---
_Page 158_

MP/M-86 Programmer's Guide 6 System Calls : Function 100

KEKE KEKE KEK KEK KEKE KKK EKER KKKEKKKEKRKEKKKKEKKKKKKKEE

\* \*
\* FUNCTION 100: SET DIRECTORY LABEL \*
\* \*
KKK KKK KERR KEKE KKK KEK KKK KK KEKE KERR KKKKKKKKKEK
\* \*
\* Create or Update a Directory Label \*
\* \*k
KRK KK KKK KERR KKK KERR EEK KKK KEKE EKKEKKKKKKKKKKK
\* \*
\* Entry Parameters: \*
\* Register CL: 64H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
\* x
KEK KK KEKE KEK KEKE KERR EKER KEKE KEKE ERE EKER KKKEKKEKKEKE

The SET DIRECTORY LABEL function creates a Directory Label or
updates the existing Directory Label for the specified drive. The
calling process passes the address of an FCB containing the name,
type, and extent fields to be assigned to the Directory Label. The
name and type fields of the referenced FCB are not used to locate
the Directory Label in the directory; they are simply copied into
the updated or created Directoy Label. The extent field of the FCB
(byte 12) contains the user's specification of the Directory Label

data byte. The definition of the Directory Label data byte is:

bit 7 - Require passwords for password protected files
6 - Perform access date and time stamping

5 - Perform update date and time stamping

4 - Make function creates XFCBs

- Assign a new password to the Directory Label

If the current Directory Label is password protected, the correct
password must be placed in the first 8 bytes of the current DMA or
have been previously established as the default password (see
Function 106). If bit 0 (the low-order bit) of byte 12 of the FCB
is set to 1, it indicates that a password for the Directory Label
has been placed in the second eight bytes of the current DMA.

---
_Page 159_

MP/M-86 Programmer's Guide 6 System Calls : Function 100

Upon return, Function 100 returns a Directory Code in register
AL with the value 0 to 3 if the Directory Label create or update was
successful, or OFFH (255 Decimal) if no space existed in the
referenced directory to create a Directory Label. Register AH is
set to zero in both of these cases. If a physical or extended error
was encountered, Function 100 performs different actions depending
on the BDOS Error Mode (see Function 45). If the BDOS Error Mode is
the default mode, the system displays a message at the console
identifying the error and terminates the calling process.
Otherwise, Function 100 returns to the calling process with register
AL set to OFFH and register AH set to one of the following physical
or extended Error Codes:

Ol : Permanent error

02 : Read/only disk

04 : Select Error

07 : File password error

---
_Page 160_

MP/M-86 Programmer's Guide 6 System Calls : Function 101

KKK KKK KKK KKK KK KKK KKK KEK KEKE KKK EKKRKKR KEKE KKK KKKKKEE

\* \*
\* FUNCTION 101: RETURN DIRECTORY LABEL \*
\* \*
KK KK KKK KK KR KKK RRR KK KKK RK KER ERR KEKE REKKK KKK EKKEKE
\* \*
\* Return Data Byte of Directory Label \*
\* for the specified Drive \*
\* \*
KOR KOR KOKORO AOR ROR IK KR TORII IO KK RR AIR KIRKE RRR KR RK
\* \*

Entry Parameters:
Register CL: 65H
DL: Drive

\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AL: Directory Label Deta Byte \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
\* \*

KKK KKK KR KK RR KEKE KKK ERE KREREKRKEKKEKREKKE

The RETURN DIRECTORY LABEL function returns the data byte of
the Directory Label for the specified drive. The calling process
passes the drive number in register E with 0 for drive A, 1 for
drive B, and so-forth through 15 for drive P in a full 16-drive
system. The format of the Directory Label data byte is shown below:

bit 7 - Require passwords for password protected files
6 - Perform access date and time stamping
5 - Perform update data and time stamping
4 - Make function creates XFCBS
0 - Directory label exists on drive

Function 101 returns the Directory Label data byte to the calling
process in register AL. Register AL equal to 0 indicates that no
Directory Label exists on the specified drive. If the function
encounters a physical error when the BDOS Error mode is in one of
the return modes (see Function 45), it returns with register AL set
to OFFH (255 Decimal) and register AH set to one of the following:

Ol

Permanent error
Select error

ee oe

---
_Page 161_

MP/M-86 Programmer's Guide 6 System Calls : Function 102

RK KK KKK KIRKE KERR KEK KERR KKK KKK RRR IKK KR REAR EKER AK

\* \*
\* FUNCTION 102: READ FILE XFCB \*
\* \*
RRR ERE REE K KEE R KE REI KEKRE ERE REK KEKE KKK AKER ARE
\* \*
\* Return Extended File Control Block \*
\* of a Disk File \*
k \*
RRR KK RE KK RRR KEKE RK K EERE IKE KK KERKK EAR KKK ERE
\* \*
\* Entry Parameters: \*
\* Register CL: 66H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical Error \*
\* BX: Same as AX \*
\* \*
KKK KKK KKK KEKE EKER KKK EKER ERK IKE KR EKRREKEKEEKREKAEE

The READ FILE XFCB function reads the directory XFCB
information for the specified file into bytes 20 through 32 of the
specified FCB. The calling process passes the address of an FCB in
which the drive, filename, and type fields have been defined.

If Function 102 is successful, it sets the following fields in
the referenced FCB:

byte 12 : XFCB password mode field

bit 7 - Read mode
bit 6 - Write mode
bit 5 - Delete mode

Byte 12 equal to 0 indicates the file has not
been assigned a password.

byte 13 - 23 : XFCB password field (encrypted)
byte 24 - 27 : XFCB Create or Access time stamp field
byte 28 - 31 : XFCB Update time stamp field

Upon return, Function 102 returns a Directory Code in register
AL with the value 0 to 3 if the XFCB read operation was successful,
Or OFFH (255 Decimal) if the XFCB was not found. Register AH is set
to zero in both of these cases. If a physical or extended error was
encountered, Function 102 performs different actions depending on
the BDOS Error Mode (see Function 45). If the BDOS Error Mode is in
the default mode, the system displays a message at the console
identifying the error and terminates the calling process.

---
_Page 162_

MP/M-86 Programmer's Guide 6 System Calls : Function 102

Otherwise, Function 102 returns to the calling process with register
AL set to OFFH and register AH set to one of the following physical
Error Codes:

O01 : Permanent error
04 : Select Error

---
_Page 163_

MP/M-86 Programmer's Guide 6 System Calls : Function 103

RRKRKRKEKKKKKERRKRKEKEKEKEKRRK KAKA AKA K RK AK

\* \*
\* FUNCTION 103: WRITE FILE XFCB \*
\* \*
RRR RK RRR RRR RR KKK REE KKK KEKE RR EK KERR KR KKK
\* \*
\* Write Extended File Control Block \*
\* of a Disk File \*
\* \*
RHE KKK KK RE KEK KKK KEKE RR KEKE KEKE RK RRAKEEEE KERR KEK
\* \*
\* Entry Parameters: \*
\* Register CL: 67H \*
\* DX: FCB Address - Offset \*
\* DS: FCB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AL: Directory Code \*
\* AH: Physical or Extended Error \*
\* BX: Same as AX \*
x \*
RK KKK KK KKK ERR RK ERR KKK KR RR KK KER ERK EKER KER AAKK

The WRITE FILE XFCB function creates a new XFCB or updates the
existing XFCB for the specified file. The calling process passes
the address of an FCB in which the drive, name, type, and extent
fields have been defined. The "ex" field, if set, specifies the
password mode and whether a new password is to be assigned to the
file. The format of the extent byte is shown below:

FCB byte 12 (ex) : XFCB password mode

bit 7 - Read mode

bit 6 - Write mode

bit 5 - Delete mode

bit 0 - assign new password to the file

If bit 0 is set to 1, the new password must reside in the second 8
bytes of the current DMA. If the FCB is currently password
Protected, the correct password must reside in the first 8 bytes of
the current DMA, or have been previously established as the default
Password (see Function 106).

Upon return, Function 100 returns a Directory Code in register
AL with the value 0 to 3 if the XFCB create or update was
successful, or OFFH (255 Decimal) if no Directory Label existed on
the specified drive, or the file named in the FCB was not found, or
no space existed in the directory to create an XFCB. Register AH is
set to zero in all of these cases. If a physical or extended error
was encountered, Function 103 performs different actions depending

---
_Page 164_

MP/M-86 Programmer's Guide 6 System Calls : Function 103

on the BDOS Error Mode (see Function 45). If the BDOS Error Mode is
the default mode, the system displays a message at the console
identifying the error and terminates the calling process.
Otherwise, Function 103 returns to the calling process with register
AL set to OFFH and register AH set to one of the following physical
or extended Error Codes:

O01 : Permanent error

02 : Read/only disk

04 : Select Error

07 : File password error

---
_Page 165_

MP/M-86 Programmer's Guide 6 System Calls : Function 104

KREKKEKRKEKEKEKRKEREKKEEKKEKKEEKREKEKRRERKEKEKKEEKREREKEKEKKREKEKE

x \*
\* FUNCTION 104: SET DATE AND TIME \*
\* \*
KEKKEKKEKERKEEKEKEKKREKRRKERKERKRKEKREKREKKKEREKKEREKRKEKEKKRKEKE
\* \*
\* Set System Date and Time \*
\* \*
KREKEKKEKKERKREEKREKRKEKRRKEKEKRKE KKK KEREKKRREEREEKREKERKKKKEKRKEKER
\* \*
\* Entry Parameters: \*
\* Register CL: 68H \*
\* DX: TOD Address - Offset \*
\* DS: TOD Address - Segment \*
\* x
KRREKEKKKEEKREKRKEKRKEKKREKRRKEARKEKEKREKREEERERKEEKREEKKEKKEKKKREEE

The SET DATE AND TIME function sets the system internal date
and time. The calling process passes the address of a 4-byte
structure containing the date and time specification. The format of
the date and time data structure is:

byte 0 - 1: Date field
byte 2 Hour field
byte 3 : Minute field

The date is represented as a 16-bit integer with day 1 corresponding
to January 1, 1978. The time is represented as two bytes: hours and

Minutes stored as two BCD digits.

Under MP/M-86, this function also sets the Second field of the
system date and time to zero.

---
_Page 166_

MP/M-86 Programmer's Guide 6 System Calls : Function 105

RKAKEKKEKKKK KKK KKK KK KKKEKKKKKKKKKKKKEKKKKKKKKEKKKKAKESE

\* x
\* FUNCTION 105: GET DATE AND TIME \*
x \*
KREKKKEKEKKKRKER REE KERR KEKE KR KEKEKKKKEKEKKKEKKKKKKER
\* \*
\* Set System Date and Time \*
\* \*
KKREKRKEKKEKKKEKKEKK KEKE KKK EKKKEEKREKREKEKKEKRREKKKKKEKKKEKE
\* \*
\* Entry Parameters: \*
\* Register CL: 69H \*
\* DX: TOD Address - Offset \*
\* DS: TOD Address - Segment \*
\* \*
\* Return Values: \*
\* TOD filled in \*
\* \*
KREKKKKKKKEKEKEKEKE KEKE KK KEKKEKEKEKKKEKRKEKKKKKKRKKEKK

The GET DATE AND TIME function obtains the system internal date
and time. The calling process passes the address of a four-byte
data structure which receives the date and time values. The format
of the data structure is the same as the format described in
Function 104. This function is equivalent to MP/M-86 Function 155
except that it does not return the seconds field of the internal
time.

---
_Page 167_

MP/M-86 Programmer's Guide 6 System Calls : Function 106

KKEKKKEKEKKE KEKE KEKE KEKE KKEKKE EKER EKRKEKKEKEKKRKKKEKKKE

\* \*
\* FUNCTION 106: SET DEFAULT PASSWORD \*
\* \*x
KEKKKKKEK KKK KEKE EERE ERE EEK KEKE RRR KERKKEKERKKEKKKEK
\* \*
\* Establish a Default Password for file access \*
\* \*
KEE KKEKEREKKEKEK KEKE KERR KERKKKKEKKKKEKKKR KKK KKK
\* \*

\* Entry Parameters: \*
\* Register CL: 6AH \*
\* DX: Password Address - Offset \*
\* DS: Password Address - Segment \*
\* \*
\* \*

KEKE KKEKEEKEKEKEK REE KEKE KEKEKEKKEERREEKEKEKKRKRKKREE

The SET DEFAULT PASSWORD function allows a process to Specify a
password value before a process accesses a file protected by the
password. When the file system accesses a password protected file,
it checks the current DMA and the default password for the correct
value. The function does not return a password error if either
password is correct. MP/M-86 maintains a default password for each
Process currently running on the system. When a process (parent)
creates a subprocess (child), the child process inherits its default
console from its parent. Note: changing the default password
does not affect other processes currently running on the system.

To make a Function 106 call, the calling process passes the
address of an eight-byte field containing the password.

---
_Page 168_

MP/M-86 Programmer's Guide 6 System Calls : Function 107

KKK EKER RK KEK KEK KK KKK KEEKRKKKK ERR RARER KKKKKKKEKKK

\* \*
\* FUNCTION 107: RETURN SERIAL NUMBER \*
\* \*k
KEKKEKKKKEKKEK KERR KKK EK RE KRKREKRKKRKKKREKKKKEKKKKKKKKKEKK
\* \*
\* Return the Current System's Serial Number \*
\* \*
KAEKKK EKER KERR KEKE KK KEKE KK EKER KKEKRKEKKKKKEKKKEKKKKK
\* \*
\* Entry Parameters: \*
\* Register CL: 6BH \*
\* DX: SERIAL Address - Offset \*
\* DS: SERIAL Address - Segment \*
\* \*
\* Return Values: \*
\* SERIAL filled in \*
\* \*
KHER EKER KEKE KEKEERKEREKRKEREK KKK RKKKKEKKEREKE

Function 107 returns the MP/M-86 serial number to the
addressed, six-byte SERIAL field.

---
_Page 169_

MP/M-86 Programmer's Guide 6 System Calls : Function 128-129

KEKEKKEKEKKKEKERKE KEKE KER ERR KR KK RK RK EKER ERK KEKRKKKEKKEAKE

\* \*
\* FUNCTION 128: MEMORY ALLOCATION \*
\* FUNCTION 129: \*
\* \*
RREKKKEKEKEKRK KKK KKK EKER KR KEKE RRR RKRKKKKKEE
\* \*
\* Allocate a Memory Segment \*
\* \*
KRAEKKKEKKEEK ERK KEKE KEKE KRRRKERKEKEKEEKRERKKKKKKKKRKE
\* \*
\* Entry Parameters: \*
\* Register CL: O80H or O81H \*
\* DX: MPB Address-Offset \*
\* DS: MPB Address~Segment \*
\* \*
\* Return Values: \*
\* Register AX: 0 (success) \*
\* OffffH (fail) \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
RREKKKEKKEKEKRKKE EEK KEKE KK RK KKEE KEKE KEEEKRREKREKEKEEEE

+----- +=----- +----- +-~--- ++---- +----- +----- ++--+--- +----- 4+---~-- +
| START | MIN | MAX | 0000 | 0000 |
+----- +----- +----- t=----- +----- +----- +----- +----- +----- +----- +

Figure 6-7. Memory Parameter Block (MPB)

START if non-zero, an absolute request at this paragraph

MIN minimum memory needed (paragraphs)

MAX maximum memory wanted (paragraphs)

0000 these fields must be zero (0). They are used

internally and for future use.

The MEMORY ALLOCATION function allows a program to allocate
extra memory. A successful allocation will allocate a contiguous
memory segment whose length is at least the MIN and no more than
the MAX number of paragraphs specified in the MPB. The START field
of the MPB is modified to be the starting paragraph of the memory
segment. The MIN and MAX fields are modified to be the length of
the memory segment in paragraphs. Memory Segments can be
explicitly released through the MEMORY FREE function. MP/M-86 will
also release all memory owned by a process at termination.

---
_Page 170_

MP/M-86 Programmer's Guide 6 System Calls : Function 130

KEKKKKKKKEKKRERKEKEKEKEKK KE KKKEKKEKRKKKRKKEKKKKKKKKKKKKE

\* \*
\* FUNCTION i30: MEMORY FREE \*
x \*
KKK KKKKKEKEKKEKEKKEKKKEEREKKKKEKEEKKKEKKKEKKKKKKKKKRKKEE
\*x \*
\* Free a Memory Segment \*
\* \*
KKRKKKKKKKEKKEKERAKKKKKKE KEK KEKE KRKKKEEKRKKKKKEKKKKREK
\* \*

\* Entry Parameters: \*
\* Register CL: 082H \*
\* DX: MFPB Address - Offset \*
\* DS: MFPB Address - Segment \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OfffLfH on failure \*
\* BX: Same as AX \*
\* CX; Error Code \*
\* \*
\* \*

KRKEKKKKKEKKEKKKKREKEKRKEKERERKEKRKEEKKRKREKREKKKEKRAKKEKKEKK

te---- +----- +----- +----- +
| START | 0000 |
+----- +----- +----- +----- +

Figure 6-8. Memory Free Parameter Block (MFPB)

The MEMORY FREE function releases memory starting at the
START paragraph to the end of a single previously allocated segment
that contains the START paragraph. If the START paragraph is the
same as that returned in the MPB of a memory allocation call, then
Function 130 releases the whole memory segment.

Under certain circumstances, MP/M-86 allows memory segments
to be shared among different processes. In this case, the system
recovers a released memory segment only when no other processes own
the memory segment.

---
_Page 171_

MP/M-86 Programmer's Guide 6 System Calls : Function 131

RREEKKKEKKEEKKEE KEKE KERR EKKE KER KR EKER ERKEEKKEKKKKEK

\* \*
\* FUNCTION 131: POLL DEVICE \*
\* \*
KEEKKKEKKEREKKEKEEKE EKER KEKE ARKKEKR EKER KER KKK REE KEKKEKKKEK
\* \*
\* Poll a Device \*
\* \*
KEKKEKKEKKEKEKEKEEKR EKER ERK KKK KER EK ERE KK KERR KEKEEKE
\* k

Entry Pérameters:
Register CL: 083H
DL: Device Number

\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OfffEH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KREEKKEKKEKEKKEKRE KERR KEKE KEKE RERKEKEKEKKKKKEK

The POLL DEVICE function is used by the XIOS to poll non-
interrupt driven devices. It is should be used whenever the XIOS
is waiting for a non-interrupt event. The calling process
relinquishes the CPU and allows MP/M-86 to poll the device at every
dispatch. The XIOS contains routines for each device number.
These routines are called through the XIOS POLL DEVICE function
(see the description in the MP/M-86 System Guide), and they return
whether the device is ready or not. When the device is ready,
MP/M-86 will restore the calling process to the 'RUN' state and
return. Upon return, the calling process knowns that the device is
ready.

---
_Page 172_

MP/M-86 Programmer's Guide 6 System Calls : Function 132

KKK KKK KKK KEKE KK KEK EEK EK RK ERK EKRKKKKKEKKKKKKEKER

\* \*
\* FUNCTION 132: FLAG WAIT \*
\* \*
KEKE KEKE KEKE KEKKEKERKEKRKARKRKEKKEKKKEKEKKKKKEKEKKERKKEKE
\* \*
\* Wait for a System Fleg \*
\* \*
KEKE KEKR KE KEKE KEKE KKKEKRKE KKK KEKEKRKKKEKREREKKEKEKKKEKEKE
\* \*

Entry Péerameters:
Register CL: 084H
DL: Flag Number

\* \*
\* x
\* \*k
\* \*
\* Return Values: x
\* Register AX: 0 on success \*
\* OfLLLH on feilure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\*k \*
\* \*

KEK KKEKEKEKKKKKKEKREKREKK KEKE KERR EKKKEKKEEKKREKKKKEKKEE

The FLAG WAIT function is used by a process to wait for an
interrupt. The process relinquishes the CPU until an interrupt
routine calls the FLAG SET function which places the waiting
process in the ‘RUN' state. When Function 132 returns to the
calling process, the interrupt has either occured, or an error
occured. An error occurs when a process is already waiting for the
flag. If the Flag was set before Function 132 was called, the
routine returns successfully without relinquishing the CPU. This
routine is meant to be used by the XIOS. The mapping between types
of interrupts and flag numbers is maintained in the X10S, although
MP/M-86 reserves flags 0,1,2 and 3 for system use,

---
_Page 173_

MP/M-86 Programmer's Guide 6 System Calls : Function 133

KEE KKKERKEKEKEKE EK KERR KK KR KEKE KEKE RE KEKE KKK KEK KEE

\* \*
\* FUNCTION 133: FLAG SET \*
\* \*
RKKKKE KKK RRA KK ERK RRR KKK EK RK KER ER ERE EK EKKKEKKEKE
\* \*
\* Set a System Flag x
\* x
KKKKKKKE KKK KEKE KKK KKK EKER KE KERR EREKRRKKKAKKAK KK
\* \*

Entry Parameters:
Register CL: 085H
DL: Flag Number

\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OffffH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KREKK KK EKER EKER KKK KEKE KEK KER KKK KEKE KKK RRR REEEK

The FLAG SET function is used by interrupt routines to
notify the system that a logical interrupt has occured. A process
waiting for this flag will be placed back into the 'RUN' state. If
there are no processes waiting, then the next process to wait for
this flag will return successfully without relinquishing the CPU.
The function detects an error if the flag has already been set and
no process has done a FLAG WAIT call to reset it.

---
_Page 174_

MP/M-86 Programmer's Guide 6 System Calls : Function 134

KKK KKK KK ERK KKK KKK ERK KKK KKK KERR KE RK KRKEKKEREKEKKER

\* \*
\* FUNCTION 134: MAKE QUEUE \*
\* \*
KKK K KKK KEKE EK KKK EKER KKK KKK KKK KKKAKKEKKKEKKK
\* \*
\* Meke a System Queue \*
\*k \*
KEKE KKK KARE KE KKK KK KKK RKEK KEKE KE KRKKKKEKEKKKRKEKKEKRKEKEKEK
\* \*

Entry Parameters:
Register CL: O86H
DX: QD Address - Offset
DS: QD Address - Segment

\* \*
\* \*k
\* \*k
\* \*
\* \*k
\* Return Values: \*
\* Register AX: 0 on success \*
\* OffLLEH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KEKE KKK KKK KK EEK EKER KKK RK KKK KRKEEKEE

+---~- +----- +----- +----- fe --te---- +----- +---~-- +
; \* 0000h | \* 0000h_ | FLAGS | NAME ...
$----+--+~—---- +--- ee bo +—-— +-—---- Soliant ta +
| ... NAME | MSGLEN |
4+------ t----- +------ prea +----- t----- +----- +----~- +
| NMSGS | \* 0000h | \* 0000h | \* 0000h |
t----- +----- t----- 4+----- +-~---- +----- +----- +----- +
| \* 0000h | BUFFER |

+----- +----- f—----f2---- +

Figure 6-8. Queue Descriptor (QD) Format
FLAGS Queue Flags. The bits are defined as follows:

OOOlH ~ Mutual Exclusion Queue

0002H - CANNOT be deleted

OOO04H - restricted to SYSTEM processes
OOO0O8H -— RSP message queue

OO10H - Used Internally

0020H - RPL address queue

0040H - Used Internally

O0O080H - Used Internally

Remaining Flags reserved for future use

NAME 8-byte Queue Name. All 8 bits of each character are
matched on an OPEN QUEUE call.

MSGLEN Number of bytes in each logical message

NMSGS Maximum number of logical messages to be supported.

---
_Page 175_

MP/M-86 Programmer's Guide 6 System Calls : Function 134

If the number of messages written to the queue equals
this maximum, no more messages are allowed until a
Message is read.

BUFFER address of the queue buffer. This buffer must be
(NMSGS \* MSGLEN) bytes long. The address is an
offset relative to the DS register. This field is
unused if the QD resides outside of the System Data
Area. Typically this field is 0 if the queue is
being created by a transient program. RSPs_ that
Create queues must initialize this field to point to
a buffer. The Data Segment of an RSP's queue is
considered part of the System Data Area unless it is
beyond 64k of the beginning of the System Data Area.

0000 for internal use. Must be initialized to zero.

Every System Queue under MP/M-86 is associated with a Queue
Descriptor that resides within the MP/M-86 System Data Area. In
the MAKE QUEUE function, the calling process passes the address of
a Queue Descriptor. If this Queue Descriptor is within the MP/M-86
System Data Area, the system uses it directly for the System Queue.
If the Queue Descriptor is outside of the System Data Area, the
System obtains a Queue Descriptor from an internal Queue Descriptor
table. If there are no unused Queue Descriptors in the internal
table, the function returns an Error Code. The size of this table
is determined by GENSYS at system generation time.

The buffer for a System Queue must also reside within the
System Data area. For non-zero length buffers, resident buffers
are used directly. The system obtains a buffer from the Queue
Buffer Area if the buffer does not reside within the System Data
Area. The size of the buffer is calculated from the NMSGS and
MSGLEN fields. The function returns an Error Code if there is not
enough unused buffer area left to accommodate this new buffer. The
size of the Queue Buffer Area is determined by GENSYS at system
generation time.

All System Queues must have unique names. The function
returns an Error Code if a System Queue already exists by the given
name,

Under MP/M-86, all System Queues must be explicitly opened

(see Function 135) before being used to read or write messages or
to delete the queue.

---
_Page 176_

MP/M-86 Programmer's Guide 6 System Calls : Function 135

KKK KKK KKK KK KKK RE RRR KEK KKK KEK EKER KEKE KE KKKEKKEKEKKKKKKE

\* \*
\* FUNCTION 135: OPEN QUEUE \*
\*x \*
KREKKK KE KKK KERR KKK KKK KEK KKK KEE REKKEKKRKEEKKKEEKKEKERKKKE
\* \*
\* Open a System Queue \*
x k
KR KK RRR KKK EK KK KEK KEK KKK KKK RKKE KERR KKKERKEKEKKRKKEKEK
\* \*

Entry Parameters:
Register CL: 087H
DX: QOPB Address - Offset
DS: QPB Address - Segment

\* \*
\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OffLFH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KKK KKK KKK KR KKK KR RK KERR KR KEE RRR RKEKKEKKERKKEE

+----- t----- +----- +----- +----- +----- $----- +----- +
| RESERVED | QUEUEID | NMSGS | BUFFER |
to----t- ~+ $—=-=-+-----+----- +----- +----- +
| NAME |
+----- +----- +----- +----~ +----- +--~--- +----- +----- +

Figure 6-9. Queue Parameter Block (QPB)

QUEUEID modified by OPEN QUEUE

NMSGS not used for OPEN QUEUE
BUFFER not used for OPEN QUEUE
NAME 8-byte System Queue name.

All System Queues under MP/M-86 must be explicitily opened
before a read, write or delete operation can be done. The OPEN
QUEUE function examines each existing System Queue and attempts to
match the name in the QPB with the name of a System Queue. All
eight bytes of the name must match for a successful open. All bits
of each byte are examined. If the open operation is successful,
the OPEN QUEUE function modifies the QUEUE ID Field of the QPB.
Once the the Queue is opened, subsequent reads, writes or a delete
are allowed.

---
_Page 177_

MP/M-86 Programmer's Guide 6 System Calls : Function 135

The function returns an Error Code if the System Queue does

not exist, or if it is restricted to SYSTEM processes and the
calling process is a USER process.

---
_Page 178_

MP/M-86 Programmer's Guide 6 System Calls : Function 136

KK KKK RK KK RR KKK KKK K RK KK EKER KERR KEK KEK KR RE RR EREKKKK

\* \*
\* FUNCTION 136: DELETE QUEUE \*
\* \*
KA KK KKK IK KK KKK RR KER KKK KKK KKK KEK RR KKK KKK KEKE
\* \*
\* Delete a System Queue \*
\* \*
RK KR KKK KKK KK KEKE KR KKKE KK KKK AKA RRR RARER EK KEK KKK EK
\* \*

Entry Parameters:
Register CL: 088H
DX: QOPB Address ~- Offset
DS: OPB Address - Segment

\* \*
\* \*
x x
x k
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OffffH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KKK KKK RK KKK KKK KEKE RRR REREEKKEREE

+----- +----- +----- +----- +----- +----- +----- +----- +
| RESERVED | QUEUEID | NMSG | BUFFER |
Sie +----- +----- to---- fe noob reco te con $—-—---- +
| NAME |
+----- 4+----- +----- +----- 4+------ +----- +----- +----- +

Figure 6-10. Queue Parameter Block (QPB)

RESERVED illed in by previous OPEN QUEUE

QUEUEID filled in by a previous OPEN QUEUE

NMSGS not used for DELETE QUEUE
BUFFER not used for DELETE QUEUE
NAME not used for DELETE QUEUE

The DELETE QUEUE function removes a System Queue from the
system, The system returns Error Codes if the Queue cannot be
deleted or if the Queue hasn't been previously opened.

---
_Page 179_

MP/M-86 Programmer's Guide 6 System Calls : Function 137

KEK KKAK KR KKK KKK RK KEK IKE KKK KKK RK KEK EKKEKEKKKKEK

\* \*
\* FUNCTION 137: READ QUEUE \*
\* \*
RRR KKK KKK KEKE KKK RK RR KK KK KKK KKK RK K KEKE RK KKK KE
\* \*
\* Read a Message from a System Queue \*
\* \*
RRR KKK RRR KKK KKK RK KK ERR KKK KEKE RAK RE RE KKK REKEEKRE KEK
\* \*

Entry Parameters:

Register CL: 089H
DX: QPB Address - Offset
DS: QPB Address - Segment

Offf£LH on failure
BX: Same as AX

\*

x

\*

\*

\*

\* Return Values:
\*

\*

\*

\* CX: Error Code
\*

\*

\*

\*

\*

\*

\*

\*

Register AX: 0 on success \*
\*

\*

\*

\*

\*

KEK KKK KEK KKK AKER KEK KK RK KKKKEEK EERE KEKRKKKEKKKKK KEK

+----- +----- +----- +----- +----- +----- +----- +----- +
| RESERVED | QUEUEID | NMSGS | BUFFER |
+--+ +----- +----~ +----- +----- +----- +---~- +----- +
| NAME |
+----- +----- 4+----- +----- +---~- +----- t----- t----- +

Figure 6-11. Queue Parameter Block (QPB)
RESERVED filled in by previous OPEN QUEUE
QUEUEID filled in by previous OPEN QUEUE
NMSGS number of messages to read

BUFFER offset of buffer relative to the current Data Segment.
Message is placed in buffer indicated.

NAME not used by READ QUEUE

The READ QUEUE function reads a message from a System Queue
that was previously opened by the calling process. The function
returns an Error Code if the Queue was not previously opened or if
the System Queue has been deleted since the OPEN QUEUE call. Tf
the NMSGS field is zero (0) or one (1), then the function reads one
message and places it into the buffer indicated by the BUFFER field
of the QPB. If there are not enough messages to read from the
Queue, the calling process waits until another process writes into
the queue before returning.

---
_Page 180_

MP/M-86 Programmer's Guide 6 System Calls : Function 138

RESERVED
QUEUEID
NMSGS

BUFFER

NAME

KKK KR KK KKK KKK KKK RIK KERR KER KEKE KKK KE EK KKEKKE KKK KKK

\* \*
\* FUNCTION 138: CONDITIONAL READ QUEUE \*
\* \*
KKK KK KKK IKK KKK RK IRR KER EKER KR RK KKK KKKEKKEKKE
\* \*
\* Conditionally Read a Message \*
\* from a System Queue \*
\* \*
KR KR RK KKK KKK RIKKI E RRR RK KK EKER EREKRER
\* \*
\* Entry Parameters: \*
\* Register CL: O08aH \*
\* DX: QPB Address - Offset \*
\* DS: QPB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OffLEH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
KK KI KK KKK RIK RRR KR KRRRRKRRRRRAEKEKRERKERRRKRKEERE KEK
+----- t----- +----- +----- +----- +----- +----- +----- +
| RESERVED | QUEUEID {| NMSGS | BUFFER |
+----- $o---- t-----+- ~$.=---4+----- 4----- +----- +
| NAME |
+----- +----- +----- +----- +----- +----- +----- +----- +
Figure 6-12. Queue Parameter Block (QPB)

filled in by previous OPEN QUEUE

filled in by previous OPEN QUEUE

number of messages to read

offset of buffer relative to the current Data Segment.

Message is placed in buffer indicated.

not used by READ QUEUE

The CONDITIONAL READ QUEUE function is analagous to the READ
QUEUE function except that it returns an Error Code if there are
not enough messages to read instead of waiting for another process
to write to the queue.

---
_Page 181_

MP/M-86 Programmer's Guide 6 System Calls : Function 139

RRKKK KK KEK KEK KKK KEKE KKK KK EK KEKE KEE KKK RR KKEKEK RK KEE

\* \*
\* FUNCTION 139: WRITE QUEUE \*
\* \*
RRR KKK KKK RK KKK RRR RK KK RK KKK KER RRR KAR KER REE KARE
\* \*
\* Write a Message to a System Queue \*
\* \*
KKKKKKKK KKK KEE ERR KKK KEKE KKK KERR KK KKK KERR KEKKE KARE
\* \*
\* Entry Parameters: \*
\* Register CL: O8bH \*
\* DX: QPB Address - Offset \*
\* DS: QPB Address - Segment \*
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* OffE£H on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
RHEKKKKKKK EKER KEKE KR KEK RRR KEK KEK KKK KKK KKRERKEKREEEKKKKK
+----- +----- +----~ +----- t+---- +----- +----- t----- +
| RESERVED | QUEUEID | NMSGS | BUFFER |
+----- +----- t----- t----- +----- +----- +----- to---- +
| NAME |
t----- +----- +----- +----- +----- t----- +----- +----~- +

Figure 6-13. Queue Parameter Block (QPB)
RESERVED filled in by previous OPEN QUEUE
QUEUEID filled in by previous OPEN QUEUE
NMSGS number of messages to write

BUFFER offset of buffer relative to the current Data Segment.
Message is read from buffer indicated.

NAME not used by WRITE QUEUE

The WRITE QUEUE function writes a message to a System Queue
that was previously opened by the calling process. The function
returns an Error Code if the Queue was not previously opened or if
the System Queue has been deleted since the OPEN QUEUE call. Tf
the NMSGS field is zero (0) or one (1), then the function reads one
message from the buffer indicated by the BUFFER field of the QPB
and writes it into the System Queue Buffer. If there is not enough
buffer space in the Queue, the calling process waits until another
Process reads from the queue before writing to the Queue and
returning.

---
_Page 182_

MP/M-86 Programmer's Guide 6 System Calls: Function 140

KKK KKK IKKE KEK KKK ERE RK KKK KEKE KEKE KKK KE KKK

\* \*
\* FUNCTION 140: CONDITIONAL WRITE QUEUE \*
\* \*
KKK KKK KKK IKK IKK RIK EEK KER EKER KERR KREKKEKEKEER
\* k
\* Conditionally Write a Message \*
\* to a System Queue \*
x \*
KKK IK IKKE KKK KKK KKK KKK KR KEKE KERR KKK KKKKER
\* \*&

Entry Parameters:
Register CL: O8cH
DX: QPB Address - Offset
DS: QPB Address - Segment

\* \*
\* \*x
\*k \*
\* \*
\* k
\* Return Values: \*
\* Register AX: 0 on success \*
\* OfffEH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KKK KK KKK KEKE KKK EKER KEKE KKK RRR RERREKEKKEKKKKEEE

+----- +---~- 4+----- +----- +----- 4+----- $----- 4---<- +
| RESERVED | QUEUEID | NMSGS | BUFFER |
+----- +----- +----- +----- t----- +----- +o---- +----- +
| NAME |
+----- +----- +----- +----- +----- +----- +----- +----- +

Figure 6-14. Queue Parameter Block (QPB)
RESERVED filled in by previous OPEN QUEUE
QUEUEID filled in by previous OPEN QUEUE
NMSGS number of messages to write

BUFFER offset of buffer relative to the current Data Segment.
Message is read from buffer indicated.

NAME not used by WRITE QUEUE

The CONDITIONAL WRITE QUEUE function performs ia analagous to
the WRITE QUEUE function except that it returns an Error Code if
there is not enough System Queue Buffer to for the message to be
written instead of waiting for another process to read from the
queue.

---
_Page 183_

MP/M-86 Programmer's Guide 6 System Calis : Function 141

KEKE KKEKEKKE KERR KEKE KK KEKE R ERE KRKKKEKEKKKKKKKE

x x
\* FUNCTION 141: DELAY \*
\* k
KRREKKKKKKEKEK KEKE KEKE K KKK KKK KEKE EKER RE KKK KEK KE KKKKK KEE
\* k
\* Delay for specified number of System Ticks \*
\* \*
KEKE EKER EEK KEK ERK KKK EKER KERR EKK KEKE
\* \*
\* Entry Parameters: \*
\* Register CL: O8dH \*
\* DX: Number of System Ticks \*
\* \*
KREKKKKEKERKE EEK EKER KKK RE KER EKER KEE KKEKREREKREE

The DELAY function causes the calling process to wait a until
the specified number of System Ticks has occured, The DELAY
function avoids the necessity of programmed delay loops. It allows
other processes to use the CPU resource while the calling process
waits.

The length of the System Tick varies among installations. A
typical System Tick is 60Hz (16.67 milliseconds). In Europe, it is
likely to be 50Hz (20 milliseconds). The exact length of the System
Tick can be obtained by reading the 'TICKSPERSEC' value from the
System Data Area. (see the MP/M-86 System Guide).

There is up to one Tick of uncertainty in the exact amount of
time delayed. This is due to the DELAY function being called
asynchronously from the actual time base. The DELAY function is
quaranteed to delay the calling process at least the number of ticks
Specified. However, when the calling process is rescheduled to run,
it may wait quite a bit longer if there are higher priority
processes waiting to run. The DELAY function is used primarily by
programs that need to wait specific amounts of time for I/O events
to occur. Under these conditions, the calling process usually has a
very high priority level. If a process with a high priority calls
the DELAY function, the actual delay is typically within a System
Tick of the amount of time wanted.

---
_Page 184_

co

MP/M-86 Programmer's Guide 6 System Calls : Function 142

KKK KKK KEK KKK KK KEK KEKE KER EKKKEKKRKEKKK KK KK KKK KK KKK KEK

\* \*
\* FUNCTION 142: DISPATCH \*
\* x
KKK KKK KKK ERK KR KKK KKK EKER KERR KEKE KKEKKKKKKEKKKKRKEKE
\* \*
\* Call System Dispatcher \*
\* \*
KK KKK KKK KKK KKK KKK KEK KKK KKK KKK KEKE KER KERR KKKKKKEEK
\* \*
\* Entry Parameters: \*
\* Register CL: O8eH \*
x \*

KEK KKK KKK KE KKK EKER KKK KK RE KKK RR K KK EKRKEKREKKKKK KK KK

The DISPATCH function forces a reschedule of processes that
are waiting to run. Normally, dispatches occur at every interrupt,
and whenever a process releases a system resource. Dispatching also
occurs whenever a process needs a system resource that is not
currently available. For a CPU-bound process, dispatch occurs at
the next System Tick.

The MP/M-86 Dispatcher is priority driven, with round-robin
scheduling of equivalent-priority processes. When a process calls
the DISPATCH function, it is rescheduled process such that processes
with higher or equivalent prioritics are given the CPU before the

calling process obtains it again.

---
_Page 185_

MP/M-86 Programmer's Guide 6 System Calls : Function 143

KEKE KERR RK KERR KERR KEKE KEKE KR KERR KERKEKER RK RK KEK

k \*
\* FUNCTION 143: TERMINATE \*
\* \*
RRKKKKEKEKKE KEKE KEE RK KK KKK KEKE REE KR RK KEE KK KEKE KKKKREKE
\* \*
\* Terminate Calling Process \*
\* x
KERR RE REKEEKE RRR EKER KKK KERR EKKEKKKEKE RK KRKKRKEKEEE
k \*
\* Entry Parameters: \*
\* Register CL: O8fH \*
\* DL: Terminate Code \*
\* \*
KKK KKK KKK KEE KR KEK KERR EK KKK EKER ER KEE KRKEKEKKEES

The TERMINATE function terminates the calling process. If
the Terminate Code is not OffH, the function can only terminate a
USER process, If the Terminate Code is OffH, the function can
terminate the calling process even though the process's SYSTEM flag
is on. Function 143 can not terminate a process with the KEEP flag
on. If the termination is successful, the function releases the
Mutual Exclusion Queues owned by the process. It also releases all
memory segments owned by the process, and returns the Process
Descriptor to the PD table. Since memory can be owned by more than
one process, the system does not recover memory segments system
until every process owning the memory segment has either terminated
or explicitly releases the memory segment with the MEMORY FREE call.

Function 143 does not return any results to the calling
process, If the function returns to the calling process then the
TERMINATE call failed for one of two reasons. Either the process
has the KEEP flag on, or it has the SYSTEM flag on, and the

Terminate Code is not OffH,

---
_Page 186_

MP/M-oo Froytatuier’S Guide B oY¥Stem Cails : Function 1%

KEK KKK KEKE KKK KKKKKRKEKEKEKEKKEKKEKKKKRKKKKKKEKRKEE

\*k \*
\* PUNCTION 144: CREATE PROCESS \*
\* \*
KKK KK KKK KKK KEKE KEI KEE KEKE REE KEKREKKEKKKKEKKKEKEKKE
\* \*
\* Create a Process \*
\* \*
KK KKK KEK KKK EK KEKE KK EKER KEKE EKER KKEKKKEK KKK
\* \*

Entry Parameters:
Register CL: O090H
DX: PD Address - Offset
DS: PD Address - Segment

\* \*
x \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* Register AX: 0 on success \*
\* Of£LLH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KK KK KE KKKKKEKEER ERE ERK ERK REE RRR KEEEREKKERKES

The CREATE PROCESS function allows a process to create a
subprocess within its own memory area. The child process shares all
memory owned by the calling process at the time of the CREATE
PROCESS call. If the Process Descriptor (PD) is outside of the
Operating System Area, the system copies it into a PD from the
internal PD Table. The function returns an Error Code if there are
no more unused PDs in the Table. The number of PDs in the PD Table
is specified by GENSYS at system generation time. The User Data
Area (UDA) can be anywhere in memory but is required to be on a
paragraph boundary. A Resident System Process (RSP) is considered
within the Operating System Area. PDs that reside within an RSP are
usually not copied into the PD Table. The only time the system
copies the PD is if it is not within 64k of the System Data Area.
Process Descriptors as well as Queue Descriptors and Queue Buffers
are required to be within the System Data Area because they are
linked together on various System Lists or are used by more than one
process. Because of this, they cannot be in the Transient Process
Area (TPA) where they cannot be protected.

More than one process can be created by a single CREATE
PROCESS call if the LINK field of the PD is non-zero. In this case,
it is assumed to point to another PD within the same Data Segment.
After it creates the first process, the function checks the Process
Descriptors LINK field. Using this linked list of PDs, a single
CREATE PROCESS call can create multiple processes.

NOTE!! The function does not check the validity of the PD
addresses passed by the calling process. An invalid PD address can
cause MP/M-86 to crash if no hardware memory protection is available
on the system.

---
_Page 187_

MP/M-86 Programmer's Guide 6 System Calls : Function 144

LINK

THREAD

STAT

PRIOR

+----- +----- t----- +----- +----- +----- +----- +----- +
| LINK | THREAD ISTAT |PRIOR| FLAG |
+----- +--~~--- +----- +----- +----- +----- +----- +----- +
| NAME |
t----- +-~-~-- +----- +----- +----- +----- +----- t----- +
| UDA [DISK | USER| RESERVED |
+----- +----- +----- +----- +----- +----- +----- +----- +
| RESERVED | PARENT |
+--~-- t-~---- 4+----- +----- +----- +----- +----- +----- +
| cnNSs | RESERVED |LIsT | RESERVED |
+----- +----- +----- +----- +----- +----- +----- +----- +
| RESERVED |
+----- +----- +----- +----- +----- +----- +----- +----- +

Figure 6-14. Process Descriptor (PD) Format

link field for insertion on current System List. If this
fields initial value is non-zero, it is assumed to point
to another PD. This field is used to create more than
one process with a single CREATE PROCESS call.

link field for insertion on Thread List. Initialized to
be zero (0).
Current Process activity. Initialized to be zero (0).

00 RUN The process is ready to run. The STAT field
will always be in this state when a process
is examining its own Process Descriptor.
The PD is on the Ready List. The Currently
running process is always at the head of
Ready List.

O01 POLL The process is polling a device. The PD is
on the Poll List.

02 DELAY The process is delaying for a specified
number of System Ticks. The PD is on the
Delay List

06 DQ The process is waiting to read a message

from a System Queue that is empty. The PD
is on the DQ List whose root is in the Queue
Descriptor of the System Queue involved.

07 NQ The process is waiting to write a message to
a System Queue whose buffer is full. The PD
is on the NQ List whose root is in the Queue
Descriptor of the System Queue involved.

08 FLAGWAIT The process is waiting for a System Flag to
be set. The PD is in the Flag Table entry
of the flag it is waiting for.

09 CIOWAIT The process is waiting to attach to a
Character I/O device (console or list) while
another process owns it. The PD is on
CQUEUE List whose root is in the Character
Control Block of the Device in question.

Current priority. Process scheduling is done based on
this field. Typical user programs run at a priority of

---
_Page 188_

MP/M-86 Programmer's Guide 6

200. 0
priority.

System Calls Function 144

priority and 255 is the ‘worst!
is a list of priority

is the 'best'
The following

assignments used by most MP/M-86 systems.

0 - 31 Interrupt handlers
32 - 63 System processes
64 - 149 Undefined
150 Initialization Process
151 - 197 Undefined
198 Terminal Message Process
199 Undefined
200 Default User Priority
201 - 254 User Processes
255 Idle Process
FLAG Bit field of flags determining run time characteristics of a
process. Initialize as needed. All undocumented flags
are used internally or are reserved for future use.
001H
sys System Process. Has priviledged access to various
features of MP/M-86. This process can only be
terminated if the Termination Code is Offh. This
process can access restricted System Queues. This
flag is turned off if the calling process is not a
System Process.
002H
KEEP This process cannot be terminated. This flag is turned
off if the calling process is not a System Process.
004H
KERNEL This process resides within the Operating System. This
flag is turned off if the PD is not within the
Operating System.
010H
TABLE This PD originated from the PD Table. When this
process terminates, the PD will be recycled into the
PD Table.
020H
RESOURCE This process is currently waiting for a resource. Set
to zero at initialization.
040H
RAW This process is doing RAW Character I/O through its
default console. Reset at each Console Call.
080H
Tc An attempt was made to terminate this process through
some external event but could not be terminated
because of the TEMPKEEP flag. Initialized to zero.
NAME Process Name. Eight bytes, all eight bits of each byte is
used for matching process names.
UDA Segment Address of this processes User Data Area.

Initialized to be the number of paragraphs from the

beginning of the calling processes Data Segment.

The

User Data Area contains process information that is not

---
_Page 189_

MP/M-86 Programmer's Guide 6 System Calls : Function 144

DISK

USER

PARENT

CNS

LIST

RESERVED

needed between processes. It also contains the System
Stack of each process. See UDA description Below.

Current default disk
Current default user number
process that created this process.

Current default console's Character Control Block.
Initialized to be the default console number,

Current default List device's Character Control Block.
Initialized to be the default List device number.

Reserved for internal Use. These fields must be
initialized to zero (0).

---
_Page 190_

MP/M-86 Prograimmer'S Guide 6 System Calls : Function 144
¥y

00h
08h
10h
18h
20h
28h
30h
38h
40h
48h
50h
58h
60h

68h

F8h

+----- t----- +----- +----- +----- +----- t-H--- +----- +
| RESERVED | DMA OFFSET | RESERVED |
+----- +----- +----- +-----/ b---- = +----- +----- +----- +
| RESERVED |
+----- +---~- +----- +----- +----- +---~- +----- fern - +
| RESERVED |
+----- +----- +----- teen a +----- t—s--- +----- +----- +
| RESERVED

+----- t----- t+----- +----- +----- +----- +----- +----- +
| AX | BX | CX | DX |
+----- +----- +----- +------ f------ f----- +~----- +----- +
| DI | SI | BP | RESERVED |
+----- +----- +----- +----- +----- t----- +----- +----- +
| RESERVED | SP | RESERVED |
+----- +----- +----- +-~--- +---~- +----- t----- +----- +
| INT 0 | INT 1 |
+----- +----- +----- +----- +----- t----- +----- +----- +
| INT 2 | INT 3 |
+----- +----- t----- +----- +----- +----- t----- +----- +
| INT 4 | RESERVED |
+----- +----- +~-~---- +----~ +----- +----- +----- +----- +
| cs | DS | ES | Ss |
+----- ta---- t----- t—---- +----- +----- +----- +----- +
| INT 224 | INT 225 |
+----- +----- +--~-=-- +----- to---- +---+-- +----- +----- +
| RESERVED |
te---- fon +----- +----- +----- +----- +----- +----- +
| | 6Fh
| |
| USER SYSTEM STACK |
| | FFh
+-~---- +----- +----- t----- +----- +----- +----- +----- +

Figure 6-15. User Data Area (UDA)

The length of the UDA is 256 bytes,and it must begin on a

paragraph boundary.

DMA OFFS

AX,BX,CX,DX,

DI,SI,BP

SP

The initial DMA offset for the new process. The
segment address of the DMA is assumed to be the same
as the initial Data Segment (see DS below).

The Initial register values for the new process.
These are typically set to zero.

The initial Stack Pointer for the new process. The
Stack Pointer is relative to the initial Stack
Segment (see SS Below). The initial stack of the
new process must be initialized with the offset of
the first instruction to be executed by the new
process. The word that the Stack Pointer points to

---
_Page 191_

MP/M-86 Programmer's Guide 6 System Calls : Function 144

INT 0, INT l,
INT 2, INT 3,
INT 4

CS,DS,ES,SS

INT 224,
INT 225

is the initial Instruction Pointer. Two words must
follow the initial IP which will be filled in with
the initial Code Segment (see cs Below) and the
initial flags. MThe Initial flags will be set to
0200H which means that interrupts are on and all
other flags are off. MP/M-86 starts a new process
by executing an Interrupt Return instruction with
the initial stack.

Low Memory

| .
+----- +----- +
SP -> | IP |
+----- +----- +
| 0 | (cS)
+----- +----- +
| 0 | (Flags)
+---~- +-----

The initial interrupt vectors for the first five
interrupt types can be set by filling in these
fields. The first word of each field is the
Instruction Pointer (IP) and the second word is the
Code Segment (CS) of the interrupt routine which
will service these interrupts. Those fields which
are zero will be initialized to be the same as the
calling processes interrupt vectors. These fields
are typically initialized to be 0.

The initial Segment Addresses for the new process are
taken from these fields. Those fields that are zero
are initialized to be the same as the calling
processes Data Segment.

Interrupts 224 and 225 are used to communicate with
MP/M-86 by typicalprograms. These interrupt vectors
will be initialized to be the same as the calling
process if these values are zero. The ability to
change these values allows a run-time System to
intercept MP/M-86 calls that its children make. The
Suggested protocal is to keep INT 225 pointing to
the MP/M-86 entry point and changing INT 224 to
point to an internal routine. When a child process
does an INT 224, the internal routine can filter
calls to MP/M-86 using INT 225 for the actual MP/M-
86 call.

---
_Page 192_

MP/M-86 Programmer's Guide 6 System Calls : Function :44

RESERVED These fields are used internally and must be
intialized to zero.

---
_Page 193_

MP/M-86 Programmer's Guide 6 System Calls : Function 145

The SET PRIORITY function sets the
to the specified value. This function is useful
gh priority during an

process

Situations where a process needs to have a hi

initialization phase, but afterwords can run at a lower priority.

RRR RRR RR KR RRR KKK KKK KKK KER ERE RKEREEEAKKK KR ARE

\* \*
\* FUNCTION 145: SET PRIORITY \*
\* \*
HHH RRR IK RE RRR ERK KER RRR KR KEKE RRA KK KR REE
\* \*
\* Set the Priority of the Calling Process \*
\* \*
RRR KKK KERRIER KK RR KERIKERI KERR ERE KKK EKER KEK
\* \*

Entry Parameters:
Register CL: 091H
DL: Priority

\*
\*
\*
\*
\*
CX: Error Code \*
\*x
\*

\*

\*

\*

\*

\* Return Values:
\*

\*

\*

RHR RRR KR KKK KEK EKER KR KK KKEKRE RRA KR

priority of the calling

in

---
_Page 194_

MP/M-86 Programmer's Guide 6 System Calls : Function 146

He RK KKK RR KR RIK KKK RRR KR KERR KEKE KERR EK KEE REE

x \*
\* FUNCTION 146: ATTACH CONSOLE \*
\* \*
Ke KK KK RIK RK IKKE REIKI REE KK REE
\* \*
\* Attach default console to calling process \*
\* \*
KKK RK TK RR IKK IK RAIA KERR AER REE RE KEE
\* \*
\* Entry Parameters: \*
\* Register CL: 092H \*
\* \*
\* Return Values: \*
\* CX: Error Code \*
\* \*
POH III IK RIK RIKI IKK RRR RR ERK KKK ERR KK ERE RE KKK KK

The ATTACH CONSOLE function attaches the default console to
the calling process. If the console is already owned by the calling
process or if it is not owned by another process, the ATTACH CONSOLE
function will immediately return with ownership established and
verified. If another process owns the console, the calling process
waits until the console becomes available. When the console becomes
free through a DETACH CONSOLE call, the process that is waiting for
the console with the highest priority will obtain it. The ATTACH
CONSOLE function is called internally by all console I/O functions
except the RAW CONSOLE functions.

---
_Page 195_

MP/M-86 Programmer's Guide 6 System Calls : Function 147

KR KKK RRR RRR KR RRR KK KERR KK RRA KERR KK AKER EERE RR

\* \*
\* FUNCTION 147: DETACH CONSOLE \*
\* \*
BRK RARER RR KEK RK ERK KK KERR KEKKEKRKEEKKEK KKK KKK AKA
\* \*
\* Detach default console from calling process \*
\* \*
RRKERKKKKEKRKKEKRRKRRKEKKEKKKEKKEKKAKA KKK KKK KKK RRR RK
\* \*
\* Entry Parameters: \*
\* Register CL: 093H \*
\* \*
\* Return Values: \*
\* CX: Error Code \*
\* \*
RIK KKK IK RRR KKK RK ERE KK KR KKK KER RRA KK KARR RE

The DETACH CONSOLE function detaches the default console from
the calling process. If the default console is not attached to the
calling process, no action is taken. If other processes are waiting
to attach to the console, the process with the highest priority will
attach the console. If there are more than one Process with the
same priority waiting for the console, it is given on a first-come
first-serve basis.

---
_Page 196_

MP/M-86 Programmer's Guide 6 System Calls : Function 148

FR RII I KR RI ROT TORR ROR TR ROR RIK KR RR IKK RK IR KKK

\* \*
\* FUNCTION 148: SET CONSOLE \*
\* \*
FORK RI I RK IK RAI III KR IKK RIKER EKER KEKE ER ERK KKK
\* \*
\* Set the calling process's default console \*
\* \*
SOK RK KK I TKK KKK IIIA KKK E
\* \*

Entry Parameters:
Register CL: 094H
DL: Console Number

\* \*
k \*k
\* \*
x \*
\* Return Values: \*
\* AX: 0 if successful \*
\* OfffEH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\*& \*
\* \*

KK IK IK IK IKK KIRK KERIKERI EERE ERK KER E KEKE K

The SET CONSOLE function changes the calling process's
default console to the value specified. If the console number
specified is not one supported by this particular implementation of
MP/M-86, the function returns an Error Code, and does not change the
default console. If the console number is valid, the function
detaches the previous default console from the calling process. The
SET CONSOLE function then attaches the new console to the calling
process through the ATTACH CONSOLE function. If another process
already owns the new console, the calling process waits until the
console becomes available.

---
_Page 197_

MP/M-86 Programmer's Guide 6 System Calls : Function 149

HK KKK ERR KERR KR KK KERR A KER ERK AKER KEE KERR E RK REK

\* \*
\* FUNCTION 149: ASSIGN CONSOLE \*
\* x
HR RIK KR KK IKK RIKER IKK K KKK RIK KEKE KR ERITH ERE KERR EK
\* \*
\* Assign default console to another process \*
\* \*
RRR KEKE RE KEKE KEKE KK ER KERR KKEKREKREKEKRE KEKE REK
\* \*
\* Entry Parameters: \*
\* Register CL: 095H \*
\* DX: ACB Address - Offset \*
\* DS: ACB Address - Segment \*
k k
\* Return Values: \*
\* CX: Error Code \*
\* \*
RRR KKEKE KEK KEKE ERK EKER EKER EERE KERR REE KKK KE

+----- +----- +-~--- +----- +
00 | CNS |MATCH| PD |

+----- +----- +----- +----- t----- +----- +----- +--~--- +
04 | NAME |

+----- +----- +----- +----- +----- +----- +----- +----- +

Figure 6-16. Assign Control Block (ACB)

CNS Console to assign

MATCH Boolean, if OFFH, the process being assigned the console
must have the CNS as its default console for a successful

ASSIGN. IF OH, no check is made.

PD Process ID of the process being assigned the console. If
this field is zero, a search is made of the thread list
for a process whose name is NAME. This field must either
be zero or a valid Process ID. If this value is not a
valid PD, an error occurs.

NAME 8-byte process name to search for. An error occurs if a
process by this name does not exist.

The ASSIGN CONSOLE function directly assigns the specified
console to a specified process. This function overrides the normal
mechanism of the ATTACH and DETACH functions. The function returns
an Error Code if a process besides the calling process owns the
console. The function ignores other processes waiting to attach to
the specified console, and they will continue to wait until the

Current owner either calls the DETACH function or terminates,

---
_Page 198_

MP/M-86 Programmer's Guide 6 System Calls : Function 150

HK KKK KK KIKI KKK KK RE KEK KER EKER KEE KEK KEKKRKE RK KK KS

\* \*
\* FUNCTION 150: COMMAND LINE INTERPRETER x
\* x
KKK KKK KIKI KK RK KKK KKK KKK KIER ERK KK EEK KER KEK KEREREE
\* \*
\* Interpret and Execute Command Line \*
\* \*
Bea KKK KKK IK KIKI KK KIKI RK KEKE KERR KER KR KHER KEK EKKE
\* \*
\* Entry Parameters: \*
\* Register CL: 096H \*
\* DX: CLBUF Address - Offset \*
\* DS: CLBUF Address - Segment \*
\* \*
\* Return Values: \*
\* AX: 0 if successful \*
\* OfffEH on error \*
\* CX: Error Code \*
\* \*
KA KI KKK KK KKK ARK KEKE KE KER ERK RE KER ERK KEKE

The COMMAND LINE INTERPRETER function obtains an ASCII command
from the Command Line Buffer (CLBUF) and then executes it. If the
calling process is attached to its default console, the CLI function
assigns the console to either the newly created process, or to the
Resident System Process (RSP) that will act on the command. The
calling process must reattach to its default console before
accessing it.

The CLI function calls the PARSE FILENAME function to parse the
command line. If an error occurs in the PARSE FILENAME function,
the CLI function returns to the calling process with the Error Code
set to the same code that the PARSE FILENAME function returned.

If there is no disk specification, for the command, the CLI
function will try to open a System Queue with the same name as the
command. If the open operation is successful, and the queue is an
RSP-type queue, the CLI function looks for a process with the same
name and assigns the calling process's default console to the RSP.
The CLI function then writes the command tail to the RSP queue. If
the queue is full, the function returns an Error Code to the calling
process. If for any reason the RSP cannot be found, the CLI assumes
the command is on disk and continues.

The CLI function opens a file with the filename being the
command and the file type being CMD. If the command has an explicit
disk specification, and the OPEN FILE function fails, the CLI
function returns an Error Code to the calling process. If there is
no disk specification with the command, the CLI function attempts to
open the command file on the default system disk. If the OPEN FILE
function succeeds, the CLI function checks the file to verify the
SYSTEM attribute is on. If this second OPEN FILE function fails or
if the DIR attribute is on, the CLI function returns an Error Code

---
_Page 199_

MP/M-86 Programmer's Guide 6 System Calls : Function 150

to the calling process.

Once the CLI function succeeds in opening the command file, it
calls the PROGRAM LOAD function. The PROGRAM LOAD function finds,
and then loads the file into an appropriate memory space. If the
PROGRAM LOAD function encounters any errors, the CLI function
returns to the calling process with the Error Code set by the LOAD
function,

A successful load operation establishes the command file in
memory with its Base Page partially initialized. The CLI function
then continues parsing the command tail to set up the Base Page
values from 050h to OFFh.

The CLI function initializes an unused Process Descriptor from
the internal PD Table, a UDA and a 96-byte stack area. The UDA and
stack are dynamically allocated from memory. The CLI function then
calls the CREATE PROCESS function. If the CLI function encounters
an error in any of these steps, it releases all memory segments
allocated for the new command, as well as the Process Descriptor,
and then returns with the appropriate Error Code set.

Once the CREATE PROCESS function returns successfully, the CLI

function assigns the calling process' default console to the new
process and then returns.

---
_Page 200_

MP/M-86 Programmer's Guide 6 System Calls : Function 151

KKK KK IK KK KKK KKK KR KKK RRR KEK RE KKK RR KK KKEKRKKEEREKE

\* k
\* FUNCTION 151: CALL RPL \*
\* \*
KK IKK KIRKE KKK IKKE KER KERR EKER KERR EERE KKEKK KKK
\* k
\* Call a function in a \*
\* Resident Procedure Library \*
k \*
HK KIKI KK KKK KKK RRR ERE KKK REKREREKKEKEKEKKKEKRKER
\* \*

Entry Parameters:
Register CL: 097H
DX: CPB Address - Offset
DS: CPB Address - Segment

\* \*
\* \*
\* \*
\* \*
\* \*
\* Return Values: \*
\* AX: O1H if RPL not found \*
\* RPL return parameter \*
\* BX: same as AX \*
\* ES: RPL return segment if addr \*
\* CX: Error Code \*
\* \*
\* \*

KK KK KEK EKER KEKE KERR REE RKKK EERE KERR KKKERKEKREKK

$-----t-- t $---- tec -- t----- +----- +----- +
| NAME |
+----- $o---- +----- +----- +----- +--+-- +----- +----- +
| PARAM |
+----- +----- +

Figure 6-17. Call Parameter Block (CPB)

NAME Name of Resident Procedure, eight ASCII characters

PARAM Parameter to send to the Resident Procedure

The CALL RPL function permits a process to call a function in
an optional Resident Procedure Library (RPL). Resident Procedure
Libraries are optionally included in MP/M-86 by GENSYS at system
generation time.

The CALL RPL function opens a System Queue by the name
specified. If the OPEN QUEUE function succeeds, Function 151 checks
the queue to verify it is an RPL-type queue. If either the OPEN
QUEUE call fails or if it is not an RPL-type queue, Function 151
returns to the calling process with an Error Code. The CALL RPL
function reads a message from the queue that contains the address of
the specified function. It then places the PARAM field of the CPB
in register DX, and the calling processes Data Segment address in
register DS. The CALL RPL function does a Far Call to the address
it obtains from the queue message. Upon return from the RPL, the

---
_Page 201_

MP/M-86 Programmer's Guide 6 System Calls : Function 151

function copies the BX register to the AX register and then returns
to the calling process.

Note: The CALL RPL function does not write the address of the
Resident Procedure back to the queue. The Resident Procedure itself
must do this. If the Resident Procedure is to be reentrant, it must
write the message into the queue upon entry. If it is to be
serially reuseable, the procedure must write the message just before
returning.

---
_Page 202_

MP/M-86 Programmer's Guide 6 System Calls : Function 152

KHRKRKKKKKK KKK KEE RRR KEE KEKE KK RK KEKE KEKE KKKKKKKEKKEKKEE

\* \*
\* FUNCTION 152: PARSE FILENAME \*
\* \*
KKK KKK KEKE KKK KKK EKER KEKE KERR KERR EKKEKKKKKKKKEKKEEK
\* \*
\* Parse an ASCII string and initialize a FCB \*
\* \*
KKK KKK KEK KK KKK KEK KKK KKK KEKE KEKE KEERKKKEKKKKKKEK
\* \*

Entry Parameters:
Register CL: 098H
DX: PFCB Address ~- Offset
DS: PFCB Address - Segment

\* \*
\*& \*
\*k \*
\* \*
\* \*
\* Return Values: \*
\* AX: OFFFFH if error \*
\* 0 if next item to parse is \*
\* end of line \*
\* address of next item to \*
\* parse \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KKK KEKE KK KERR RR KERR KERR EREKEKKKKEKEKEE

+----- tooo +----- +----- +
| FILENAME | FCBADR |
+----- +----- +----- +----- +

Figure 6-18. Parse Filename Control Block (PFCB)

FILENAME Offset of an ASCII file specification to parse. The
offset is relative to the same Data Segment as the PFCB.

FCBADR Offset of a File Control Block to initialize. The offset
is relative to the same Data Segment as the PFCB

The PARSE FILENAME function parses an ASCII file specification
(FILENAME) and prepares a File Control Block (FCB). The calling
process passes the address of a data structure called the Parse
Filename Control Block, (PFCB) in register DX. The PFCB contains
the address of the ASCII filename string followed by the address of
the target FCB.

Function 152 assumes the file specification to be in the
following form:

{D:} {FILENAME} {.TYP}{;PASSWORD}

where those items enclosed in curly brackets are optional.

---
_Page 203_

MP/M-86 Programmer's Guide 6 System Calls : Function 152

The PARSE FILENAME function parses the first file specification
it finds in the input string. The function first eliminates leading
blanks and tabs. The function then assumes the file specification
ends on the first delimiter it hits that is out of context with the
specific field it is parsing. For instance, if it finds a colon (:)
and it is not the second character of the file specification, the
colon delimits the whole file specification. The function
recognizes the following characters as delimiters:

space

tab

return

null

; (semicolon) - except before password field

= (equal)

< (less than)

> (greater than)

- (dot) - except after filename and before type
: (colon) - except before filename and after drive
, (comma)

{ (left square bracket)

] (right square bracket)

/ (slant)

$ (dollar)

If the function reaches a non-graphic character (in the range 1
through 31), not listed above, it treats it as an error.

The Parse Filename function initializes the specified FCB as
follows:

byte 0 The drive field is set to the specified drive. If
the drive is not specified, the default value is
used. O=default, 1=A, 2=B, etc.

byte 1-8 The name is set to the specified file name. All
letters are converted to upper-case. If the name is
not eight characters long, the remaining bytes in
the filename field are padded with blanks. If the
filename has an asterick (\*), all remaining bytes in
the filename field are filled in with question marks
(?). The function returns an error if the filename
is more than eight bytes long.

byte 9-11 The type is set to the specified file type. If no
type is specified, the type field is initialized to
blanks. All letters are converted to upper-case.
If the type is not three characters long, the
remaining bytes in the file type field are padded
with blanks. If an asterick (\*) occurs, all
remaining bytes are filled in with question marks

---
_Page 204_

MP/M~86 Programmer's Guide 6 System Calls : Function 152

(?). The function returns an error if the type
field is more than 3 bytes long.

byte 12-15 Filled in with zeros

byte 16-23 The password field is set to the specified password.
If no password is specified, it is initialized to
blanks. If the password is not eight characters
long, remaining bytes are padded with blanks. All
letters are converted to upper-case. The function
returns an error if the password field is more than
eight bytes long.

byte 24-25 The offset of the beginning of the password in the
FILENAME string is placed here. If no password is
specified, this field is set to zero. Note that the
password indicated by this field is in the FILENAME
string which is not modified by the PARSE FILENAME
function. If there are any lower-case characters in
the password, they must be converted to upper-case
to ensure the password matches the password field of
the FCB.

byte 26 The number of characters in the specified password is
placed here. If no password is specified, this
field is set to zero.

If the function encounters an error, it sets all fields that
have not been parsed are set to their default values, and then
returns OFFFFh in register AX indicating the error.

On a successful parse, the PARSE FILENAME function checks the
next item in the FILENAME string. It skips over trailing blanks and
tabs and look at the next character. If the character is a null
(20H) or carriage return (OdH), it returns a 0 indicating the end of
the FILENAME string. If the next character is a delimiter, it
returns the address of the delimiter. If the next character is not
a delimiter, it returns the address of the delimiting blank or tab.

If the first non-blank or non-tab character in the FILENAME
string is a null or carriage return, the PARSE FILENAME function
returns a 0 indicating the end of string, and initializes the FCB to
its default values.

If the PARSE FILENAME function is to be used to parse a
subsequent filename in the FILENAME string, the returned address
should be advanced over the delimiter before placing it in the PFCB.

---
_Page 205_

MP/M-86 Programmer's Guide 6 System Calls : Function 153

RRR KKEKK EKER ERK KR KR RK ERR EKER KEKE KEKE EEK

\* \*
\* FUNCTION 153: GET CONSOLE \*
\* \*
REREKKEEKKEKKE EKER KEKE KEK KERR RE KEER KK EEKREKEEKKKKKKK
\* \*
\* Return the Calling Process' Default Console \*
\* \*
REKKKKKEKKEEKEEKKKEKKEKEKKEEKKEEKEKEEKEKK KR RAKE KKK KR EK
\* . \*

Entry Parameters:
Register CL: 099H

\* \*
\* \*
\* \*
\* Return Values: \*
\* AL: Console number \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* \*
\* \*

KEKE KEKE KEKE KK KEK KEKERKEEKEKKEREKKEKKKR KKK

The GET CONSOLE function returns the calling processes default
console.

---
_Page 206_

MP/M-86 Programmer's Guide 6 System Calls : Function 154

KK KK HIKER IKKE KERIKERI KEKE REE RK KKEKEEEER

\* \*
\* FUNCTION 154: GET SYSDAT ADDRESS \*
\* \*
KKK KKK KKK KKK KKK KKK AKER EEK IER EK EKER KEKKKKEKESK
\* x
\* Return the address of the System Data Area \*
\* \*
KKK KIKI KKK KERR REE KERR EEK KR EEK ERE KER KKK KEKE
\* x

Entry Parameters:
Register CL: O9AH

\* \*
\* \*
\* x
\* Return Values: \*
\* AX: SYSDAT Address - Offset \*
\* BX: Same as AX \*
\* ES: SYSDAT Address - Segment \*
\* \*
\* x

KH KKK KKK EEK KEKE KEK ERR RR KE ERK EKKEKKKKRKKEE

The GET SYSDAT function returns the address of the System Data
Area. The System Data Area contains all Process Descriptors, Queue
Descriptors, the roots of system lists and other internal data that
is used by MP/M-86. See the MP/M-86 System Guide for the format of
the System Data Area.

---
_Page 207_

MP/M-86 Programmer's Guide 6 System Calls : Function 155

DAY

HOUR

MIN

SEC

KKK KEK KKK ERK EK KEK KKK RE KERR KEK KEKE EERE

\* \*
\* FUNCTION 155: GET DATE AND TIME \*
\* \*
RK EKER EER KERR EEK EKER ERK KEE KEKE RE KEKE EKA KSK
\* \*
\* Get Current System Time and Day \*
\* \*
KREKKKK KEKE KR KEKE RRR EERE EKER EKER ER EEEKEK KEKE KE
\* \*
\* Entry Parameters: \*
\* Register CL: 09BH \*
\* DX: TOD Address - Offset \*
\* DS: TOD Address - Segment \*
\* \*
\* Return Values: \*
\* TOD filled in \*
\* \*
KREKKEKE KEKE KEKE EKER EERE KEKE KKK EK

+----~ +----- +----- +----- +----- +
| DAY |HOUR | MIN | SEC ]
+----- +----- +----- +----- +----- +

Figure 6-19. Time Of Day Structure (TOD)

The number of days since 1 January 1978. The day is
Stored as a 16-bit integer.

The current hour of the current day. The hour is
represented as a 24 hour clock in 2 binary coded decimal
(BCD) digits.

The current minute of the current hour. The minute is
stored as 2 BCD digits.

The current second of the current minute. The second is
stored as 2 BCD digits.

The GET DATE AND TIME function returns the current encoded date

and time in the TOD structure passed by the calling process.

---
_Page 208_

MP/M-86 Programmer's Guide 6 System Calls : Function 156

KKK KEK KKK IKKE KEKE REE RRR KK KER KERR KEKE KEKE KKKKKKEKKEE

\* \*
\* FUNCTION 156: Return PD Address \*
\* \*
KK KK KKK IK IKE KEKE KEKE EE IKKE EKER RKKKEKEKRKREKK KK
\* \*
\* Return the Address of the calling process's \*
\* Process Descriptor \*
\* \*
KKK KI IKK KKK KEK KEIR RK EE KE ARKEEKEKKKERREEKE RK KKEKEEK
\* \*

Entry Parameters:
Register CL: O9CH

k \*
\* \*
\* \*
\* Return Values: \*
\* AX: PD Address - Offset \*
\* BX: Same as AX \*
\* ES: PD Address - Segment \*
k \*x
x \*

KIKI KKK KEK KE KEKE KK EKER EERE KKK RRR KEE EKKKEEE

The RETURN PROCESS DESCRIPTOR ADDRESS function obtains the
address of the calling process' Process Descriptor. The format of
the Process descriptor is described in the CREATE PROCESS function
description.

---
_Page 209_

MP/M-86 Programmer's Guide 6 System Calls : Function 157

PD

TERM

CNS

HIRI RR RIK IK RRR IIR KERR KK IK ER RARER KEK KE RE KR KE

\* \*
\* FUNCTION 157: ABORT SPECIFIED PROCESS \*
\* \*
RRR KKK RRR RR KK KR KKK RIKER RR KKK KR KARE KEKE AKER RAK
\* \*
\* Terminate a Process by Name or PD Address \*
\* \*

RRR RRR ARERR KKK RRR EKER RR RAKE RK RRA E KER KE RK EAE
\*

Entry Parameters:
Register CL: O9DH
DX: APB Address - Offset
DS: APB Address - Segment

AL: Return Code
BL: Same as AL

\*
\*
\*
\*
\*
\*
\*
\*
\*
\* CX: Error Code
\*

\*k

\*k

\*

\*

\*

\*

Return Values: \*
\*

\*

\*

\*

\*

RRKK KKK KKK EERE KEKE KERR RK KERR RAKRAKREEKR AKA ARKE

+----- +----- +----- +----- +----- +----- +

| PD | TERM | CNS | 00 |

+----- t----- +----- +----- +---~-- +~----- +----- +----- +
| NAME |
+--~--- +----- +---=- t----- +----- +---~- +--~--- +----~ +

Fugure 6-20. Abort Parameter Block (APB)

Process Descriptor Offset of the Process to be terminated,
If this field is zero, a match is attempted with the NAME
and CNS fields to find the process. If this field is
non-zero, the NAME and CNS fields are ignored.

Termination Code. This field corresponds to the
Termination Code of Function 143. If the low-order byte
is OFFH, Function 143 can abort a specified system
process; otherwise a system process is not affected. A
System process is identified by the sys flag in the
Process Descriptor's FLAG field.

This field is reserved for future use and must be set to
zero.

Default console of Process to be aborted. If the PD field
is 0, the ABORT SPECIFIED PROCESS function scans the
Thread List for a PD with the same NAME and CNS fields as
Specified in the APB. Function 157 only aborts the first
process that it finds. Subs\*quent calls must be made to
abort all processes with the same NAME and CNS.

---
_Page 210_

MP/M-86 Programmer's Guide 6 System Calls : Function 157

NAME Name of the process to be aborted. As in the CNS field,
the NAME field is used to find the process to be aborted.
This is only used if the PD field is 0.

The ABORT SPECIFIED PROCESS function permits a process to
terminate another specified process. The calling process passes the
address of a data structure called an Abort Parameter Block,
initialized as described above.

If the Process Descriptor address is known, it can be filled
in and the process name and console can be omitted. Otherwise, the
Process Descriptor address field should be a 0 and the process name
and console must be specified. In either case, the calling process
must supply the termination code, which is the same parameter passed
to the TERMINATE PROCESS function.

---
_Page 211_

MP/M-86 Programmer's Guide 6 System Calls : Function 158

RAK KAA KER KKK RRR KR KEKE RRR ERR IKK RARER KK RAK

\* \*
\* FUNCTION 158: ATTACH LIST \*
\* \*
KKK KKK KIKI RRR RRR KKK KKK KAR RAKE EKER KKK EA KAKA
\* \*
\* Attach to the Calling Process's \*
\* Default List Device \*
\* \*
KAKA KKK KERR RK RAKE KR RR KEK KR ARKEKREK EKA ERR RR
\* \*
\* Entry Parameters: \*
\* Register CL: O9EH \*
\* \*
\* Return Values: \*
\* CX; Error Code \*
\* \*
HK RR ERK ERK KK RK K EKER ERK RRR EKER KEKE K KAR K KK RAE AE

The ATTACH LIST function attaches the default list device of
the calling process. If the list device is already attached to some
other process, the calling process relinquishes the CPU until the
other process detaches from the list device. When the list device
becomes free and the calling Process is the highest priority process
waiting for the list device, the attach operation takes place.

---
_Page 212_

cr

MP/M-86 Programmer's Guide 6 System Calls : Function 159

HOR RIT KOR TR KK RIT RR KK KKK RR KK KEK KERR KKK KKK

\* \*
\* PUNCTION 159: DETACH LIST \*
\* \*
OORT IR RTI ROO TORR RIO RKO IIR IR RRR KER KKK KK KKK
\* \*
\* Detach the Calling Process's \*
\* Default List Device \*
\* \*
DK A KK TOR TR RR IAT TTR RK RRR KKK KEE KERR RR KK RR RK
\* \*
\* Entry Parameters: \*
\* Register CL: O9FH \*
\* \*
\* Return Values: \*
\* CX: Error Code \*
\* \*
BRR KI KK IKK RK RR RK RRR RRR IRE KERR KR KEK KEK KKK

The DETACH LIST function detaches the default list device of
the calling process. If the list device is not currently attached,
no action takes place.

---
_Page 213_

MP/M-86 Programmer's Guide 6 System Calls : Function 160

KERR KKK KKK ERE KKK RRR KERR KRAERRKEKRKKEKKK AK KK KEKE KKK K

\* \*
\* FUNCTION 160: SET LIST \*
\* \*
RRR KKK KKK RK ERK KK RE KE KERR EKER KKK KKK KKK KEK
\* \*
\* Set the Calling Process's Default List Device \*
\* \*
KKK EKER KERR KEKE KER KKK KAKA KKK ERK KKK AKKK RK
\* \*
\* Entry Parameters: \*
\* Register CL: OAOQOH \*
\* DL: List Device \*
x \*
\* Return Values: \*
\* CX: Error Code \*
\* \*
KRKEKKKEKE KEK RK KEK KEKE KKK E KEKE RRR KEKE KAR KEKE KK

The SET LIST function detaches the list device Currently
attached to the calling process and then attaches the specified list
device. If the list device to be attached is already attached to
another process, the calling process relinquishes the CPU until the
other process detaches from the list device. When the list device
becomes free and the calling process is the highest priority process
waiting for the device, the attach operation takes place.

---
_Page 214_

MP/M-86 Programmer's Guide 6 System Calls : Function l6l

KK KK KK I KKK KKK IKK IK KK RRR KKK KEE KEKE KEKE KEKE

\* \*
\* PUNCTION 161: CONDITIONAL ATTACH LIST \*
\* \*
HK KKK KI KK RIK KKK KEKE KEK KKK KKK KER KER RK KKK KKK KEKE
\* \*
\* Conditionally Attach to the \*
\* Default List Device \*
\* \*
KKK KK IKK KK KKK KKK KKK IKKE KKK KK KKK KEKKEKE
\* \*

Entry Parameters:
Register CL: OAI1H

\* \*
\* \*
\* \*
\* Return Values: \*
\* AX: 0 if attach 'OK' x
\* OFFFFH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\*k \*
\* \*

KKK KKK IKK KEKE KEKE KKK ERE KKK ERE EKRKEKERKEKE

The CONDITIONAL ATTACH LIST function attachs the default list
device of the calling process only if the list device is currently
available.

If the list device is currently attached to another process,
the function returns a value of OFFH indicating that the list device
could not be attached. The function returns a value of 0 to
indicate that either the list device is already attached to the
process, or that it was unattached and a successful attach operation
was made.

---
_Page 215_

MP/M-86 Programmer's Guide 6 System Calls : Function 162

RRR RK KKK KERR KERR E KEK KK KKK KE ERK KKK ERK REE KERR RAK

\* \*
\* FUNCTION 162: CONDITIONAL ATTACH CONSOLE \*
\* \*
RRR KK KKK KERR KER KKK KEKE KKK KEK KKK EKER KAKA KKK KIER
\* \*
\* Conditionally Attach to the Default Console \*
\* \*
RRR KKK KKK ARK ERR KERR KR EK KKK RRR AKER RE KERRIER
\* \*

Entry Parameters:
Register CL: OA2H

\* \*
\* \*
\* \*
\* Return Values: \*
\* AX: 0 if attach 'OK' \*
\* OFFFFH on failure \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

RHKEKKKKHEKEKEKEKRKEK KEKE KKKKERRERKEKEKKKK KKK KKK KKK

The CONDITIONAL ATTACH CONSOLE function attaches the default
console of the calling process only if the console is currently
unattached,

If the console is currently attached to another process, the
function returns a value of OFFH indicating that the console could
not be attached. The function returns a value of 0 to indicate that
either the console is already attached to the process or that it was
unattached and a successful attach operation was made.

---
_Page 216_

MP/M-86 Programmer's Guide 6 System Calls : Function 163

KK RK KKK KKK KKK KKK KKK KEK KKK KEKE KKK RK KR KKEKKEERKEE

\* \*
\* FUNCTION 163: RETURN MP/M VERSION NUMBER \*
\* \*
KK KKK KKK KKK RK KK KK KKK KK IKKE KEK KE KKKKRKK KK KKKEEKEKK
\* \*
\* Return the version of current MP/M-86 system \*
\* \*
KKK KKK KKK KKK IK RR KIER EKER KK KKK KEKE KKK KEKKREKEE
\* \*

Entry Parameters:
Register CL: OA3H

\* \*
\* \*
\* \*
\* Return Values: \*
\* AX: Version Number (01120H) \*
\* BX: Same as AX \*
\* CX: Error Code \*
\* \*
\* \*

KK KK KI KKK RRR KKK KEKE REE RR KK RRR EKER KKK EEKK

The RETURN MP/M VERSION NUMBER function provides information
which allows version independent programming. The function returns
a two-byte value, with AH set to 011H for MP/M-86 and AL set to the
MP/M-86 verion level. A value of 01120H indicates MP/M-86 2.0.

---
_Page 217_

MP/M-86 Programmer's Guide 6 System Calls : Function 164

RRKKKE KER KER KEKE KEE KKK KKK KKK KKK KEKE RR KER REE EEKRE

\* \*
\* FUNCTION 164: GET LIST NUMBER \*
\* \*
KREKKK KEKE KEKE KEKE ERK EKER RRR KKK KK KKK KKK
\* \*
\* Return the Calling Process's \*
\* Default List Device \*
x \*
KEE KKK KKE KK KKK KKK KK KKK KERR KERRIER EKER RE EKEKEKAK
\* x

Entry Parameters:
Register CL: OA4H

k \*
\* \*
\* \*
\* Return Values: \*
\* AL: List Device Number \*
\* BL: Same as AL \*
\* CX: Error Code \*
\* \*
x \*

KRKEKKKKEEKKEKKEKRK EKER ERK EKER KEKEKRKEKEKKKKEKKKKKREK

The GET LIST NUMBER function returns the default list device
number of the calling process.

---
_Page 218_

---
_Page 219_

SECTION 7

INTRODUCTION TO ASM

7.1 Assembler Operation

ASM-86 processes an 8086 assembly language source file in three
passes and produces three output files, including an 8086 machine
language file in hexadecimal format. This object file may be in
either Intel or Digital Research hex format, which are described in
Appendix C. ASM-86 is shipped in two forms: an 8086 cross-
assembler designed to run under CP/M on an Intel 8080 or Zilog 2-80
based system, and a 8086 assembler designed to run under MP/M-86 on
an Intel 8086 or 8088 based system. ASM-86 typically produces three
output files from one input file as shown in Figure 7-1, below.

<file name>.A86 - contains source

<file name>.LST - contains listing

<file name>.H86 - contains assembled program in
hexadecimal format

<file name>.SYM - contains all user-defined symbols

Figure 7-1. ASM-86 Source and Object Files

Figure 7-1 also lists ASM-86 filename extensions. ASM-86
accepts a source file with any three letter extension, but if the
extension is omitted from the invoking command, it looks for the
Specified file name with the extension .A86 in the directory. If
the file has an extension other than .A86 or has no extension at
all, ASM-86 returns an error message.

The other extensions listed in Figure 7-1 identify ASM-86
output files. The .LST file contains the assembly language listing
with any error messages. The .H86 file contains the machine
language program in either Digital Research or Intel hexadecimal
format. The .SYM file lists any user-defined symbols.

---
_Page 220_

MP/M-86 Programmer's Guide 7.1 Assembler Operation

Invoke ASM-86 by entering a command of the following form:
ASM86 <source filename> [ $ <optional parameters> ]

Section 7.2 explains the optional parameters. Specify the source
file in the following form:

[<optional drive>:]<filename>[.<optional extension>]
where
<optional drive> is a valid drive letter specifying
the source file's location. Not
needed if source is on current

drive.

<filename> is a valid CP/M filename of 7 to 8
characters.

<optional extension> is a valid file extension of 1 to 3
characters, usually .A86.
Some examples of valid ASM-86 commands are:
A>ASM86 B:BI0S88
A>ASM86 BIOS88.A86 $FI AA HB PB SB

A>ASM86 D: TEST

Once invoked, ASM-86 responds with the message:
CP/M 8086 ASSEMBLER VER xX.xX

where x.x is the ASM-86 version number. ASM-86 then attempts to
open the source file. If the file does not exist on the designated
drive, or does not have the correct extension as described above,
the assembler displays the message:

NO FILE

If an invalid parameter is given in the optional parameter list,
ASM-86 displays the message:

PARAMETER ERROR

After opening the source, the assembler creates the output
files. Usually these are placed on the current disk drive, but they
may be redirected by optional parameters, or by a drive
specification in the the source file name. In the latter case, ASM-
86 directs the output files to the drive specified in the source
file name.

---
_Page 221_

MP/M-86 Programmer's Guide 7.1 Assembler Operation

During assembly, ASM-86 aborts if an error condition such as
disk full or symbol table overflow is detected. When ASM-86 detects
an error in the source file, it places an error message line in the
listing file in front of the line containing the error. Each error
message has a number and gives a brief explanation of the error.
Appendix H lists ASM-86 error messages. When the assembly is
complete, ASM-86 displays the message:

END OF ASSEMBLY. NUMBER OF ERRORS: n

7.2 Optional Run-time Parameters

The dollar-sign character, $, flags an optional string of run-
time parameters. A parameter is a single letter followed by a
single letter device name specification. The Parameters are shown
in Table 7-1, below.

Table 7-1. Run-time Parameter Summary

Parameter To Specify Valid Arguments
A source file device A, By, Cy, «ee P
H hex output file device A... P, X, Y, Z
P list file device A... P, X, Y, Z
S symbol file device A... P, X, Y¥, Z
F format of hex output file I, D

All parameters are optional, and can be entered in the command
line in any order. Enter the dollar sign only once at the beginning
of the parameter string. Spaces may separate parameters, but are
not required. No space is permitted, however, between a parameter
and its device name.

A device name must follow parameters A, H, P and S. The
devices are labeled:

A, B, C, ... P or xX, Y, Z
Device names A through P respectively specify disk drives A
through P. xX specifies the user console (CON:), Y specifies the
line printer (LST:), and Z suppresses output (NUL:).
If output is uirected to the console, it may be temporarily

Stopped at any time by typing a control-S. Restart the output by
typing a second control-S or any other character.

---
_Page 222_

MP/M-86 Programmer's Guide 7.2 Optional Run-time Parameters

The F parameter requires either an I or a D argument. When I
is specified, ASM-86 produces an object file in Intel hex format. A
D argument requests Digital Research hex format. Appendix C
discusses these formats in detail. If the F parameter is not
entered in the command line, ASM-86 produces Digital Research hex
format.

Table 7-2. Run-time Parameter Examples

Command Line Result

ASM86 IO Assemble file IO.A86, produce IO.HEX,
IO.LST and IO.SYM, all on the default
drive.

ASM86 IO.ASM $ AD SZ Assemble file IO.ASM on device D,
produce IO.LST and IO.HEX, no symbol
file.

ASM86 IO $ PY SX Assemble file I0.A86, produce IO.HEX,

route listing directly to printer,
output symbols on console.

ASM86 IO $ FD Produce Digital Research hex format.

ASM86 I0 S$ FI Produce Intel hex format.

7.3 Aborting ASM-86

You may abort ASM-86 execution at any time by hitting any key
on the console keyboard. When a key is pressed, ASM-86 responds
with the question:

USER BREAK. OK(Y/N) ?

A Y response aborts the assembly and returns to the operating
system. An N response continues the assembly.

---
_Page 223_

SECTION 8

ELEMENTS OF ASM-86 ASSEMBLY LANGUAGE

8.1 ASM-86 Character Set

ASM~-86 recognizes a subset of the ASCII character set. The
valid characters are the alphanumerics, special characters, and non-
printing characters shown below:

aN
hr
= 4
oO uU
QO
mx
un
tH
ca
<<
z=
x xX
mK
NN

A
a

How
wag
N wang
& DO FF
Ul Fh
ano a
“NO md
CO he Ht
wu.g

+

+ -

@$

I
~~
~~
_
ww
~

-

°

-
=

e

space, tab, carriage-return, and line-feed

Lower-case letters are treated as upper-case except within
Strings. Only alphanumerics, special characters, and spaces may
appear within a string.

8.2 Tokens and Separators

A token is the smallest meaningful unit of an ASM-86 source
program, much as a word is the smallest meaningful unit of an
English composition. Adjacent tokens are commonly separated by a
blank character or space. Any sequence of spaces may appear
wherever a single space is allowed. ASM-86 recognizes horizontal
tabs as separators and interprets them as spaces. Tabs are expanded
to spaces in the list file. The tab stops are at each eighth
column.

8.3 Delimiters

Delimiters mark the end of a token and add special meaning to
the instruction, as opposed to separators, which merely mark the end
of a token. When a delimiter is present, separators need not be
used. However, separators after delimiters can make your program
easier to read.

Table 8-1 describes ASM-86 separators and delimiters. Some

delimiters are also operators and are explained in greater detail in
Section 8.6.

---
_Page 224_

MP/M-86 Programmer's Guide

Character

20H

09H

CR

LF

me

Table 8-l.

Name
space

tab
carriage return

line feed

semicolon

colon

period

dollar sign

plus

minus

esterisk

siash

at-sign
underscore

exclamation
point

apostrophe

8.3 Delimiters

Separators and Delimiters

Use
separator

legal in source files,
expanded in list files

terminate source lines
legal after CR; if within
source lines, it is inter-
preted as a space

start comment field
identifies a label,

used in segment override

specification

forms variables from

numbers

notation for "present value
af Arn ttann nrintarll

UL LUCALCAVE rY HLL CL

arithmetic operator for
addition

arithmetic operator for
subtraction

arithmetic operator for
multiplication

arithmetic operator for
division

legal in identifiers
legal in identifiers
logically terminates a
statement, thus allowing
multiple statements on a

single source line

delimits string constants

---
_Page 225_

MP/M-86 Programmer's Guide 8.4 Constants

8.4 Constants

A constant is a value known at assembly time that does not
change while the assembled program is executed. A constant may be
either an integer or a character string.

8.4.1 Numeric Constants

A numeric constant is a 16-bit value in one of several bases.
The base, called the radix of the constant, is denoted by a trailing
radix indicator, The radix indicators are shown in Table 8-2,
below.

Table 8-2. Radix Indicators for Constants
Indicator Constant Type Base

binary

octal

octal

decimal 1
hexadecimal l

TVUOOW
NOWMLf

ASM-86 assumes that any numeric constant not terminated with a
radix indicator is a decimal constant. Radix indicators may be

upper or lower case.

A constant is thus a sequence of digits followed by an optional
radix indicator, where the digits are in the range for the radix.
Binary constants must be composed of 0's and 1's. Octal digits
range from 0 to 7; decimal digits range from 0 to 9. Hexadecimal
constants contain decimal digits as well as the hexadecimal digits A
(10D), B (11D), C (12D), D (13D), E (14D), and F (15D). Note that
the leading character‘of a hexadecimal constant must be either a
decimal digit so that ASM-86 cannot confuse a hex constant with an
identifier, or leading 0 to prevent this problem. The following are
valid numeric constants:

1234 1234D 1100B 1111000011110000B

1234H OFFEH 33770 137729
33770 OFE3H 1234d Offffh

---
_Page 226_

MP/M-86 Programmer's Guide 8.4 Constants

8.4.2 Character Strings

ASM-86 treats an ASCII character string delimited by
apostrophes as a string constant. All instructions accept only one-
or two-character constants as valid arguments. Instructions treat a
one-character string as an 8-bit number. A two-character string is
treated as a 16-bit number with the value of the second character in
the low-order byte, and the value of the first character in the
high-order byte.

The numeric value of a character is its ASCII code. ASM-86
does not translate case within character strings, so both upper-— and
lower-case letters can be used. Note that only alphanumerics,
special characters, and spaces are allowed within strings.

A DB assembler directive is the only ASM-86 statement that may
contain strings longer than two characters. The string may not
exceed 255 bytes. Include any apostrophe to be printed within the
string by entering it twice. ASM-86 interprets the two keystrokes
'' as a single apostrophe. Table 8-3 shows valid strings and how
they appear after processing:

Table 8-3. String Constant Examples

‘Tal -> a
'Ab''ca' -> Ab'cd
"I like CP/M' -> I like CP/M
perro ooy t
'ONLY UPPER CASE' -> ONLY UPPER CASE
‘only lower case' -> only lower case

8.5 Identifiers

Identifiers are character sequences which have a special,
symbolic meaning to the assembler. All identifiers in ASM-86 must
obey the following rules:

1. The first character must be alphabetic (A,...Z,
AypeeeZ).

2. Any subsequent characters can be either alphabetical
or a numeral (0,l,....-9). ASM-86 ignores the special
characters @ and , but they are still legal. For
example, a_b becomes ab.

3. Identifiers may be of any length up to the limit of
the physical line.

---
_Page 227_

MP/M-86 Programmer's Guide 8.5 Identifiers

Identifiers are of two types. The first are keywords, which
have predefined meanings to the assembler. The second are symbols,
which are defined by the user. The following are all valid
identifiers:

NOLIST

WORD

AH

Third street
How_are you today
variable@number@1234567890

8.5.1 Keywords

A keyword is an identifier that has a predefined meaning to the
assembler, Keywords are reserved; the user cannot define an
identifier identical to a keyword. For a complete list of keywords,
see Appendix D.

ASM-86 recognizes five types of keywords: instructions,
directives, operators, registers and predefined numbers. 8086
instruction mnemonic keywords and the actions they initiate are
defined in Section 10. Directives are discussed in Section 9.
Section 8.6 defines operators. Table 8-4 lists the ASM-86 keywords
that identify 8086 registers.

Three keywords are predefined numbers: BYTE, WORD, and DWORD.
The values of these numbers are 1, 2 and 4, respectively. In
addition, a Type attribute is associated with each of these numbers.
The keyword's Type attribute is equal to the keyword's numeric
value. See Section 8.5.2 for a complete discussion of Type
attributes,

---
_Page 228_

MP/M-86 Programmer's Guide 8.5 Identifiers

Table 8-4. Register Keywords

Register Numeric

Symbol Size Value Meaning
AH 1 byte 100 B Accumulator-High-Byte
BH 1 " 111 B Base-Register-High-Byte
CH 1 " 101 B Count-Register-High-Byte
DH 1 " 110 B Data-Register-High-Byte
AL i " 000 B Accumulator-Low-Byte
BL } " 011 B Base-Register-Low-Byte
CL 1 " 001 B Count-Register-Low-Byte
DL 1" 010 B Data-Register-Low-Byte
AX 2 bytes 000 B Accumulator (full word)
BX 2 " O11 B Base-Register "
CX 2 " 001 B Count-Register "
DX 2 " 010 B Data-Register "
BP 2 " 101 B Base Pointer
SP 2 " 100 B Stack Pointer
SI 2 " 110 B Source Index
DI 2 " 111 B Destination Index
cs 2 " 01 B Code-Segment-Register
DS 2 " 11 B Data-Segment-—Register
ss 2 " 10 B Stack-Segment-Register
ES 2 " 00 B Extra-Segment-—Register

8.5.2 Symbols and Their Attributes

A symbol is a user-defined identifier that has attributes which
specify what kind of information the symbol represents. Symbols
fall into three categories:

e variables
e labels
e numbers

Variables identify data stored at a particular location in
memory. All variables have the following three attributes:

---
_Page 229_

MP/M-86 Programmer's Guide 8.5 Identifiers

e Segment ~ tells which segment was being assembled when the
variable was defined,

e Offset - tells how many bytes there are between the
beginning of the segment and the location of this variable,

e Type - tells how many bytes of data are manipulated when
this variable is referenced.

A Segment may be a code-segment, a data-segment, a stack-
Segment or an extra-segment depending on its contents and the
register that contains its Starting address (see Section 9.2). A
segment may start at any address divisible by 16. ASM-86 uses this
boundary value as the Segment portion of the variable's definition.

The Offset of a variable may be any number between 0 and OFFFFH
or 65535D. A variable must have one of the following Type
attributes:

e BYTE
e WORD
@ DWORD

BYTE specifies a one-byte variable, WORD a two-byte variable
and DWORD a four-byte variable. The DB, DW, and DD directives
respectively define variables as these three types (see Section 9).
For example, a variable is defined when it appears as the name for a

storage directive:
VARIABLE DB 0

A variable may also be defined as the name for an EQU directive
referencing another label, as shown below:

VARIABLE EQU ANOTHER_VARIABLE
Labels identify locations in memory that contain instruction

Statements. They are referenced with jumps or calls. All labels
have two attributes:

@ Segment
e Offset

Label segment and offset attributes are essentially the same as
variable segment and offset attributes. Generally, a label is
defined when it precedes an instruction. A colon, :, Separates the
label from instruction; for example:

LABEL: ADD AX,BX

A label may also appear as the name for an EQU directive
referencing another label; for example:

LABEL EQU ANOTHER_LABEL

---
_Page 230_

MP/M-86 Programmer's Guide 8.5 Identifiers

Numbers may also be defined as symbols. A number symbol is
treated as if you had explicitly coded the number it represents.
For example:

Number five EQU 5
MOV AL,Number five

is equivalent to:
MOV AL,5

Section 8.6 describes operators and their effects on numbers
and number symbols.
8.6 Operators

ASM-86 operators fall into the following categories:
arithmetic, logical, and relational operators, segment override,
variable manipulators and creators. Table 8-5 defines ASM-86
operators. In this table, a and b represent two elements of the
expression. The validity column defines the type of operands the

operator can manipulate, using the or bar character, |, to separate
alternatives.

Table 8-5. ASM-86 Operators

Syntax Result Validity

Logical Operators

a XOR b bit-by-bit logical EXCLUSIVE a, b = number
OR of a and b.

a OR b bit-by-bit logical OR of a a, b = number
and b.

a AND b bit-by-bit logical AND of a a, b = number
and b.

NOT a logical inverse of a: all 0's a = 16-bit
become 1's, 1's become 0'S. number

---
_Page 231_

MP/M-86 Programmer's Guide

Syntax

a EQ b

a MOD b

a SHL b

a SHR b

Table 8-5. (continued)

Result
Relational Operators

returns OFFFFH if a= b,
otherwise 0.

returns OFFFFH if a < b,
otherwise 0.

returns OFFFFH if a <= b,
otherwise 0.

returns OFFFFH if a> b,
otherwise 0.

returns OFFFFH if a >= b
otherwise 0,

returns OFFFFH if a <> b,
otherwise 0.

Arithmetic Operators

arithmetic sum of a and b.

arithmetic difference of
a and b.

does unsigned multiplication
of a and b,

does unsigned division of a
and b.

returns remainder of a/b.
returns the value which
results from shifting a to
left by an amount b.
returns the value which
results from shifting a to
the right by an amount b.

gives a.

gives 0 - a.

8.6 Operators

Validity

a, b=
unsigned number

a, b=
unsigned number

a, b=
unsigned number

a, b=
unsigned number

a, b=
unsigned number

a, b=
unsigned number

a = variable,
label or number
b = number

a = variable,
label or number
b = number

number

a, b

a, b = number

a, b = number

a, b = number

a, b = number

number

mw
ul

number

97)
i

---
_Page 232_

MP/M-86 Programmer's Guide

Syntax

<seg reg>:
<addr exp>

SEG a

OFFSET a

TYPE a

LENGTH a

LAST a

a PTR b

Table 8-5. (continued)

Result
Segment Override

overrides assembler's choice
of segment register.

8.6 Operators

Validity

<seg reg> =
CS, DS, SS
or ES

Variable Manipulators, Creators

creates a number whose value
is the segment value of the
variable or label a.

creates a number whose value
is the offset value of the
variable or label a.

creates a number. If the
variable a is of type BYTE,
WORD or DWORD, the value of
the number will be 1, 2 or 4,
respectively.

creates a number whose value
is the LENGTH attribute of
the variable a. The length
attribute is the number of
bytes associated with the
variable,

if LENGTH a > 0, then LAST a
= LENGTH a - 1; if LENGTH a =
0, then LAST a = QO.

creates virtual variable or
label with type of a and
attributes of b

creates variable with an
offset attribute of 4,
Segment attribute is current
segment.

creates label with offset
equal to current value of
location counter; segment
attribute is current
segment.

a = label |
variable

a = label |
variable

a = label |
variable

a = label |
variable

a = label |
variable

a = BYTE |

WORD, | DWORD
b = <addr exp>

a = number

no argument

---
_Page 233_

MP/M-86 Programmer's Guide 8.6 Operators

8.6.1 Operator Examples

Logical operators accept only numbers as operands. They
perform the boolean logic operations AND, OR, XOR, and NOT. For

example:

OOFC MASK EQU OFCH

0080 SIGNBIT EQU 80H
0000 B180 MOV CL,MASK AND SIGNBIT
0002 BO03 MOV AL,NOT MASK

Relational operators treat all operands as unsigned numbers.
The relational operators are EQ (equal), LT (less than), LE (less
than or equal), GT (greater than), GE (greater than or equal), and
NE (not equal). Each operator compares two operands and returns all
ones (OFFFFH) if the specified relation is true and all zeros if it
is not. For example:

000A LIMIT1 EQU 10

0019 LIMIT2 EQU 25
0004 B8FFFF MOV AX,LIMIT1 LT LIMIT2
0007 B80000 MOV AX, LIMIT1L GT LIMIT2

Addition and subtraction operators compute the arithmetic sum
and difference of two operands. The first operand may be a
variable, label, or number, but the second operand must be a number.
When a number is added to a variable or label, the result is a
variable or label whose offset is the numeric value of the second
operand plus the offset of the first operand. Subtraction from a
variable or label returns a variable or label whose offset is that
of first operand decremented by the number specified in the second
operand. For example:

0002 COUNT EQU 2
0005 DISP1 EQU 5
OOOA FF FLAG DB OF FH
000B 2EA00B00 MOV AL, FLAG+1
OOOF 2E8A0EOFOO MOV CL,FLAG+DISP1
0014 B303 MOV BL,DISP1-COUNT

The multiplication and division operators \*, /, MOD, SHL, and
SHR accept only numbers as operands. \* and / treat all operators as
unsigned numbers. For example:

0016 BE5500 MOV S1,256/3
0019 B310 MOV BL,64/4
0050 BUFFERSIZE EQU 80
001B B8A000 MOV AX,BUFFERSIZE \* 2

---
_Page 234_

MP/M-86 Programmer's Guide 8.6 Operators

Unary operators accept both signed and unsigned operators as
shown below:

OO1E B123 MOV CL,+35
0020 BOO? MOV AL,2--5
0022 B2F4 MOV DL,-12

When manipulating variables, the assembler decides which
segment register to use. You may override the assembler's choice by
specifying a different register with the segment override operator.
The syntax for the override operator is <segment register>
<address expression> where the <segment register> is CS, DS, SS, or
ES. For example:

0024 368B472D MOV AX,SS:WORDBUFFER[BX]
0028 268B0E5B00 MOV CX,ES: ARRAY

A variable manipulator creates a number equal to one attribute
of its variable operand. SEG extracts the variable's segment value,
OFFSET its offset value, TYPE its type value (1, 2, or 4), and
LENGTH the number of bytes associated with the variable. LAST
compares the variable's LENGTH with 0 and if greater, then
decrements LENGTH by one. If LENGTH equals 0, LAST leaves it
unchanged. Variable manipulators accept only variables as
operators. For example:

002D OOO000UD0000U WORDBUFEER DW G,0,0

0033 0102030405 BUFFER DB 1,2,3,4,5
0038 B80500 MOV AX, LENGTH BUFFER
003B B80400 MOV AX,LAST BUFFER
003E B80100 MOV AX, TYPE BUFFER
0041 B80200 MOV AX,TYPE WORDBUFFER

The PTR operator creates a virtual variable or label, one valid
only during the execution of the instruction. It makes no changes
to either of its operands. The temporary symbol has the same Type
attribute as the left operator, and all other attributes of the
right operator as shown below.

0044 C60705 MOV BYTE PTR [BX], 5
0047 8A07 MOV AL,BYTE PTR [BX]
0049 FFO4 INC WORD PTR [SI]

The Period operator, ., creates a variable in the current data
segment. The new variable has a segment attribute equal to the
current data segment and an offset attribute equal to its operand.
Its operand must be a number. For example:

004B Al0000 MOV AX, .0
004E 268B1E0040 MOV BX, ES: .4000H

---
_Page 235_

MP/M-86 Programmer's Guide 8.6 Operators

The Dollar-sign operator, $, creates a label with an offset
attribute equal to the current value of the location counter. The
label's segment value is the same as the current code Segment. This
operator takes no operand. For example:

0053 EQSFDFF JMP $
0056 EBFE JMPS $
0058 E9FD2F JMP $+3000H

8.6.2 Operator Precedence

Expressions combine variables, labels or numbers with
operators. ASM-86 allows several kinds of expressions which are
discussed in Section 8.7. This section defines the order in which
operations are executed should more than one operator appear in an
expression,

In general, ASM-86 evaluates expressions left to right, but
operators with higher precedence are evaluated before operators with
lower precedence. When two operators have equal precedence, the
left-most is evaluated first. Table 8-6 presents ASM-86 operators
in order of increasing precedence.

Parentheses can override normal rules of precedence. The part
of an expression enclosed in parentheses is evaluated first. If
parentheses are nested, the innermost expressions are evaluated
first. Only five levels of nested parentheses are legal. For
example:

15/3 + 18/9

= 5 +
15/(3 + 18/9) = 1

22:3

---
_Page 236_

MP/M-86 Programmer's Guide 8.6 Operators

Table 8-6. Precedence of Operations in ASM-86

Order Operator Type Operators
1 Logical XOR, OR
2 Logical AND
3 Logical NOT
4 Relational EQ, LT, LE, GT,
GE, NE
5 Addition/subtraction +, -
6 Multiplication/division \*, /, MOD, SHL,
SHR
7 Unary +, -
8 Segment override <segment override>:
9 Variable manipulators, SEG, OFFSET, PTR,
creators TYPE, LENGTH, LAST
10 Parentheses/brackets (), C ]
ll Period and Dollar -7 $

8.7 Expressions

ASM-86 allows address, numeric, and bracketed expressions. An
address expression evaluates to a memory address and has three
components:

e A segment value
e An offset value
e A type

Both variables and labels are address expressions. An address
expression is not a number, but its components are. Numbers may be
combined with operators such as PTR to make an address expression.

A numeric expression evaluates to a number. It does not
contain any variables or labels, only numbers and operands.

Bracketed expressions specify base- and index- addressing
modes. The base registers are BX and BP, and the index registers
are DI and SI. A bracketed expression may consist of a base
register, an index register, or a base register and an index
register.

---
_Page 237_

MP/M-86 Programmer's Guide 8.7 Expressions

Use the + operator between a base register and an index register to
specify both base- and index-register addressing. For example:

MOV variable[bx],0
MOV AX, [BX+DI]
MOV AX, [SI]

8.8 Statements

Just as "tokens" in this assembly language correspond to words
in English, so are statements analogous to sentences. A statement
tells ASM-86 what action to perform. Statements are of two types:
instructions and directives. Instructions are translated by the
assembler into 8086 machine language instructions. Directives are
not translated into machine code but instead direct the assembler to
perform certain clerical functions.

Terminate each assembly language statement with a carriage
return (CR) and line feed (LF), or with an exclamation point, !,
which ASM-86 treats as an end-of-line. Multiple assembly language
statements can be written on the same physical line if separated by
exclamation points.

The ASM-86 instruction set is defined in Section 10. The
syntax for an instruction statement is:

[label:] [prefix] mnemonic [ operand(s) ] [;comment]

where the fields are defined as:

label:
A symbol followed by ":" defines a label at the current
value of the location counter in the current segment.
This field is optional.
prefix
Certain machine instructions such as LOCK and REP may
prefix other instructions. This field is optional.
mnemonic

A symbol defined as a machine instruction, either by the
assembler or by an EQU directive. This field is optional
unless preceded by a prefix instruction. If it is
omitted, no operands may be present, although the other
fields may appear. ASM-86 mnemonics are defined in
Section 10.

---
_Page 238_

MP/M-86 Programmer's Guide 8.8 Statements

operand(s)
An instruction mnemonic may require other symbols to
represent operands to the instruction. Instructions may
have zero, one or two operands.

comment
Any semicolon (;) appearing outside a character string
begins a comment, which is ended by a carriage return.

Comments improve the readability of programs. This field
is optional.

ASM-86 directives are described in Section 9. The syntax for a
directive statement is:
[name] directive operand(s) [;comment]

where the fields are defined as:

name
Unlike the label field of an instruction, the name field
of a directive is never terminated with a colon.
Directive names are legal for only DB, DW, DD, RS and
RQU. For DB, DW, DD and RS the name is optional; for EQU
it is required.

directive
One of the directive keywords defined in Section 9.

operand(s)
Analogous to the operands to the instruction mnemonics.
Some directives, such as DB, DW, and DD, allow any
operand while others have special requirements.

comment

Exactly as defined for instruction statements.

---
_Page 239_

SECTION 9

ASSEMBLER DIRECTIVES

9.1 Introduction

Directive statements cause ASM-86 to perform housekeeping
functions such as assigning portions of code to logical segments,
requesting conditional assembly, defining data items, and specifying
listing file format. General syntax for directive statements
appears in Section 8.8.

In the sections that follow, the specific syntax for each
directive statement is given under the heading and before the
explanation. These syntax lines use special symbols to represent
possible arguments and other alternatives. Square brackets, [],
enclose optional arguments. Angle brackets, <>, enclose
descriptions of user-supplied arguments. Do not include these
symbols when coding a directive.

9.2 Segment Start Directives

At run-time, every 8086 memory reference must have a 16-bit
segment base value and a 16-bit offset value. These are combined to
produce the 20-bit effective address needed by the CPU to physically
address the location. The 16-bit segment base value or boundary is
contained in one of the segment registers CS, DS, SS, or ES. The
offset value gives the offset of the memory reference from the
segment boundary. A 16-byte physical segment is the smallest
relocatable unit of memory.

ASM-86 predefines four logical segments: the Code Segment, Data
Segment, Stack Segment, and Extra Segment, which are respectively
addressed by the CS, DS, SS, and ES registers. Future versions of
ASM-86 will support additional segments such as multiple data or
code segments. All ASM-86 statements must be assigned to one of the
four currently supported segments so that they can be referenced by
the CPU. A segment directive statement, CSEG, DSEG, SSEG, or ESEG,
specifies that the statements following it belong to a specific
segment. The statements are then addressed by the corresponding
segment register. ASM-86 assigns statements to the specified
Segment until it encounters another segment directive.

Instruction statements must be assigned to the Code Segment.
Directive statements may be assigned to any segment. ASM-86 uses
these assignments to change from one segment register to another.
For example, when an instruction accesses a memory variable, ASM-86
must know which segment contains the variable so it can generate a
segment override prefix byte if necessary.

---
_Page 240_

MP/M-86 Programmer's Guide 9.2 Segment Start Directives

9.2.1 The CSEG Directive

CSEG <numeric expression>
CSEG
CSEG $

This directive tells the assembler that the following
statements belong in the Code Segment. All instruction statements
must be assigned to the Code Segment. All directive statements are
legal within the Code Segment.

Use the first form when the location of the segment is known at
assembly time; the code generated is not relocatable. Use the
second form when the segment location is not known at assembly time;
the code generated is relocatable. Use the third form to continue
the Code Segment after it has been interrupted by a DSEG, SSEG, or
ESEG directive. The continuing Code Segment starts with the same
attributes, such as location and instruction pointer, as the
previous Code Segment.

9.2.2 The DSEG Directive

DSEG <numeric expression>
DSEG
DSEG $

This directive specifies that the following statements belong
to the Data Segment. The Data Segment primarily contains the data
allocation directives DB, DW, DD and RS, but all other directive
statements are also legal. Instruction statements are illegal in
the Data Segment.

Use the first form when the location of the segment is known at
assembly time; the code generated is not relocatable. Use the
second form when the segment location is not known at assembly time;
the code generated is relocatable. Use the third form to continue
the Data Segment after it has been interrupted by a CSEG, SSEG, or
ESEG directive. The continuing Data Segment starts with the same
attributes as the previous Data Segment.

9.2.3 The SSEG Directive

SSEG <numeric expression>
SSEG
SSEG $

The SSEG directive indicates the beginning of source lines for
the Stack Segment. Use the Stack Segment for all stack operations.
All directive statements are legal in the Stack Segment, but
instruction statements are illegal.

---
_Page 241_

MP/M-86 Programmer's Guide 9.2 Segment Start Directives

Use the first form when the location of the segment is known at
assembly time; the code generated is not relocatable. Use the
second form when the segment location is not known at assembly time;
the code generated is relocatable. Use the third form to continue
the Stack Segment after it has been interrupted by a CSEG, DSEG, or
ESEG directive. The continuing Stack Segment starts with the same
attributes as the previous Stack Segment.

9.2.4 The ESEG Directive

ESEG <numeric expression>
ESEG
ESEG $

This directive initiates the Extra Segment. Instruction
Statements are not legal in this segment, but all directive
Statements are.

Use the first form when the location of the segment is known at
assembly time; the code generated is not relocatable. Use the
second form when the segment location is not known at assembly time;
the code generated is relocatable. Use the third form to continue
the Extra Segment after it has been interrupted by a DSEG, SSEG, or
CSEG directive. The continuing Extra Segment starts with the same
attributes as the previous Extra Segment.

9.3 The ORG Directive
ORG <numeric expression>

The ORG directive sets the offset of the location counter in
the current segment to the value specified in the numeric
expression. Define all elements of the expression before the ORG
directive because forward references may be ambiguous.

In most segments, an ORG directive is unnecessary. If no ORG
is included before the first instruction or data byte in a segment,
assembly begins at location zero relative to the beginning of the
segment. A segment can have any number of ORG directives.

---
_Page 242_

MP/M-86 Programmer's Guide 9,4 The IF and ENDIF Directives

9.4 The IF and ENDIF Directives

IF <numeric expression>
< source line l >
< source line 2 >

< source line n >
ENDIF

The IF and ENDIF directives allow a group of source lines to be
included or excluded from the assembly. Use conditional directives
to assemble several different versions of a single source program.

When the assembler finds an IF directive, it evaluates the
numeric expression following the IF keyword. Tf the expression
evaluates to a non-zero value, then <source line 1> through <source
line n> are assembled. If the expression evaluates to zero, then
all lines are listed but not assembled. All elements in the numeric
expression must be defined before they appear in the IF directive.
Nested IF directives are not legal.

9.5 The INCLUDE Directive

INCLUDE <file name>

This directive includes another ASM-86 file in the source text.
For example:

INCLUDE EQUALS.A86

Use INCLUDE when the source program resides in several
different files. INCLUDE directives may not be nested; a source
file called by an INCLUDE directive may not contain another INCLUDE
statement. If <file name> does not contain a file type, the file
type is assumed to be .A86. If no drive name is specified with <file
name>, ASM-86 assumes the drive containing the source file.

9.6 The END Directive
END
An END directive marks the end of a source file. Any
subsequent lines are ignored by the assembler. END is optional. If

not present, ASM-86 processes the source until it finds an End-Of-
File character (1AH).

---
_Page 243_

MP/M-86 Programmer's Guide 9.7 The EQU Directive

9.7 The EQU Directive

symbol EQU <numeric expression>
symbol EQU <address expression>
symbol EQU <register>

symbol EQU <instruction mnemonic>

The EQU (equate) directive assigns values and attributes to
user-defined symbols. The required symbol name may not be
terminated with a colon. The symbol cannot be redefined by a
subsequent EQU or another directive. Any elements used in numeric
or address expressions must be defined before the EQU directive
appears.

The first form assigns a numeric value to the symbol, the
second a memory address. The third form assigns a new name to an
8086 register. The fourth form defines a new instruction (sub) set.
The following are examples of these four forms:

0005 FIVE EQU 2\*2+1
0033 NEXT EQU BUFFER
0001 COUNTER EQU CX
MOVVV EQU MOV
005D 8BC3 MOVVV AX, BX

9.8 The DB Directive

[symbol] DB <numeric expression>[,<numeric expression>..]
[symbol] DB <string constant>[,<string constant>...]

The DB directive defines initialized Storage areas in byte
format. Numeric expressions are evaluated to 8-bit values and
sequentially placed in the hex output file. String constants are
placed in the output file according to the rules defined in Section
8.4.2. A DB directive is the only ASM-86 statement that accepts a
string constant longer than two bytes. There is no translation from
lower to upper case within strings. Multiple expressions or
constants, separated by commas, may be added to the definition, but
May not exceed the physical line length.

Use an optional symbol to reference the defined data area
throughout the program. The symbol has four attributes: the
Segment and Offset attributes determine the symbol's memory
reference, the Type attribute specifies Single bytes, and Length
tells the number of bytes (allocation units) reserved.

---
_Page 244_

MP/M-86 Programmer's Guide 9.8 The DB Directive

The following statements show DB directives with symbols:

OO5F 43502F4D2073 TEXT DB "CP/M system',0
797374656D00

O06B El AA DB "a" + 80H

006C 0102030405 Xx DB 1,2,3,4,5

0071 B9DC00 MOV CX, LENGTH TEXT

9.9 The DW Directive

[symbol] DW <numeric expression>[,<numeric expression>..]
[symbol] DW <string constant>[,<string constant>...]

The DW directive initializes two-byte words of storage. String
constants longer than two characters are illegal. Otherwise, DW
uses the same procedure to initialize storage as DB. The following
are examples of DW statements:

0074 0000 CNTR DW 0

0076 63C166C169C1 JMPTAB DW SUBR1,SUBR2,SUBR3

007C 010002000300 DW 1,2,3,4,5,6
040005000600

9.10 The DD Directive
[symbol] DD <numeric expression>[,<numeric expression>..]

The DD directive initializes four bytes of storage. The Offset
attribute of the address expression is stored in the two lower
bytes, the Segment attribute in the two upper bytes. Otherwise, DD
follows the same procedure as DB. For example:

1234 CSEG 1234H
0000 6CC134126FC1 LONG JMPTAB | DD ROUT1,ROUT2
0008 72C1341275C1 DD ROUT3,ROUT4

---
_Page 245_

MP/M-86 Programmer's Guide 9.11 The RS Directive

9.11 The RS Directive
[symbol] RS <numeric expression>

The RS directive allocates storage in memory but does not
initialize it. The numeric expression gives the number of bytes to
be reserved. An RS statement does not give a byte attribute to the
optional symbol. For example:

0010 BUF RS 80
0060 RS 4000H
4060 RS 1

9.12 The RB Directive
[symbol] RB <numeric expression>
The RB directive allocates byte storage in memory without any
initialization. This directive is identical to the RS directive
except that it does give the byte attribute.
9.13 The RW Directive
[symbol] RW <numeric expression>
The RW directive allocates two-byte word storage in memory but

does not initialize it. The numeric expression gives the number of
words to be reserved. For example:

4061 BUFF RW 128
4161 RW 4000H
Clol RW 1

9.14 The TITLE Directive
TITLE <string constant>
ASM-86 prints the string constant defined by a TITLE directive
Statement at the top of each printout page in the listing file. The
title character string should not exceed 30 characters. For
example:

TITLE 'CP/M monitor'

9.15 The PAGESIZE Directive
PAGESIZE <numeric expression>

The PAGESIZE directive defines the number of lines to be
included on each printout page. The default pagesize is 66.

---
_Page 246_

MP/M-86 Programmer's Guide 9.16 The PAGEWIDTH Directive

9.16 The PAGEWIDTH Directive

PAGEWIDTH <numeric expression>

The PAGEWIDTH directive defines the number of columns printed

across the page when the listing file is output. The default
pagewidth is 120 unless the listing is routed directly to the
terminal; then the default pagewidth is 79.
9.17 The EJECT Directive

EJECT

The EJECT directive performs a page eject during printout. The
EJECT directive itself is printed on the first line of the next

page.
9.18 The SIMFORM Directive
SIMFORM

The SIMFORM directive replaces a form-feed (FF) character in
the print file with the correct number of line-feeds (LF). Use this
directive when printing out on a printer unable to interpret the
form-feed character.
9.19 The NOLIST and LIST Directives

NOLIST
LIST

The NOLIST directive blocks the printout of the following
lines. Restart the listing with a LIST directive.

---
_Page 247_

SECTION 10

THE ASM-86 INSTRUCTION SET

10.1 Introduction

The ASM-86 instruction set includes all 8086 machine
instructions. The general syntax for instruction statements is
given in Section 8.7. The following sections define the specific
syntax and required operand types for each instruction, without
reference to labels or comments. The instruction definitions are
presented in tables for easy reference. For a more detailed
description of each instruction, see Intel's MCS-86 Assembly
Language Reference Manual. For descriptions of the instruction bit
patterns and operations, see Intel's MCS-86 User's Manual.

The instruction-definition tables present ASM-86 instruction
statements as combinations of mnemonics and operands. A mnemonic is
a symbolic representation for an instruction, and its operands are
its required parameters. Instructions can take zero, one or two
operands. When two operands are specified, the left operand is the
instruction's destination operand, and the two operands are
separated by a comma.

The instruction-definition tables organize ASM-86 instructions
into functional groups. Within each table, the instructions are
listed alphabetically. Table 10-1 shows the symbols used in the
instruction-definition tables to define operand types.

Table 10-1. Operand Type Symbols

Symbol Operand Type
numb any NUMERIC expression
numb8 any NUMERIC expression which
evaluates to an 8-bit number
acc accumulator register, AX or AL
reg any general purpose register,

not segment register

regl6 a 16-bit general purpose register,
not segment register

segreg any segment register: CS, DS, SS,
or ES

---
_Page 248_

MP/M-86 Programmer's Guide 10.1 Introduction

Table 10-1. (continued)
Symbol Operand Type

mem any ADDRESS expression, with or
without base- and/or index-
addressing modes, such as:

variable
variablet+3
variable[bx]
variable[STI]
variable[BXx+SI]
[BX]

[BP+DI]

simpmem any ADDRESS expression WITHOUT base-
and index- addressing modes, such as:

variable
variablet+4

mem|reg any expression symbolized by "reg"
or “mem"

mem|regl6 an
Ww,
rt

y expression symbolized by
' iem | € bi

i bit t+ be 15

i) v MUL AWUOe VO LU

label any ADDRESS expression which
evaluates to a label

lab8 any “label" which is within +/- 128
bytes distance from the instruction

The 8086 CPU has nine single-bit Flag registers which reflect
the state of the CPU. The user cannot access these registers
directly, but can test them to determine the effects of an executed
instruction upon an operand or register. The effects of
instructions on Flag registers are also described in the
instruction-definition tables, using the symbols shown in Table 10-2
to represent the nine Flag registers.

---
_Page 249_

MP/M-86 Programmer's Guide

Table 10-2.

10.1 Introduction

Flag Register Symbols

Auxiliary-Carry-Flag
Carry-Flag
Direction-Flag
Interrupt-Enable-Flag
Overflow-Flag
Parity-Flag

Sign-Flag

Trap-Flag

Zero-Flag

10.2 Data Transfer Instructions

There are four classes of data transfer operations: general
purpose, accumulator specific, address-object and flag. Only SAHF
and POPF affect flag settings. Note in Table 10-3 that if acc = AL,
a byte is transferred, but if acc = AX, a word is transferred.

IN

IN

LAHF

LDS

LEA

LES

MOV

MOV

Table 10-3.
Syntax

acc,numb8 | numb16

acc ,DX

regl6,mem

regl6,mem

regl6,mem

reg,mem|reg

mem|reg,reg

Data Transfer Instructions
Result

transfer data from input port given
by numb8 or numb1l6 (0-255) to
accumulator

transfer data from input port given
by DX register (O0-OFFFFH) to
accumulator

transfer flags to the AH register

transfer the segment part of the
memory address (DWORD variable) to
the DS segment register, transfer
the offset part to a general
purpose 16-bit register

transfer the offset of the memory
address to a (16-bit) register

transfer the segment part of the
memory address to the ES segment
register, transfer the offset part
to a 16-bit general purpose register
move memory or register to register

move register to memory or register

---
_Page 250_

MP/M-86 Programmer's Guide

MOV

MOV

MOV

OUT

OUT

POP

POP

POPF

PUSH

PUSH

PUSHF

SAHF

XCHG

XCHG

XLAT

Table 10-3.

Syntax

mem|reg,numb

segreg,mem|regl6

mem|regl6,segreg

numb8 |numbl6,acc

DX,acc

mem|regl6

segreg

mem|regl6

segreg

reg,mem|reg

mem|reg,reg

mem|reg

4.2 Data Transfer Instructions

(continued)
Result

move immediate data to memory or
register

move memory or register to segment
register

move segment register to memory or
register

transfer data from accumulator

to output port (0-255) given by
numb8 or numbl6

transfer data from accumulator to
output port (O-OFFFFH) given by Dx
register

move top stack element to memory or
register

move top stack element to segment
register; note that CS segment
register not allowed

transfer top stack element to flags

move memory or register to top
stack element

move segment register to top stack
element

transfer flags to top stack element
transfer the AH register to flags

exchange
register

register and memory or

exchange memory or register and
register

perform table lookup translation,
table given by "mem|reg", which is
always BX. Replaces AL with AL
offset from BX.

---
_Page 251_

MP/M-86 Programmer's Guide 10.3 Arithmetic, Logic, and Shift

10.3 Arithmetic, Logical, and Shift Instructions

The 8086 CPU performs the four basic mathematical operations in
several different ways. It supports both 8- and 16-bit operations
and also signed and unsigned arithmetic.

Six of the nine flag bits are set or cleared by most arithmetic
operations to reflect the result of the operation. fTable 10-4
summarizes the effects of arithmetic instructions on flag bits.
Table 10-5 defines arithmetic instructions and Table 10-6 logical
and shift instructions.

Table 10-4. Effects of Arithmetic Instructions on Flags

CF is set if the operation resulted in a carry out of
(from addition) or a borrow into (from subtraction)
the high-order bit of the result; otherwise CF is
cleared.

AF is set if the operation resulted in a carry out of
(from addition) or a borrow into (from subtraction)
the low-order four bits of the result; otherwise AF
is cleared.

ZF is set if the result of the operation is Zero;
otherwise ZF is cleared.

SF is set if the result is negative.

PF is set if the modulo 2 sum of the low-order eight
bits of the result of the operation is 0 (even
parity); otherwise PF is cleared (odd parity).

OF is set if the operation resulted in an overflow; the

size of the result exceeded the capacity of its
destination.

---
_Page 252_

MP/M-86 Programmer's Guide

AAA

AAD

AAM

AAS

ADC

ADC

ADC

ADD

ADD

ADD

CBW

CWD

CMP

CMP

CMP

DAA

DAS

DEC

Table 10-5.

Syntax

reg,mem|reg

mem|reg,reg

mem|reg,numb

reg,mem|reg

mem|reg,reg

mem|reg,numb

reg,mem|reg

mem|reg,reg

mem|reg,numb

mem|reg

10.3 Arithmetic, Logic, and Shift

Arithmetic Instructions
Result

adjust unpacked BCD (ASCII) for
addition - adjusts AL

adjust unpacked BCD (ASCII) for
division - adjusts AL

adjust unpacked BCD (ASCII) for
multiplication - adjusts AX

adjust unpacked BCD (ASCII) for
subtraction - adjusts AL

add (with carry) memory or
register to register

add (with carry) register to memory
or register

add (with carry) immediate data to
memory or register

add memory or register to register
add register to memory or register

add immediate data to memory or
register

convert byte in AL to word in AH by
sign extension

convert word in AX to double word
in DX/AX by sign extension

compare register with memory or
register

compare memory or register with
register

compare data constant with memory
or register

decimal adjust for addition,
adjusts AL

decimal adjust for subtraction,
adjusts AL

subtract 1 from memory or register

---
_Page 253_

MP/M-86 Programmer's Guide

INC

DIV

IDIV

IMUL

MUL

NEG

SBB

SBB

SBB

SUB

SUB

SUB

10.3 Arithmetic, Logic, and Shift

Table 10-5. (continued)

Syntax
mem|reg

mem| reg

mem|reg

mem| reg

mem| reg

mem|reg

reg,mem|reg

mem|reg,reg

mem|reg,numb

reg,mem| reg

mem|reg,reg

mem|reg,numb

Result
add 1 to memory or register

divide (unsigned) accumulator (AX
or AL) by memory or register.

If byte results, AL = quotient, AH
= remainder. If word results, AX =
quotient, DX = remainder

divide (signed) accumulator (AX or
AL) by memory or register -
quotient and remainder stored as in
DIV

multiply (signed) memory or
register by accumulator (AX or
AL) - if byte, results in AH, AL.
If word, results in DX, AX

multiply (unsigned) memory or
register by accumulator (AX or
AL) - results stored as in IMUL

two's complement memory or
register

subtract (with borrow) memory or
register from register

subtract (with borrow) register
from memory or register

subtract (with borrow) immediate
data from memory or register

subtract memory or register from
register

subtract register from memory or
register

subtract data constant from memory
or register

---
_Page 254_

MP/M-86 Programmer's Guide 10.3 Arithmetic, Logic, and Shift

Table 10-6. Logic and Shift Instructions
Syntax Result

AND reg,mem|reg perform bitwise logical "and" of a
register and memory register

AND mem|reg,reg perform bitwise logical "and" of
memory register and register

AND mem|reg,numb perform bitwise logical "and" of
memory register and data constant

NOT mem| reg form ones complement of memory
or register

OR reg,mem|reg perform bitwise logical "or" of
a register and memory register

OR mem|reg,reg perform bitwise logical "or" of
memory register and register

OR mem|reg,numb perform bitwise logical "or" of
memory register and data constant

RCL mem|reg,1l rotate memory or register 1 bit
left through carry fiag

RCL mem|reg,CL rotate memory or register left
through carry flag, number of bits
given by CL register

RCR mem|reg,1 rotate memory or register 1 bit
right through carry flag

RCR mem|reg,CL rotate memory or register right
through carry flag, number of bits
given by CL register

ROL mem|reg,1l rotate memory or register 1 bit
left
ROL mem|reg,CL rotate memory or register left,

number of bits given by CL register

ROR mem{reg,l rotate memory or register 1 bit
right
ROR mem|reg,CL rotate memory or register right,

number of bits given by CL register

SAL mem|reg,l shift memory or register 1 bit
left, shift in low-order zero bits

---
_Page 255_

MP/M-86 Programmer's Guide 10.3 Arithmetic, Logic, and Shift

Table 10-6. (continued)
Syntax Result

SAL mem|reg,CL shift memory or register left,
number of bits given by CL
register, shift in low-order zero
bits

SAR mem|reg,1 shift memory or register 1 bit
right, shift in high-order bits
equal to the original high-order
bit

SAR mem|reg,CL shift memory or register right,
number of bits given by CL
register, shift in high-order bits
pqual to the original high-order

it

SHL mem|reg,1l shift memory or register 1 bit
left, shift in low-order zero bits
~ note that SHL is a different
mnemonic for SAL

SHL mem|reg,CL shift memory or register left,
number of bits given by CL
register, shift in low-order zero
bits - note that SHL is a
different mnemonic for SAL

SHR mem|reg,1l shift memory or register 1 bit
right, shift in high-order zero
bits

SHR mem|reg,CL shift memory or register right,

number of bits given by CL
register, shift in high-order zero
bits

TEST reg,mem|reg perform bitwise logical "and" of a
register and memory or register
- set condition flags but do not
change destination

TEST mem|reg,reg perform bitwise logical "and" of
memory register and register - set
condition flags but do not
change destination

TEST mem|reg,numb perform bitwise logical "and" -
test of memory register and data
constant - set condition flags
but do not change destination

---
_Page 256_

MP/M-86 Programmer's Guide 10.3 Arithmetic, Logic, and Shift

Table 10-6. (continued)

Syntax Result
XOR reg,mem|reg perform bitwise logical "exclusive
OR" of a register and memory or
register
XOR mem|reg,reg perform bitwise logical “exclusive

OR" of memory register and register

XOR mem|reg,numb perform bitwise logical "exclusive
OR" of memory register and data
constant

10.4 String Instructions

String instructions take one or two operands. The operands
specify only the operand type, determining whether operation is on
bytes or words. If there are two operands, the source operand is
addressed by the SI register and the destination operand is
addressed by the DI register. The DI and SI registers are always
used for addressing. Note that for string operations, destination
operands addressed by DI must always reside in the Extra Segment
(ES).

Table 10-7. String Instructions

Syntax Result
CMPS mem|reg,mem| reg subtract source from destination,
affect flags, but do not return
result.
LODS mem| reg transfer a byte or word from the

source operand to the accumulator.

MOVS mem|reg,mem|reg move 1 byte (or word) from source
to destination.

SCAS mem|reg subtract destination operand from
accumulator (AX or AL), affect
flags, but do not return result.

STOS mem| reg transfer a byte or word from
accumulator to the destination
operand.

---
_Page 257_

MP/M-86 Programmer's Guide 10.4 String Instructions

Table 10-8 defines prefixes for String instructions. A
prefix repeats its string instruction the number of times contained
in the CX register, which is decremented by 1 for each iteration.
Prefix mnemonics precede the string instruction mnemonic in the
statement line as shown in Section 8.8.

Table 10-8. Prefix Instructions

Syntax Result
REP repeat until CX register is zero
REPZ repeat until CX register is zero

and zero flag (ZF) is not zero
REPE equal to "REPZ"

REPNZ repeat until CX register is zero
and zero flag (ZF) is zero

REPNE equal to "REPNZ"

10.5 Control Transfer Instructions
There are four classes of control transfer instructions:

calls, jumps, and returns
conditional jumps
iterational control
interrupts

All control transfer instructions cause program execution to
continue at some new location in memory, possibly in a new code
segment. The transfer may be absolute or depend upon a certain
condition. Table 10-9 defines control transfer instructions. In
the definitions of conditional jumps, "above" and "below" refer to
the relationship between unsigned values, and "greater than" and
"less than" refer to the relationship between signed values.

---
_Page 258_

MP/M-86 Programmer's Guide 10.5 Control Transfer Instructions

Table 10-9. Control Transfer Instructions
Syntax Result

CALL label push the offset address of the next
instruction on the stack, jump to
the target label

CALL mem|regl6 push the offset address of the next
instruction on the stack, jump to
location indicated by contents of

specified memory or register

CALLF label push CS segment register on the
stack, push the offset address of
the next instruction on the stack
(after CS), jump to the target
label

CALLF mem push CS register on the stack,
push the offset address of the next
instruction on the stack, jump to
location indicated by contents of
specified double word in memory

INT numb8 push the flag registers (as in
PUSHF), clear TF and IF flags,
transfer control with an indirect
call through any one of the 256
interrupt-vector elements - uses
three levels of stack

INTO if OF (the overflow flag) is
set, push the flag registers (as in
PUSHF), clear TF and IF flags,
transfer control with an indirect
call through interrupt~-vector
element 4 (location 10H) - if the
OF flag is cleared, no operation
takes place

IRET transfer control to the return
address saved by a previous
interrupt operation, restore saved
flag registers, as well as CS and
IP - pops three levels of stack

JA lab8 jump if "not below or equal" or
"above" ( (CF or ZF)=0 )

---
_Page 259_

MP/M-86 Programmer's Guide 10.5 Control 'rransfer Instructions

Table 10-9. (continued)

Syntax Result
JAE lab8 jump if "not below" or "above or
equal" ( CF=0 )
JB 1ab8 jump if "below" or "not above or

equal" ( CF=1 )

JBE lab8 jump if "below or equal" or "not
above" ((CF or ZF)=1 )

Jc lab8 same as "JB"

JCXZ lab8 jump to target label if cx register
is zero

JE lab8 jump if "equal" or "zero" ( ZF=1 )

JG lab8 jump if "not less or equal" or

"greater" (((SF xor OF) or ZF)=0 )

JGE lab8 jump if "not less" or "greater or
equal" ((SF xor OF)=0 )

JL lab8 jump if "less" or "not greater or
equal" ((SF xor OF)=1 )

JLE lab8 jump if "less or equal" or "not
greater" (((SF xor OF) or ZF)=1 )

JMP label jump to the target label

JMP mem|regl6 jump to location indicated by
contents of specified memory or
register

JMPF label jump to the target label possibly
in another code segment

JMPS lab8 jump to the target label within +/-
128 bytes from instruction

JNA lab8 same as "JBE"

JNAE lab8s same as "JB"

JNB lab8 same as "JAE"

JNBE labs same as "JA"

JNC labs same as "JNB"

---
_Page 260_

MP/M-86 Programmer's Guide 10.5 Control Transfer Instructions

Table 10-9. (continued)

Syntax Result

JNE labs jump if "not equal" or "not zero"
( ZF=0 )

JNG lab8 same as "JLE"

JNGE lab& same as "JL"

JNL lab8 same as "JGE"

JNLE lab8 same as "JG"

JNO lab8 jump if "not overflow" ( OF=0 )

JNP lab8 jump if "not parity" or “parity
odd"

JNS lab8 jump if "not sign"

JNZ lab8 same as “JNE"

JO lab8 jump if "overflow" ( OF=l1 )

JP labs jump if "parity" or "parity even"
( PF=l )

JPE lab8 same as "JP"

JPO lab8 same as "JNP"

Js lab8 jump if "sign" ( SF=1 )

JZ lab8 same as "JE"

LOOP lab8 decrement CX register by one, jump

to target label if CX is not zero

LOOPE lab8 decrement CX register by one, jump

to target label if CX is not zero

LOOPNE

LOOPNZ

LOOPZ

lab8

labs

lab8&

and the ZF flag is set -

"loop

while zero" or “loop while equal"

decrement CX register by
to target label if CX is
and ZF flag is cleared -
while not zero" or "loop

equal"

same as "LOOPNE"

same as "LOOPE"

one, jump
not zero
"loop
while not

---
_Page 261_

MP/M-86 Programmer's Guide 10.5 Control Transfer Instructions

Table 10-9. (continued)
Syntax Result

RET return to the return address pushed
by a previous CALL instruction,
increment stack pointer by 2

RET numb return to the address pushed by a
previous CALL, increment stack
pointer by 2+numb

RETF return to the address pushed by a
previous CALLF instruction,
increment stack pointer by 4
RETF numb return to the address pushed by a
previous CALLF instruction,
increment stack pointer by 4+numb
10.6 Processor Control Instructions
Processor control instructions manipulate the flag registers.
Moreover, some of these instructions can synchronize the 8086 CPU
with external hardware.
Table 10-10. Processor Control Instructions
Syntax Results
CLC clear CF flag
CLD clear DF flag, causing string

instructions to auto-increment the
operand pointers

CLI clear IF flag, disabling maskable
external interrupts

CMC complement CF flag

ESC numb8 ,mem|reg do no operation other than compute

the effective address and place it
on the address bus (ESC is used by
the 8087 numeric co-processor),
"numb&" must be in the range 0, 63

---
_Page 262_

MP/M-86 Programmer's Guide 10.6 Processor Control Instructions

Table 10-10. (continued)
Syntax Results

LOCK PREFIX instruction, cause the 8086
processor to assert the "bus-lock"
signal for the duration of the
operation caused by the following
instruction - the LOCK prefix
instruction may precede any other
instruction - buslock prevents
co-processors from gaining the bus;
this is useful for shared-resource
semaphores

HLT cause 8086 processor to enter halt
state until an interrupt is
recognized

STC set CF flag

STD set DF flag, causing string
instructions to auto-decrement the
operand pointers

STI set IF flag, enabling maskable
external interrupts

WAIT cause the 8086 processor to enter a

"wait" state if the signal on its
"TEST" pin is not asserted

---
_Page 263_

SECTION 11

CODE-MACRO FACILITIES

11.1 Introduction to Code-macros

ASM~86 does not support traditional assembly-language macros,
but it does allow the user to define his own instructions by using
the Code-macro directive. Like traditional macros, code-macros are
assembled wherever they appear in assembly language code, but there
the similarity ends. Traditional macros contain assembly language
instructions, but a code-macro contains only code-macro directives.
Macros are usually defined in the user's symbol table; ASM-86 code-
macros are defined in the assembler's symbol table. A macro
simplifies using the same block of instructions over and over again
throughout a program, but a code-macro sends a bit stream to the
output file and in effect adds a new instruction to the assembler

Because ASM-86 treats a code-macro as an instruction, you can
invoke code-macros by using them as instructions in your program.
The example below shows how MAC, an instruction defined by a code-
macro, can be invoked.

XCHG BX,WORD3
MAC PAR1,PAR2
MUL AX,WORD4

Note that MAC accepts two operands. When MAC was defined,
these two operands were also classified as to type, size, and so on
by defining MAC's formal parameters. The names of formal parameters
are not fixed. They are stand-ins which are replaced by the names
or values supplied as operands when the code-macro is invoked. Thus
formal parameters "hold the place" and indicate where and how the
Operands are to be used.

The definition of a code-macro starts with a line specifying
its name and its formal parameters, if any:

CodeMacro <name> [<formal parameter list>]
where the optional <formal parameter list> is defined:

<formal name>:<specifier letter>[<modifier letter>] [<range>]

---
_Page 264_

MP/M-86 Programmer's Guide 11.1 Introduction to Code-Macros

As stated above, the formal name is not fixed, but a place
holder. I£ formal parameter list is present, the specifier letter
is required and the modifier letter is optional. Possible
specifiers are A, C, D, E, M, R, S, and X. Possible modifier
letters are b, d, w, and sb. The assembler ignores case except
within strings, but for clarity, this section shows specifiers in
upper-case and modifiers in lower-case. Following sections describe
specifiers, modifiers, and the optional range in detail.

The body of the code-macro describes the bit pattern and formal
parameters. Only the following directives are legal within code-
macros:

SEGFIX
NOSEGFIX
MODRM
RELB
RELW

These directives are unique to code-macros, and those which
appear to duplicate ASM-86 directives (DB, DW, and DD) have
different meanings in code-macro context These directives
discussed in detail in later sections. The definition of a code-
macro ends with a line:

aye VeVi CMe e 24sTCos 2pvrvrs are

EndM

CodeMacro, EndM, and the code-macro directives are all reserved
words. Code-macro definition syntax is defined in Backus-Naur-like
form in Appendix G. The following examples are typical code~-macro
definitions.

CodeMacro AAA
DB 37H
EndM

CodeMacro DIV divisor:Eb
SEGFIX divisor

DB 6FH
MODRM divisor
EndM

CodeMacro ESC opcode:Db(0,63) ,src:Eb
SEGFIX src
DBIT 5(1BH) ,3 (opcode (3) )
MODRM opcode,src

EndM

---
_Page 265_

MP/M-86 Programmer's Guide 11.2 Specifiers

11.2 Specifiers
Every formal parameter must have a specifier letter that

indicates what type of operand is needed to match the formal
parameter. Table 11-1 defines the eight possible specifier letters.

Table 11-1. Code-macro Operand Specifiers

Letter Operand Type
A Accumulator register, AX or AL.
Cc Code, a label expression only.
D Data, a number to be used aS an

immediate value.

E Effective address, either an M
(memory address) or an R (register).

M Memory address. This can be either
a variable or a bracketed register
expression,

R A general register only.
5 Segment register only.
X A direct memory reference,

11.3 Modifiers

The optional modifier letter is a further requirement on the
operand. The meaning of the modifier letter depends on the type of
the operand. For variables, the modifier requires the operand to be
of type: "b" for byte, "w" for word, "d" for double-word and "sb"
for signed byte. For numbers, the modifiers require the number to
be of a certain size: "b" for -256 to 255 and "w" for other numbers.
Table 11-2 summarizes code-macro modifiers.

---
_Page 266_

MP/M-86 Programmer's Guide 11.3 Modifiers

Table 11-2. Code-macro Operand Modifiers

Variables Numbers
Modifier Type Modifier Size
b byte b -256 to 255
wW word Ww anything else
| dword
sb signed
byte

11.4 Range Specifiers

The optional range is specified within parentheses by either
one expression or two expressions separated by a comma. The
following are valid formats:

(numberb)
(register)
(numberb,numberb)
fwerramhanv hk vantatarl
(brulbnoby,L cy Avoveeky

(register ,numberb)
(register,register)

Numberb is 8-bit number, not an address. The following example
specifies that the input port must be identified by the DX register:

CodeMacro IN dst:Aw,port:Rw(DX)

The next example specifies that the CL register is to contain the
"count" of rotation:

CodeMacro ROR dst:Ew,count:Rb(CL)

The last example specifies that the "opcode" is to be immediate
data, and may range from 0 to 63 inclusive:

CodeMacro ESC opcode:Db(0,63) ,adds:Eb

---
_Page 267_

MP/M-86 Programmer's Guide 11.5 Code-macro Directives

11.5  Code-macro Directives

Code-macro directives define the bit pattern and make further
requirements on how the operand is to be treated. Directives are
reserved words, and those that appear to duplicate assembly language
instructions have different meanings within a code-macro definition.
Only the nine directives defined here are legal within code-macro
definitions.

11.5.1 SEGFIX

If SEGFIX is present, it instructs the assembler to determine
whether a segment-override prefix byte is needed to access a given
memory location. If so, it is output as the first byte of the
instruction. If not, no action is taken. SEGFIX takes the form:

SEGFIX <formal name>

where <formal name> is the name of a formal parameter which rep-
resents the memory address. Because it represents a memory address,
the formal parameter must have one of the specifiers E, M or X.

11.5.2 NOSEGFIX

Use NOSEGFIX for operands in instructions that must use the ES
register for that operand. This applies only to the destination
operand of these instructions: CMPS, MOVS, SCAS, STOS. The form of
NOSEGFIX is:

NOSEGFIX segreg,<formname>

where segreg is one of the segment registers ES, CS, SS, or DS and
<formname> is the name of the memory-address formal parameter, which
must have a specifier E, M, or X. No code is generated from this
directive, but an error check is performed. The following is an
example of NOSEGFIX use:

CodeMacro MOVS si_ptr:Ew,di_ptr:Ew
NOSEGFIX ES,di_ptr

SEGFIX si_ptr
DB OA5H
EndM

9GR

---
_Page 268_

MP/M-86 Programmer's Guide 11.5 Code-macro Directives

11.5.3  MODRM

This directive intructs the assembler to generate the ModRM
byte, which follows the opcode byte in many of the 8086's
instructions. The ModRM byte contains either the indexing type or
the register number to be used in the instruction, It also
specifies which register is to be used, or gives more information to
specify an instruction.

The ModRM byte carries the information in three fields: The mod
field occupies the two most significant bits of the byte, and
combines with the register memory field to form 32 possible values:
8 registers and 24 indexing modes.

The reg field occupies the three next bits following the mod
field. It specifies either a register number or three more bits of
opcode information. The meaning of the reg field is determined by
the opcode byte.

The register memory field occupies the last three bits of the
byte. It specifies a register as the location of an operand, or
forms a part of the address-mode in combination with the mod field
described above.

For further information of the 8086's instructions and their
bit patterns, see Intel's 8086 Assembly Language Programing Manual
and the Intel 8086 Family User's Manual. The forms of MODRM are:

MODRM <form name>,<form name>
MODRM NUMBER7,<form name>

where NUMBER7 is a value 0 to 7 inclusive and <form name> is the
name of a formal parameter. The following examples show MODRM us

CodeMacro RCR dst:Ew,count:Rb(CL)

SEGFIX dst
DB OD3H
MODRM 3,dst
EndM
CodeMacro OR dst:Rw,src:Ew
SEGFIX src
DB OBH
MODRM dst,srce
EndM

11.5.4 RELB and RELW

These directives, used in IP-relative branch instructions,
instruct the assembler to generate displacement between the end of
the instruction and the label which is supplied as an operand. RELB
generates one byte and RELW two bytes of displacement. The
directives the following forms:

---
_Page 269_

MP/M-86 Programmer's Guide 11.5 Code-macro Directives
RELB <form name>
RELW <form name>

where <form name> is the name of a formal parameter with a "c"
(code) specifier. For example:

CodeMacro LOOP place:Cb

DB OE 2H
RELB place
EndM

11.5.5 DB, DW and DD

These directives differ from those which occur outside of code-
macros. The form of the directives are:

DB <form name> | NUMBERB
DW <form name> | NUMBERW
DD <form name>

where NUMBERB is a single-byte number, NUMBERW is a two-byte number,
and <form name> is a name of a formal parameter. For example:

CodeMacro XOR dst:Ew,src:Db

SEGFIX dst

DB 81H

MODRM 6,dst

DW src
EndM

11.5.6 DBIT

This directive manipulates bits in combinations of a byte or
less. The form is;

DBIT <field description>[,<field description>]
where a <field description>, has two forms:

<number><combination>
<number>(<form name>(<rshift>) )

where <number> ranges from 1 to 16, and specifies the number of bits
to be set. <combination> specifies the desired bit combination.
The total of all the <number>s listed in the field descriptions must
not exceed 16. The second form shown above contains <form name>, a
formal parameter name that instructs the assembler to put a certain
number in the specified position. This number normally refers to
the register specified in the first line of the code-macro. The
numbers used in this special case for each register are:

---
_Page 270_

MP/M-86 Programmer's Guide 11.5 Code-macro Directives

o
<
WNHRrFONDUBWNHFONDUBWNHO

<rshift>, which is contained in the innermost parentheses,
specifies a number of right shifts. For example, "0" specifies no
shift, "1" shifts right one bit, "2" shifts right two bits, and so
on. The definition below uses this form.

CodeMacro DEC dst:Rw

DBIT 5(9H) ,3(dst(0))
EndM

---
_Page 271_

MP/M-86 Programmer's Guide 11.5 Code-macro Directives

The first five bits of the byte have the value 9H, If the
remaining bits are zero, the hex value of the byte will be 48H. If
the instruction:

DEC DX
is assembled and DX has a value of 2H, then 48H + 2H = 4AH, which is
the final value of the byte for execution. If this sequence had
been present in the definition:

DBIT 5(9H) ,3(dst(1))

then the register number would have been shifted right once and the
result would had been 48H + 1H = 49H, which is erroneous.

---
_Page 272_

---
_Page 273_

SECTION 12

DDT-86

12.1 DDT-86 Operation

The DDT-86™ program allows the user to test and debug programs
interactively in a MP/M-86 environment. The reader should be
familiar with the 8086 processor, ASM-86 and the MP/M-86 operating
system as described in the MP/M-86 System Guide.

12.1.1 Invoking DDT-86
Invoke DDT-86 by entering one of the following commands:

DDT86
DDT86 filename

The first command simply loads and executes DDT-86. After
displaying its sign-on message and prompt character, - , DDT-86 is
ready to accept operator commands. The second command is similar to
the first, except that after DDT-86 is loaded it loads the file
specified by filename. If the file type is omitted from filename,
-CMD is assumed. Note that DDT-86 cannot load a file of type .H86.
The second form of the invoking command is equivalent to the
sequence:

A>DDT86
DDT86 x.x
-Efilename

At this point, the program that was loaded is ready for execution.

12.1.2 DDT-86 Command Conventions

When DDT-86 is ready to accept a command, it prompts the
operator with a hyphen, -. In response, the operator can type a
command line or a CONTROL-C or TC to end the debugging session (see
Section 12.1.4). A command line may have up to 64 characters, and
must be terminated with a carriage return. While entering the
command, use standard CP/M line-editing functions (TX, TH, TR, etc.)
to correct typing errors. DDT~-86 does not process the command line
until a carriage return is entered,

The first character of each command line determines the command

action. Table 12-1 summarizes DDT-86 commands. DDT-86 commands are
defined individually in Section 12.2.

---
_Page 274_

MP/M-85 Programmer's Guide 12.1 DDT-86 Operation

Table 12-1. DDT-86 Command Summary
Commend Action

enter assembly language statements
display memory in hexadecimal and ASCII
load program for execution

fill memory block with a constant

begin execution with optional breakpoints
hexadecimal arithmetic

set up file control block and command tail
list memory using 8086 mnemonics

move memory biock

read disk file into memory

set memory to new values

trace program execution

untraced program monitoring

show memory layout of disk file read
write contents of memory block to disk
examine and modify CPU state

XESS CHNDZEMrHAODBDHAUS

The command character may be followed by one or more arguments,
which may be hexadecimal values, file names or other information,
depending on the command. Arguments are separated from each other
by commas or spaces. No spaces are allowed between the command
Character and the first aryumentl.

12.1.3 Specifying a 20-Bit Address

Most DDT-86 commands require one or more addresses as operands.
Because the 8086 can address up to 1 megabyte of memory, addresses
must be 20-bit values. Enter a 20-bit address as follows:

SSSS:0000

where ssss represents an optional 16-bit segment number and oooo is
a 16-bit offset. DDT-86 combines these values to produce a 20-bit
effective address as follows:

ssss0
+ o000

The optional value ssss may be a 16-bit hexadecimal value or
the name of a segment register. If a segment register name is
specified, the value of ssss is the contents of that register in the
user's CPU state, as indicated by the X command. If omitted, a
default value appropriate to the command being executed, as
described in Section 12.4.

---
_Page 275_

MP/M-86 Programmer's Guide 12.1 DDT-86 Operation

12.1.4 Terminating DDT-86

Terminate DDT-86 by typing a TC in response to the hyphen
prompt. This returns control to the CCP. Note that MP/M-86 does
not have the SAVE facility found in CP/M for 8-bit machines. Thus
if DDT-86 is used to patch a file, write the file to disk using the
W command before exiting DDT-86.

12.1.5 DDT-86 Operation with Interrupts

DDT-86 operates with interrupts enabled or disabled, and
preserves the interrupt state of the Program being executed under
DDT-86. When DDT-86 has control of the CPU, either when it is
initially invoked, or when it regains control from the Program being
tested, the condition of the interrupt flag is the same as it was
when DDT-86 was invoked, except for a few critical regions where
interrupts are disabled. While the Program being tested has control
of the CPU, the user's CPU State, which can be displayed with the x
command, determines the state of the interrupt flag.

12.2 DDT-86 Commands

This section defines DDT-86 commands and their arguments. DDT-
86 commands give the user control of Program execution and allow the
user to display and modify system memory and the CPU state,

12.2.1 The A (Assemble) Command

The A command assembles 8086 mnemonics directly into memory.
The form is:

As

where s is the 20-bit address where assembly is to start. DDT-86
responds to the A command by displaying the address of the memory
location where assembly is to begin. At this point the operator
enters assembly language statements as described in Section 4 on
Assembly Language Syntax. When a statement is entered, DDT-86
converts it to binary, places the value(s) in memory, and displays
the address of the next available memory location. This process
continues until the user enters a blank line or a line containing
only a period,

DDT-86 responds to invalid statements by displaying a question
mark, ? , and redisplaying the current assembly address.

---
_Page 276_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

12.2.2 The D (Display) Command

The D command displays the contents of memory as B-bit or l6-
bit hexadecimal values and in ASCII. The forms are:

D

Ds
Ds,f
DW
DWs
DWs,f

where s is the 20-bit address where the display is to start, and f
is the 16-bit offset within the segment specified in s where the
display is to finish.

Memory is displayed on one or more display lines. Each display
line shows the values of up to 16 memory locations. For the first
three forms, the display line appears as follows:

ssss:0000 bb bb... bb cc... ©

where ssss is the segment being displayed and oooo is the offset
within segment ssss. The bb's represent the contents of the memory
locations in hexadecimal, and the c's represent the contents of
memory in ASCII. Any non-graphic ASCII characters are represented
by periods.

In response to the first form shown above, DDT-86 displays
memory from the current display address for 12 display lines. The
response to the second form is similar to the first, except that the
display address is first set to the 20-bit address s. The third
form displays the memory block between locations s and f. The next
three forms are analogous to the first three, except that the
contents of memory are displayed as 16-bit values, rather than 8-bit
values, as shown below:

SSSS:0000 wWwWww Wwww . . . WWWw CCCC . « « CC

During a long display, the D command may be aborted by typing
any character at the console.
12.2.3 The E (Load for Execution) Command

The E command loads a file into memory so that a subsequent G,
T or U command can begin program execution. The E command takes the
form:

E<filename>

where <filename> is the name of the file to be loaded. If no file
type is specified, .CMD is assumed. The contents of the user

segment registers and IP register are altered according to the
information in the header of the file loaded.

---
_Page 277_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

An E command releases any blocks of memory allocated by any
previous E or R commands or by programs executed under DDT-86. Thus
only one file at a time may be loaded for execution.

When the load is complete, DDT-86 displays the start and end
addresses of each segment in the file loaded. Use the V command to
redisplay this information at a later time.

If the file does not exist or cannot be successfully loaded in
the available memory, DDT-86 issues an error message,

12.2.4 The F (Fill) Command

The F command fills an area of memory with a byte or word
constant. The forms are:

Fs,f,b
FWs,f,w

where s is a 20-bit starting address of the block to be filled, and
f is a 16-bit offset of the final byte of the block within the
segment specified in s.

In response to the first form, DDT-86 stores the 8-bit value b
in locations s through £. In the second form, the 16-bit value wis
Stored in locations s through f in standard form, low 8 bits first
followed by high 8 bits.

If s is greater than f or the value b is greater than 255, DDT-
86 responds with a question mark. DDT-86 issues an error message if
the value stored in memory cannot be read back successfully,
indicating faulty or non-existent RAM at the location indicated,

12.2.5 The G (Go) Command

The G command transfers control to the program being tested,
and optionally sets one or two breakpoints. The forms are:

G

G,bl
G,b1l,b2
Gs

Gs,bl
Gs,bl,b2

where s is a 20-bit address where program execution is to start, and
bl and b2 are 20-bit addresses of breakpoints. If no segment value
is supplied for any of these three addresses, the segment value
defaults to the contents of the CS register.

---
_Page 278_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

In the first three forms, no starting address is specified, so
DDT-86 derives the 20-bit address from the user's CS and IP
registers. The first form transfers control to the user's program
without setting any breakpoints. The next two forms respectively
set one and two breakpoints before passing control to the user's
program, The next three forms are analogous to the first three,
except that the user's CS and IP registers are first set to s.

Once control has been transferred to the program under test, it
executes in real time until a breakpoint is encountered. At this
point, DDT-86 regains control, clears all breakpoints, and indicates
the address at which execution of the program under test was
interrupted as follows:

\*SsSSS:0000

where ssss corresponds to the CS and ooo00 corresponds to the IP
where the break occurred. When a breakpoint returns control to DDT-
86, the instruction at the breakpoint address has not yet been
executed.

12.2.6 The H (Hexadecimal Math) Command

The H command computes the sum and difference of two 16-bit
values. ‘the form is:

Ha,b

where a and b are the values whose sum and difference are to be
computed. DDT-86 displays the sum (ssss) and the difference (dddd)

ssss dddd

12.2.7 The I (Input Command Tail) Command

The I command prepares a file control block and command tail
buffer in DDT-86's base page, and copies this information into the
base page of the last file loaded with the E command. The form is:

I<command tail>

where <command tail> is a character string which usually contains
one or more filenames. The first filename is parsed into the
default file control block at 005CH. The optional second filename
(if specified) is parsed into the second part of the default file
control block beginning at 006CH. The characters in <command tail>
are also copied into the default command buffer at OO80H. The
length of <command tail> is stored at 0080H, followed by the
character string terminated with a binary zero.

---
_Page 279_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

If a file has been loaded with the E command, DDT-86 copies the
file control block and command buffer from the base page of DDT-86
to the base page of the program loaded. The location of DDT-86's
base page can be obtained from the SS register in the user's CPU
state when DDT-86 is invoked. The location of the base page of a
program loaded with the E command is the value displayed for DS upon
completion of the program load.

12.2.8 The L (List) Command

The L command lists the contents of memory in assembly
language. The forms are:

L
Ls
Ls,f

where Ss is a 20-bit address where the list is to Start, and f is a
16-bit offset within the segment specified in s where the list is to
finish.

The first form lists twelve lines of disassembled machine code
from the current list address. The second form sets the list
address to s and then lists twelve lines of code. The last form
lists disassembled code from s through £. In all three cases, the
list address is set to the next unlisted location in preparation for
a subsequent L command. When DDT-86 regains control from a program
being tested (see G, T and U commands), the list address is set to
the current value of the CS and IP registers.

Long displays may be aborted by typing any key during the list
process. Or, enter TS to halt the display temporarily.

The syntax of the assembly language statements produced by the
L command is described in Section 10.

12.2.9 The M (Move) Command

The M command moves a block of data values from one area of
memory to another. The form is:

Ms,f,d

where s is the 20-bit starting address of the block to be moved, f
is the offset of the final byte to be moved within the segment
described by s, and d is the 20-bit address of the first byte of the
area to receive the data. If the segment is not specified in d, the
Same value is used that was used for s. Note that if d is between 5
and £, part of the block being moved will be overwritten before it
is moved, because data is transferred starting from location s.

---
_Page 280_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

12.2.10 The R (Read) Command

The R command reads a file into a contiguous block of memory.
The form is:

R<filename>
where <filename> is the name and type of the file to be read.

DDT-86 reads the file into memory and displays the start and
end addresses of the block of memory occupied by the file. AV
command can redisplay this information at a later time. The default
display pointer (for subsequent D commands) is set to the start of
the block occupied by the file.

The R command does not free any memory previously allocated by
another R or E command. Thus a number of files may be read into
memory without overlapping. The number of files which may be loaded
is limited to seven, which is the number of memory allocations
allowed by the BDOS, minus one for DDT-86 itself.

If the file does not exist or there is not enough memory to
load the file, DDT-86 issues an error message.

12.2.11 The S (Set) Command

The S command can change the contents of bytes or words of
memory. The forms are:

SS
SWS

where s is the 20-bit address where the change is to occur.

DDT-86 displays the memory address and its current contents on
the following line. In response to the first form, the display is:

ssss:o000o0 bb
and in response to the second form
SSSS:0000 WWWW

where bb and wwww are the contents of memory in byte and word
formats, respectively.

In response to one of the above displays, the operator may
choose to alter the memory location or to leave it unchanged. If a
valid hexadecimal value is entered, the contents of the byte (or
word) in memory is replaced with the value. If no value is entered,
the contents of memory are unaffected and the contents of the next
address are displayed. In either case, DDT-86 continues to display
successive memory addresses and values until either a period or an
invalid value is entered.

---
_Page 281_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

DDT-86 issues an error message if the value stored in memory
cannot be read back successfully, indicating faulty or non-existent
RAM at the location indicated.

12.2.12 The T (Trace) Command

The T command traces program execution for 1 to OFFFFH program
steps. The forms are:

T
Tn
TS
TSn

where n is the number of instructions to execute before returning
control to the console.

Before an instruction is executed, DDT-86 displays the current
CPU state and the disassembled instruction. In the first two forms,
the segment registers are not displayed, which allows the entire CPU
state to be displayed on one line. The next two forms are analogous
to the first two, except that all the registers are displayed, which
forces the disassembled instruction to be displayed on the next line
as in the X command.

In all of the forms, control transfers to the program under
test at the address indicated by the CS and IP registers. If nis
not specified, one instruction is executed. Otherwise DDT-86
executes n instructions, displaying the CPU state before each step,
A long trace may be aborted before n steps have been executed by
typing any character at the console.

After a T command, the list address used in the L command is
set to the address of the next instruction to be executed.

Note that DDT-86 does not trace through a BDOS interrupt
instruction, since DDT-86 itself makes BDOS calls and the BDOS is
not reentrant. Instead, the entire sequence of instructions from
the BDOS interrupt through the return from BDOS is treated as one
traced instruction.

---
_Page 282_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

12.2.13 The U (Untrace) Command

The U command is identical to the T command except that the CPU
state is displayed only before the first instruction is executed,
rather than before every step. The forms are:

U
Un
US
USn

where n is the number of instructions to execute before returning
control to the console. The U command may be aborted before n steps
have been executed by striking any key at the console.

12.2.14 The V (Value) Command

The V command displays information about the last file loaded
with the E or R commands. The form is:

V

If the last file was loaded with the E command, the V command
displays the start and end addresses of each of the segments
contained in the file. If the last file was read with the R
command, the V command displays the start and end addresses of the
block of memory where the file was read. If neither the R nor E
commands have been used, DDT-86 responds to the V command with a

question mark, ?.

12.2.15 The W (Write) Command
The W command writes the contents of a contiguous block of
memory to disk. The forms are:

W<filename>
W<filename>,s,f

where <filename> is the filename and file type of the disk file to
receive the data, and s and f are the 20-bit first and last
addresses of the block to be written. If the segment is not
specified in f, DDT-86 uses the same value that was used for s.

If the first form is used, DDT-86 assumes the s and f£ values
from the last file read with an R command. If no file was read with
an R command, DDT-86 responds with a question mark, ?. This form is
useful for writing out files after patches have been installed,
assuming the overall length of the file is unchanged.

---
_Page 283_

MP/M-86 Programmer's Guide 12.2 DDtT-86 Commands

In the second form where s and f are specified as 20-bit
addresses, the low four bits of s are assumed to be 0. Thus the
block being written must always start ona Paragraph boundary.

If a file by the name specified in the W command already
exists, DDT-86 deletes it before writing a new file.

12.2.16 The X (Examine CPU State) Command

The X command allows the operator to examine and alter the CPU
State of the program under test. The forms are:

X
Xr
Xf

where r is the name of one of the 8086 CPU registers and f is the
abbreviation of one of the CPU flags. The first form displays the
CPU state in the format:

AX BX CX e 8 ef SS ES IP
~-----~-+- XXXX XXXX XXXX . . 2. XXXX XXXX XXXX
<instruction>

The nine hyphens at the beginning of the line indicate the state of
the nine CPU flags. Each position may be either a hyphen,
indicating that the corresponding flag is not set (0), or a l-
Character abbreviation of the flag name, indicating that the flag is
set (1). The abbreviations of the flag names are shown in Table 12-
2. <instruction> is the disassembled instruction at the next
location to be executed, which is indicated by the CS and IP
registers.

Table 12-2. Flag Name Abbreviations
Character Name

Overflow
Direction
Interrupt Enable
Trap

Sign

Zero

Auxiliary Carry
Parity

Carry

AWTPNHAHHDO

---
_Page 284_

MP/M-86 Programmer's Guide 12.2 DDT-86 Commands

The second form allows the operator to alter the registers in
the CPU state of the program being tested. The r following the % is
the name of one of the 16-bit CPU registers. DDT-86 responds by
displaying the name of the register followed by its current value.
If a carriage return is typed, the value of the register is not
changed. If a valid value is typed, the contents of the register
are changed to that value. In either case, the next register is
then displayed. This process continues until a period or an invalid
value is entered, or the last register is displayed.

The third form allows the operator to alter one of the flags in
the CPU state of the program being tested. DDT-86 responds by
displaying the name of the flag followed by its current state. If a
carriage return is typed, the state of the flag is not changed. If
a valid value is typed, the state of the flag is changed to that
value, Only one flag may be examined or altered with each Xf
command. Set or reset flags by entering a value of 1 or 0.

12.3 Default Segment Values

DDT-86 has an internal mechanism that keeps track of the
current segment value, making segment specification an optional part
of a DDT-86 command. DDT-86 divides the command set into two types
of commands, according to which segment a command defaults if no
segment value is specified in the command line.

The first type of command pertains to the code segment: A
(Assemble), L (List Mnemonics) and W (Write). These commands use
the internal type-1 segment value if no segment value is specified
in the command.

When invoked, DDT-86 sets the type-1l segment value to 0, and
changes it when one of the following actions is taken:

e When a file is loaded by an E command, DDT-86 sets the type-l
segment value to the value of the CS register.

e When a file is read by an R command, DDT-86 sets the type-l
segment value to the base segment where the file was read.

e When an X command changes the value of the CS register, DDT-86
changes the type-l segment value to the new value of the CS
register.

e When DDT-86 regains control from a user program after aG, Tor
U command, it sets the type-1l segment value to the value of the
cS register.

e When a segment value is specified explicitly in an A or L

command, DDT-86 sets the type-l segment value to the segment
value specified.

---
_Page 285_

MP/M~86 Programmer's Guide 12.3 Default Segment Values

The second type of command pertains to the data segment: D
(Display), F (Fill), M (Move) ands (Set). These commands use the
internal type-2 segment value if no segment value is specified in
the command.

When invoked, DDT-86 sets the type-2 segment value to 0, and
changes it when one of the following actions is taken:

e When 4 file is loaded by an &E command, DDT-86 sets the type-2
segment value to the value of the DS register.

e When a file is read by an R command, DDT-86 sets the type-2
segment value to the base segment where the file was read.

e When an X command changes the value of the DS register, DDT-86
changes the type-2 segment value to the new value of the DS
register.

e When DDT-86 regains control froma user program after aG, Tor
U command, it sets the type-2 segment value to the value of the
DS register.

e When a segment value is specified explicitly in an D, F, Mor S
command, DDT-86 sets the type-2 segment value to the segment
value specified.

When evaluating programs that use identical values in the CS
and DS registers, all DDT-86 commands default to the same segment
value unless explicitly overridden.

Note that the G (Go) command does not fall into either group,
Since it defaults to the cs register.

---
_Page 286_

MP/M-86 Programmer's Guide 12.3 Default Segment Values

Table 12-3 summarizes DDT-86's default segment values.

Table 12-3. DDT-86 Default Segment Values

Command type-l type-2

A x

D x

E c c

F x

G c c

H

I

L X

M Xx

R c c

5 x

T c c

U c c

Vv

W X

Xx c c
xX - use this segiient default if none specified;

change default if specified explicitly

c - change this segment default

---
_Page 287_

MP/M-86 Programmer's Guide 12.4 Assembly Language Syntax

12.4 Assembly Language Syntax for A and L Commands

In general, the syntax of the assembly language statements used

in the A and L commands is standard 8086 assembly language. Several
minor exceptions are listed below.

DDT-86 assumes that all numeric values entered are hexadecimal.

Up to three prefixes (LOCK, repeat, segment override) may
appear in one statement, but they all must precede the opcode
of the statement, Alternately, a prefix may be entered on a
line by itself.

The distinction between byte and word string instructions is
made as follows:

byte word

LODSBLODSW

STOSB STOSW
SCASB SCASW
MOVSB MOVSW
CMPSB CMPSW

The mnemonics for near and far contro. transfer instructions
are as follows:

short normal far

JMPS JMP JMPF
CALL CALLF
RET RETF

If the operand of a CALLF or JMPF instruction is a 20-bit
absolute address, it is entered in the form:

SSSS:0000

where ssss is the segment and oo000 is the offset of the
address.

---
_Page 288_

MP/M-86 Programmer's Guide 12.4 Assembly Langusge Syntax
Y J

e Operands that could refer to either a byte or word are
ambiguous, and must be preceded either by the prefix "BYTE" or
"WORD". These prefixes may be abbreviated to "BY" and "WO".
For example:

INC BYTE [BP]
NOT WORD [1234]

Failure to supply a prefix when needed results in an error
message.

e Operands which address memory directly are enclosed in square
brackets to distinguish them from immediate values. For

example:
ADD AX,5 ;add 5 to register AX
ADD AX, [5] ;add the contents of location 5 to AX

e The forms of register indirect memory operands are:

[pointer register]
[index register]
[pointer register + index register]

Lean v

where the pointer registers are BA aid BP, ana tnc iack
registers are SI and DI. Any of these forms may be preceded by

a numeric offset. For example:

ADD BX, [BP+S1]
ADD BX,3{BP+STI]
ADD BX,1D47[BPtSTI]

12.5 DDT-86 Sample Session
In the following sample session, the user interactively debugs

a simple sort program. Comments in italic type explain the steps
involved.

---
_Page 289_

MP/M-86 Programmer's Guide 12.5 DDT-86 Sample Session

Source file of program to test.
A>type sort.a86
; simple sort program

,

sort:
mov si,0 pinitialize index
mov bx,offset nlist ;bx = base of list
mov sw,0 iClear switch flag
comp:
mov al, [bx+si] rget byte from list
cmp al,l[bx+si] rcompare with next bvte
jna inci sdon\*t switch if in order
xcha al,l[px+si] ;do First vart of switch
mov {bx+si],al 7d0 second part
mov sw,l ;set switch flag
inci:
inc si ;increment index
emp si,count rend of list?
nz comp 7no, keen going
test sw,l ;Gone - anv switches?
inz sort 7yves, sort some more
done:
jmp done iget here when list ordered
‘
dseg
orq 100h ;leave space for base page
t
nlist db 3,8,4,6,31,6,4,1
count equ offset $ - offset nlist
sw db 0
end

Assemble program.
A>asm86 sort

CP/M 8086 ASSEMBLER VER i.1

END OF PASS 1

END OF PASS 2

END OF ASSEMBLY. NUMBER OF ERRORS: 0

Type fisting Jcte generated by ASH-&6.

A>type sort.ist
CP/M ASM86 1.1 SOURCE: SORT.A86 PAGE 1

simple sort vorogram

sort:
0000 BEOOOD mov si,0 sinitialize index
0003 BBOOOL] mov bx,offset nlist ;bx = base of list
0006 ©606080100 mov sw,0 ;Clear switch flag
comp:
000B 8A00 mov al, {bx+si] iget byte from list
000D 3A4001 cmp al,l[bx+si] ;compare with next byte
0010 760A jna inci ;don\*t switch if in order
0012 864001 xchq al,l{bx+si] ;do first part of switch
0015 8800 mov [bx+si],al ;do second part
0017 C606080101 mov sw, l ;set switch flag
inci:
OOlc 46 inc si sincrement index
001D 83FE08 emp si,count rend of list?
0020 7559 jnz comp ino, keep qoing
0022 F606080101 test sw,l ;done ~- any switches?
0027 75n7 inz sort ;ves, sort some more
done:
0029 EQFOFF jmp done ;qet here when list ordered
dseg
org 160h ;leave space for base page

---
_Page 290_

MP/M-86 Programmer's Guide 12.5 DDT-86 Sample Session

Q0100 630804061F06 nlist ab 3,8,4,6,31,6,4,1
0008 count eau offset S$ - offset nlist
9108 990 Sw db 9
end
END OF ASSEMBLY. NUMBER OF ERRORS: 0

Tune sumbot table sive generated by ASN- 86.
A>tyne sort.svm

00090 VARIABLES
0100 NLIST 0108 SW

0000 NUMBERS
0008 COUNT

0000 LABELS
O0GOB COMP 0029 DONE O01C INCT 5000 SORT

Tyne hex fide generated by ASM-86.
A>type sort.n86

:0400000300000000F9
:13000081BE0000BB0001C5060801008A003A4001760A8640018 8000606080 16"
+11001881014683FE0875E9F60608010175D7 E9FDFFEE
:09010082030804061F0604010035

:O0000001FF

Generate CMO gate stom .H86 file.
A>gencmd sort

BYTES READ 9039
RECORDS WRITTEN 04
Invoke DDT-85 and Lowd SORT.CHE.

A>ddt86 sort
MNDT8E 1.0

START END
CS 047N:0000 047N:002F
pS 0480:0000 0480:010F

Display initial register vakues.
AX BX CX DX SP BP ST DI cs nS ss ES IP
tee tetieietatel 0000 0000 0090 0000 119E 0000 0000 0000 047D 0480 0491 9480 9000
MOV S1,0000

. . : , e
Disassemble the beginning of the cede segment.

047D:0000 MOV $1,0000
047D:0003 MOV BxX,0100
047D:0006 MOV BYTE [0108},00
047D:0008 MOV AL, [BX+SI]
0470:000D CMP AL, O1L[BX+ST]

047D:0010 JBE oolc
047N:0012 XCHG AL, OL[BX+ST]

047D:0015 MOV [BX+SI],AL
047D:0017 MOV BYTE [0108] ,01
047D:001C INC SI
047D:001D CMP S1I,0008
047D:0020 JNZ 0008
Display the start of the data segment.
-d190,10€
6480:0100 93 08 04 06 1F 06 04 O01 00 00 00 00 00 00 00 00 ..ceeee rears wees

---
_Page 291_

MP/M-86 Programmer's Guide

12.5

Disassemble the wst of the code.

047D:0022 TEST BYTE [0108],01
047D:0027 JINZ 0000
0470:0029 Jmp 0029
047D:002C ADD {BX+SI],AL
047D:002E AnD [BX+SI],AL
047D:0030 DAS
047D:0031 AnD {BX+ST],AL
047D:0033 ??5 6c
047D:0034 pop ES
047D:0035 ADD {BX],CL
047D:0037 ADD [BX+SI],AX
0470:0039 ??= 6F

39 Execute program facm IP (=0) setting breakpoint at 29H.
-g,
\*047D:0029 Breakpoint encuuntened.

Display sorted dist.
~d100,10F
0430:0100 00 00 00 00 90 90 00 00 00 00 oc 00 oo 00 0 00

Doesn't Look good; reload sale,
-esort
START END
CS 047D:0000 047N:002F
DS 0480:0000 0480:0L0OF

Trace 3 instructions.

AX BX CX
~P- 0000 0100 o000
Z-P~ 0000 0100 Go00
Z-P- 0000 0100 0000
\*047D:000B

DX

SP
119"
LL9OE
LLIE

Be

SI

nT

IP

a

MOV
MOV

MOV

Trace some more.
-t3
AX BX CX
-~---%-P~ 0000 0100 O000
-----2Z-P= 0003 0100 0000
-~~-S-A-C 0003 0100 o000
\*047N:001C

DX

SP
119E
119E
L19E

BP

St

DT

IP
000B
ooon

MOV
CMP
JBE

Display unsorted List.
~d100,10€
0480:0100 03 08 04 06 1F 06 04 01 00 00 00 00 00 00 00

Display next instructions to be executed.

-1l
047D:001C INC SI
047D:001D cmp SI,0008
047D:0020 INZ 000B
047D;0022 TEST BYTE [0108],01
047D:0027 JINZ 0000
047D:0029 JMp 0029
047N:002c ADD {BX+SI],AL
047D:002E ADN [BX+SI1,AL
647D:0030 DAS
047D:0031 ADD (BX+SI1,AL
047D:0033 225 6C
0479:0034 PoP ES

Trace some more.
~t3

AX BX CX DX SP BP SI DT IP

~--~S-A-C 0003 0100 9000 0000 119E 0000 0000 0000 oO1c INC
C 0003 0100 0000 0000 119E 0000 0001 0000 o01D CMP
~777S-APC 0003 0100 0000 0000 119E 9000 0001 0000 0020 ING
\*0470D:000B

DDT-86 Sample Session

Ce

S1,0000
Bx,0100
BYTE [0108] ,90

AL, [RX+5T]
AL, OL{BX+ST]}
oolc

SI
ST,9008
)00R

---
_Page 292_

MP/M-86 Programmer's Guide 12.5 DDT-86 Sample Session

Dispdad constructions stem turrend TP.
947n:000B MOV AL, (BX+ST?
947D:000D CMP AL, OL{[3X+5T]
9479:0010 JRE QOL¢e
047D:0012 XCHS AL, O1L[BX+S1}
947D:CO01L5 MOV (3X+ST1,AL
947D:0017 MOV BYTE (0108},01
G47p:001C INC SI
0470:001D CMP SI,0008
047D:0029 JNZ 0008
047D:0022 TEST BYTE [(0108],01
047D:0027 INZ 0000

047D:0029 JMP 9029

-ts

AX BX CX DX SP BP SI DI IP
----S-APC 0003 0100 0000 0000 119 9000 0001 0000 O00B MOY AL, [BX+ST]
-=--3-APC 0008 0100 0000 0000 119 cO000 0001 0000 O00N CMP AL,OL{BX+STI]

woe --- 9008 0100 9000 9000 119F 0000 0001 0000 0010 JRE oo1c
\*947N:9012

047D:0012 XCHG  AL,O1{BX+ST]

047D:0015 MOV [BX+ST} ,AL
0470:0017 MOV BYTE [0108],01
G47N:001C INC St

047D:001D CMP SI,9008
047D:0020 INZ 000B

047D:0022 TEST BYTE [0108],01
047D:0027 INZ 0000

047D:0029 IMP 0029

047D:002C ADD [BX+ST],AL
047D:002E ADD {[BX+SI],AL

047D:0030 DAS

Go until sudteh has been performed.

\*047D:0020
Display cost.

-4100,10£

59480:0100 03 04 08 06 1F 06 04 01 O01 00 00 00 90 96 00 00 ....... creer eee

: Looks dike 4 and & were suctched uray. (And toggle 1s truce.)

AX BR Ox DX SP BP STI DT TP

----S-APC 0004 0100 0000 0000 119£ C000 0002 0000 0020 JINZ 0008
\*0470:000B

1 Display next instructions.
047D:000B MOV AL, [BX+ST1}
047D:0000 CMP AL, OL[BX+ST]
047D:0010 JBE ogic
047D:0012 XCHG AL,O1L[BX+ST]
047p:0015 MOV [BX+SI1,AL
047D:0017 MOV BYTE [0108}],01
047D:001C INC STI
047D:001D CMP ST,0008
047D:0020 JINZ 0900B
047D:0022 TEST BYTE [0108],01
047D:0027 JINZ 0000
047D:0029 JMP 0029

Sinee switch worked, Zet's retoad and check boundary conditions.
-esort
START END

CS 047D:0000 047D:002F
DS 0480:0000 0480:010F

---
_Page 293_

MP/M-86 Programmer's Guide 12.5 DDT-86 Sample Session

Make <€ quicker by setting List Length to 3. (Could atsa nave used s47d=le
-ald to patch. )
047D:001D cmp si,3
047D:0020
Display unsorted List.
-d100
0480:0100 03 08 04 06 1F 06 04 01 00 00 00 00 00 00 00 OM ................
0480:0110 00 00 00 00 00 00 00 00 00 00 00 oO 00 00 00 00 ................
9480:0120 00 00 00 00 00 00 00 00 00 CO 00 00 00 20 20 20 .............
39 Set breakpoint when first 3 eLements Of List should be serted.
-J,
\*047D:0029

-d100,10¢°%¢ Lf List 48 sorted.
0480:0100 03 04 06 08 1F 06 04 01 00 CO 00 09 00 00 NO 00 ................

esort Interesting, the sourth eLement seems to have been sorted in.

START END
CS 047D:0000 047D:002F
DS 0480:0000 0480:010F

Let's tay again with some tracing.
-ald
047D:001D cmp si,3
047D:0020

~t9

AX BX CX DX SP BP SI DI IP
~--7-=Z-P- 0006 0100 0000 0000 119F 0000 0003 0000 0000 mov ST,0000
—7-=-2-P- 0006 0100 0000 0000 119% 0000 0000 0000 0003 may BX,0100
~~---2~-P- 0006 0100 0000 0000 119E 0000 0000 0000 0006 Mov BYTE [0108] ,00
~----2-P- 0006 0100 0000 0000 119E 0000 0000 0000 O00R Moy AL, [BX+ST]
~-7-~-2-P- 0003 0100 0000 0000 119& 0000 0000 0000 cC00N cmp AL,OL[BX+S5T]
~~~=S-A-C 0003 0100 0009 0000 i19E 0000 0000 0000 0010 38F a01LC
----S-A-C 0003 0100 0000 0000 119E 0000 0000 n000 OOIc INC SI

calaaataetetetentes C 0003 0100 0000 0000 119& 0000 9001 0000 001D cmp S1T,0003
~777S~-A-C 0003 0100 0000 0000 119E 0000 0001 0000 0020 gNz 000R
\*047D:000B

047D:000B MOV AL, [BX+ST]
047D:000D CMP AL, OL(BX+ST]
047D:0010 JBE od1c

047p:0012 xcHG AL,O1L[BX+ST]
047D:0015 MoV (BX+S1],AL
047D:0017 MOV BYTE [0108] ,01
047D:001C INC SI

047D:001D CMP S1I,0003

047D:0020 JINZ 000B
0470:0022 TEST BYTE (0108],01
047D:0027 INZ 0000

047D:0029 IMP 0029

-t3

AX BX CX DX SP BP SI DI IP
~---S-A-C 0003 0100 0000 0000 119E 0000 0001 0000 0008 MnyV AL, [2X+ST]
~~--S-A-C 0008 0100 90000 0000 119& 0000 00C1 0000 000n cmp AL, OLIBX+ST]

aaleahetealeatetares 0008 0100 0000 0000 119F 0000 0001 0000 0010 IBF 9gl¢c
\*047D:0012

-l
047D:0012 XCHG AL,0O1[BX+ST]
047N:0015 Mov {BX+SI],AL

047D:0017 MoV BYTE [0108],01
047D:001¢ INC SI

047D:001D CMP SI,0003
047D:0020 gNz 000B

047D:0022 Test BYTE [0108],01

---
_Page 294_

MP/M-86 Programmer's Guide 12.5 DDT-86 Sample Session
-t3
AX BX CX OX SP BP SI DI IP
wwnn----- 0008 0100 9000 0000 119% 0000 0001 0000 0012 XCHG = AL,OL[BX+ST]
wenn nnn--- 9004 0100 0000 0000 119F 9000 0001 0009 9015 “OV 'BX+ST] ,AL
aa------- 9004 0100 9000 0000 119F 0600 0001 6900 0017 “nv BYTE {01081,01
\*0470:001C
~d100,10£
0480:0100 03 04 98 06 LF 06 04 01 91 00 00 D0 00 00 00 00 ...... eee ee eens
So gan, $0 good.
-t3
AX 3X CX OX SP 3P ST OI IP
--------- 0004 0100 0000 0000 119% 0000 0001 9000 OOlc INC 31
--------- 0004 0100 0000 0000 119% 0000 0002 0000 001D CMP 51,0003
----S-APC 0004 0100 0000 0000 119F 9000 0002 0000 9020 JINZ 0008
\*047D:0008
047D:000B MOV AL, [BX+SI]
047D:000D CMP AL, OL{BX+ST]
047D:0010 JBE 901c
047D:0012 XCHG AL, O1[BX+ST}
047D:0015 MOV (BX+SI] ,AL
047D:0017 MOV BYTE [0108],01
047D:001C INC SI
047D:001D CMP ST, 0003
047D:0020 INZ 0008
047D:0022 TEST BYTE [0108],01
047D:0027 SNZ 0000
0479:0029 IMP 0029
ax 3X CX DX SP BP SI MI IP
----S-APC 0004 9109 0000 0000 “119E 9000 9002 0000 0008 MOY AL, (BX+ST}
~---S-APC 0008 0100 0000 0000 119£ 0000 0002 0000 GOOD cMP AL, OL[BX+SI]
wane n= = 9008 0109 0000 0000 1L9E 0000 0002 0000 0010 JRE oolc
\*047D:0012

Sure enough, ct's comparing the third and jourth elements og the ecst.

-esort Reload program,
START END

CS 047N:0000 047D:002F

DS 0480:0000 0480:010F

047D:0009 MOV $1,0900
047D:9003 MOV BX,0100
047D:0006 MOV BYTE [0108] ,00
047D:000B MOV AL, [BX+STI]
047D:000D CMP AL, OL{BX+ST]
047D:0010 JBE goic

047p:0012 XCHG AL,OL(BX+ST]
04790:0015 MOV {(BX+ST] ,AL
047D:0017 MOV BYTE {0198},91
047D:001C INC SI

047N:001D CMP SI,0008
047D:0020 JINZ 000B

Patch Length.
~ald
047D:001D cmp si,7
0479D:0020

Tay 4t out.
-9,29
\*047D:0029

---
_Page 295_

MP/M-86 Programmer's Guide 12.5 DDT-86 Sample Session

See 45 Cd84t 48 Surted.
-d100,10£

0480:0100 01 03 04 04 96 06 08 1F 00 00 90 00 00 00 00 00 ..... Le eee
Loezs better; tet's dnstakl patch in dis file. To do thes, we
~rsort.cmd must read CMD sile including neader, so we use R
START END command.

2000:0000 2000;:01FF

First 80h bytes cuntain header, so ecde stants at &O0h.
2000:0080 MOV SI,0000
2000:0083 MOV BX ,09100
2000:0086 MOV BYTE [01081,00
2000:008B MOV AL, [BX+ST]

2000:008D CMP AL, O1(BX+ST}
2000:0090 JBE 009C
2000:0092 XCHG AL,O1L(BX+ST]
2000:0095 MOV [BX+SI],AL

2000:0097 MOV BYTE (0108],01
2000:009C INC SI

2000:003D CMP ST,0008
2000:00A0 JINZ 9088

. Tnstale patch,
-aid
2000:009D cmp si,7

. Uaeee site back to disk. {Length of §éf0 assumed to be uncharged
“wSsoTh.cma Since no Length speedcgced.}

Retvad <42e.

START END
CS 947D:0000 047D:002F
DS 0480:0000 0480:010F

Vertsy that patch was <astalled.

047D:0000 MOV $1,0000

047D:0003 MOV BX,0100
947D:0006 MOV BYTE [0108] ,00
047D:000B MOV AL, [BX+ST]
047D:000D CMP AL, OL{BX+ST]

947D:0010 JBE o01lc

047D:0012 XCHG AL, O1L{BX+ST]
947D:0015 MOV (BX+SI],AL
047D:0017 MOV BYTE [(0108],01
047p:001C INC SI

047D:001D CMP ST,0007
047D:0020 INZ 000B

Run <t.
-g,29

\*0470:0029

Still Looks good. Sip <t!
-d100,10f
0480:0100 01 03 04 04 06 06 08 IF 00 00 00 00 00 NO 00 00 .... we eee eee
-"Cc
A>

---
_Page 296_

---
_Page 297_

APPENDIX A

ASM-86 INVOCATION

Command: ASM86

Syntax: ASM86 <filename> { $ <parameters> }

where
<filename> is the 8086 assembly source file
(drive and extension are optional)
<parameters> are a one-letter type followed by
&é one-letter device from the table
below.
Default file extension: -A86

Parameters:

form: $ Td where T = type and d = device

Table A-l. Parameter Types and Devices

TYPES: A H P S F
DEVICES:

A - P X X X Xx

X x X X

Y x x x

Z 4 X xX

I X
D d

xX = valid, @ = default
Valid Parameters

Except for the F type, the default device is the the current default
drive.

---
_Page 298_

MP/M-86 Programmer's Guide

MO Uo Y

ASM86

ASM86

ASM86

ASM86

ASM86

Table A-2.

controls
controls
controls
controls
controls

Appendix A ASM-86 Invocation

Parameter Types

location of ASSEMBLER source file
location of HEX file

location of PRINT file

location of SYMBOL file

type of hex output FORMAT

Table A-3. Device Types

OHNK x!

P Drives A - P
console device
printer device
byte bucket
Intel hex format
Digital Research hex format

Table A-4. Invocation Examples

IO

IO.ASM $ AD SZ

IO0 $ PY SX

Io $ FD

10 $ FI

Assemble file I0.A86, produce IO.HEX
IO.LST and I0O.SYM.

Assemble file IO.ASM on device D,
produce IO.LST and IO.HEX,

no symbol file.

Assemble file I0.A86, produce IO.HEX,
route listing directly to printer,
output symbols on console.

Produce Digital Research hex format.

Produce Intel hex format.

---
_Page 299_

APPENDIX B

MNEMONIC DIFFERENCES FROM THE INTEL ASSEMBLER

The CP/M 8086 assembler uses the same instruction mnemonics
as the INTEL 8086 assembler except for explicitly specifying far
and short jumps, calls and returns. The following table shows

the four differences:

Table B-l. Mnemonic Differences

Mnemonic Function CP/M INTEL
Intra segment short jump: JMPS JMP
Inter segment jump: JMPF JMP
Inter segment return: RETF RET
Inter segment call: CALLF CALL

---
_Page 300_

---
_Page 301_

APPENDIX C

ASM--86 HEXADECIMAL OUTPUT FORMAT

At the user's option, ASM-86 produces machine code in either
Intel or Digital Research hexadecimal format.
The Intel format is identical to the format
defined by Intel for the 8086. The Digital Research format is
nearly identical to the Intel format, but adds segment information
to hexadecimal records. Output of either format can be input to
the GENCMD, but the Digital Research format automatically
provides segment identification. A segment is the smallest unit of
a program that can be relocated,

Table C-1 defines the sequence and contents of bytes

in a hexadecimal record,
Each hexadecimal record has one of the four formats

shown in Table cC-2.,
An example of a hexadecimal record is shown below.
Byte number=> 0123456789 ..............n

Contents=> :1l1laaaattaddd i......... ¢c CR LP

Table C-1l. Hexadecimal Record Contents

Byte Contents Symbol
0 record mark :

1-2 record length 1 il
3-6 load address aaaa
7-8 record type t t
9-(n-1) data bytes dd.....d
n- (n+l) check sum cc
n+2 carriage return CR

n+3 line feed LF

---
_Page 302_

Table C-2. Hexadecimal Record Formats

Record type Content Format
00 Data record : ll aaaa DT <data...> CC
Ol End-of-file : QO 0000 O01 FF
Extended address
02 mark : 02 0000 ST ssss cc
03 Start address : 04 0000 03 ssss iiii cc
ll => record length - number of data bytes
ce => check sum - sum of all record bytes
aaaa => 16 bit address
ssss => 16 bit segment value
iiii => offset value of start address
DT => data record type
st => segment address record type

It is in the definition of record type (DT and ST) that Digital
Research's hexadecimal format differs from Intel's. Intel defines
one value each for the data record type and the segment address
type. Digital Rescarch identifies each record with the seament that

conteins it, as shown in Table C-3.

---
_Page 303_

Symbol

DT

ST

Intel

Value

Table cC-3.

Segment Record Types

's Digital's

81H

82H

83H

84H

85H

86H
87H

88H

Value

Meaning

for data belonging to all
8086 segments

for data belonging to the
CODE segment

for data belonging to the
DATA segment

for data belonging to the
STACK segment

for data belonging to the
EXTRA segment

for all segment address
records

for a CODE absolute segment
address

for a DATA segment address
for a STACK segment address

for a EXTRA segment address

---
_Page 304_

---
_Page 305_

BYTE

EQ
NE
PTR
LAST

DB

RB

ORG
EJECT
INCLUDE

DB
RELW

AH
BP
CX
DX

APPENDIX D

RESERVED WORDS

Table D-l. Reserved Words

Predefined Numbers

WORD DWORD
Operators
GE GT LE LT
OR AND MOD NOT
SEG SHL SHR XOR
TYPE LENGTH OFFSET
Assembler Directives
DD DW IF RS
RW END ENDM EQU
CSEG DSEG ESEG SSEG
ENDIF TITLE LIST NOLIST
SIMFORM PAGESIZE CODEMACRO PAGEWIDTH
Code-macro directives
DD DW DBIT RELB
MODRM SEGFIX NOSEGFIX
8086 Registers
AL AX BH BL
BX CH CL cs
DH DI DL DS
ES SI SP Ss

Instruction Mnemonics - See Appendix E.

---
_Page 306_

---
_Page 307_

Mnemonic

AAA
AAD
AAM
AAS
ADC
ADD
AND
CALL
CALLF
CBW
CLC
CLD
CLI
CMC
CMP
CMPS
CWD
DAA
DAS
DEC
DIV
ESC
HLT
IDIV
IMUL
IN
INC
INT
INTO
IRET

Table E-1l.

APPENDIX E

ASM-86 INSTRUCTION SUMMARY

Description

ASCII adjust for Addition
ASCII adjust for Division
ASCII adjust for Multiplication
ASCII adjust for Subtraction
Add with Carry

Add

And

Call (intra segment)

Call (inter segment)
Convert Byte to Word

Clear Carry

Clear Direction

Clear Interrupt

Complement Carry

Compare

Compare Byte or Word (of string)
Convert Word to Double Word
Decimal Adjust for Addition
Decimal Adjust for Subtraction
Decrement

Divide

Escape

Halt

Integer Divide

Integer Multiply

Input Byte or Word
Increment

Interrupt

Interrupt on Overflow
Interrupt Return

Jump on Above

Jump on Above or Equal

Jump on Below

Jump on Below or Equal

Jump on Carry

Jump on CX Zero

Jump on Equal

Jump on Greater

Jump on Greater or Equal
Jump on Less

Jump on Less or Equal

ASM-86 Instruction Summary

Section

10.3
10.3
10.3
10.3
10.3
10.3
10.3
10.5
10.5
10.3
10.6
10.6
10.6
10.6
10.3
10.4
10.3
10.3
10.3
10.3
10.3
10.6
10.6
10.3
10.3
10.2
10.3
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5
10.5

---
_Page 308_

Mnemonic

JMP
JMPF
JMPS
JNA
JNAE
JNB
JNBE
JNC
JNE
JNG
JNGE
JNL
JNLE
JNO
JNP
JNS
JNZ
JO
JP
JPE
JPO
Js
Jz
LAHF
LDS
LEA
LES
LOCK
LODS
LOOP
LOOPE
LOOPNE
LOOPNZ
LOOPZ
MOV
MOVS
MUL
NEG
NOT
OR
OUT

Table E-l.

Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Junp
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Jump
Load
Load
Load
Load
Lock
Load
Loop
Loop
Loop
Loop
Loop
Move
Move

De

(intra
(inter
(8 bit
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Not
on Ove
on Par
on Par
on Par
on Sig
Of Zer

(continued)
scription

segment)
segment)
displacement)
Above

Above or Equal
Below

Below or Equal
Carry

Equal

Greater
Greater or Equal
Less

Less or Equal
Overflow
Parity

Sign

Zero

rflow

ity

ity Even

ity Odd

n
O

AH with Flags

Pointe
Effect
Pointe
Bus

Byte o
While
While
While
While

Byte o

Multiply
Negate

Not
Or

r into DS

ive Address

r into ES

r Word (of string)

Equal

Not Equal
Not Zero

Zero

r Word (of string)

Output Byte or Word

10.3

10.2

---
_Page 309_

Mnemonic

POP
POPF
PUSH
PUSHF
RCL
RCR
REP
RET
RETF
ROL
ROR
SAHF
SAL
SAR
SBB
SCAS
SHL
SHR
STC
STD
STI
STOS
SUB
TEST
WAIT
XCHG
ALAT
XOR

Table E-1. (continued)
Description

Pop

Pop Flags

Push

Push Flags

Rotate through Carry Left
Rotate through Carry Right
Repeat

Return (intra segment)
Return (inter Segment)
Rotate Left

Rotate Right

Store AH into Flags

Shift Arithmetic Left
Shift Arithmetic Right
Subtract with Borrow

Scan Byte or Word (of string)
Shift Left

Shift Right

Set Carry

Set Direction

Set Interrupt

Store Byte or Word (of string)
Subtract

Test

Wait

Exchange

Translate

Exclusive Or

Section

10.2
10.2
10.2
10.2
10.3
10.3
10.4
10.5
10.5
10.3
10.3
10.2
10.3
10.3
10.3
10.4
10.3
10.3
10.6
10.6
10.6
10.4
10.3
10.3
10.6
10.2
10.2
10.3

---
_Page 310_

---
_Page 311_

APPENDIX F

SAMPLE PROGRAM

Listing F-l1. Sample Program APPF.A86

CP/M ASM86 1.09 SOURCE: APPF.A86 Terminal Input/Output
PAGE 1

title 'Terminal Input/Output!
pagesize 50
pagewidth 79

simform

p\*\*\*\*\*\* Terminal I/O subroutines \*\*\*\*\*kk\*
; The following subroutines

; are included:

; CONSTAT - console status

i CONIN - console input

} CONOUT - console output

i

; Each routine requires CONSOLE NUMBER
; in the BL - register

?

BREKKEKKKKKKKKKKKKEE

\* Jump table: \*
REKKEKKKKEKKEKRKKKKE

~e Me Ne

CSEG ; Start of code segment
jmp_tab:

0000 E90600 jmp constat

0003 E91900 jmp conin

0006 E92B00 jmp conout

RKEKKKEKKEKKEKREKKKKKKKRKKKEKEE

\* I/O port numbers \*
RAEKKKKEKEKKEKKEKEKKEKKKEKKEEKE

me we Me Ne te

---
_Page 312_

CP/M ASM86 1
PAGE 2

OO11l

0009 53E83F00

Bo00
8Al17

EC
224706
BOFF

O000D
OO0E

CP/M ASM86 1.

\* CON

we Se Me Me Ne te Se

; Exit:

.
’

‘
constat:

Entry

.09 SOURCE: APPF.A86 Terminal Input/Output
i
; Terminal 1:
;
instatl equ 10h ; input status port
indatal equ llh ; input port
outdatal equ llh ; output port
readyinmaskl equ Olh ; input ready mask
readyoutmaskl equ 02h ; output ready mask
; Terminal 2:
instat2 equ 12h ; input status port
indata2 equ 13h ; input port
outdata2 equ 13h : output port
readyinmask2 equ 04h ; input ready mask
readyoutmask2 equ 08h ; output ready mask

RKKKKKKKEKK

STAT \*

REKKKKKEEKKEE

: BL - reg = terminal no
AL - reg = 0 if not ready
Offh if ready

push bx ! call okterminal
constatl:

push dx
mov dh,O ; read status port
mov dl,instatustab [BX]
in al,dx
and al,readyinmasktab [bx]
jz constatout
mov al,Qffh

09 SOURCE: APPF.A86 Terminal Input/Output

---
_Page 313_

PAGE

OO1A

OO1F
002B
OO2E
O02F

003A
003¢

O03E

CP/M
PAGE

003F

5A5BOAC0C3

53E82900
E8E7FF
74FB

B600
8A5702
EC

247F
5A5BC3

53E81400

B600
8A17

EC

ASM86 1.09

224708
74FA

constatout:

pop dx !

pop bx ! or al,al ! ret

me me Me Me Ne Me te Me Ne

conin:
coninl:

onout:

conoutl:

SOURCE

RKKKKKKEKE

\* CONIN \*
KAKKKAKAK

BL
AL

Entry:
Exit:

bx !
cons
coni
dx
dh,0
dl,i
al,d
al,7
dx !

push
call
jz
push
mov
mov
in
and
pop

RKKKKKKKE

\* CONOUT
KAKA KKK

Entry: B
A

bx !
dx
ax
dh,0
dl,i

push
push
push
mov
mov

in al,d

: APPF.A86

and
jz

al,r
cono

terminal no

- reg =
= read character

- reg

call okterminal !

tatl 7; test status
nl

; read character
ndatatab [Bx]
x
fh ; strip parity bit
pop bx ! ret

\*
\*
\*

L - reg = terminal no
L - reg = character to print
call okterminal
; test status
nstatustab [BX]
x

Terminal Input/Output

eadyoutmasktab [BX]
utl

---
_Page 314_

004C

OOSA

CP/M ASM86 1.09

PAGE

8A5704
EE
5A5BC3

OADB
740A
80FB03
FECB
B700
C3

5B5BC3

;
error:

me Ne

eT eT ee

pop ax ; write byte
mov dl,outdatatab [BX]

out dx,al

pop dx ! pop bx ! ret

+ttt+ttt+4tt++

+ OKTERMINAL +
+4t4+44++4444+

Entry: BL - reg = terminal no

kterminal:

or bl,bl
Jz error
cmp bl,length instatustab + 1
jae error

dec bl

mov bh,O

ret

pop bx ! pop bx ! ret ; do nothing

oKKKEKKKEKKEKKKEK ONG of code segment KREEKKEKKKKEKKKKEK

KRKKKKKEKKEKKKEKKEKE
\* Data seament \*
KEKKKKKEKKKEKKEKER

dseg

KKEKKKKKKKEKEKREEKERKEKEKEKKKKE

\* Data for each terminal \*
KKEKKEKKEEKKKEKKKKKKKKK KKK

SOURCE: APPF.A86 Terminal Input/Output
;

instatustab db instatl,instat2

indatatab db indatal,indata2
outdatatab db outdatal,outdata2
readyinmasktab db readyinmaskl,readyinmask2
readyoutmasktab db readyoutmaskl, readyoutmask2

---
_Page 315_

’

pRKKKKAKKKKKKEKRK end of file KAEKKKKEKKKEKEKKKEKKKKKEEKKEE
end

END OF ASSEMBLY. NUMBER OF ERRORS: 0

---
_Page 316_

---
_Page 317_

APPENDIX G

CODE-MACRO DEFINITION SYNTAX

<codemacro> ::= CODEMACRO <name> [<formalSlist>]
[<listS$ofSmacro$directives>]
ENDM

<name> ::= IDENTIFIER

<formalSlist> ;::= <parameter$descr>[{,<parameterS$descr>}]

<parameter$descr> ::= <form$name>:<specifier$letter>
<modifier$letter>[(<range>) ]

<specifier$letter> ::=A !|Cl|]DIE{mMIRIs | Xx
<modifierSletter> ::= b | w1|dJ{ sb
<range> ::= <singleSrange>|<double$range>

<singleSrange> ::= REGISTER | NUMBERB

<doubleSrange> NUMBERB,NUMBERB | NUMBERB,REGISTER |

REGISTER,NUMBERB | REGISTER,REGISTER

<listSof$macro$directives> ::= <macroS$directive>
{<macro$directive> }

<macro$directive> ::= <db> | <dw> | <dd> | <segfix> |
<nosegfix> | <modrm> | <relb> |
<relw> | <dbit>

<db> ::= DB NUMBERB | DB <form$name>
<dw> ::= DW NUMBERW | DW <form$name>
<dd> ::= DD <form$name>

<segfix> ::= SEGFIX <form$Sname>
<nosegfix> ::= NOSEGFIX <form$name>

<modrm> ::= MODRM NUMBER7,<form$name> |
MODRM <formSname>,<form$name>

<relb> ::= RELB <formSname>
<relw> ::= RELW <form$name>

<dbit> ::= DBIT <field$descr>{,<fieldSdescr>}

---
_Page 318_

<field$Sdescr> ::= NUMBERIS ( NUMBERB ) |
NUMBER1S ( <form$name> ( NUMBERB ) )

<form$name> ::= IDENTIFIER

NUMBERB is 8-bits

NUMBERW is 16-bits

NUMBER7 are the values 0, l,. .« , 7
NUMBER15 are the values 0, 1,. . , 15

---
_Page 319_

APPENDIX H

ASM~86 ERROR MESSAGES

There are two types of error messages produced by ASM-86:
fatal errors and diagnostics. Fatal errors occur when ASM-86 is
unable to continue assembling. Diagnostics messages report
problems with the syntax and semanics of the program being
assembled, The following messages indicate fatal errors
encountered by ASM-86 during assembly:

NO FILE

DISK FULL

DIRECTORY FULL

DISK READ ERROR
CANNOT CLOSE

SYMBOL TABLE OVERFLOW
PARAMETER ERROR

ASM-86 reports semantic and Syntax errors by placing a
numbered ASCII message in front of the erroneous source line. If
there is more than one error in the line, only the first one is
reported. Table H-1l summarizes ASM-86 diagnostic error messages.

---
_Page 320_

Number

WMANAHDU BWNHNH OO

Table H-l1. ASM-86 Diagnostic Error Messages

Meaning

ILLEGAL FIRST ITEM

MISSING PSEUDO INSTRUCTION

ILLEGAL PSEUDO INSTRUCTION

DOUBLE DEFINED VARIABLE

DOUBLE DEFINED LABEL

UNDEFINED INSTRUCTION

GARBAGE AT END OF LINE - IGNORED
OPERAND(S) MISMATCH INSTRUCTION
ILLEGAL INSTRUCTION OPERANDS

MISSING INSTRUCTION

UNDEFINED ELEMENT OF EXPRESSION
ILLEGAL PSEUDO OPERAND

NESTED "IF" ILLEGAL - "IF" IGNORED
ILLEGAL "IF" OPERAND - "IF" IGNORED

NO MATCHING "IF" FOR "ENDIF"

SYMBOL ILLEGALLY FORWARD REFERENCED - NEGLECTED
DOUBLE DEFINED SYMBOL - TREATED AS UNDEFINED
INSTRUCTION NOT IN CODE SEGMENT

FILE NAME SYNTAX ERROR

NESTED INCLUDE NOT ALLOWED

ILLEGAL EXPRESSION ELEMENT

MISSING TYPE INFORMATION IN OPERAND(S)
LABEL OUT OF RANGE

MISSING SEGMENT INFORMATION IN OPERAND
ERROR IN CODEMACROBUILDING

---
_Page 321_

APPENDIX I

DDT-86 ERROR MESSAGES

Table I-1. DDT-86 Error Messages

Error Message Meaning

AMBIGUOUS OPERAND An attempt was made to assemble a command

with an ambiguous operand. Precede the
operand with the prefix "BYTE" or
"WORD",

CANNOT CLOSE The disk file written by a W command
cannot be closed.

DISK READ ERROR The disk file specified in an R command
could not be read properly.

DISK WRITE ERROR A disk write operation could not be
successfully performed during a Ww
command, probably due to a full disk.

INSUFFICIENT MEMORY There is not enough memory to load the
file specified in an R or E command.

MEMORY REQUEST DENIED A request for memory during an R command
could not be fulfilled, Up to eight
blocks of memory may be allocated at a

given time.

NO PILE The file specified in an R or E command
could not be found on the disk.

NO SPACE There is no space in the directory for the
file being written by a W command.

VERIFY ERROR AT s:o The value placed in memory by a Fill, Set,
Move, or Assemble command could not be
read back correctly, indicating bad RAM
Or attempting to write to ROM or non-
existent memory at the indicated
location,

---
_Page 322_

---
_Page 323_

OOFF
OOEO

000D
OO0A

000A
OOOE
OOAO
O0A4

004B

p RRR KKKK

APPENDIX J

TMP LISTING

RRR KKK RIKI RRR RRR IKKE RRR RIK KE REE EKA RAR RK

Terminal Message Process

The TMP determines the user interface to MPM,
Much of the interface is available though
System calls. This TMP takes advantage of
as much as possible for Simplicity. The TMP

‘

7\* could, for instance, be easily modified to
;\* force logins and have non-standard defaults.
~\*

‘

7\* With a little more work, The TMP could do all
;\* command parsing and File Loading instead of
;\* using the CLI COMMAND FUNCTION. This is also
i\* the place to AUTOLOAD Programs for specific
i\* users. Suggestions are given in the MP/M-86
;\* SYSTEM'S GUIDE.

7x

PIU COCIOIIIIIORAE IRE RHR EIK EEE REAHEREAAE REE EERE EE
true equ Offh

false equ 0

unknown equ 0

mpmint equ 224 ; int vec for mpm
cr equ 13

lf equ 10

mpm _conout equ 2

mpm conwrite equ 9

mpm conread equ 10

mpm_diskselect equ 14

mpm_getdefdisk equ 25

mpm _usercode equ 32

mpm _conattach equ 146

mpm condetach equ 147

mpm_setdefcon equ 148

mpm clicmd equ 150

Inpm parse equ 152

mpm setdeflst equ 160

mpm _getdeflst equ 164

ps_run equ 00 7; on ready list root
pf sys equ 00ih ; system process

pf _ keep equ 002h ; do not terminate

S Mpmseg equ word ptr 40H ;begin MPM segment
s_sysdisk equ byte ptr 04bh ;system disk

Ss nens equ byte ptr 47H sys. consoles
S_version equ word ptr 78h ;ofst ver. str in SUP

---
_Page 324_

MP/M-86 Programmer'

000Cc
OOOF
001C
QO1D
OO1E
OO1F

0000 CDE0C3

0003 8A160400E8AD

OO0A
OOOF

LE8E1E0000
8A164BO01F
E8A502

0017 8A160400E882

OO1E

E8AFO2
1E8E1E0000

s Guide

rsp top
rsp_md
rsp_pd
rsp_uda
rsp bottom

e no memory
e no_pd

e q full

e illdisk

e badfname
e badftype
e bad load
e bad_read
e bad_open
e nullicmd

e ill ist

e ill_passwd

equ
equ
equ
equ
equ

008h
010h
040h
140h

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

Appendix J

me te te ee Be ee Se Se Ne Ne

TMP Listing

cant find memory

no free pd's

full queue

illegal disk #

illegal filename
illegal filetype

bad ret. from BDOS load
bad ret. from BDOS read
bad ret. from BDOS open
null command sent
illegal list device
illegal password

SI OR TOR I TOR I TOR TORR TOR RIOR RK RK KI RRR RI KIRKE RRR RRR ER KE

‘
~\*
U
’
’

~\*

;\* TMP Shared Code and Constant Area

| KH IT KI KI KI TOR RIK IRI RIK ERR RR RIKER IKKE IKKE RRR RIKER EE

‘

cseg
org

usu
Woee th

ian
ro i

int mpmint

ct
leo u

=e

!or

set default console # =

et

mov dl,defconsole

push ds
mov dl,.s_sysdisk
call setdisk

set default disk =
mov ds,sysdatseg

set default user # =

mov dl,defconsole

call attach
push ds

; INTERFACE ROUTINE FOR SYSTEM ENTRY POINTS

; PROGRAM MAIN - INITIALIZATION

TMP#

call setconsole

drive A
pop ds
console
call setuser

print version

! mov ds,sysdatseg

---
_Page 325_

MP/M-86 Programmer's Guide Appendix J : TMP Listing

002A
002E

003F

004C
OO04F
005B
OO5E

006A

OO6F

OO7A

OO7F

8B167800 mov dx,.S version

SELE4000 mov ds,.S mpmseg

E881021F call print_ds string ! pop ds
E8A002 call detach

THIS IS WHERE A LOGIN ROUTINE MIGHT
BE IMPLEMENTED. THE DATA FILE THAT
CONTAINS THE USER NAME AND PASSWORD
MIGHT ALSO CONTAIN AN INITIAL DEFAULT
DISK AND USER NUMBER FOR THAT USER.

i it eT eT

.S=2==S=>=2S=>==>=>==>=

nextcommand: ; LOOP FOREVER

7 attach console

E89802 call attach
; print CR,LF if we just sent command
803E61020074 cmp cmdsent,false ! je noclearline
C606610200 mov cmdsent,false
E85E02 call crlf

noclearline:

\*; set up and print user prompt

\* get current default user # and disk
; this call should be made on every

; loop in case the last command

i has changed the default.

B20DE84D02 mov dl,cr ! call prchar
E84F02 call getuser
8AD3E8 3302 mov dl,bl ! call prnum
E86A02 call getdisk
B24102D3 mov d1l,'A' ! add dl,bl
E83B02 call prchar
BADFO2 mov dx,offset prompt
E84402 call print string
7 Read Command from Console
BADE0N1E87002 mov dx,offset read _buf ! call conread
7 echo newline
B20AE8 2A02 mov dl,lf ! call prchar
; Make sure not a null command
8DLEE0OO1 lea bx,clicb cmd
803EDF010074 cmp read blen,O ! je gonextcmd
803F3B7422 cmp byte ptr [bx],';' ! je gonextcmd

; see if disk change
; if 'X:' change def disk to x
803EDF010275 cmp read _ blen,2 ! jne clicall

---
_Page 326_

MP/M-86 Programmer's Guide

OO8A

008C
OO8E

OO9E
OOA1

O0A4
OOA7
OOQAC

OOB1l
00B4
00B7
OOBA
OOBC

OOBE
oocl
00c4
00Cc7
00C9
OOCF
00D3
00DO6

00D9
0O0DD
O0E2
OOE6
OOEA
OOEE

OOF5

1lE
807F013A

8A17
80E25F
80EA41

80FA007208
8OFA0F7703

E81B02
E991FF

BBEOO1]
AODFO1B400
03D8C60700

B94000
BEEOO1]
BF8802
1E07
F3A5

BE6802
BF8802
E87601

E310
2BDB8A1EDFO1
81C38802
C60724
E9E300

891E6202
83FB007508
8AlLEDFO1
81038802
C6072443
803E68020074

E9A900

gonextcmd:

goodparse:

haveatail:

Appendix J TMP Listing

cmp byte ptr l[{bx],':'
jne clicall

; change default disk

mov dl, [bx] ;get disk name
and d1,5fh ;Upper Case
sub dl,'A' ;disk number

; check bounds
cmp d1,0 ! jb gonextcmd
cmp d1,15 ! ja gonextcmd

; select default disk
call setdisk

jmp nextcommand

; SEND CLI COMMAND

; put null at end of input
bx,offset clicb cmd

al,read blen ! mov ah,0
bx,ax ! mov byte ptr [bx] ,0

mov
mov
add

for err

; copy command string
i to check

Tatar
eaves

weawmn er bin

; veporcing
: for built in commands...
cx,64

mov si,offset clicb_cmd

mov di,offset savebuf

push ds ! pop es

rep movsw

and
and

mov

; parse front to see if
; built in command
mov si,offset fcb
mov di,offset savebuf
call parsefilename
jcexz goodparse
sub bx,bx ! mov bl,read_blen
add bx,offset savebuf
mov byte ptr [bx],'$'
jmp clierror

mov
cmp

parseret,bx

bx,0 ! jne haveatail

mov bl,read blen

add bx,offset savebuf

byte ptr [bx],'$' ! inc bx
fcob,0 ! je try builtin

mov
cmp

jmp not_builtin
; is it USER command?

---
_Page 327_

MP/M-86 Programmer's Guide

OOF8
OOFC
OOFF
O101
O10B
O1OF
O115
O1iD
012B
012D
013C
O13F
O14A

014D
QO15B
015D
OL6A
O016D
O17B
O17E
018A
018D

BE680246
BFB703
OEO7
B90400F3A7
BEG802
8B3E6202
83FF007425

E82501
83F900751C
BE6802

8B14
E82701
S8OFBOF7708
8AD3
E87001
E90600
BA4D03
E86F01
BA6E03
E86901
E85CO0l
8AD3E84001
E85B01
EQE8FE

try builtin:

usererr:

pruser:

notuser:
BE680246
BFBFO3
OEO7
B90400F3A7
BE6802
8B3E6202
83FF007424
E8D000
83F900751B
BE6802
8B14
E8D200
80FBFF
8AD3
E84101
E306
BA7FO3
E81B01
BAA303
E81501
E83501
8AD3E8ECOO0

printererr:

Prprinter:

Appendix J TMP Listing

mov si,offset fcb ! inc si
mov di,offset usercemd
push cs ! pop es
mov cx,4 ! repz cmpsw
jnz notuser
mov si,offset fcb
mov di,parseret
cmp di,OQ ! je pruser

inc di
call parsefilename
cmp cx,0 ! jne pruser

mov si,offset feb
inc si
mov dx,[si]
call a to b
cmp bl1,15 !
mov dl,bl
call setuser
jmp pruser
mov dx,offset usererrmsg
call printstring
mov dx,offset usermsg
call printstring
call getuser
mov dl,bl ! call prnum
call crlf
jmp nextcommand

ja usererr

mov si,offset fcb ! inc si
mov di,offset printercmd
push cs ! pop es
mov cx,4 ! repz cmpsw
jnz notprinter
Mov si,offset fcb
mov di,parseret
cmp di,O ! je prprinter
inc di
call parsefilename
cmp cx,0 ! jne prprinter
mov si,offset fcb
inc si
mov dx,[si]
call ato b
cmp bl1,0ffh
je printererr
mov dl,bl
call setlist
jcxz prprinter
mov dx,offset printemsg
call printstring
mov dx,offset printermsg
call printstring
call getlist
mov dl,bl ! call prnum

---
_Page 328_

MP/M-86 Programmer's Guide

019B
O19E

O1A1

O1A6
O1AB
O1Bl
01B4
O1B9

O1BC
01icl
01C6

01C9
O1CE

01D4
O1D9

O1DF
O1E4
O1E9
Q1EE
O1F3

O1F9
O1FE

020E

Appendix J TMP Listing

E80761 call crlf
E994FE jmp nextcommand
notprinter:
not builtin:
; initialize Cli Control Block
C606DFO100 mov clicb net,0
:; make cli call
C606G102FF mov cmdsent,true
8D16DF01B196 lea dx,clicb ! mov cl,mpm_clicmd
E84CFE call mpm
83FB007503 cmp bx,0 ! jne clierror
EQ979FE jmp nextcommand
clierror:
; Cli call unsuccesful, analyze and display err msg
i input: CX = ERROR CODE
snuil command?
83F91F7508 cmp cx,e nullcmd ! jne not_nulicmd
C606610200 mov cmdsent,false 7
E96CFE jmp nextcommand
not nullcmd:
~ ;no memory?
83F9037506 cmp cx,e no_memory ! jne memory ok
BAE402E94E00 mov dx,offset memerr ! jmp showerr
memory ok:
;no pd in table?
83F90C7506 cmp cx,e no_pd ! jne pd_ok
BAF702E94 300 mov ax,offset pderr ! jmp showerr
pd_ok:
;bad file spec?
83F918740F cmp cx,e badfname ! je fname_bad
83F917740A cmp cx,e illdisk ! je fname_bad
83F9267405 cmp cx,e ill passwd ! je fname_bad
83F9197506 cmp cx,e badftype ! jne fname_ok

BA0603E92900 fname bad:
fname ok:

83F91C7405 cmp
83F91D7506 cmp
BA1503E91900 load bad:

load_ok:
83F91E7506 cmp
BA2103E90E00

open_ok:
83F90F7506 cmp
BA3703E90 300

que_ok:

mov dx,offset fnameerr ! jmp showerr

s:bad load?
cx,e bad_load ! je load_bad
cx,e bad read ! jne load_ok
mov dx,offset loaderr ! jmp showerr

;bad open?
cx,e bad_open ! jne open_ok
mov dx,offset openerr ! jmp showerr

;RSP que full?

cx,e q full ! jne que _ok

mov dx,offset qfullerr ! jmp showerr

z;some other error...

---
_Page 329_

MP/M-86 Programmer's Guide

O21F

022E
023A

023D

024D
025B
026A
027B
O27F

028C
028D

029E
02A0
02A5

02A8
02AD
02B2
02B7
02BC
02C1
02C6
02CB

BA3503

BA8802E88900

B23AE86B00
B220E86600
5A

E87100E86B00

E9F8FD

B99800
BB6402
893F897702
8BD3E9B3FD

80FE207504
8AF2B230

80FE307229
80FE397724
80FA30721F
80FA39771A

80EE3080EA30

B800008AC2
52B10A
FOEL5A
8AD6B600
03C2
8BD8C3
B3FFC3

80FA0A720A
B231E80700
5A80EA0A
80C230

B102E962FD
B2FF
B1l20E95BFD
BAE102

LE8CC88ED8
E802001FC3
B1OQE949FD
B194E944FD
BLOEE93FFD
B119E93AFD
B1lAOE935FD
B1A4E930FD

Showerr:

parsefilename: ;

a_to_b:

atob2char:

atoberr:
prnum:

prnum_one:

prchar:
getuser:
setuser:
crlf:

printstring:

print_ds string:mov
setconsole:

setdisk:
getdisk:
setlist:
getlist:

Appendix J TMP Listing

mov dx,offset catcherr
;jmp showerr

; Print Error String

7 input: DX = address of Error
: string in CSEG
push dx
mov dx,offset savebuf ! call print_ds string
mov dl,':' ! call prchar
mov dl," ' ! call prchar
pop dx
call printstring ! call crlf
jmp nextcommand
SI = fcb DI = string
mov cx,mpm parse

mov bx,offset pcb
mov [bx],di ! mov 2[bx],si
mov dx,bx ! jmp mpm
;dl = ist char, dh = 2nd char
cmp dh,' ' ! jne atob2char
mov dh,dl ! mov dl1,'o'
adh,'0' ! jb atoberr
dh,'9' ! ja atoberr
dl,'0' ! jb atoberr
dl,'9' ! ja atoberr
sub dh,'0' ! sub dl,'o!
mov ax,0 ! mov al,dl
push dx ! mov cl,10
mul cl ! pop dx
mov dl,dh ! mov dh,O
add ax,dx
mov bx,ax ! ret
mov bl,Offh ! ret
; dl = num (0-15)
emp d1,10 ! jb prnum_one
push dx
mov dl,'1l' ! call prchar
pop dx ! sub dl,10
add dl,'o'
; jmp prchar
mov cl,mpm_conout !
mov d1,0ffh
mov cl,mpm_uSercode ! jmp mpm
mov dx,offset crlfstr
ijmp printstring
push ds ! mov ax,cs ! mov ds,ax
call print_ds string ! pop ds !
cl,mpm_conwrite ! jmp mpm

cmp
cmp
cmp
cmp

jmp mpm

ret

mov cl,mpm_setdefcon ! jmp mpm
mov cl,mpm_diskselect ! jmp mpm
mov cl,mpm_getdefdisk ! jmp mpm
mov cl,mpm_setdeflst ! jmp mpm

mov cl,mpm_getdeflst ! jmp mpm

---
_Page 330_

MP/M-86 Programmer's Guide Appendix J : TMP Listing

02D0 B192E92BFD attach: mov cl,mpm_conattach ! jmp mpm
02D5 B1S3E926FD detach: mov cl,mpm_condetach ! jmp mpm
O2DA BLOAEQ21FD conread: mov cl,mpm_conread ! jmp mpm

CRI RR RR RIT RK RK I ITOK RRR RIK KKK RK RIK RR KK KR IER KK
\*

\* CONSTANTS (IN SHARED CODE SEGMENT)

’
.
’
.
’
~\*
’
,

HK IR RIK IK TR RIKKI KKK KR RR RIKER KEK KERR RARE KER ERK KARE

O2DF 3E24 prompt db '>$!

O2E1 ODOA24 erlfstr db 13,10,'$'

O2E4 3F4E6F742045 memerr db '2Not Enough Memory$'
6E6F75676820
4D656D6F7279

O2F7 3F5044205461 pderr db '2PD Table Fulls'
626065204675
6C6C24

0306 3F4261642046 fnameerr db '?Bad File Spec$'
696C65205370
656324

0315 3F4C6F616420 loaderr db '?Load Error$'
4572726F7224

0321 3F43616E2774 openerr db '?can''t Find Commands'
2046696E6420
4360FODOD616E

0335 3F24 catcherr db '2$! |

0337 3F5253502043 qfullerr db '2RSP Command Que Full$'
6FGD6D616E64
205175652046
756C6C24

034D ODOA496E7661 usererrmsg db 13,10,'Invalid User Number,'
606964205573
6572204E756D
6265722C

0363 2049474E4F52 db ' IGNORED',13,10,'$'
45440D0A24

036E OD0A55736572 usermsg db 13,10,'User Number = $'
204E756D6265
72203D2024

037F ODOA496E7661 printemsg db 13,10,'Invalid Printer Number,'
6C6964205072
696E74657220
4E6756D626572
2C

0398 2049474E4F52 db ' IGNORED!',13,10,'$'
45440D0A24

03A3 ODOA5072696E printermsg db 13,10,'Printer Number = $'
746572204E75
6D626572203D

---
_Page 331_

MP/M-86 Programmer's Guide Appendix J : TMP Listing

03B7 555345522020 usercmd db "USER '

O3BF 5052494E5445 printercmd db "PRINTER !

BR RK KEKE KERR KKK ARERR KEK IKK KKK RE KK KKK EERE KKEEEKRKKE RR

~%\*

f

3\*

;\* TMP Data Area - this area is copied once for
;\* each system console. The 'defconsole'
;\* field is unique for each copy

;\* - Each Data Area is run by a common

;\* shared code segment.

;\*

7%
,

RRKK KKK KEKE EEK KKK KER RRR EK RE KEIR RE RE KE KKK KEE KEKE RK AK

DSEG
org rsp top
0000 0000 sysdatseg dw 0
0002 4700 sdatvar dw s_ncns
0004 0000 defconsole db 0,0
0006 000000000000 dw 0,0,0,0,0
00000000
org rsp _ pd
0010 00000000 pd dw 0,0 ; link fields
0014 00 db ps run ; Status
0015 Ccé ab 198 ; priority
0016 0300 dw pf Systpf_ keep ; flags
0018 546D70202020 db 'Tmp ; Name
0020 0400 dw offset uda/l0h j; uda seg
0022 0000 db 0,0 ; disk,user
0024 0000 db 0,0 ; ldisk,luser
0026 FFFF dw Offffh 7 mem
0028 00000000 dw 0,0 ; dvract,wait
002C 0000 db 0,0 7; Org,net
O02E 0000 dw 0 ; parent
0030 0000 db 0,0 3; Cns,abort
0032 0000 db 0,0 3; Cin,cout
0034 0000 db 0,0 ; Ist,sf3
0036 0000 db 0,0 ; sf4,sf5
0038 00000000 dw 0,0 ; reserved
003C 00000000 dw 0,0 7; pret,scratch
org rsp_uda
0040 000040010000 uda dw 0,offset dma,0,0 370-7
0048 000000000000 dw 0,0,0,0 ;8-fh
0050 000000000000 dw 0,0,0,0 710-17

---
_Page 332_

MP/M-86 Programmer's Guide

OOAO

01C0
01C6
O1cCC
01D2
01D8
O1DA
O1DC

000000000000
000000000000
000000000000
oo0d0000D801
000000000000
000000000000
000000000000
000000000000
000000000000
000000000000

CCCCCCCCECCE
CCCCCCCCCCCC
ceccccecceccccc
cccccccccccc

O1DE
O1DE
O1DF
O1DF
O1DF
O1EO

dw
dw
dw
dw
dw
dw
dw
dw
dw

dw

org
dma rb

stack aw
dw
dw
dw
stack_top aw
dw
aw

maxcmdlen equ

Appendix J TMP Listing
0,0,0,0 ;18-1£
0,0,0,0 20-27
0,0,0,0 728-2£
0,0,offset stack _top,0 330-37
0,0,0,0 ;38-3£
0,0,0,0 740-47
0,0,0,0 ;48-4£
0,0,0,0 750-57
0,0,0,0 758-5£
0,0,0,0 760-67

rsp_bottom

nm L Nn
CGecoch,Cccech,Qccech

Ocecch,0cccch,0cccch
Oeccch,0cccch,0cccch
Occecch,0cccch,0cccch

offset tmp ; code offset
unknown : code seg
unknown ; init. flags

; the Read Console Buffer and the
; Cli Control Block share the same memory

read_buf rb
read_maxcmd db
clicb rb
clicb net rb
read _ blen rb
clicb cmd rb
cmdsent db
parseret dw
pcb dw

dw
fcb rb
savebuf rb

0)

maxcmdlen + 1

false

offset savebuf
offset fcb

---
_Page 333_

MP/M-86 Programmer's Guide Appendix J : TMP Listing

;make Sure hes is formed

0308 00 db 0)

end

---
_Page 334_

---
_Page 335_

APPENDIX K

ECHO LISTING

ECHO - Resident System Process
Print Command tail to console

i aT

‘

; DEFININTIONS
OOEO mpmint equ 224 ;Mpm entry interrupt
0009 mpm conwrite equ 9 sprint string
0086 mpm qmake equ 134 7Create queue
0087 mpm qopen equ 135 7;Open queue
0089 mpm qread equ 137 ;read queue
008B mpm qwrite equ 139 ;write queue
0091 mpm setprior equ 145 ;set priority
0093 mpm_condetach equ 147 ;detach console
0094 mpm setdefcon equ 148 7;set default console
0030 pdlen equ 48 ;length of Process
; Descriptor
0020 p_cns equ byte ptr 020h ;default cns
0012 p_ disk equ byte ptr 012h ;default disk
0013 p_user equ byte ptr 013h ;default user
0024 p_list equ byte ptr 024h ;default list
0000 ps run equ 0 7PD run status
0002 pf_keep equ 2 ;PD nokill flag
0000 rsp_top equ 0 irsp offset
0010 rsp pd equ 010h 7PD offset
0040 rsp _uda equ 040h ;UDA offset
0140 rsp bottom equ 140h ;end rsp header

CODE SEGMENT

me Ne Ne

CSEG

org 0
0000 CDEO mpm: int mpmint
0002 C3 ret

mains: ;Ccreate ECHO queue
0003 B186BAC301 mov cl,mpm_qmake ! mov dx,offset qd
0008 E8F5FF call mpm
7Open ECHO queue

000B B187BA0903 mov cl,mpm_qopen ! mov dx,offset qpb
0010 E8EDFF call mpm

;set priority to normal

---
_Page 336_

MP/M-86 Progréimmer's Guide Appendix K : ECHO Listing

0013 B1Y1BAC800 mov cl,mpm_setprior ! mov dx,200
0018 ESESFF call mpm
;ES points to SYSDAT
001B 8E060000 mov es,sdatseg
loop: ; forever
;read cmdtail from queue
OO1F B189BA0903 mov cl,mpm_qread ! mov dx,offset qpb
0024 E8D9FF call mpm
;set default values from PD
0027 8BLE8302 mov bx,pdadr
mov dl,es:p_disk[bx] ;p_disk=0-15
inc dl ! mov disk,dl ;make disk=1-16

mov dl,es:p_user{bx]
mov user,dl
mov dl,es:p_list[bx]
mov list,dl

et tt tt Bt al)

002B 268A5720 mov dl,es:p_cns[bx]
O002F 88161903 IMov console,dl
;set default console
; mov dl,console
0033 B19O4E8C8FF mov cl,mpm_setdefcon ! call mpm
pecan omdtail and look for 'S' or 0.
;when found, replace w/ cr,lf,'S'
0038 8D1E8502B024 lea bx,cmdtail ! mov al,'$' ! mov ah,0
B400
0040 8BD381C28300 mov dx,bx ! add da@x,131
nextchar:
0046 3BDA770B cmp bx,dx ! ja endcmd
004A 38077407 cmp [bx],al ! je endcemd
OO4E 38277403 cmp [bx],ah ! je endcemd
0052 43EBF1 inc bx ! jmps nextchar
endemd:
0055 C6070D mov byte ptr [bx] ,13
0058 C647010A mov byte ptr l1{bx],10
005C C6470224 mov byte ptr 2{bx],'$'
;write command tail
0060 8D168502B109 lea dx,cmdtail ! mov cl,mpm_conwrite
0066 E897FF call mpm
;detach console
0069 8A161903 mov dl,console
O006D B1O3E88EFF mov cl,mpm_condetach ! call mpm
;done, get next command
0072 EBAB jmps loop
;
; DATA SEGMENT

---
_Page 337_

o000c

002C
OO02E

MP/M-86 Programmer's Guide

000000000000 sdatseg

000000000000
00000000

00000000 pd

BE

454348 4F2020

00000000

000000
000000000000

OOCODFO10000 uda

000000000000
000000000000
000000000000
000000000000
000000000000
000000007D02
000000000000
000000000000
000000000000
000000000000
000000000000

rsp top
dw

dw
dw

csp _pd
dw
db

db
dw

dw
db
db
dw
dw
db
dw
db
db
db
db
dw
rsp _uda
dw
dw
dw
dw
dw
dw
dw
dw
dw
dw
dw

dw

Appendix K

0,0
ps_run
pf_keep
"ECHO '

ffset uda/l0h
Pa)
7,0

0,offset dma,0,0

0,0,0,0
0,0,0,0
0,0,0,0
0,0,0,0

0,0,0,0

0,0,offset stack tos,0

0,0,0,0
0,0,0,0
0,0,0,0
0,0,0,0

0,0,0,0

me Me Me me Ne

St Ty

=e

ECHO Listing

link, thread
status
priority
flags

name

uda seg
disk,user
load dsk,usr
mem
dvract,wait

console
list
;10h
;20h

7 30h

740h

750h

---
_Page 338_

MP/M-86 Programmer's Guide Appendix K : ECHO Listing

OOAO 000000000000 dw 0,0,0,0 ;oO0h
Onerene)
org rsp bottom
0140 qbuf rb 131 ;Queuve buffer
01C3 0000 qd dw 0 ; link
O0ic5 0000 db 0,0 pnet,org
o1c7 Q000 dw 0 ;flags
0109 4543484F2020 db "ECHO ' ;name
01D1 8300 dw 131 s;msglen
01D3 0100 dw 1 ;nmsgs
01D5 00000000 dw 0,0 ;dq,nqg
01D9 00000000 dw 0,0 ;msgent ,msgout
O1DD 4001 dw offset qbuf ;buffer addr.
O1DF dma rb 128
025F cccccccccccc stack dw Ocecch,0cccch,0cccch
0265 CCCCCCCCCCCC dw Occech,0cccch,O0cccch
026B CCCCCCCCCCCC dw Ocecch,0cecch,0cccch
0271 CCCCCCCCCCCC dw Occecch,0cccch,0cccch
0277 CCCCCCCCCCCC dw Occecch,0cccch,0cccch
027D 0300 stack tos dw offset main ; start offset
UZ/F UUUO aw C ; start seg
0281 0000 dw 0 ; init flags
0283 pdadr cw 1 ; QOPB Buffer
0285 cmdtail rb 129 ; starts here
0306 OD0OA24 db 13,10,'S$'
0309 0000 qpb db 0,0 ;must be zero
030B 0000 dw 0 7;queue ID
030D 0100 dw 1 znmsgs
O30F 8302 dw offset pdadr ;buffer addr.
0311 4543484F2020 db ‘ECHO ' ;name to open
0319 O00 console dab 0
;disk db 0
puser db 0
slist db 0)
end

---
_Page 339_

MP/M-86 Programmer's Guide

Number

WODAIHDOPBPWNHYrFO

L System Function Summary

APPENDIX L

SYSTEM FUNCTION SUMMARY

Table L-l.
Function Name

System Reset
Console Input
Console Output

Raw Console Input
Raw Console Output
List Output

Direct Console I/0
Get I/O Byte

Set I/O Byte

Print String

Read Console Buffer
Get Console Status
Return Version Number
Reset Disk System
Select Disk

Open File

Close File

Search for First
Search for Next
Delete File

Read Sequential
Write Sequential
Make File

Rename File

Return Login Vector
Return Current Disk
Set DMA Address

Get Addr(Alloc)
Write Protect Disk
Get R/O Vector

Set File Attributes
Get Addr(disk parms)
Set/Get User Code
Read Random

Write Random
Compute File Size
Set Random Record
Reset Drive

Access Drive

Free Drive

Write Random w 0-fill
Test and Write Record
Lock Record

System Function Summary

Input Parameters

none
none

DL = char
none

DL = char
DL = char
see def
kk

Not supported

kk
DX = .Buffer

DX = .Buffer
none

none

none

DL = Disk Number
DX = .FCB

DX = .FCB

DX = .FCB

none

DX = .FCB

DX = .FCB

DX = .FCB

DX = .FCB

DX = .FCB

none

none

DX = .DMA

none

none

none

DX = .FCB

none

see def

DX = .FCB

DX = .FCB

DX = .FCB

DX = .FCB

DX = drive Vect
DS = drive Vect
DS = drive Vect
DS = .FCB

DS = .FCB

DS = .FCB

(Current DMA Addr ->

Returned values

none
AL =
none
AL =
none
none
see def

under MP/M-86 \*\*

char

char

Not supported under MP/M-86 \*\*

none

see def

AL = 00/01

AL= Version#

see def

see def

AL = Dir Code

AL = Dir Code

AL = Dir Code

AL = Dir Code

AL = Dir Code

AL = Err Code

AL = Err Code

AL = Dir Code

AL = Dir Code

AX = Login Vect\*

AX = Cur Disk#

none

AX = .Alloc

see def

AX = R/O Vect\*

see def

AX = .DPB

see def

AL = Err Code

AL = Err Code

r0, rl, r2

r0, rl, r2

AL = Err Code

none

none

AL = Err Code

AL = Err Code

AL = Err Code
File ID)

---
_Page 340_

MP/M-86 Programmer's Guide L System Function Summary

Table L-l. (continued)

Number Function Name Input Parameters Returned Values
43 Unlock Record DX = .FCB AL = Err Code
(Current DMA ADDR -> File ID)
44 Set Multi-Sector Count DL= # of Sectors AL = Rtn Code
45 Set BDOS Error Mode see def none
46 Get Disk Free Space DL = Disk # see def
47 Chain To Program see def none
48 Flush Buffers none see def
50 Direct BIOS Call DX = BD Addr. AX = BIOS return
51 Set DMA Base DX = DMA Seg.Addr none
52 Get DMA Base none AX = DMA Offset
53 Get Max Mem DX = MCB Addr see def
54 Get Abs Max DX = MCB Addr see def
55 Alloc Mem DX = MCB Addr see def
56 Alloc Abs Max DX = MCB Addr see def
57 Free Mem DX = MCB Addr see def
58 Free All Mem none none
59 Program Load DX = FCB Addr AX = B.P.Seg
100 Set Directory Label DX = .FCB AL = Dir Code
101 Return Directory Label DX = Disk # AL = Label Data
102 Read File XFCB DX = .XFCB AL = Dir Code
103 Write File XFCB DX = .XFCB AL = Dir Code
104 Set Date and Time DX = .TOD none
105 Get Date and Time DX - ,TOD none
106 Set Default Password DX = .Password none
107 Return Serial Number DX = .serialnmb serialnmb set
128 Absolute Memory Rqst DX = .MD AX = Err Code
129 Relocatable Mem Rqst DX = .MD AX = Err Code
130 Memory Free DX = .MD none
131 Poll DL = Device none
132 Flag Wait DL = Flag AX = Err Code
133 Flag Set DL = Flag AX = Err Code
134 Make Queue DX = QD addr none
135 Open Queue DX = QPB Addr AX = Err Code
136 Delete Queue DX = QPB Addr AX = Err Code
137 Read Queue DX = QPB Addr none
138 Conditional Read Queue DX = QPB Addr AX = Err Code
139 Write Queue DX = QPB Addr none
140 Conditional Write Queue DX = QPB Addr AX = Err Code
141 Delay DX = #ticks none
142 Dispatch none none
143 Terminate Process DL = Term. Code none
144 Create Process DX = PD Addr none
145 Set Priority DL = Priority none
146 Attach Console none none
147 Detach Console none none
148 Set Console DL = Console none
149 Assign Console DX = ACB Addr AX = Err Code
150 Send CLI Command DX = CLBUF Addr none
151 Call RPL DX = CPB Addr AX = result
152 Parse Filename DX = PFCB Addr see def
153 Get Console Number none AL = console #

---
_Page 341_

MP/M-86 Programmer's Guide L

System Function Summary

Table L-l. (continued)
Number Function Input Parameters Returned Values
154 System Data Address none AX = Sys Data Addr
155 Get Date and Time DX = TOD Addr none
156 Return PD Addr none AX = PD Addr
157 Abort Spec. Process DX = ABP Addr AL = Return Code
158 Attach List none none
159 Detach List none none
160 Set List DL = List # none
161 Cond. Attach List none AX = Err Code
162 Cond. Attach Console none AX = Err Code
163 MPM Version Number none AX = Version #
164 Get List Number none AL = list #

The following abbreviations are used in the table.

Addr = Address
Cond. = Conditional

Proc = Process

Rqst = Request

Spec. = Specified

term. = Terminate

char = ASCII character

Dir = Directory

Err = Error

Vect = Vector

Note: DL is the low-order half of register DX,

half of register AX,

and AL is the low-order

---
_Page 342_

---
_Page 343_

APPENDIX M

GLOSSARY

BCD: Acronym for Binary Coded Decimal. Representation of decimal
numbers using binary digits. See Appendix N for binary
representations of ASCII codes.

block: Basic unit of disk Space allocation under MP/M-86. Each
disk drive has a fixed block size (BLS) defined in its Disk
Parameter Block in the XIOS. The block size can be 1K, 2K, 4K, 8K
or 16K consecutive bytes. Blocks are numbered relative to zero so
that each block is unique and has a byte displacement in a file of
the Block Number times the Block Size.

boolean: Variable that can only have two values; usually
interpreted as true/false , or on/off.

Checksum Vector (CSV): Contiguous data area in the XIOS with one
byte for each directory sector to be checked, i.e. CKS bytes. A
Checksum Vector is initialized and maintained for each logged-in
drive. Each directory access by the system results in a checksum
calculation which is compared with that in the Checksum Vector. If
there is a discrepancy the drive is set to read-only status. This
prevents the user from inadvertantly switching disks without
logging-in the new disk. If not logged-in, the new disk is treated
the same as the old one and data on it may be destroyed if writing
is done,

CMD: File type for MP/M-86 command files. These are machine
language object modules ready to be loaded and executed. Any file
with this type may be executed by simply typing the file name after
the drive prompt (e.g. 'A>'). For example, the program PIP.CMD may
be executed by simply typing 'PIP',

command: Set of instructions that are executed when the command
name is typed after the System prompt. These instructions INay be
"built-in" the MP/M-86 System or may reside on disk as a file of
type 'CMD. In general, MP/M-86 commands consist of three parts: the
command name, the command tail, and a carriage return.

console: Primary I/O device used by MP/M-86,. It usually consists
of a CRT screen for displaying output and a keyboard for input.

control character: Non-printing ASCII character produced on the
console by holding down the 'CTRL' (CONTROL) key while striking the
Character key (e.g. control-H means "hold down 'CTRL' and hit 'H'),.
Control characters are sometimes indicated using the up-arrow symbol
(\*), e.g. 'control-H! may be represented as '“H'. Certain control
characters are treated as Special commands by MP/M-86,

Default Buffer: 128-byte buffer maintained at 0080H in the Base

Page. When the CLI loads a CMD file it initializes this buffer to
the command tail, i.e. any characters typed after the CMD file name,

---
_Page 344_

The first byte at 0080H contains the length of the command tail
while the command tail itself begins at OO81H. A binary zero
terminates the command tail. value. The 'I' command under DDT
initializes this buffer in the same way as the CLI.

Default FCB: One of two FCBS maintained at 0Q0O5CH and 0Q06CH
respectively, in the Base Page. The CLI function initializes the
first default FCB from the first delimited field in the command tail
and initializes the second default FCB from the next field in the
command tail.

delimiters: ASCII characters used to separate constituent parts of
a file specification. The CLI function recognizes certain delimiter
characters aS :.=;<> ', 'blank' and ‘carriage return'. Several
MP/M-86 commands also treat ,[]()$ as delimiter characters. It is
advisable to avoid the use of delimiter characters and lower-case
characters in filenames.

directory: Portion of a disk containing entries for each file on
the disk and locations of the blocks allocated to the files. Each
file directory element is in the formof a 32-byte FCB, although one
file may have several elements depending on its size. The maximum
number of directory elements supported is specified in the drive's
Disk Parameter Block.

directory element: 32-byte element associated with each disk file.
A file may have more than one directory element associated with it.
There are four directory elements per directory sector. Directory
elements may also be refered to as directory FCBs.

directory entry: File entry displayed when using the DIR command.
This term may also be used to refer to a physical directory element
(FCB).

disk: Magnetic media used for mass storage of data in the computer
system. The term disk may refer to either a diskette, removable
cartridge disk or fixed hard disk.

Disk Parameter Block (DPB): Table residing in the XIOS that defines
the characteristics of a drive in the disk subsystem used with MP/M-
86. The address of the DPB is in the Disk Parameter Header at
DPbase + OAH. Drives with the same characteristics may use the same
Disk Parameter Header, and thus the same DPB. However drives with
different characteristics must each have their own Disk Parameter
Header and DPB's. The address of the drives Disk Parameter Header
must be returned in registers HL when the BDOS calls the SELDSK
entry point in the BIOS. BDOS Function 31 returns the DPB address.

Disk Parameter Header (DPH): 16-byte area in the xI05S containing
information about the disk drive and a scratchpad area for certain
BDOS operations. Given n disk drives, the Disk Parameter Headers
are arranged in a table whose first row of 16 bytes corresponds to
drive 0, with the last row corresponding to drive n-l.

extent (EX): 16K consecutive bytes ina file. Extents are numbered

---
_Page 345_

from 0 to 31. One extent may contain 1, 2, 4, 8 or 16 blocks. EX
is the extent number field of a FCB and is a one byte field at FCB +
12, where FCB labels the first byte in the FCB. Depending on the
Block Size (BLS) and the maximum data Block Number (DSM), a FCB may
contain 1, 2, 4, 8 or 16 extents. The EX field is normally set to 0
by the user but contains the current extent number during file I/O.
The term 'FCB Folding' is used to describe FCB's containing more
than one extent. In CP/M version 1.4 each FCB contained only one
extent. Users attempting to perform Random Record I/O and maintain
CP/M 1.4 compatibility should be aware of the implications of this
difference,

file: Collection of data containing from zero to 242,144 records.
Each record contains 128 bytes and can contain either binary or
ASCII data. ASCII data files consist of lines of data delineated by
carriage return line feed sequences, meaning that one 128-byte
record might contain one or more lines of text. Files consist of
one or more extents, with 128 records per extent. Each file has one
or more directory elements yet shows as only one directory entry
when using the DIR command.

File Control Block (FCB): 36 consecutive bytes designated by the
user for file I/O functions. The FCB fields are explained in
Section 2.4. The term FCB is also used to refer a directory element
in the directory portion of the allocated disk space. These contain
the same first 32 bytes of the FCB, lacking only the Current Record
and Random Record Number bytes.

HEX file format: Absolute output of ASM and MAC for the Intel 8080.
A HEX file contains a sequence of absolute records which give a load
address and byte values to be stored starting at the load address.
(See Section 4.3).

I/O: Acronym for Input/Output operations or routines handling the
input and output of data in the computer system.

logical drive: Logically distinct region of a physical drive. A
physical drive may be divided into one or more logical drives, and
designated with specific drive references (i.e., d:a or d:f, etc.).
Thus at the user interface, it appears that there are several disks
in the system.

Base Page: Memory region between 0000H and O100H relative to the
beginning of the data segment used to hold critical system
Parameters and which functions primarily as an interface region
between user programs and the BDOS module. Note that in the 8080
Model, the code and data are intermixed in the code segment.

Parse: Separate a command line into its constituent parts.

physical drive: Peripheral hardware device used for mass storage of
data within the computer system.

read-only: Condition in which a drive may be read but not written
to. A drive may be set to read-only status by using the SET or STAT

---
_Page 346_

utilities. The only other way a drive may be set to read-only
status is if the checksum computed on a directory access does not
match that stored in CSV when the drive is logged-in. This protects
the user from switching disks without executing a disk reset. Files
may also be set to read-only status with the Set or STAT utilities
or the SET FILE ATTRIBUTES function (Function 30). Read-only is
often abbreviated as "R/O".

record: Smallest unit of data in a disk file that can be read or
written. A record consists of 128 consecutive bytes whose byte
displacement in a file is the product of the Record Number times
128. A 128-byte record in a file occupies one 128-byte sector on
the disk. If the blocking and deblocking algorithm is used several
records may occupy each disk sector.

reentrant code: Code that can be used by one process while another
is already executing it. Reentrant code must not be self-modifying;
that is, it must be pure code and not contain data. The data for
reentrant code can be kept in a separate data area or placed on the
stack.

sector: 128 consecutive bytes in a disk file. A sector is the
basic unit of data read and written on the disk by the XIOS. A
sector can be one 128-byte record in a file or a sector of the
directory. In some disk subsystems the disk sector size is larger
than 128 bytes, usually a power of two such as 256, 512, 1024 or

2048 bytes. These disk sectors are refered to as Host Sectors.
When the Host Sector size is larger than 128 bytes, Host Sectors
must be buffered in memory and the 128-byte sectors must be blocked

and deblocked from them.

spooling: Accumulating printer output ina file while the printer
is kept busy printing so that programs with LIST output are not
forced to wait until the printer is available.

Source file: ASCII text file usually created with a text editor
which is an input file to a system program, such as a language
translator or text formattor.

stack: Reserved area of memory where the processor saves the return
address when it receives a Call instruction. When the processor
encounters a Return instruction, it restores the current address on
the stack to the Instruction Pointer. Data such as the contents of
the registers can also be saved on the stack. The Push instruction
places data on the stack and the Pop instruction removes it. 8086
stacks are 16 bits wide; instructions operating on the stack add and
remove stack items one word at a time. An item is pushed onto the
stack by decrementing the stack pointer (SP) by 2 and writing the
item at the SP address. In other words, the stack grows downward in
memory.

track: Concentric ring on the disk; the standard IBM single density
diskettes have 77 tracks. Each track consists of a fixed number of
numbered sectors. Tracks are numbered from 0 to one less than the
number of tracks on the disk. Data on the disk media is accessed by

---
_Page 347_

combinations of track and sector numbers.

user: Logically distinct subdivision of the directory. Each
directory can be divided into 16 user numbers.

vector: Memory location used as an entry point into the operating
System used for making system calls or interrupt handling.

wildcard: Filename containing either "2?" or "\*" characters. The

BDOS directory search functions will match "2?" with any single
character and "\*" with multiple characters,

---
_Page 348_

---
_Page 349_

This appendix contains tables of the ASCII symbols,

APPENDIX N

ASCII AND HEXADECIMAL CONVERSIONS

including

their binary, decimal, and hexadecimal conversions.

Table N-
Meaning

acknowledge

bell

backspace

cancel

Carriage return
device control
delete

data link escape
end of medium
enquiry

end of transmission
escape

end of transmission
end of text

form feed

1. ASCII Symbols

Symbol

Meaning

file separator

group separator
horizontal tabulation
line feed

negative acknowledge
null

record separator
shift in

shift out

start of heading
space

Start of text
substitute
synchronous idle
unit Separator
vertical tabulation

---
_Page 350_

Binary

0000000
o0000001
0000010
0000011
0000100
0000101
0000110
0000111
0001000
0001001
0001010
0001011
0001100
0001101
0001110
0001111
0010000
0010001
0010010
0010011
0010100
0010101

ANTATITNA
vuavi ly

0010111
0011000
0011001
0011011
0011100
0011101
0011110
0011111
0100000
0100001
0100010
0100011
0100100
0100101
0100110
0100111
0101000
0101001
0101010
0101011
0101100
0101101
0101110
0101111
0110000
0110001
0110010

Table N-2.

Decimal

O01
oll

euene)

Vow

gnO9a
ULTZ

ASCII Conversion Table

Hexadecimal

Ol
OA
OB
0c
OD
OE
OF
1l
1B
1C
1D

ASCII

(CTRE- -A

(CTRL-F

)
)
)
(CTRL- -D)
)
)
(CTRL-G)

(CTRL-N)
(CTRL-O)
(CTRL-P)
(CTRL-Q)
(CTRL-R)
(CTRL-S)
(CTRL-T)
(CTRL-U)
(CTRL-V)
(CTRL-W)
(CTRL-X)
(CTRL-Y)
(CTRL-[)
(CTRL-)
(CTRL-] )
(CTRL-\*)
(CTRL—_)

(SPACE)

+ Rm TR OO MD HE

Nr One,

---
_Page 351_

Binary

0110011
0110100
0110101
0110110
0110111
0111000
0111001
0111010
0111011
0111100
0111101
0111110
0111111
1000000
1000001
1000010
1000011
1000100
1000101
1000110
1000111
1001000
1001001
1001010
1001011
1001100
1001101
1001110
1001111
1010000
1010001
1010010
1010011
1010100
1010101
1010110
1010111
1011000
1011001
1011010
1011011
1011100
1011101
1011110
1011111
1100000
1100001
1100010
1100011
1100100

Table N-2.

Decimal

O51

(continued)

Hexadecimal

3A
3B
3C
3D
3E
3F
4A
4B
4C
4D
4E
4F

ASCII

-e OO TID Ow w

TINK XE SCHON DMOUVOZZORUHTAWMUAWPaWy Wane

pe

BOTY =A

---
_Page 352_

Binary

1100101
1100110
1100111
1101000
1101001
1101010
1101011
1101100
1101101
1101110
1101111
1110000
1110001
1110010
1110011
1110100
1110101
1110110
1110111
1111000
1111001
1111010
1iiidii
1111100
1111101
1111110
1111111

Table N-2.

Decimal

lll

tas

(continued)

Hexadecimal

ASCII

te ANKE KM ES ECMTHNMT OD B PwU TO mo

DEL

---
_Page 353_

A

A-Base, 45

AAA, 240

AAD, 240

AAM, 240

AAS, 240

Abort Parameter Block,

ABORT SPECIFIED PROCESS,

absolute address, 54

ACCESS DRIVE, 36, 120

Access stamp, 84

ADC, 240

ADD, 240

address conventions in ASM-86,

address expression, 224

ALLOCATE ABSOLUTE MEMORY,

ALLOCATE MEMORY function,

allocate storage, 233

allocation vector, 104

ambiguous file reference,

AND, 242

Archive Attribute, 21

arithmetic operators, 221

ASSIGN CONSOLE function,

Assign Control Block, 185

ATTACH CONSOLE function,

ATTACH LIST function,

attribute bits, 21

22,

68,

72, 199

B

Bad Sector error, 37

base extent, 111, 113

Base Page initialization,

Base Page - 8080 Model, 50

Base Page - Compact Model,

Base Page - Initial Data
Segment, 43

Base Page - Small Model, 51

Basic Disk Operating System,
7, 13

BDOS, 7

BDOS Error Mode, 132

BDOS file system, 15, 17

BDOS Multi-Sector Count, 95

bit map, 101

INDEX

bit vector, 106

blocking, 33
blocking/deblocking, 33, 135
bracketed expression, 224
burst mode, 32

Cc

CALL, 246

Call Parameter Block, 188

CALL RPL function, 188

CBW, 240

CCB, 7

CHAIN TO PROGRAM, 134

Character Control Block, 7,
175, 177

Character I/O module, 7

Character string, 214

checksum, 29, 84

checksum verification, 29

child process, 66

CIO, 7

CLC,

CLD, 249

CLI, 23, 249

CLI function,

CLOCK, 6

CLOSE FILE function, 26, 86

closing files, 20

CMC, 249

CMD, 8

CMD file,

CMP, 240

CMPS, 244

Code Group Descriptor, 59, 60

code segment, 228

code-macro directives, 255

code-macros, 251

command file, 8

Command Line Buffer, 186

COMMAND LINE INTERPRETER
function, 186

Command Queue Message, 62

Command RSP, 61

Compact Memory Model,

Compact Model, 49, 52

COMPUTE FILE SIZE function,

conditional assembly, 230

43, 59

---
_Page 354_

CONDITIONAL ATTACH CONSOLE
function, 203

CONDITIONAL ATTACH LIST
function, 202

conditional read, 5

CONDITIONAL READ QUEUE
function, 62, 168

conditional write, 5

CONDITIONAL WRITE QUEUE, 170

CONSOLE INPUT function, 68

console output, 209

CONSOLE OUTPUT function, 69

CONSOLE STATUS function, 79

constants, 213

contiguous memory segment, 157

control characters, 68

control transfer instructions,

converting 8080 programs to
MP/M-86, 54

CPU resource, 3

CREATE PROCESS function,
59, 174

Creation, 98

creation of output files,

CSEG, 225

Current Record field, 83

current record position, 48

current user number, 13, 110

CWD, 240

43,

D

DAA, 241

DAS, 241

data, 153

data area, 13

data block size, 17

Data Group Descriptor, 60, 66
data segment, 228

data transfer, 237

date stamp, 26

DB, 231

DD, 232

deblocking, 33

DEC, 241

default DMA base, 137

default DMA buffer, 48
default drive, 47
define data area,
DELAY, 6

DELAY function,
Delay List, 175
DELETE FILE function, 91
Delete Mode, 26

DELETE QUEUE function, 65, 166
delimiters, 15, 191, 211
detach character, 68
DETACH CONSOLE function,
DETACH LIST function, 200
DIRECT BIOS CALL function,
Direct Memory Address, 103
directive statement, 226
directory area, 13
Directory Code, 39, 40, 41
directory functions, 14
Directory Label, 14, 23,
25, 27, 146, 148
disk directory area, 17
Disk Parameter Block, 34,
DISK SYSTEM RESET, 34
DISPATCH function, 172
Dispatcher, 4
Dispatching, 3
DIV, 241
address, 43
base, 43
Buffer, 63, 64, 103
offset, 43, 138, 178
default address, 81
dollar 223
DQ List,
drive
drive

24,

|

capacity, 17

related functions, 14
drive reset operation, 34
drive select code, 15
drive-related functions, 14
DSEG, 228

DW, 232

E

ECHO, 60, 64, 66
effective address,
EJECT, 234
END, 230
end-of-line,
ENDIF, 230
EQU, 231
Error Code, 10,
Error Flag, 39,
Error Handling, 10
error messages, 38

error mode, 14, 37
ESC, 249

ESEG, 229
expressions, 224

extended error codes, 41
extended errors, 37
extended file, 32

---
_Page 355_

Extended Input/Output System,

extent, 93, 95
extra Segment, 229
F

Far
Far
FCB
FCB
FCB

Call Instruction, 52

Return, 43, 50

checksum, 30

format, 23

length, 18

FCB Area 1, 47

FCB Area 2, 48

File Access, 31

file access functions, 14

file attributes, 21, 107

File Control Block FCB, 18

File directory elements, 20

file format, 18

File ID, 19, 27,

File locking, 7

file name extensions, 207

file naming conventions, 16

File R/O error, 37

file references, 13

File Security, 29

file size, 17

file specification, 15

file system, 14, 29, 32

file type field, 13, 15

file types, 16

filename field, 13, 15

Flag 1 - the system tick flag,

flag bits, 236, 239

flag registers, 236

FLAG SET function, 161

FLAG WAIT, 6

FLAG WAIT function,

FLUSH BUFFERS, 33

FLUSH BUFFERS function, 135

formal parameters, 251

31, 84, 126,

FREE ALL MEMORY function, 144
FREE DRIVE, 30, 36

FREE DRIVE function, 121

free memory, 67

FREE MEMORY function, 143
Function 151 - CALL RPL, 9

G

G-Form, 43

G-Length, 45

G-Max, 45
G-Min, 45
GENCMD, 53, 56, 59
GENSYS, 8, 59, 66

GET CONSOLE function, 193

GET DATE AND TIME function,
154, 195

GET DISK FREE SPACE function,

Get DMA base, 138

GET LIST NUMBER function, 205

GET SYSDAT function, 194

Group Descriptor, 43

H

Header Record, 43

header record - CMD file, 49,

HLT, 250

I

I/O BYTE, 75
identifiers,
IDIV, 241
IDLE, 1l
IDLE process, 5

IF, 230

IMUL, 241

IN, 237

INC, 241

INCLUDE, 230

independent group, 47

inital stack - 8080 model, 50
initial stack, 52
initialized storage, 231
initializing an FCB, 19
Instruction Pointer, 50, 179
instruction statement, 225
INT, 246

INT 224, 179

INT 225, 179

Intel HEX File Format, 56
Intel utilities, 54

interface attribute £5°, 83,
86, 91

interface attribute f£6%, 83

interface attribute f£7%, 84

interface attribute £8%, 84

interface attributes, 22, 28

INTO, 246

invoking ASM-86, 208

---
_Page 356_

IRET, 246
IRET instruction, 64, 65
IRET structure, 64, 66

J

JA, 246
JB, 247
JCXZ, 247
JE, 247
JG, 247
JL, 247
JLE, 247
IMP, 247
JNA, 247
INB, 247
INE, 248
ING, 248
INL, 248
JNO, 248
INP, 248
INS, 248
INZ, 248
JO, 248
IP, 248
3S, 248

JZ, 248
K

KEEP flag, 173
keywords, 215

L

labels, 217, 255

LAHF, 237

LDS, 237

LEA, 237

LES, 237

LIST, 234

LIST OUTPUT function, 72
location counter, 229
LOCK, 250

lock list, 20, 29, 31, 127
LOCK RECORD function, 126
Locked Mode, 27

LODS, 244

log-in operation, 34
logical drive, 13, 17
logical interrupt, 161
logical operators, 221
login vector, 101

LOOP, 248

M

M80 byte, 47

MAKE FILE, 25, 26, 28

Make File function, 21

MAKE FILE function, 97

MAKE QUEUE function, 65, 163

maximum memory size, 55

MEM, 7

memory, 48

MEMORY ALLOCATION function,

Memory Control Block, 139,
140, 141, 142, 143

Memory Free Parameter Block,

memory models, 49

Memory Module, 7

Memory Parameter Block, 157

memory protection, 174

memory absolute, 140

Memory initialization, 43

memory largest available
region, 139

minimum memory value, 55

miecallaneons functions, 14

mnemonic, 225

modifiers, 253

MOV, 237

MOVS, 244

MPMLDR, 34

MUL, 241

multi-sector count, 14, 32,
93, 103, 112, 113, 115,
123, 125, 126, 128, 130,

Multi-Sector I/0, 32

Mutual exclusion, 6

Mutual exclusion queues, 5, 67

MxXdisk, 6

N

name field, 226

NEG, 241

networking interfaces, 3
nibble, 41

NOLIST, 234

NOT, 242

NO List, 175

number symbols, 218
numeric constants, 213
numeric expression, 224

---
_Page 357_

O

offset, 217

offset value, 227

one second flag - Flag 2, 6

OPEN FILE, 28

OPEN FILE function,

open mode, 28

OPEN QUEUE function, 164

Operator precedence, 223

operators, 218

optional run-time parameters,

OR, 242

order of operations, 223

ORG, 229

OUT, 238

Output files,

21, 28, 83

207, 208

P

PAGESIZE, 233

PAGEWIDTH, 234

parent/child relationship, 48

Parse Filename, 15, 16

Parse Filename Control Block,

PARSE FILENAME function, 43,

password, 13

password address,

password field, 15

password length, 47

Password protection, 25

passwords, 25, 26

period operator, 222

permanent drive, 34

permanent drives, 36

Physical error, 37

physical error codes, 41

Physical errors, 37

POLL DEVICE function, 159

Poll List, 175

POP, 238

Predefined numbers, 215

prefix, 225, 245

PRINT STRING function, 75

printer echo, 68

Printer output, 209

Priority, 176, 181

Priority of transient process,

priority-driven, 5
Process, l, 29, 30, 31
Process Descriptor, 4, 6, 175

Process Descriptor
initialization, 43

process priority, 171

program, l

PROGRAM LOAD function, 45,

PTR operator,

PUSH, 238

Q

50,

qualified reset, 35

Queue Buffer, 5

Queue Descriptor, 5, 162

Queue Descriptor - RSP Command
Queue, 65

Queue Parameter Block, 65,
164, 166, 167, 168, 169,

R

R/O error, 37

radix indicators, 213

Random Record Number, 18, 48,
111, 113, 116, 118, 123,
126, 129

RAW CONSOLE INPUT function, 70

RAW CONSOLE OUTPUT function,

RCL, 242

RCR, 242

READ CONSOLE BUFFER function,

RB,

READ FILE XFCB, 27

READ FILE XFCB function, 149
Read Mode, 26

READ QUEUE, 6

READ QUEUE function, 62, 167
READ RANDOM function, 111
READ SEQUENTIAL function, 93

Read/Only attribute, 21

read/only attribute tl’, 84

Read/only Mode, 28, 31

Ready List, 4, 175

ready process, 4

Real-Time Monitor, 3

record, 18

record buffer, 33

record locking, 30, 31

register AL, 39

register initialization, 64

Register Usage For System
Function Calls, 9

---
_Page 358_

registers, 215

relational operators, 221

removeable drive, 34, 36

RENAME FILE function, 99

REP, 245

RESET DISK SYSTEM function,

RESET DRIVE, 34

RESET DRIVE function,

reset state, 81, 119

Resident Procedure Library,

Resident Procedure Library
RPL, 8

Resident System Process, 8,
59, 186

Resident System Processes
RSPs, 1

RET, 249

Return and Display Mode,

return codes, 39

RETURN CURRENT DISK function,

RETURN

Return

RETURN

RETURN MP/M VERSION NUMBER
function, 204

RETURN PD ADDRESS function,

RETURN PROCESS DESCRIPTOR
ADDRESS, 196

RETURN VERSION NUMBER,

ROL, 242

ROR, 242

round-robin scheduled, 5

RS, 233

RSP Command Queue, 61

RSP copies - 8080 Model, 61

RSP copies - Small Model, 61

RSP Header, 60, 63

RSP Memory Models, 59

RSP Process Descriptor,

RSP Stack, 65

RSP UDA, 63

RSP User Data Area, 64

RSP - 8080 Memory Model,

RSP CMD Header Record,

DIRECTORY LABEL,
Error Mode, 132
LOGIN VECTOR function,

63, 64

- 59,

RSP -
RSP -
RSP -
RSP -
RSP -
RSP -
RSP -

ECHO, 59

multiple copies, 60
Process Descriptor,
shared code, 61
Small Memory Model, 60
TMP, 59

UDA, 59

RSPs, 8

RTM, 3

RUN state, 159, 160, 161
run-time options, 209

running process, 4

RW, 233

Ss

SAHF, 238

SAL, 242

SAR, 243

SBB, 241

SCAS, 244

SEARCH FOR FIRST function, 88

SEARCH FOR NEXT function, 90

segment, 217

segment base values, 227

segment group memory
requirements, 54

segment override operator, 222

segment register change, 52

segment register
initialization, 50

segment start directives, 227

SELECT DISK function, &2

Select error, 37

separators, 211

sequential I/O processing, 32

serial number, 156

SET BDOS ERROR MODE, 37

SET CONSOLE function, 184

SET DATE AND TIME function,

SET DIRECTORY LABEL, 25,

SET DMA ADDRESS function,

Set DMA base, 137

SET FILE ATTRIBUTES function,

21, 107

SET LIST function, 201

SET MULTI-SECTOR COUNT, 32,
41, 131

SET PRIORITY function, 181

SET RANDOM RECORD function,

shared access mode, 31

Shared access to files, 7

SHL, 243

SHR, 243

SIMFORM, 234

Small Memory Model, 45, 51

Small Model, 49

Source files, 18

Sparse files, 18

specifiers, 253

---
_Page 359_

SSEG, 228
Stack Pointer,
stack segment,
stamp, 98
start scroll character, 68

Starting ASM-86, 208
statements, 225

STC, 250

STD, 250

STI, 250

stop scroll character, 68

STOS, 244

string constant, 214

string operations, 244

SUB, 241

SUP, 3

Supervisor, 3

Suspended process, 4

Symbols, 231

SYSDAT, 8

SYSDAT segment address, 8

System Attribute, 21

SYSTEM attribute, 187

system attribute t2°, 83

System Data Area, 63, 65,
163, 174, 194

SYSTEM flag, 173

System Function Calling
Conventions, 9

System generation, 8,

system Lock List, 67,

SYSTEM Process, 165

System process - TICK, 6

System processes, 1

System queue, 5, 163

SYSTEM RESET function,

System Tick, 171, 172

System timing, 6

66,

29, 59
120, 121

43, 67

T

tab character, 68

Terminate Code, 173

terminate character, 68

TERMINATE function, 43, 67,
68, 173

Termination Code, 197

TEST, 243

TEST AND WRITE RECORD, 32

TEST AND WRITE RECORD
function, 123

time of day, 6

Time of Day Structure, 195

time stamping, 14

time stamps, 26

TITLE,
TOD, 27
Transient Process Area, 174
transient processes, 1, 3
Transient Programs, 8

type, 217

U

UDA, 4
UDA initialization, 43
unary operators, 222
unconditional read, 5
UNLOCK RECORD function,

Unlocked Mode, 27, 31
Update stamp, 87

User 0, 23

User Data Area, 174,
user directories, 22
user number, 23

USER Process, 165

31,

Vv

variable manipulator, 222
variables, 216
virtual size, 116
WwW

WAIT, 250

WRITE FILE, 25

WRITE FILE XFCB function,
Write Mode, 26

WRITE PROTECT DISK function,

36, 105
WRITE QUEUE function, 169
WRITE RANDOM function, 113

WRITE RANDOM WITH ZERO FILL
function, 122
WRITE SEQUENTIAL, 95

x

XCHG, 238

XFCB, 23

XFCB password mode,
XIOS, 8, 33

XLAT, 238

XOR, 244

149, 151

---
_Page 360_

8080 keyword, 53

8080 Memory Model, 45, 48
8080 Model, 49, 50, 63
96-byte initial stack, 43

---
_Page 361_

MP/M-86'™ Operating System

PROGRAMMERS GUIDE

Corrections to the First Printing - September 1981

Compiled October 5, 1981

PAGE 19

Figure 2-1. File Control Block Format

bytes 13 and 14 are labelled
change:

-»- Jex|sl]s2|re| ...

to:

--- jexles|rs|rc] ...

12 13 14 15

PAGE 93

Function 20: READ SEQUENTIAL

in fiqure near end of page
change:

255 : Physical Error;
to:

255 : Physical Error;

PAGE 112

Function 33: READ RANDOM

in fiqure near top of page
change:

255 : Physical Error;
to:

255 : Physical Error;

wrong.

refer

refer

refer

refer

to

to

to

to

register

register

register

register

AH

AH

---
_Page 362_

PAGE 114

Function 34: WRITE RANDOM

in figure near top of page
change:

255 : Physical Error; refer to register H
tos

255 : Physical Error; refer to register AH

PAGE 176

Function 144: CREATE PROCESS

The figure near the top describing process
priorities should be as follows:

1 Initialization Process
2 - 31 Interrupt handlers
32 - 63 System Processes
64 - 189 Undefined
190 RSP Initialization
191 - 196 Undefined :
197 MPMSTAT
198 Terminal Message Proccess
135 Underined
200 Default Priority
201 - 254 User Processes
255 Idle Process

---
_Page 363_

SOFTWARE CHANGES IN ASM-86 “AND ppT-86™

ASM-86

1. Forward references in EQU’s are flagged as errors.

2. A ! in a comment is ignored, comments extend to the
physical end of line.

3. New directives: IFLIST and NOIFLIST are to control listing
of false IF blocks.

4. IF directives may be nested to 5 levels.

5. New mnemonics implemented:
a. JC, JNC
b. CMPSB, CMPSW, LODSB, LODSW, MOVSB, MOVSW,

SCASB, SCASW, STOSB, STOSW

6. JNBE implemented correctly.

7. Segment override prefix is allowed in source operand of
string instructions.

8. Relational operators in expressions return OFFFFH if true.

9. Abort if invalid command tail encountered.

10. Abort if symbol table overflows.

ll. Abort if disk or directory full.

12. Incomplete string flagged as error (no terminating quote).

13. Error reported if an invalid numeric quantity appears in
EQU directive.

14. Source files are opened in RO mode for multiple access
under MP/M-86.

15. Format of .LST file:

a. form feed at start of file

b. no form feed at end of file

c. no cr,lf at top of each page

d. fewer lines per page

€. spaces between hex bytes deleted to allow more space
for comments

f. errors printed when NOLIST active

g. absolute address field for relative instructions

---
_Page 364_

16. Format of .SYM file:
a. form feed at start of file
b. symbols alphabetized within groups
c. tabs expanded if symbols sent to printer ($SY)

17. Include files:
a. file type defaults to .A86
b. file type may have fewer than three characters
c. abort if include file not found
d. default to same drive as source when $a switch used

18. Programs with INCLUDE directives will assemble correctly
under CP/M 1.4.

19. About 5.5K more space available for symbol table.

20. Use factor indicated at end of assembly (% usage of symbol
table space).

21. Runs somewhat faster (especially with $PZ switch).

1. User programs default to CCP stack, rather than local
stack in DDT86.

2. A command line starting with a “;\* is treated as a comment.

3. Interrupts are disabled while a single instruction is being
traced.

4. BDOS error mode is set to return BDOS errors for MP/M-86.
5. Files are closed after reading and loading for MP/M-86.

6. New Block Compare function implemented, with the same
command form as the move function.

