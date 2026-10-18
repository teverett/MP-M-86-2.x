# Intro to CPM Feat and Facilities

> OCR text extracted from `Intro_to_CPM_Feat_and_Facilities.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `Intro_to_CPM_Feat_and_Facilities.pdf`.

---

---
_Page 01_

lt DIGITAL AbSEARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

AN INTRODUCTION TO CP/M FEATURES AND FACILITIES

COPYRIGHT (c) 1976, 1977, 1978

DIGITAL RESEARCH

REVISION OF JANUARY 1978

---
_Page 02_

Copyright (c) 1976, 1977, 1978 by Digital Research. All rights
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

WW DIGITAL RESEARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

AN INTRODUCTION TO CP/M FEATURES AND FACILITIES

COPYRIGHT (c) 1976, 1977, 1978
DIGITAL RESEARCH

REVISION OF JANUARY 1978

---
_Page 04_

Copyright (c) 1976, 1977, 1978 by Digital Research. All rights re-
served. No part of this publication may be reproduced, transmitted,
transcribed, stored in a retrieval system, or translated into any language
or computer language, in any form or by any means, electronic,
mechanical, magnetic optical, chemical, manual or otherwise, without
the prior written permission of Digital Research, Post Office Box 579,
Pacific Grove, California 93950.

Disclaimer

Digital Research makes no representations or warranties with respect to
the contents hereof and specifically disclaims any implied warranties of
merchantability or fitness for any particular purpose. Further, Digital
Research reserves the right to revise this publication and to make
changes from time to time in the content hereof without obligation of
Digital Research to notify any person of such revision or changes.

---
_Page 05_

Section

Table of Contents

INTRODUCTION eeseeeeeeenaeaeceseeaaeeseeaecennseeaaneaeseeeaeed

FUNCTIONAL DESCRIPTION OF CP/M TO RRR ERRRERERERERERERERERE SE)
2.1. General Command Structure ,.ccccoceccccccccccce
Ze2e File References TOR eeee eR ERR RERRERERERERRERERERER ERE]

SWITCHING DISKS Seer eecsesesasesessesnseeseaseneesecs

HE FORM OF BUILT-IN COMMANDS eseeeeeeeeaseeeseseaoana

e ERA afn cr (RR RR RRR RRR RRERERERERERERRERRSEER ERR SEERA

DIR afn cr @@eevecveevoeaeeseaeseseeeaeeoeeeeonaeeaeana sess

REN ufnl=ufn2 cr S@e@eeeeaseeeaaaeeaeseaceaeeeaeeaeaeanaeeee
SAVE n ufn cr TR RRR RRR RERRERRERRERRERSSE ERR
e TYPE ufn cr @eeeceonaseaseseseeeeseneasaeneseeenaanan

T

4.1
4.2
4.3
4.4
4.5

LINE EDITING AND OUTPUT CONTROL, scccccccccccccccccce

TRANSIENT COMMANDS (RR RRR ERRERRRERRERRERRERRSRR ERE EES)

STAT cr eeeeveaseseeceaaeeeezvegeeneescoaaaeaeeeeeeeeeaaened

ASM uffn cr @eeeeocoaoaveeoaoseeaaeeeevenaceoaneaeseaneeenea
LOAD ufn cr (RRR RRR RRO RRR RRERRERRRRRR ERR RR)

PIP cr (Raa R eRe Re RRR RRR RRR ERR RRR RR RR RRR REE RE )

ED UfN CY wccccccccccccccccccccscccescnsescece
SYSGEN cr ([RRaRERRSRERRERRERRERRARERR ERR ERR RE RE RE ERERERRE ER EF
SUBMIT ufn parm#1 ... PALM#N CL Leccccccccccce
DUMP ufn cr (RR RRR RRR RRR R RR RRSRRRRRRRERR RRR EERE SE
MOVCPM cr (RR RR RER ER ER ERR ERRE RE REREREREREREER RE REE ERE I

DAKHAAAAGAACAS)
e
OD~AID UI hWNH EF
e

BDOS ERROR MESSAGES @eeeaseeceeooeoovnnaeeoaesnaeaeaeaeeeeeaenead

OPERATION OF CP/M ON THE MDS @eeoeanveaeeeeeeoanvnaaaanegane

Page

WWW

fon)

oO DO ~] ~I)

---
_Page 06_

---
_Page 07_

1. INTRODUCTION.

CP/M is a monitor control program for microcomputer system development
which uses IBM-compatible flexible disks for backup storage, Using a computer
Mainframe based upon Intel’s 8688 microcomputer, CP/M provides a general
environment for program construction, storage, and editing, along with
assembly and program check-out facilities, An important feature of CP/M is
that it can be easily altered to execute with any computer configuration which
uses an Intel 8988 (or Zilog Z-8@) Central Processing Unit, and has at least
16K bytes of main memory with up to four IBM-compatible diskette drives. A
detailed discussion of the modifications reguired for any particular hardware
environment is given in the Digital Research document entitled "CP/M System
Alteration Guide." Although the standard Digital Research version operates on
a single-density Intel MDS 880, several different hardware manufacturers
support their own input-output drivers for CP/M.

The CP/M monitor provides rapid access to programs through a
comprehensive file management package. The file subsystem supports a named
file structure, allowing dynamic allocation of file space as well as
sequential and random file access, Using this file system, a large number of
distinct programs can be stored in both source and machine executable form.

CP/M also supports a powerful context editor, Intel-compatible assembler,
and debugger subsystems. Optional software includes a _ powerful
Intel-compatible macro assembler, symbolic debugger, along with various
high-level languages, When coupled with CP/M’s Console Command Processor, the
resulting facilities equal or excel similar large computer facilities,

CP/M is logically divided into several distinct parts:

BIOS Basic I/O System (hardware dependent)
BDOS Basic Disk Operating System

CCP Console Command Processor

TPA Transient Program Area

The BIOS provides the primitive operations necessary to access the
diskette drives and to interface standard peripherals (teletype, CRI, Paper
Tape Reader/Punch, and user-defined peripherals), and can be tailored by the
user for any particular hardware environment by "patching" this portion of
CP/M, The BDOS provides disk management by controlling one or more disk
drives containing independent file directories, The BDOS implements disk
allocation strategies which provide fully dynamic file construction while
minimizing head movement across the disk during access, Any particular file
may contain any number of records, not exceeding the size of any single disk.
In a standard CP/M system, each disk can contain up to 64 distinct files. The

---
_Page 08_

BDOS has entry points which include the following primitive operations which
can be programmatically accessed:

SEARCH Look for a particular disk file by name.

OPEN Open a file for further operations,

CLOSE Close a file after processing,

RENAME Change the name of a particular file,

READ Read a record from a particular file,

WRITE Write a record onto the disk.

SELECT Select a particular disk drive for further
operations,

The CCP provides symbolic interface between the user’s console and the
remainder of the CP/M system, The CCP reads the console device and processes
commands which include listing the file directory, printing the contents of
files, and controlling the operation of transient programs, such as
assemblers, editors, and debuggers. The standard commands which are available
in the CCP are listed in a following section.

The last segment of CP/M is the area called the Transient Program Area
(TPA). The TPA holds programs which are loaded from the disk under command of
the CCP. During program editing, for example, the TPA holds the CP/M text
editor machine code and data areas, Similarly, programs created under CP/M
can be checked out by loading and executing these programs in the TPA,

It should be mentioned that any or all of the CP/M component subsystems
can be "overlayed" by an executing program. That is, once a user’s program is
loaded into the TPA, the CCP, BDOS, and BIOS areas can be used as the
program’s data area, A “bootstrap” loader is programmatically accessible
whenever the BIOS portion is not overlayed; thus, the user program need only
branch to the bootstrap loader at the end of execution, and the complete CP/M
monitor is reloaded from disk,

It should be reiterated that the CP/M operating system is partitioned
into distinct modules, including the BIOS portion which defines the hardware
environment in which CP/M is executing, Thus, the standard system can be
easily modified to any non-standard environment by changing the peripheral
drivers to handle the custom system.

---
_Page 09_

2. FUNCTIONAL DESCRIPTION OF CP/M.

The user interacts with CP/M primarily through the CCP, which reads and
interprets canmands entered through the console, In general, the CCP
addresses one of several disks which are online (the standard system addresses
up to four different disk drives). These disk drives are labelled A, B, C,
and D. A disk is “logged in" if the CCP is currently addressing the disk, In
order to clearly indicate which disk is the currently logged disk, the CCP
always pranpts the operator with the disk name followed by the symbol ">"
indicating that the CCP is ready for another command. Upon initial start up,
the CP/M system is brought in from disk A, and the CCP displays the message

XXK CP/M VER mm

where xx is the memory size (in kilobytes) which this CP/M system manages, and
m.m is the CP/M version number, All CP/M systems are initially set to operate
in a 16K memory space, but can be easily reconfigured to fit any memory size
on the host system (see the MOVCPM transient canmand). Following system
signon, CP/M automatically logs in disk A, prompts the user with the symbol
"A>" (indicating that CP/M is currently addressing disk "A"), and waits for a
command, The cammands are implemented at two levels: built-in commands and
transient canmands,

2el. GENERAL COMMAND STRUCTURE.
Built~in canmands are a part of the CCP program itself, while transient

commands are loaded into the TPA from disk and executed, The built-in
commands are

ERA Erase specified files,

DIR List file names in the directory.

REN Rename the specified file,

SAVE Save memory contents in a file,

TYPE Type the contents of a file on the logged disk.

Nearly all of the canmands reference a particular file or group of files. The
form of a file reference is specified below.

2.2. FILE REFERENCES,

A file reference identifies a particular file or group of files on a
particular disk attached to CP/M, These file references can be either
"unambiguous" (ufn) or "“ambiguous" (afn). An unambiguous file reference
uniquely identifies a single file, while an ambiguous file reference may be

---
_Page 10_

satisfied by a number of different files.

File references consist of two parts: the primary name and the secondary
name. Although the secondary name is optional, it usually is generic; that
is, the secondary nane "ASM," for example, is used to denote that the file is
an assembly language source file, while the primary name distinguishes each
particular source file, The two names are separated by a "." as shown below:

PPPPPPPP. SSS

where pppppppp represents the primary name of eight characters or less, and
sss is the secondary name of no more than three characters, As mentioned
above, the name

PPPPPPPP

is also allowed and is equivalent to a secondary name consisting of three
blanks. The characters used in specifying an wunambiguous file reference
cannot contain any of the special characters

<>e re t= 2?\* CT]
while all alphanumerics and remaining special characters are allowed.

An ambiguous file reference is used for directory search and pattern
matching. The form of an ambiguous file reference is similar to an
unambiguous reference, except the symbol "?" may be interspersed throughout
the primary and secondary names, In various cammands throughout CP/M, the °?"
symbol matches any character of a file name in the "?" position, Thus, the
ambiguous reference

X?Z .C?M
is satisfied by the unambiguous file names
XYZ .COM
and
X3Z ,.CAM
Note that the anbiguous reference
x x
is equivalent to the anbiguous file reference

2222222727 .272?

while

---
_Page 11_

PPpppppp.\*
and

\* sss
are abbreviations for

PPPPPPPP. ???
and
222222??,SSS
respectively, As an example,
DIR ®

is interpreted by the CCP as a command to list the names of all disk files in
the directory, while

DIR X.Y
searches only for a file by the name X.Y Similarly, the canmand
DIR X?Y.C?M

causes a search for all (unambiguous) file names on the disk which satisfy
this ambiguous reference.

The following file names are valid wmambiguous file references:
x XYZ GAMMA
X.Y XYZ .COM GAMMA ,1
As an added convenience, the programmer can generally specify the disk
drive name along with the file name. In this case, the drive name is given as
a letter A through Z followed by a colon (:). The specified drive is then
“logged in” before the file operation occurs, Thus, the following are valid
file names with disk name prefixes:
A:X.Y BsxXYZ C:GAMMA
Z2XY¥Z COM B:X.A?M C:\* ASM
It should also be noted that all alphabetic lower case letters in file

and drive names are always translated to upper case when they are processed by
the CCP,

---
_Page 12_

3. SWITCHING DISKS,

The operator can switch the currently logged disk by typing the disk
drive nane (A, B, C, or D) followed by a colon (:) when the CCP is waiting for
console input. Thus, the sequence of prompts and commands shown below might
occur after the CP/M system is loaded fram disk A:

16K CP/M VER 1.4

A>DIR List all files on disk A,
SAMPLE ASM

SAMPLE PRN

ADB: Switch to disk B.

B>DIR \*,ASM List all “ASM” files on B,
DUMP ASM

FILES ASM

BoA: Switch back to A,

---
_Page 13_

4, THE FORM O@ BUILT-IN OOMMANDS,

The file and device reference forms described above can now be used to
fully specify the structure of the built-in canmands,. In the description
below, assume the following abbreviations:

ufn - unambiguous file reference
afn - ambiguous file reference
cr - carriage return

Further, recall that the CCP always translates lower case characters to upper
case characters internally. Thus, lower case alphabetics are treated as if
they are upper case in canmand names and file references,

4.1 ERA afn cr

The ERA (erase) command removes files from the currently logged-in disk
(i.e., the disk name currently prompted by CP/M preceding the ">"). The files
which are erased are those which satisfy the ambiguous file reference afn,
The following examples illustrate the use of ERA:

ERA X.Y The file named X.Y on the currently logged disk
is removed fram the disk directory, and the space
is returned,

ERA X,\* All files with primary name X are removed from
the current disk,

ERA \*,ASM All files with secondary name ASM are removed
fram the current disk,

ERA X?Y,C?M All files on the current disk which satisfy the
ambiguous reference X?Y,C?M are deleted,

ERA \*,\* Erase all files on the current disk (in this case
the CCP prompts the console with the message
"ALL FILES (Y/N) ?"
which requires a Y response before files are
actually removed).

ERA B:\*, PRN All files on drive B which satisfy the ambiguous
reference ??2?7??22?.PRN are deleted, independently
of the currently logged disk.

---
_Page 14_

4.2. DIR afn cr
The DIR (directory) command causes the names of all files which satisfy
the anbiguous file name afn to be listed at the console device. As a special
case, the canmand
DIR

lists the files on the currently logged disk (the cammand "DIR" is equivalent
to the canmand "DIR \*.\*"). Valid DIR commands are shown below.

DIR X.Y

DIR X?Z.C?M

DIR ??.Y

Similar to other CCP commands, the afn can be preceded by a drive name,

The following DIR cammands cause the selected drive to be addressed before the
directory search takes place,

DIR B:

DIR B:X.Y

DIR B:\*,A?M

If no files can be found on the selected diskette which satisfy the
directory request, then the message "NOT FOUND" is typed at the console,

4.3. REN ufnl=ufm2 cr

The REN (rename) command allows the user to change the names of files on
disk, The file satisfying ufn2 is changed to ufnl. The currently logged disk
is assumed to contain the file to rename (ufnl)., The CCP also allows the user
to type a left-directed arrow instead of the equal sign, if the user’s console
supports this graphic character, Examples of the REN command are

REN X.Y=Q.R The file Q.R is changed to X.Y.
REN XYZ .COM=XYZ .XXX The file XYZ.XXX is changed to XYZ.COM.

The operator can precede either ufnl or ufn2 (or both) by an optional
drive address. Given that ufnl is preceded by a drive name, then ufn2 is
assumed to exist on the same drive as ufml. Similarly, if ufm2 is preceded by
a drive name, then ufnl is assumed to reside on that drive as well. If both
ufnl and ufn2 are preceded by drive names, then the same drive must be

---
_Page 15_

specified in both cases, The following REN commands illustrate this format,

REN A:X,ASM = Y.ASM The file Y.ASM is changed to X.ASM on
Grive A,

REN B:ZAP,BAS=ZOT, BAS The file ZOT.BAS is changed to ZAP,BAS
on drive B.

REN B:A,ASM = B:A,BAK The file A.,BAK is renamed to A.ASM on
drive B.

If the file ufnl is already present, the REN command will respond with
the error "FILE EXISTS" and not perform the change, If ufn2 does not exist on
the specified diskette, then the message “NOI FOUND" is printed at the
console,

4.4. SAVE n uf cr
The SAVE cammand places n pages (256-byte blocks) onto disk from the TPA
and names this file ufm. In the CP/M distribution system, the TPA starts at
1@@H (hexadecimal), which is the second page of memory. Thus, if the user’s
program occupies the area from 1@9H through 2FFH, the SAVE command must
specify 2 pages of memory. The machine code file can be subsequently loaded
and executed. Examples are:
SAVE 3 X.COM Copies 1@0H through 3FFH to X.COM,
SAVE 48 Q Copies 198@H through 28FFH to Q (note
that 28 is the page count in 28FFH,
and that 28H = 2\*16+8 = 46 decimal).
SAVE 4 X.Y Copies 100H through 4FFH to X.Y.

The SAVE command can also specify a disk drive in the afn portion of the
command, as shown below,

SAVE 18 B:ZOT,COM Copies 18 pages (10@H through GAFFH) to
the file ZOT.COM on drive B.

4.5. TYPE ufn cr

The TYPE command displays the contents of the ASCII source file ufn on
the currently logged disk at the console device. Valid TYPE commands are

TYPE X.Y

---
_Page 16_

TYPE X.PLM
TYPE XXX
The TYPE canmand expands tabs (clt-I characters), assumming tab positions
are set at every eighth colum, The ufn can also reference a drive name as
shown below.

TYPE B:X.PRN The file X.PRN from drive B is displayed.

---
_Page 17_

5. LINE EDITING AND OUTPUT OONTROL.

The CCP allows certain line editing functions while typing commamd lines.

rubout

ctl-U
ctl-x

ctl-R

ctl-E

ctl-C
ctl=Z

Delete and echo the last character typed at the
console,

Delete the entire line typed at the console,
(Same as ctl-U)

Retype current canmand line: types a "clean line" fol-
lowing character deletion with rubouts,

Physical end of line: carriage is returned, but line
is not sent until the carriage return key is depressed,

CP/M system reboot (warm start)

End input from the console (used in PIP and ED).

The control functions ctl-P and ctl-S affect console output as shown below.

ctl-P

ctl-S

Copy all subsequent console output to the currently
assigned list device (see the STAT command). Output
is sent to both the list device and the console device
until the next ctl-P is typed.

Stop the console output temporarily. Program execution
and output continue when the next character is typed
at the console (e.g., another ctl-S), This feature is
used to stop output on high speed consoles, such as
CRI’s, in order to view a segment of output before con-
tinuing.

Note that the ctl~key sequences shown above are obtained by depressing the
control and letter keys simultaneously. Further, CCP command lines can
generally be up to 255 characters in length; they are not acted upon until the
carriage return key is typed,

ll

---
_Page 18_

6. TRANSIENT COMMANDS.

Transient canmands are loaded from the currently logged disk and executed
in the TPA, The transient cammands defined for execution under the CCP are
shown below, Additional functions can easily be defined by the user (see the
LOAD command definition).

STAT List the number of bytes of storage remaining on the
currently logged disk, provide statistical information
about particular files, and display or alter device
assignment,

ASM Load the CP/M assembler and assemble the specified
program from disk,

LOAD Load the file in Intel “hex” machine code format and
produce a file in machine executable form which can be
loaded into the TPA (this loaded program becomes a
new command under the CCP),

DDT Load the CP/M debugger into TPA and start execution,

PIP Load the Peripheral Interchange Program for subsequent
disk file and peripheral transfer operations,

ED Load and execute the CP/M text editor program.

SYSGEN Create a new CP/M system diskette,

SUBMIT Submit a file of commands for batch processing.

DUMP Dump the contents of a file in hex,

MOVCPM Regenerate the CP/M system for a particular memory
size,

Transient commands are specified in the same manner as built-in commands, and
additional canmands can be easily defined by the user, As an added
convenience, the transient command can be preceded by a drive name, which
causes the transient to be loaded from the specified drive into the TPA for
execution. Thus, the command

B:STAT

causes CP/M to temporarily "log in" drive B for the source of the STAT
transient, and then return to the original logged disk for subsequent
processing.

---
_Page 19_

The basic transient canmands are listed in detail below.
6.l. STAT cr

The STAT command provides general statistical information about file
storage and device assignment. It is initiated by typing one of the following
forms:

STAT cr
STAT “command line” cr

Special forms of the "command line" allow the current device assignment to be
examined and altered as well. The various canmand lines which can be
specified are shown below, with an explanation of each form shown to the
right,

STAT cr If the user types an empty command line, the STAT
transient calculates the storage remaining on all
active drives, and prints a message

x: R/W, SPACE: nnnk
or
x: R/O, SPACE: nnnK

for each active drive x, where R/W indicates the
drive may be read or written, and R/O indicates
the drive is read only (a drive becomes R/O by
explicitly setting it to read only, as shown
below, or by inadvertantly changing diskettes
without performing a warm start). The space
remaining on the diskette in drive x is given
in kilobytes by nnn,

STAT x: cr If a drive name is given, then the drive is
selected before the storage is computed, Thus,
the command "STAT B:" could be issued while
logged into drive A, resulting in the message

BYTES REMAINING ON Bs: nnnK
STAT afn cr The command line can also specify a set of files
to be scanned by STAT. The files which satisfy
afn are listed in alphabetical order, with stor-
age requirements for each file under the heading

RECS BYTS EX D: FILENAME,TYP
rrrr bbbK ee d:pppppppp.sss

where rrrr is the number of 128-byte records

---
_Page 20_

allocated to the file, bbb is the number of kilo-
bytes allocated to the file (bbb=rrrr\*128/1924),
ee is the numer of 16K extensions (ee=bbb/16),

da is the drive name containing the file (A...2),
ppoppppp is the (up to) eight-character primary
file name, and sss is the (up to) three-character
secondary name, After listing the individual
files, the storage usage is summarized,

STAT x:afn cr As a convenience, the drive name can be given
ahead of the atn, In this case, the specified
drive is first selected, and the form "STAT afn"
is executed.

STAT x:=R/0 cr This form sets the drive given by x to read-only,
which remains in effect until the next warm or
cold start takes place. When a disk is read-only,
the message

BDOS ERR ON x: READ ONLY

will appear if there is an attempt to write to
the read-only disk x. CP/M waits until a key

is depressed before performing an automatic warm
start (at which time the disk becomes R/W).

The STAT command also allows control over the physical to logical device
assignment (see the IOBYTE function described in the manuals "CP/M Interface
Guide" and "CP/M System Alteration Guide"). In general, there are four
logical peripheral devices which are, at any particular instant, each assigned
to one of several physical peripheral devices, The four logical devices are
named:

CON: The system console device (used by CCP
for communication with the operator)

RDR: The paper tape reader device

PUN: The paper tape punch device

LST: The output list device

The actual devices attached to any particular computer system are driven
by subroutines in the BIOS portion of CP/M. ‘Thus, the logical RDR: device,
for example, could actually be a high speed reader, Teletype reader, or
cassette tape. In order to allow some flexibility in device naming and
assignment, several physical devices are defined, as shown below:

---
_Page 21_

TTY: Teletype device (slow speed console)

CRT: Cathode ray tube device (high speed console)

BAT: Batch processing (console is current RDR:,
output goes to current LST: device)

UC1: User-defined console

PTR: Paper tape reader (high speed reader)

UR1: User-defined reader #1

UR2: User-defined reader #2

PIP: Paper tape punch (high speed punch)

UPL: User-defined punch #1

UP2: User-defined punch #2

LPT: Line printer

ULL: User-defined list device #1

It must be emphasized that the physical device names may or may not
actually correspond to devices which the names imply. That is, the PIP:
device may be implemented as a cassette write operation, if the user wishes,
The exact correspondence and driving subroutine is defined in the BIOS portion
of CP/M, In the standard distribution version of CP/M, these devices
correspond to their names on the MDS 888 development system.

The possible logical to physical device assignments can be displayed by
typing
STAT VAL: cr

The STAT prints the possible values which can be taken on for each logical
device:

CON. = TTY: CRI: BAT: UC1L:
RDR: = TTY: PYR: URI]: UR2:
PUN: = TTY: PIP: UPl: UP2:
LST: = TTY: CRY: LPT: UL:

In each case, the logical device shown to the left can take any of the four
physical assignments shown to the right on each line, The current logical to
physical mapping is displayed by typing the command

STAT DEV: cr

---
_Page 22_

which produces a listing of each logical device to the left, and the current
corresponding physical device to the right. For example, the list might
appear as follows:

CON: = CR:
ROR: = URI]:
PUN: = PIP:
LST: = TTY:

The current logical to physical device assignment can be changed by typing a
STAT command of the form

STAT 1dl1 = pdl, 142 = pd2 , ... , 1dn = pdn cr
where ldl through ldn are logical device names, and pdl through pdn are
compatible physical device names (i.e., ldi and pdi appear on the same line in
the "VAL:" canmand shown above). The following are valid STAT commands which
change the current logical to physical device assignments:

STAT CON:=CRTI: cr
STAT PUN: = TTY:,LST:=LPT:, RDR:=TTY: cr

6.2. ASM uf cr
The ASM command loads and executes the CP/M 8888 assembler, The ufn
specifies a source file containing assembly language statements where the
secondary name is assumed to be ASM, and thus is not specified. The following
ASM canmands are valid:
ASM X
ASM GAMMA

The two-pass assembler is automatically executed. If assembly errors occur
during the second pass, the errors are printed at the console,

The assembler produces a file
X.PRN
where x is the primary name specified in the ASM command, The PRN file
contains a listing of the source program (with imbedded tab characters if

present in the source program), along with the machine code generated for each
statement and diagnostic error messages, if any. The PRN file can be listed

---
_Page 23_

at the console using the TYPE command, or sent to a peripheral device using
PIP (see the PIP command structure below). Note also that the PRN file
contains the original source program, augmented by miscellaneous assembly
information in the leftmost 16 colums (program addresses and hexadecimal
machine code, for example). Thus, the PRN file can serve as a backup for the
original source file: if the source file is accidently removed or destroyed,
the PRN file can be edited (see the ED operator’s guide) by removing the
leftmost 16 characters of each line (this can be done by issuing a single
editor "macro" cammand). The resulting file is identical to the original
source file and can be renamed (REN) from PRN to ASM for subsequent editing
and assembly. The file

Xx HEX

is also produced which contains 8888 machine language in Intel “hex" format
Suitable for subsequent loading and execution (see the LOAD command). For
complete details of CP/M’s assembly language program, see the "CP/M Assembler
Language (ASM) User’s Guide,”

Similar to other transient commands, the source file for assembly can be
taken fran an a.ternate disk by prefixing the assembly language file name by a
disk drive name. Thus, the command

ASM B:ALPHA cr
loads the assembler from the currently logged drive and operates upon the

source program ALPHA.ASM on drive B. The HEX and PRN files are also placed on
drive B in this case,

6.3. LOAD ufn cr
The LOAD cammand reads the file ufn, which is assumed to contain "hex"
format machine code, and produces a memory image file which can be
subsequently executed, The file name ufn is assumed to be of the form
x HEX

and thus only the name x need be specified in the command. The LOAD command
creates a file named

x.0OM
which marks it as containing machine executable code. The file is actually
loaded into memory and executed when the user types the file name x
immediately after the prampting character ">" printed by the CCP.
In general, the CCP reads the name x following the prompting character

and looks for a built-in function name. If no function name is found, the CCP
searches the system disk directory for a file by the name

---
_Page 24_

x COM

If found, the machine code is loaded into the TPA, and the program executes,
Thus, the user need only LOAD a hex file once; it can be subsequently
executed any number of times by simply typing the primary name. In this way,
the user can "invent" new commands in the CCP, (Initialized disks contain the
transient commands as COM files, which can be deleted at the user’s option.)
The operation can take place on an alternate drive if the file name is
prefixed by a drive name. Thus,

LOAD B:BETA

brings the LOAD program into the TPA from the currently logged disk and
operates upon adrive B after execution begins,

It must be noted that the BETA.HEX file must contain valid Intel format
hexadecimal machine code records (as produced by the ASM program, for example)
which begin at 19@H, the beginning of the TPA. Further, the addresses in the
hex records must be in ascending order; gaps in unfilled memory regions are
filled with zeroes by the LOAD command as the hex records are read. Thus,
LOAD must be used only for creating CP/M standard "COM" files which operate in
the TPA, Programs which occupy regions of memory other than the TPA can be
loaded under DDT.

6.4. PIP cr

PIP is the CP/M Peripheral Interchange Program which implements the basic
media conversion operations necessary to load, print, punch, copy, and combine
Gisk files, The PIP program is initiated by typing one of the following forms

(1) PIP cr
(2) PIP “command line" cr

In both cases, PIP is loaded into the TPA and executed, In case (1), PIP
reads canmand lines directly from the console, prompted with the "\*"
character, wntil an empty command line is typed (i.e., a single carriage
return is issued by the operator). Each successive command line causes some
media conversion to take place according to the rules shown below. Form (2)
of the PIP command is equivalent to the first, except that the single command
line given with the PIP command is automatically executed, and PIP terminates
immediately with no further prompting of the console for input command lines.
The form of each canmand line is

destination = source#l, source#2, ... , source#n cr

where "destination" is the file or peripheral device to receive the data, and

---
_Page 25_

"source#l, ..., Source#n” represents a series of one or more files or devices
which are copied fram left to right to the destination,

When multiple files are given in the command line (i.e, n> 1), the
individual files are assumed to contain ASCII characters, with an assumed CP/M
end-of-file character (ct]-Z) at the end of each file (see the O parameter to
override this assumption). The egual symbol (=) can be replaced by a
left-oriented arrow, if your console supports this ASCII character, to improve
readability. Lower case ASCII alphabetics are internally translated to upper
case to be consistent with CP/M file and device name conventions, Finally,
the total command line length cannot exceed 255 characters (ctl-E can be used
to force a physical carriage return for lines which exceed the console width).

The destination and source elements can be unambiguous references to CP/M
source files, with or without a preceding disk drive name. That is, any file
can be referenced with a preceding drive name (A:, B:, C:, or D:) which
defines the particular drive where the file may be obtained or stored. When
the drive name is not included, the currently logged disk is assumed,
Further, the destination file can also appear as one or more of the source
files, in which case the source file is not altered wtil the entire
concatenation is camplete, If the destination file already exists, it is
removed if the canmand line is properly formed (it is not removed if an error
condition arises). The following cammand lines (with explanations to the
right) are valid as input to PIP:

X=Yor Copy to file x from file y,
where X and Y are unambiquous
file names; Y remains unchanged,

X =Y,2 cr Concatenate files Y and Z and
copy to file xX, with Y and Z
unchanged,

X.ASM=Y.ASM,Z.ASM,FIN,ASM cr Create the file X.ASM from the

concatenation of the Y, Z, and
FIN files with type ASM,

NEW.ZOT = B:OLD,ZAP cr Move a copy of OLD,.ZAP from drive
B to the currently logged disk:
name the file NEW.ZOT.

B:A.U = BsB.V,A:C.W,D.X cr Concatenate file B.V from drive B
with C.W from drive A and D.X.
from the logged disk; create
the file A.U on drive B.

For more convenient use, PIP allows abbreviated commands for transferring
files between disk drives. The abbreviated forms are

---
_Page 26_

PIP x:=afn cr
PIP x:=y:afn cr
PIP ufn = y: cr
PIP x:umM = y: cr

The first form copies all files from the currently logged disk which satisfy
the afn to the same file names on drive x (x = A...Z). The second form is
equivalent to the first, where the source for the copy is drive y (y = A...
Zz). The third form is eguivalent to the command "PIP ufn=y:ufn cr" which
copies the file given by ufn from drive y to the file ufn on drive x. The
fourth form is equivalent to the third, where the source disk is explicitly
given by y.

Note that the source and destination disks must be different in all of
these cases, If an afn is specified, PIP lists each ufn which satisfies the
afn as it is being copied. If a file exists by the same name as the
destination file, it is removed upon successful completion of the copy, and
replaced by the copied file,

The following PIP commands give examples of valid disk-to-disk copy
operations:

B:=\* COM cr Copy all files which have the
secondary name “COM” to drive B
from the current drive,

A:=B:ZAP,\* cr Copy all files which have the
primary name "ZAP" to drive A
from drive B.

ZAP,ASM=B: cr Equivalent to ZAP,ASM=B:ZAP.ASM
B:ZOT, COM=A: cr Eguivalent to B:ZOT,COM=A:ZOT.COM
B:=GAMMA.BAS cr Same as B:GAMMA.BAS=GAMMA, BAS
B:=A:GAMMA,BAS cr Same as B:GAMMA.BAS=A:GAMMA,BAS

PIP also allows reference to physical and logical devices which are
attached to the CP/M system. The device names are the same as given under the
STAT command, along with a number of specially named devices. The logical
devices given in the STAT command are

CON: (console), RDR: (reader), PUN: (punch), and LST: (list)

while the physical devices are

---
_Page 27_

(console, reader, punch, or list)
(console, or list), UC1: (console)
(reader), UR]: (reader), UR2: (reader)
(punch), UPl: (punch), UP2: (punch)
LPT: (list), ULl: (list)

(Note that the "BAT:" physical device is not included, since this assignment
is used only to indicate that the RDR: and LST: devices are to be used for
console input/output.)

The RDR, LST, PUN, and CON devices are all defined within the BIOS
portion of CP/M, and thus are easily altered for any particular I/O system.
(The current physical device mapping is defined by IOBYTE; see the "CP/M
Interface Guide" for a discussion of this function), The destination device
must be capable of receiving data (i.e., data cannot be sent to the punch),
and the source devices must be capable of generating data (i.e., the LST:
device cannot be read).

The additional device names which can be used in PIP commands are

NUL: Send 4@ "nulls" (ASCII @°s) to the device
(this can be issued at the end of punched output).

EOF: Send a CP/M end-of-file (ASCII ct1-Z) to the
destination device (sent automatically at the
end of all ASCII data transfers through PIP).

INP: Special PIP input source which can be "patched"
into the PIP program itself: PIP gets the input
data character-by-character by CALLing location
183H, with data returned in location 1@9H (parity
bit must be zero),

OUT: Special PIP output destination which can be
patched into the PIP program: PIP CALLs location
166H with data in register C for each character
to transmit, Note that locations 109H through
1FFH of the PIP memory image are not used and
can be replaced by special purpose drivers using
DDI (see the DDI operator’s manual).

PRN: Same as LST:, except that tabs are expanded at
every eighth character position, lines are
numbered, and page ejects are inserted every 60
lines, with an initial eject (same as [t8np]).

File and device names can be interspersed in the PIP commands, In each
case, the specific device is read until end-of-file (ctl-Z for ASCII files,
and a real end of file for non-ASCII disk files). Data from each device or
file is concatenated from left to right until the last data source has been

---
_Page 28_

read. The destination device or file is written using the data from the
source files, and an end-of-file character (ctl-Z) is appended to the result
for ASCII files. Note if the destination is a disk file, then a temporary
file is created ($$$ secondary name) which is changed to the actual file name
only upon successful canpletion of the copy. Files with the extension "COM"
are always assumed to be non-ASCII,

The copy operation can be aborted at any time by depressing any key on
the keyboard (a rubout suffices). PIP will respond with the message "ABORTED"
to indicate that the operation was not completed. Note that if any operation
is aborted, or if an error occurs during processing, PIP removes any pending
commands which were set up while using the SUBMIT command,

It should also be noted that PIP performs a special function if the
destination is a disk file with type "HEX" (an Intel hex formatted machine
code file), and the source is an external peripheral device, such as a paper
tape reader, In this case, the PIP program checks to ensure that the source
file contains a properly formed hex file, with legal hexadecimal values and
checksum records, When an invalid input record is found, PIP reports an error
message at the console and waits for corrective action, It is usually
sufficient to open the reader and rerun a section of the tape (pull the tape
back about 29 inches). When the tape is ready for the re-read, type a single
carriage return at the console, and PIP will attempt another read. If the
tape position cannot be properly read, simply continue the read (by typing a
return following the error message), and enter the record manually with the ED
program after the disk file is constructed. For convenience, PIP allows the
end-of-file to be entered from the console if the source file is a RODR:
device, In this case, the PIP program reads the device and monitors the
keyboard. If ctl-Z is typed at the keyboard, then the read operation is
terminated normally.

Valid PIP cammands are shown below,

PIP LST: = X.PRN cr Copy X.PRN to the LST device and
terminate the PIP program,

PIP cr Start PIP for a sequence of
cammands (PIP prompts with "\*"),

\*QON:=X.ASM,Y.ASM,Z.ASM cr Concatenate three ASM files and
copy to the CON device,

\*X HEX=CON: , Y.HEX,PTR: cr Create a HEX file by reading the
COON (until a ctl-Z is typed), fol-
lowed by data from Y.HEX, followed
by data from PIR until a ctl-Z is
encountered,

\*or Single carriage return stops PIP,

---
_Page 29_

PIP PUN:=NUL: ,X.ASM,EOF:,NUL: cr Send 4@ nulls to the punch device;
then copy the X.ASM file to the
punch, followed by an end-of-file
(ctl-Z) and 4@ more null charac-
ters,

The user can also specify one or more PIP parameters, enclosed in left
and right square brackets, separated by zero or more blanks, Each parameter
affects the copy operation, and the enclosed list of parameters must
immediately follow the affected file or device, Generally, each parameter can
be followed by an optional decimal integer value (the S and Q parameters are
exceptions), The valid PIP parameters are listed below.

B Block mode transfer: data is buffered by PIP until an ASCII
x-off character (ctl-S) is received from the source device.
This allows transfer of data to a disk file from a continuous
reading device, such as a cassette reader. Upon receipt of
the x-off, PIP clears the disk buffers and returns for more
input data, The amount of data which can be buffered is de-
pendent upon the memory size of the host system (PIP will
issue an error message if the buffers overflow).

Dn Delete characters which extend past column n in the transfer
of data to the destination from the character source, This
parameter is used most often to truncate long lines which are
sent to a (narrow) printer or console device.

E Echo all transfer operations to the console as they are being
performed.
F Filter form feeds from the file, All imbedded form feeds are

removed, The P parameter can be used simultaneously to
insert new form feeds,

H Hex data transfer: all data is checked for proper Intel hex
file format. Non-essential characters between hex records
are removed during the copy operation. The console will be
prompted for corrective action in case errors occur,

I Ignore ":88" records in the transfer of Intel hex format
file (the I parameter automatically sets the H parameter),

L Translate upper case alphabetics to lower case,

N Add line numbers to each line transferred to the destination
starting at one, and incrementing by 1. Leading zeroes are
suppressed, and the number is followed by a colon, If N2
is specified, then leading zeroes are included, and a tab is
inserted following the number, The tab is expanded if T is

---
_Page 30_

set.

‘e) Object file (non-ASCII) transfer: the normal CP/M end of
file is ignored,

Pn Include page ejects at every n lines (with an initial page
eject). If n= 1 or is excluded altogether, page ejects
occur every 6@ lines, If the F parameter is used, form feed
suppression takes place before the new page ejects are
inserted.

Qstz Quit copying from the source device or file when the
string s (terminated by ctl-Z) is encountered,

Sstz Start copying from the source device when the string s is
encountered (terminated by ctl-Z). The S and Q parameters
can be used to “abstract” a particular section of a file
(such as a subroutine). The start and quit strings are al-
ways included in the copy operation,

NOTE - the strings following the s and gq parameters are
translated to upper case by the CCP if form (2) of the
PIP command is used. Form (1) of the PIP invocation, how-
ever, does not perform the automatic upper case translation,
(1) PIP cr
(2) PIP “command line” cr

Tn Expand tabs (ctl-I characters) to every nth colum during the
transfer of characters to the destination from the source,

U Translate lower case alphabetics to upper case during the
the copy operation.

Vv Verify that data has been copied correctly by rereading
after the write operation (the destination must be a disk
file).

Z Zero the parity bit on input for each ASCII character,

The following are valid PIP commands which specify parameters in the file
transfer:

PIP X.ASM=B: [v] cr Copy X.ASM from drive B to the current drive
and verify that the data was properly copied.

PIP LPT:=X.ASM [nt8u] cr Copy X.ASM to the LPT: device; number each
line, expand tabs to every eighth column, and
translate lower case alphabetics to upper
case,

---
_Page 31_

PIP PUN:=X.HEX[i] ,Y.ZOT[h] cr First copy X.HEX to the PUN: device and
ignore the trailing ":80" record in X.HEX;
then continue the transfer of data by reading
Y.ZOT, which contains hex records, including
any ":8%" records which it contains,

PIP X.LIB = Y.ASM [ SSUBR1:\*z qJMP L3?z ] cr Copy from the file Y,ASM
into the file X.LIB. Start the copy when the
string "SUBR1:" has been found, and quit copy-
ing after the string “JMP L3" is encountered,

PIP PRN:=X.ASM [p59] Send X,ASM to the LST: device, with line num
bers, tabs expanded to every eighth column,
and page ejects at every 5@th line. Note that
nt8p6@ is the assumed parameter list for a PRN
file; p5@ overrides the default value.

6.5. ED ufn cr

The ED program is the CP/M system context editor, which allows creation
and alteration of ASCII files in the CP/M environment. Complete details of
operation are given the ED user’s manual, "ED: a Context Editor for the CP/M
Disk System." In general, ED allows the operator to create and operate upon
source files which are organized as a sequence of ASCII characters, separated
by end-of-line characters (a carriage-return line-feed seguence). There is no
practical restriction on line length (no single line can exceed the size of
the working memory), which is instead defined by the number of characters
typed between cr’s. The ED program has a number of commands for character
string searching, replacement, and insertion, which are useful in the creation
and correction of programs or text files under CP/M. Although the CP/M has a
limited memory work space area (approximately 5800 characters in a 16K CP/M
system), the file size which can be edited is not limited, since data is
easily “paged” through this work area.

Upon initiation, ED creates the specified source file, if it does not
exist, and opens the file for access, The programmer then "appends" data from
the source file into the work area, if the source file already exists (see the
A canmand), for editing. The appended data can then be displayed, altered,
and written from the work area back to the disk (see the W command).
Particular points in the program can be automatically paged and located by
context (see the N command), allowing easy access to particular portions of a
large file,

Given that the operator has typed

ED X.ASM cr

---
_Page 32_

the ED program creates an intermediate work file with the name

X.$S$

to hold the edited data during the ED run. Upon completion of ED, the X.ASM
file (original file) is renamed to X.BAK, and the edited work file is renamed
to X.ASM. Thus, the X.BAK file contains the original (unedited) file, and the
X.ASM file contains the newly edited file. The operator can always return to
the previous version of a file by removing the most recent version, and
renaming the previous version, Suppose, for example, that the current X,ASM
file was improperly edited; the seguence of CCP command shown below would
reclaim the backup file.

DIR X,\* Check to see that BAK file
is available,

ERA X.ASM Erase most recent version,

REN X,ASM=X,. BAK Rename the BAK file to ASM,

Note that the operator can abort the edit at any point (reboot, power failure,
ctl-C, or Q canmand) without destroying the original file. In this case, the
BAK file is not created, and the original file is always intact,

The ED program also allows the user to "ping-pong" the source and create
backup files between two disks. The form of the ED command in this case is

ED ufn da:

where ufn is the name of a file to edit on the currently logged disk, and d is
the name of an alternate drive, The ED program reads and processes the source
file, and writes the new file to drive d, using the name ufn. Upon campletion
of processing, the original file becomes the backup file, Thus, if the
operator is addressing disk A, the following command is valid:

ED X.ASM Bs:

which edits the file X.ASM on drive A, creating the new file xX.$$S on drive
B. Upon canpletion of a successful edit, A:X.ASM is renamed to A:X.BAK, and
B:X.$$$ is renamed to B:X.ASM. For user convenience, the currently logged
disk becomes drive B at the end of the edit, Note that if a file by the name
B:X.ASM exists before the editing begins, the message

FILE EXISTS
is printed at the console as a precaution against accidently destroying a

source file, In this case, the operator must first ERAse the existing file
and then restart the edit operation.

---
_Page 33_

Similar to other transient canmands, editing can take place on a drive
different fram the currently logged disk by preceding the source file name by
a drive name, Examples of valid edit requests are shown below

ED A:X.ASM Edit the file X.ASM on drive A, with
new file and backup on drive A.

ED B:X.ASM A: Edit the file X.ASM on drive B to the
temporary file X.$$$ on drive A, ©
termination of editing, change X.ASM
on drive B to X.BAK, and change x.$$$
on drive A to X,ASM,

6.6. SYSGEN cr

The SYSGEN transient canmand allows generation of an initialized diskette
containing the CP/M operating system. The SYSGEN program prompts the console
for canmands, with interaction as shown below.

SYSGEN cr Initiate the SYSGEN program.
SYSGEN VERSION mm SYSGEN sign-on message,

SOURCE DRIVE NAME (OR RETURN TO SKIP)

Respond with the drive name (one
of the letters A, B, C, or D) of
the disk containing a CP/M sys-
tem; usually A. If a copy of
CP/M already exists in memory,
due to a MOVCPM command, type a
cr only. Typing a drive name

x will cause the response:

SOURCE ON x THEN TYPE RETURN Place a diskette containing the
CP/M operating system on drive
x (x is one of A, B, C, or D),
Answer with cr when ready.

FUNCTION OCOMPLETE System is copied to memory.
SYSGEN will then prompt with:

DESTINATION DRIVE NAME (OR RETURN TO REBOOT)

If a diskette is being ini-
tialized, place the new disk
into a drive and answer with
the drive name, Otherwise, type
a cr and the system will reboot
from drive A, Typing drive name
x will cause SYSGEN to prompt

---
_Page 34_

DESTINATION ON x THEN TYPE RETURN Place new diskette into drive
x; type return when ready.

FUNCTION COMPLETE New diskette is initialized
in drive x.

The “DESTINATION” prompt will be repeated until a single carriage return is
typed at the console, so that more than one disk can be initialized.

Upon canpletion of a successful system generation, the new diskette
contains the operating system, and only the built-in commands are available,
A factory-fresh IBM-compatible diskette appears to CP/M as a diskette with an
empty directory; therefore, the operator must copy the appropriate OOM files
fram an existing CP/M diskette to the newly constructed diskette using the PIP
transient,

The user can copy all files from an existing diskette by typing the PIP
command

PIP Bz: = A: \*,\*[v] cr

which copies all files from disk drive A to disk drive B, and verifies that
each file has been copied correctly. The name of each file is displayed at
the console as the copy operation proceeds,

It should be noted that a SYSGEN does not destroy the files which already
exist on a diskette; it results only in construction of a new operating
system. Further, if a diskette is being used only on drives B through D, and
will never be the source of a bootstrap operation on drive A, the SYSGEN need
not take place, In fact, a new diskette needs absolutely no initialization to
be used with CP/M,

6.7. SUBMIT ufn parm#l ... parm#n cr

The SUBMIT command allows CP/M commands to be batched together for
automatic processing, The ufm given in the SUBMIT command must be the
filename of a file which exists on the currently logged disk, with an assumed
file type of “SUB.” The SUB file contains CP/M prototype commands, with
possible parameter substitution, The actual parameters parm#]1 ... parm#n are
substituted into the prototype canmands, and, if no errors occur, the file of
substituted canmands are processed sequentially by CP/M.

---
_Page 35_

The prototype canmand file is created using the ED program, with
interspersed "$" parameters of the form

$1 $2 $3 ... $n

corresponding to the number of actual parameters which will be included when
the file is submitted for execution. When the SUBMIT transient is executed,
the actual parameters parm#1 ... parm#n are paired with the formal parameters
$1 ... $n in the prototype commands, If the number of formal and actual
parameters does not correspond, then the submit function is aborted with an
error message at the console, The SUBMIT function creates a file of
substituted canmands with the name

$$$.SUB

on the logged disk, When the system reboots (at the termination of the
SUBMIT), this canmmand file is read by the CCP as a source of input, rather
than the console. If the SUBMIT function is performed on any disk other than
drive A, the canmands are not processed until the disk is inserted into drive
A and the system reboots. Further, the user can abort camnmand processing at
any time by typing a rubout when the command is read and echoed. In this
case, the $$$.SUB file is removed, and the subsequent commands come from the
console, Command processing is also aborted if the CCP detects an error in
any of the canmands, Programs which execute under CP/M can abort processing of
command files when error conditions occur by simply erasing any existing
$$$.SUB file,

In order to introduce dollar signs into a SUBMIT file, the user may type
a "$$" which reduces to a single "S$" within the command file. Further, an
up-arrow symbol "\*" may precede an alphabetic character x, which produces a
Single ctl-x character within the file.

The last canmand in a SUB file can initiate another SUB file, thus
allowing chained batch commands,

Suppose the file ASMBL.SUB exists on disk and contains the prototype
commands
ASM $1
DIR $1.\*
ERA \*, BAK
PIP $2:=$1,PRN
ERA $1,.PRN

and the canmand
SUBMIT ASMBL X PRN cr

is issued by the operator. The SUBMIT program reads the ASMBL.SUB file,

substituting "X" for all occurrences of $1 and "PRN" for all occurrences of
$2, resulting in a $$$,SUB file containing the commands

---
_Page 36_

ASM X

DIR X.\*

ERA \*,BAK

PIP PRN:=X. PRN
ERA X. PRN

which are executed in sequence by the CCP,

The SUBMIT function can access a SUB file which is on an alternate drive
by preceding the file name by a drive name. Submitted files are only acted
upon, however, when they appear on drive A, Thus, it is possible to create a
submitted file on drive B which is executed at a later time when it is
inserted in drive A.

6.8. DUMP ufn cr

The DUMP program types the contents of the disk file (ufn) at the console
in hexadecimal form, The file contents are listed sixteen bytes at a time,
with the absolute byte address listed to the left of each line in
hexadecimal. Long typeouts can be aborted by pushing the rubout key during
printout, (The source listing of the DUMP program is given in the "CP/M
Interface Guide” as an example of a program written for the CP/M environment.)

6.9. MOVCPM cr

The MOVCPM program allows the user to reconfigure the CP/M system for any
particular memory size. Two optional parameters may be used to indicate (1)
the desired size of the new system and (2) the disposition of the new system
at program termination. If the first parameter is amitted or a "\*" is given,
the MOVCPM program will reconfigure the system to its maximum size, based upon
the kilobytes of contiguous RAM in the host system (starting aat @0@0H). If
the second parameter is omitted, the system is executed, but not permanently
recorded; if "\*" is given, the system is left in memory, ready for a SYSGEN
operation. The MOVCPM program relocates a memory image of CP/M amd places
this image in memory in preparation for a system generation operation, The
command forms are:

MOVCPM cr Relocate and execute CP/M for manage-
ment of the current memory configura-
tion (memory is examined for contigu-
ous RAM, starting at 160H). Upon com
pletion of the relocation, the new
system is executed but not permanently
recorded on the diskette, The system
which is constructed contains a BIOS
for the Intel MDS 89.

---
_Page 37_

MOVCPM n cr Create a relocated CP/M system for
management of ann kilobyte system (n
must be in the range 16 to 64), and
execute the system, as described above,

MOVCPM \* \* cr Construct a relocated memory image for
the current memory configuration, but
leave the memory image in memory, in
preparation for a SYSGEN operation,

MOVCPM n \* cr Construct a relocated memory image for
an n kilobyte memory system, and leave
the memory image in preparation for a
SYSGEN operation,

The canmand
MOVCPM \* \*

for example, constructs a new version of the CP/M system and leaves it in
memory, ready for a SYSGEN operation, The message

READY FOR "SYSGEN" OR
“SAVE 32 CPMxx,QOM"

is printed at the console upon campletion, where xx is the current memory size
in kilobytes. The operator can then type

SYSGEN cr Start the system generation,

SOURCE DRIVE NAME (OR RETURN TO SKIP) Respond with a cr to skip
the CP/M read operation since the system
is already in memory as a result of the
previous MOVCPM operation,

DESTINATION DRIVE NAME (OR RETURN TQ REBOOT)
Respond with B to write new system
to the diskette in drive B. SYSGEN
will prompt with:

DESTINATION ON B, THEN TYPE RETURN
Ready the fresh diskette on drive
B and type a return when ready.

Note that if you respond with "A" rather than "B" above, the system will be
written to drive A rather than B, SYSGEN will continue to type the prompt:

DESTINATION DRIVE NAME (OR RETURN TO REBOOT)

until the operator responds with a single carriage return, which stops the

---
_Page 38_

SYSGEN program with a system reboot,

The user can then go through the reboot process with the old or new
diskette. Instead of performing the SYSGEN operation, the user could have

typed
SAVE 32 CPMxx.COM

at the canpletion of the MOVCPM function, which would place the CP/M memory
image on the currently logged disk in a form which can be “patched.” This is
necessary when operating in a non-standard environment where the BIOS must be
altered for a particular peripheral device configuration, as described in
the"CP/M System Alteration Guide."

Valid MOVCPM commands are given below:

MOVCPM 48 cr Construct a 48K verskon of CP/M and start
execution,
MOVCPM 48 \* cr Construct a 48K version of CP/M in prepara-

tion for permanent recording; response is

READY FOR "SYSGEN" OR
"SAVE 32CPM48,COM"

MOVCPM \* \* cr Construct a maximum memory version of CP/M
and start execution,

It is important to note that the newly created system is serialized with
the number attached to the original diskette and is subject to the conditions
of the Digital Research Software Licensing Agreement,

---
_Page 39_

7. BDOS ERROR MESSAGES,

There are three error situations which the Basic Disk Operating System
intercepts during file processsing. When one of these conditions is detected,
the BDOS prints the message:

BDOS ERR ON x: error
where x is the drive name, and "error" is one of the three error messages:

BAD SECTOR
SELECT
READ ONLY

The "BAD SECTOR" message indicates that the disk controller electronics
has detected an error condition in reading or writing the diskette, This
condition is generally due to a malfunctioning disk controller, or an
extremely worn diskette, If you find that your system reports this error more
than once a month, you should check the state of your controller electronics,
and the condition of your media, You may also encounter this condition in
reading files generated by a controller produced by a different manufacturer,
Even though controllers are claimed to be IBM-compatible, one often finds
small differences in recording formats, The MDS-80@ controller, for example,
requires two bytes of one’s following the data CRC byte, which is not reguired
in the IBM format. As a result, diskettes generated by the Intel MDS can be
read by almost all other IBM-compatible systems, while disk files generated on
other manufacturer’s equipment will produce the "BAD SECTOR" message when read
by the MDS, In any case, recovery from this condition is accomplished by
typing a ctl-C to reboot (this is the safest!), or a return, which simply
ignores the bad sector in the file operation, Note, however, that typing a
return may destroy your diskette integrity if the operation is a directory
write, so make sure you have adequate backups in this case,

The “SELECT” error occurs when there is an attempt to address a drive
beyond the A through D range. In this case, the value of x in the error
message gives the selected drive. The system reboots following any input from
the console,

The "READ ONLY” message occurs when there is an attempt to write to a
diskette which has been designated as read-only in a STAT command, or has been
set to read-only by the BDOS, In general, the operator should reboot CP/M
either by using the warm start procedure (ctl-C) or by performing a cold start
whenever the diskettes are changed. If a changed diskette is to be read but
not written, BDOS allows the diskette to be changed without the warm or cold
start, but internally marks the drive as read-only, The status of the drive
is subsequently changed to read/write if a warm or cold start occurs, Upon
issuing this message, CP/M waits for input from the console, An automatic
warm start takes place following any input,

---
_Page 40_

8, OPERATION OF CP/M ON THE MDS,

This section gives operating procedures for using CP/M on the Intel MDS
microcomputer development system. A basic knowledge of the MDS hardware and
software systems is assumed,

CP/M is initiated in essentially the same manner as Intel’s ISIS
operating system, The disk drives are labelled @ through 3 on the MBS,
corresponding to CP/M drives A through D, respectively, The CP/M system
diskette is inserted into drive 8, amd the BOOT and RESET switches are
depressed in sequence, The interrupt 2 light should go on at this point. The
space bar is then depressed on the device which is to be taken as the system
console, and the light should go out (if it does not, then check connections
and baud rates). The BOOT switch is then turned off, and the CP/M signon
message should appear at the selected console device, followed by the "A>"
system prompt. The user can then issue the various resident and transient
commands

The CP/M system can be restarted (warm start) at any time by pushing the
INT @ switch on the front panel. The built-in Intel ROM monitor can be
initiated by pushing the INT 7 switch (which generates a RST 7), except when
operating under DDI, in which case the DDI program gets control instead,

Diskettes can be removed from the drives at any time, and the system can
be shut down during operation without affecting data integrity. Note,
however, that the user must not remove a diskette and replace it with another
without rebooting the system (cold or warm start), unless the inserted
diskette is “read only."

Due to hardware hang-ups or malfunctions, CP/M may type the message
BDOS ERR ON x: BAD SECTOR

where x is the drive which has a permanent error, This error may occur when
drive doors are opened and closed randomly, followed by disk operations, or
may be due to a diskette, drive, or controller failure. The user can
optionally elect to ignore the error by typing a single return at the
console, The error may produce a bad data record, requiring re-initialization
of up to 128 bytes of data, The operator can reboot the CP/M system and try
the operation again,

Termination of a CP/M session requires no special action, except that it
is necessary to remove the diskettes before turning the power off, to avoid
random transients which often make their way to the drive electronics,

It should be noted that factory-fresh IBM-campatible diskettes should be
used rather than diskettes which have previously been used with any ISIS
version, In particular, the ISIS “FORMAT” operation produces non-standard
sector numbering throughout the diskette, This non-standard numbering
seriously degrades the performance of CP/M, and will operate noticeably slower

---
_Page 41_

than the distribution version, If it becomes necessary to reformat a diskette
(which should not be the case for standard diskettes), a program can be
written under CP/M which causes the MDS 88@ controller to reformat with
sequential sector numbering (1-26) on each track,

Sy a a a “A he oe oo

Note: “MDS 800" and "ISIS" are registered trademarks of Intel Corporation,

---
_Page 42_

---
_Page 43_

SL) ee Fe
ree a as
ates eS

==

= Ltd

oe
Neither se
CS tan

SS
as
Feet ety

yi

Ne

etatae eee

SS
; 3
eons eee RS
Es ea eS.
Si re ae

aes ee

co

ea eciae
= SS
— == —

Se i a es aes
Se

---
_Page 44_

SL) ee Fe
ree a as
ates eS

==

= Ltd

oe
Neither se
CS tan

SS
as
Feet ety

yi

Ne

etatae eee

SS
; 3
eons eee RS
Es ea eS.
Si re ae

aes ee

co

ea eciae
= SS
— == —

Se i a es aes
Se


