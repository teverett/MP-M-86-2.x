# SID ZSID

> OCR text extracted from `SID_ZSID.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `SID_ZSID.pdf`.

---

---
_Page 01_

Copyright

Copyright (c) 1978 by Digital Research. All rights
reserved. No part of this publication may be repro-
duced, transmitted, transcribed, stored in a retrieval
system, or translated into any language or computer
language, in any form or by any means, electronic,
mechanical, magnetic, optical, chemical, manual or
otherwise, without the prior written permission of
Digital Research, Post Office Box 579, Pacific Grove,
California 93950.

Trademark

The names CP/M, SID, MAC, and Digital Research
are trademarks of Digital Research.

Disclaimer

Digital Research makes no representations or war-
ranties with respect to the contents hereof and specifi-
eally disclaims any implied warranties of merchant-
ability or fitness for any particular purpose. Further,
Digital Research reserves the right to revise this
publication and to make changes from time to time
in the content hereof without obligation of Digital
Research to notify any person of such revision or
changes.

---
_Page 02_

---
_Page 03_

SECTION

l.

TABLE OF CONTENTS

SID OPERATION UNDER CPM .... .

1.1.
1.2.

SID Startup . .....-. -»
SID Command Input .....

SID SYMBOLIC EXPRESSIONS ,.. .

SID UTILITIES ....

4.1.
4.2.
4.3.

Literal Hexadecimal Numbers
Literal Decimal Numbers ..
Literal Character Values.
Symbolic References ....
Qualified Symbols .....
Symbolic Operators ....
Sample Symbolic Expressions

COMMANDS .. . ° . °

The Assemble (A) Command .
The Call (C) Command ...
The Display Memory (D)

Command

The Fill Memory (F) Command .

The Go (G) Command ....
The Hexadecimal Value
The Input Line (I) Command
The List Code (L) Command .
The Move Memory (M) Command
The Pass Counter

The Set Memory (S) Command
The Trace Mode (T) Command
The Untrace Mode
The Examine CPU State (X)

The HIST Utility

Utility Operation ,.
The TRACE Utility .

SID SAMPLE DEBUGGING SESSIONS ..

(P) Command
The Read Code/Symbols (R) Comm

(U) Command :
Command

\*

(H) Comma

and

nd

eo 8 8 @

os ¢ 8 @

---
_Page 04_

---
_Page 05_

1. SID OPERATION UNDER CP/M

The CP/M symbolic debugger, called SID, expands upon
the features of the CP/M standard debugger described in the
manual “CP/M Dynamic Debugging Tool (DDT) User's Guide" and
provides greatly enhanced facilities for assembly level

program checkout. Specifically, SID includes real-time
breakpoints, fully monitored execution, symbolic
disassembly, assembly, and memory display and fill

functions. Further, SID operates with "utilities" which can
be dynamically loaded with SID to provide traceback and
histogram facilities. The various functions of SID are
given in the sections which follow.

1.1. SID Startup.

The SID program is initiated by typing one of the
following commands:

(a) SID

(b) SID x.y

(c) SID x.HEX
(d) SID x.UTL
(e) SID x.y u.v
(£) SID \* u.v

In each case, SID loads into the topmost portion of the
Transient Program Area (TPA) and overlays the Console
Command Processor portion of CP/M (see the "CP/M Interface
Guide" and "CP/M Alteration Guide" for a discussion of
memory uSe conventions). Memory organization before SID is
loaded is shown in Figure 1, while Figure 2 shows the memory
configuration after SID is loaded and relocated. Due to the
relocation process, SID is independent of the exact memory
size which CP/M manages in a particular computer
configuration.

(High Memory) | |

(Low Memory) | JMP BDOS |

ae eee we ee

Figure 1. Memory Configuration Before SID Loads.

---
_Page 06_

SID Users Guide Section 1

Figure 2, Memory Configuration After SID Loads.

After loading and relocating, SID alters the BDOS
entry address to reflect the reduced memory size, as shown
in Figure 2, and frees the lower portion of the TPA for use
by the program under test. Note that although SID occupies
only 6K of upper memory when operating, the self-relocation
process necessitates a minimum 28K CP/M system for initial
setup, leaving about 16K for the test program,

Command form (a) above loads and executes SID without
loading a test program into the TPA. This form is often
used when the operator wishes to examine memory or write and
test simple programs using the built-in assembly features of
SID.

Form (b) above is similar to (a) except that the
program given by x.y is automatically loaded for subsequent
test. Note that although x.y is loaded into the TPA, it is
not executed until SID passes program control to the program
under test using one of the commands C (Call), G (Go), T

(Trace), or U (Untrace). It is the programmer's
responsibility to ensure there is enough space in the TPA to
hold the test program as well as the debugger. If the

program x.y does not exist on the diskette or cannot be
loaded, the standard "?" error response is issued by SID.
If no load error occurs, the SID response is:

NEXT PC END
nnnn pppp eeee

where nnnn, pppp, and eeee are hexadecimal values which
indicate the next free address following the loaded program,
the initial value of the program counter, and the logical
end of the TPA, respectively. Thus, nnnn is normally’ the
beginning of the data area of the program under test, pppp
is the starting program counter (set to the beginning of the
TPA), and eeee is the last memory location available to the
test program, aS shown in Figure 3. Although x.y uSually

---
_Page 07_

SID Users Guide Section 1

contains machine code, the operator can name an ASCII file,
in which case these program addresses are less meaningful.

| BDOS |
| |
| SID |
eeee: | (Free Space) |
nnnn: | |
| |
| (Test |
pppp: | program) |
| JMP SID |

Figure 3. Memory Configuration After Test Program Load.

Command form (c) is similar to form (b) except that
the test program is assumed to be in Intel "hex" format, as
directly produced by ASM or MAC. In this case, the initial
program counter is obtained from the last record of the hex
file unless this value is zero, in which case the program
counter is set to the beginning of the TPA. As discussed in
the ASM and MAC manuals, the program counter value can be
given on the "END" statement in the source program. Again,
it is the programmer's responsibility to ensure that the hex
records do not overlay portions of the SID debugger or CP/M
Operating System. If the hex file does not exist, or if
errors occur in the hex format, the "?" response is issued
by SID. Otherwise, the principal program locations shown in
the previous paragraph are listed at the console.

Command form (d) is used when a SID utility function
is to be included. In this case, SID is first loaded and
relocated as above. The utility function is then loaded
into the TPA. Utility functions are also self-relocating
and immediately move to the top of the TPA, placing
themselves directly below the SID program. The BDOS entry
address is changed to reflect the reduced TPA, as shown in

Figure 4. Generally, the utility program prints sign-on
information and may or may not prompt for input from the
console. Exact details of utility operation are given in

the section entitled "SID Utilities."

---
_Page 08_

SID Users Guide Section l

ee a a ae i eS ee

Figure 4. Memory Configuration Following Utility Load.

Command form (e) is similar to (c), except that the
symbol table given by u.v is loaded with the program x.y.
Symbol information is loaded from the base of SID downward
toward the program under test, as shown in Figure 5.

| (UTL If |
| Present)

SYMBOLS

JMP BDOS

eee ee wee ee

Figure 5. Memory Configuration Following Symbol Load.

The symbol table is in the format produced by the CP/M
Macro Assembler. In particular, the symbol table must be a
sequence of address and symbol name pairs, where the address
consists of four hexadecimal digits, separated by a space
from the symbol which takes on this address value. The
symbol consists of up to.16 graphic ASCII characters
terminated by one or more tabs (ctl-I) or a carriage return
line feed sequence. Note that the operator can optionally
create or alter a symbol table using the CP/M editor, as

---
_Page 09_

SID Users Guide Section l

long as this format is followed (see the manual "ED: the
CP/M Context Editor" for editing details).

The response following program load will be as_ shown
in command form (b) above, giving essential program
locations. When SID begins symbol load, the message:

SYMBOLS

is printed indicating that any subsequent error is due to
the symbol load _ process. In particular, the "?" error
following the SYMBOLS response is due to a non-existent or
incorrectly formatted symbol file.

Examples of typical commands which start the SID
program are shown below.

COMMAND FORM COMMAND EXAMPLE
(a) SID
(b) SID DUMP.COM
(b) SID DUOMP,ASM
(c) SID SAMPLE.HEX
(c) SID DUMP.HEX
(d) SID TRACE.UTL
(d) SID HIST.UTL
(e) SID DUMP.COM DUMP.SYM
(e) SID DUMP.HEX DUMP.SYM
{e) SID TEST.COM TEST, ZOT
(£) SID \* DUMP.SYM

1.2. SID Command Input.

Command input to SID consists of a series of "command
lines" which direct the actions of the SID program. These
commands allow display of memory and CPU registers, and
direct the execution and breakpoint operations during test
program debugging.

SID prompts the console for input by typing "#" when
ready to accept the next command. Each command is based
upon a Single letter, followed by optional parameters, and
terminated by a carriage return. Note that all standard
line editing features of CP/M are available, with a maximum
of 64 command command characters. The CP/M line editing
functions are:

---
_Page 10_

SID Users Guide Section l

ct1l-c CP/M system reboot, return to CCP
ctl-E Physical end-of-line

ctl-P Print console output (on/off toggle)
ctl1-R Retype current input line

ct1-s Stop/start console output

cet1-U Delete current input line

ct1-xX (Same as ctl-U)

ctl-Z End of console input (not used in SID)

rubout Delete and echo last character

where the "ctl" function indicates that the control key is
held down while the particular function key is depressed.
Note further that the ctl-R, ctl-U, and ctl-X keys cause
CP/M to type a "#" at the end of the line to indicate that
the line is being discarded.

Various SID commands produce long typeouts at the
console (see the "D" command which displays memory, for
example). In this case, the operator can abort the typeout
before it completes by typing any key at the console (a
"return" suffices).

The single letter commands which direct the actions of
SID are typed at the beginning of the command line. The
valid commands are summarized below (lower case command
letters are translated to upper case automatically):

Assemble directly to memory

Call to memory location from SID
Display memory in hex and ASCII
Fill memory with constant value

Go to test program for execution
Hexadecimal arithmetic

Input CCP command line

List 8488 mnemonic instructions
Move memory block

Pass point set, reset, and display
Read test program and symbol table
Set memory to data values

Trace test program execution
Untrace (monitor) test program
Examine state of CPU registers

KSCHHODVEOCHAUADON Pe

Although the details of each of the commands are given in
later sections, nearly all of the commands accept parameters
following the letter which governs the command actions. The
parameters may be counters or memory addresses, and may
appear in both literal and symbolic form, but eventually
reduce to values in the range @-65535 (four hexadecimal
digits).

As an example, the "display memory" command can _ take
the form

Dssss,eeee

---
_Page 11_

SID Users Guide Section l

where D is the command letter, and ssss and eeee are
“command parameters" which give the starting and ending
addresses for the display, respectively. In their simplest
form, ssss and eeee can be literal hexadecimal values, such
as

D1GG, 300

which instructs SID to print the hexadecimal and ASCII
values contained in memory locations 61808 through 0308.

Although the operator can usually refer to program
listings to obtain absolute machine addresses, SID supports
more comprehensive mechanisms for quick access to machine
addresses through program symbols. In particular, the
command parameters can consist of “symbolic expressions"
which are described fully in the following section.

---
_Page 12_

---
_Page 13_

2. SID SYMBOLIC EXPRESSIONS

An important facility of SID is the ability to
reference absolute machine addresses’ through symbolic
expressions. Symbolic expressions may involve names
obtained from the program under test which are included in
the "SYM" file produced by the CP/M Macro Assembler, or may
consist of literal values in hexadecimal, decimal, or ASCII
character string form. These values can then be combined
with various operators to provide access to subscripted and
indirectly addressed data or program areas. The purpose of
this section is to completely describe symbolic expressions
so that they may be incorporated aS command parameters in
the individual command forms which follow this section.

2.1. Literal Hexadecimal Numbers.

SID normally accepts and displays values in the
hexadecimal number base to form 16-bit values from up to
four hexadecimal digits. The valid hexadecimal digits
consist of the decimal digits @ through 9 along with the
hexadecimal digits A, B, C, D, E, and F, corresponding to
the decimal values 18 through 15, respectively. Note that
SID translates lower case hexadecimal digits to upper case
outside of string apostrophes.

A literal hexadecimal number in SID consists of one or

more contiguous hexadecimal digits. If four digits are
typed then the leftmost digit is most significant, while the
rightmost digit is least significant. If the number

contains more than four digits, the rightmost four are taken
as significant, and the remaining leftmost digits are
discarded. The values to the left below produce the
hexadecimal and decimal values shown following the "“#" to
the right below.

INPUT VALUE HEXADECIMAL DECIMAL
1 B8G1 #1
188 G1¥O #256
fffe FFFE #65534
10908 OVOD #0
38001 8001 #32769

2.2. Literal Decimal Numbers.

Although SID's normal number base is hexadecimal, the
operator can override this base on input by preceding the
number by a "#" symbol which indicates that the following
number is in the decimal base. In this case, the number
which follows must consist of one or more decimal digits (@
through 9) with the most significant digit on the left and
the least significant digit on the right. Decimal values
are padded or truncated according to the rules of
hexadecimal numbers, as described above, by converting the
decimal number to the equivalent hexadecimal value.

---
_Page 14_

SID Users Guide Section 2

The input values shown to the left below produce’ the
internal hexadecimal values shown to the right below:

INPUT VALUE HEXADECIMAL VALUE
\#9 0009
\#19 OOBA
\#256 01008
\#65535 FFFF
\#65545 00689

2.3. Literal Character Values.

As an operator convenience, SID also accepts one or
more graphic ASCII characters enclosed in string apostrophes
(‘) as literal values in expressions. Characters remain as
typed within the paired apostrophes (i.e., no case
translation occurs) with the leftmost character treated as
the most significant, and the rightmost character treated as
least significant. Each character is translated internally
to its two hexadecimal digit ASCII encoded form. Similar to
hexadecimal numbers, character strings of length one are
padded on the left with zero, while strings of length
greater than two are truncated to the rightmost two
characters, discarding the leftmost remaining characters.

Note that the enclosing apostrophes are not included
in the character string, nor are they included in the
character count, with one exception. In order to include
the possibility of writing strings which include
apostrophes, a pair of contiguous apostrophes are reduced to
a single apostrophe and included in the string as a normal
graphic character.

The strings shown to the left below produce the
hexadecimal values shown to the right below. (For these
examples, note that upper case ASCII alohabetics begin at
the encoded hexadecimal value 41, lower case alphabetics
beqin at 61, a space is hexadecimal 29, and an apostrophe is
encoded as hexadecimal 60).

INPUT STRING HEXADECIMAL VALUE
‘A' 8041
‘AB' 4142
"ABC' 4243
‘aA’ 6141
oprae 868
puseod 686g
" A’ 2041
‘A ! 4126

2.4. Symbolic References.

Given that a symbol table is present during a SID
debugging session, the operator may reference values

---
_Page 15_

SID Users Guide Section 2

associated with symbols through three forms of a symbol
reference:

(a) -s
(b) @s
(c) =s

where s represents a sequence of one to sixteen characters
which match a symbol in the table,

Form (a) produces the address value (i.e., the value
associated with the symbol in the table) corresponding to
the symbol s. Form (b) produces the double precision 16-bit
“word" value contained in the two memory locations given by
-S, while form (c) results in the single precision 8-bit
"byte" value at .s in memory. Suppose, for example, that
the inout symbol table contains two symbols, and appears as:

810808 GAMMA 8182 DELTA

Further, suppose that memory starting at #188 contains’ the
following byte data values:

G1900: 82, 6101: 3E, 6102: 4D, 81803: 22

Based upon this symbol table and these memory values,
the symbol references shown to the left below vroduce the
hexadecimal values shown to the right below. Recall that
lo-bit 8888 memory values are stored with the least
significant byte first, and thus the word values at 91908 and
6192 are 3E@2 and 224D, respectively.

SYMBOL REFERENCE HEXADECIMAL VALUE
» GAMMA 0169
-DELTA 8162
@GAMMA 3E92
@DELTA 224D
=GAMMA Q902
=DELTA 684D

2.5. Qualified Symbols.

It should be noted that duplicate symbols can occur in
the symbol table due to separately assembled or compiled
modules which independently use the same name for differing

subroutines or data areas. Further, block structured
languages, such as PL/M, allow nested name definitions which
are identical, but non-conflicting. Thus, SID allows

reference to "qualified symbols" which take the form
S1/S2/ . . . /Sn

where Sl through Sn represent symbols which are present in
the table during a particular session,

SID always searches the symbol table from the first to

---
_Page 16_

SID Users Guide Section 2

last symbol, in the order the symbols appear in the input
file. In the case of a qualified symbol, SID begins by
matching the first Sl symbol, then scans for a match with
symbol $2, continuing until symbol Sn is matched. If this
search and match procedure is not successful, SID prints the
"2" response to the console. Suppose, for example, that the
symbol table appears as

0190 A 63808 B @200 A 35660 C 20F8 A 8102 A

in the input file, with memory initialized as shown in the
previous section. The ungualified and qualified symbol
references shown to the left below produce the hexadecimal
values shown to the right below.

SYMBOL REFERENCE HEXADECIMAL VALUE
JA 9190
@A 3E02
.A/A 9200
.C/A/A 9162
=C/A/A g04D
@B/A/A 20FD

2.6. Symbolic Operators,

Literal numbers, strings, and symbol references can be
combined into symbolic expressions using unary and _ binary
"+" ang@ “-" delimiters. The entire sequence of numbers,
symbols, and operators must be written without imbedded
blanks. Further, the sequence is evaluated from
left-to-right, producing a four digit hexadecimal value at
each step in the evaluation. Overflow and underflow are
both ignored as _ the evaluation proceeds. The final value
becomes the command parameter, whose interpretation depends
upon the particular command letter which precedes it.

When placed between two operands, the "+" indicates
addition of the previously accumulated value. The value of
the following literal or symbolic value is added, and
becomes the new accumulated value to this point in the
evaluation. If the expression begins with a unary "+" then
the immediately preceding (completed) symbolic expression is
taken as the initial accumulated value (zero is assumed at
SID startup). For example, the command:

DFEGO+#128,+5

contains the first expression "FEQ@@+#128" which adds FE9@
and (decimal) 128 to produce FE8@ as the starting value for

this display command. The second expression “+5" begins
with a unary "+" which indicates that the previous
expression value (FE8@) is to be used as the base for this

symbolic expression, vroducing the value FE85 for the end of
the display operation. Thus, the command given above is
equivalent to:

Il

---
_Page 17_

SID Users Guide Section 2

DFE80,FE85

The "-" symbol causes SID to subtract the literal
number or symbol reference from the 16-bit value accumulated
thusfar in the symbolic expression, If the expression

begins with a minus sign, then the initial accumulated value
is taken as zero. That is,

-x is computed as O@-x
where x is any valid symbolic expression. The command:
DFFOO-~268 ,-#512
for example, is equivalent to the simple command

DFDOO,FEGO

A special up-arrow operator, denoted by "“"", is
present in SID to denote the top-of-stack in the program
under test. In general, a sequence of n up-arrow operators
extracts the nth stacked item in the test program, but does
not change the test program stack content or stack pointer.
This particular operator is used most often in conjunction
with the G (Go) command to set a breakpoint at a return from
a subroutine during test, and is described fully under the G
command.

2.7. Sample Symbolic Expressions.

The formulation of SID symbolic expressions is most
often closely related to the program structures in the
program under test. Suppose we wish to debug a-= sorting
program which contains the data items listed below:

LIST: names the base of a table of byte values to
sort, assuming there are no more than 255 elements, denoted
by LIST(@), LIST(1), ... , LIST(255).

N: is a byte variable which gives the actual number
of items in LIST, where the value of N is less than 256.
The items to sort are stored in LIST(@) through LIST(N-1).

I: is the byte subscript which indicates the next
item to compare in the sorting process. That is, LIST(I) is
the next item to place in sequence, where I is in the range
@ through N-l.

Given these data areas, the command

D.LIST,+255

for example, displays the entire area reserved for sorting:

LIST(@), LIST(1), . . . , LIST(255)

---
_Page 18_

SID Users Guide Section 2

The command
D.LIST,+=I

displays the LIST vector up to and including the next item
to sort:

LIST(@), LIST(1), . .. , LIST(I)

The command:
D.LIST+=I ,+@

displays only LIST(I). Finally, the command:
D.LIST,+=N-1

displays only the area of LIST which holds active items to
sort:

LIST(@), LIST(1), . .. , LIST(N-1)
The exact manner in which symbolic expressions are
used within SID is dependent upon the individual command

which is issued by the operator. These commands are listed
in some detail in the section which follows.

---
_Page 19_

3. SID COMMANDS.

SID commands are entered at the console following the
"#" prompt, and direct the debugging vrocess by allowing
alteration and display of machine functions as well as
controlling execution of the program under test.

The commands which SID accepts are listed and
described in alphabetical order in the sections’ which
follow.

3.1. The Assemble (A) Command.

The A command allows the operator to insert 8988
machine code and operands into the current memory image
using standard Intel mnemonics, along with symbolic
references to operands. The command forms are:

(a) As

(bo) A

(c) —-A

where s represents any valid symbolic expression. Form (a)
begins inline assembly at the address given by s, where each
successive address is displayed until a null line (i.e., a
Single carriage return) is typed by the operator. Form (b)
is equivalent to (a), except the starting address for the
assembly is taken from the last assembled, listed, or traced
address (see the "L", "T", and "U"“ commands). The following
command sequence, for example, assembles a short program
into the transient program area (note that each command line
is terminated by a carriage return):

Alda begin assembly at 9100
6100 MVI A,19 load A with hex 19
8182 DCR A decrement A register
6183 JINZ 108 loop until zero

01606 RST 7 return to debugger
G187 single carriage return

As each successive address is prompted, the operator may
either enter a mnemonic instruction, or return to SID
command mode by entering a single carriage return (a _ single
"." is also accepted to terminate inline assembly to be
consistent with the "S" command).

Delimiter characters which are acceptable between
mnemonic and operand fields include space or tab sequences.

Invalid mnemonics or ill-formed operand fields produce
"?" errors. In this case, control returns back to command
mode, where the operator can proceed with another command
line, or simply return to assembly mode by typing a_ single
"A" since the assumed starting address is automatically
taken from the last assembled address.

---
_Page 20_

SID Users Guide Section 3

The assembler/disassembler portion of SID is a
separate module, and can be removed to increase the
available debugging space. Thus, form (c) is entered to

remove the module, returning approximately 1 1/2 K bytes.
Since the entire SID debugger requires approximately 6 kK
bytes, this reduces SID requirements to about 4 1/2 K bytes.
When the assembler/disassembler module is removed in this
Manner, the A and L commands are effectively removed.
Further, the trace and untrace functions display only the
hexadecimal codes, and the traceback utility displays only
hexadecimal addresses. Any existing symbol information is
also discarded at this point, although such information can
be reloaded (see the “I" and "R" commands).

Examples of valid assemble commands are shown below:

A109
A#100
A.CRLF+5
A@GAMMA+ @X-=I1
A+30

Given that the command Al9@ has been entered, the
following interaction could take place between SID and the
operator:

SID PROMPT OPERATOR INPUT

8106 MVI C,.A-.B

G192 LXI H, SOURCE

8195 LXI D,+199

8198 MOV A,M

8199 INX H

G1BA STAX D

G198B INX D

@19C “DCR C

G19D JNZ 168

9119 ("return" only)
where A, B, and SOURCE are symbols which are active in the
symbol table. In this case, SID computes the address
difference between A and B as the operand for the MVI
instruction. The LXI H operand becomes the address of
SOURCE, while the LXI D instruction receives the operand
value .SOURCE+1@6 since .SOURCE was the immediately
preceeding symbolic expression value. This particular

program segment would move a block of memory determined by
the address values of the corresponding symbols.

3.2. The Call (C) Command.
The C command performs a call to an absolute location

in memory, without disturbing the register state of the
program under test. The forms are:

---
_Page 21_

SID Users Guide Section 3

(a) Cs
(b) Cs,b
(c) Cs,b,d

Although the C command is designed for use with SID
utilities, it can be used to perform calls on test program
subroutines to perform program initialization, or to make
CP/M BDOS calls which initialize various system parameters
before executing the test program.

Form (a) above performs a call on absolute location s,
where s is a symbolic expression, In this case, registers
BC = #868 and DE = @868 in the call. Normal exit from the
subroutine is through execution of a RET instruction which
returns control to SID, followed by a normal system prompt.

Form (b) above is equivalent to (a), except that the
BC register pair is set to the value of expression b, while
DE is set to 9000,

Form (c) is similar to (b): the BC register pair is
set to the value b while the DE pair is set to the value of
d. Several examples of valid C commands are shown below.
Refer also to the SID utility discussion for examples of the
C command in utility initialization, data collection, and
display functions.

C109
C#4096
C.DISPLAY
C@JIMPVEC+=X
C.CRLF,#34
C.CRLF,@X,+=X

3.3. The Display Memory (D) Command.

The D command is used to display selected segments of
memory in both byte (8-bit) and word (16-bit) formats. The
display appears in both byte and ASCII form in the output.
The forms of the D command are:

(a) Ds
(ob) Ds,f
(c) D

(d) D,f
(e) DWs
(f) DWs,f
(g) DW
(h) DW,f

Forms (a) through (d) display memory in byte format,
while forms (e) through (h) display memory in word format.
The byte format display appears as:

aaaa bb bb bb... bbcc...ce

---
_Page 22_

SID Users Guide Section 3

where aaaa is the base address of the display line and_ the
sequence of (up to) 16 bb vairs represents the hexadecimal
representation of the data stored starting at address aaaa.
The sequence of c's represent the same data area displayed
in ASCII format, where possible. A period (.) is displayed
as a place holder when the data item does not correspond to
a graphic character.

Byte mode displays are "normalized" to address
boundaries which are a multiple of 16. That is, if the
Starting address aaaa is not a multiple of 16, then the
display line is printed to the next boundary address which
is a multiple of 16. Each display line which follows
contains 16 data elements until the last display line is
encountered.

Command forms (e) through (h) display in word mode
which is similar to the byte mode display described above,
except that the data elements are printed in a double byte
format:

aaaa WWWW WWWW . . « WWWW CC . . . CC

where aaaa is the starting address for the display line and
the sequence of (up to 8) wwww's represent the data items

which are stored in memory beginning at aaaa. Similar to
the byte mode display, the sequence of c's represent the
decoded ASCII characters starting at address aaaa. As in

the byte mode display, a period is displayed as a place
holder when the character in that position is  non-graphic.
Contrary to the byte mode display, address normalization to
modulo 16 address boundaries does not occur in the word mode
display. Recall that 80889 double words are stored with the
least significant byte first, and thus the word mode display
reverses each byte pair so that the individual data items
are displayed as four digit hexadecimal numbers with the
most significant digits in the high order positions.

Command form (a) displays memory in byte format
starting at location s for 1/2 of a standard CRT screen (12
lines). This form of the command is useful when the
operator wishes to view a segment of memory beginning ata
particular position, with an indefinite ending address.
Command form (b) is similar to (a), but specifies a
particular ending address. In this case, the start address
is taken as s with the display continuing through address f.
Recall that excessively long typeouts can be aborted by
depressing any keyboard character, such as a return. Form
(c) is similar to (a) and (b), except the starting address
for the display is taken from the last displayed address, or
from the value of the memory address registers (HL) in the
case that no previous display has occurred since the last
breakpoint. It is often convenient, for example, to use
form (a) to display a segment of memory, followed by a

---
_Page 23_

SID Users Guide Section 3

sequence of D commands of form (c) to continue the display.
Each D command displays another 1/2 screen of memory.
Command form (d) is similar to (b) except the starting
address is taken automatically as described in form (c)
above.

Assume, for example, that decimal values 1 through 256
are stored in memory starting at hexadecimal address §9100.
The command:

D198,12A
will produce the expanded form of the display shown below:

180 91 02 O03 84 (etc.) BE OF 18 .. (etc.) ..
9116 11 12 13 14 (etc.) 1E 1F 20 .. (etc.) .
@120 21 22 23 24 (etc.) 29 2A 2B !"#S%&'() \*+

Command forms (e) through (h) parallel the byte
display formats given by (a) through (h), except that the
display is given in word format. Form (e) displays in word
format from location s for 1/2 screen, while form (f)
displays from location s through location f. Form (g)
displays from the last display location, or from HL if there
has been an immediately preceding breakpoint with no
intervening display. Form (h) is similar to (g), but
displays through location f. The command:

DW106,128

for example, produces the expanded form of the following
output lines:

©1806 @201 0483 (etc.) BE@D 1OOF .. (etc.) ..
@11@ 1211 1413 (etc.) 1lE1D 2@1F .. (etc.) .
@126 2221 2423 (etc.) 2928 2B2A 1"#S%&' () \*+

Examples of valid D commands are:
DF3F
D#100,#280
D.GAMMA, . DELTA+#36
D.GAMMA
DW@ALPHA, +#198
3.4. The Fill Memory (F) Command.

The F command fills memory with a constant byte value,
and takes the form:

Fs,f,a
where s is the starting address for the fill, f£ is the

ending (inclusive) address for the fill, and d is the 8-bit
data item to store in locations s through f. It is the

---
_Page 24_

SID Users Guide Section 3

operator's responsibility to not fill memory locations which
are occupied by the resident portions of CP/M, including
areas reserved for SID. Examples of valid F commands are:

F18@,3FF,FF
F.GAMMA,+#100,#23
F@ALPHA, +=1,=X

3.5. The GO (G) Command,

The G command is used to pass ovrogram control to a
program under test. Execution proceeds in real-time from
the address specified by the G command. That is, the G
command releases processor control from SID to the program
under test. Execution does not return to SID until a break
or pass point is reached (see the "P"“ command for a
discussion of pass points). The operator can force a return
to SID, however, by interrupting the processor with a
“restart 7" (RST 7), provided by the program under test, or
forced by external hardware such as front panel control
switches, if available.

The several G command forms are:

(a) G

{(b) Gp

(c) G,a

(d) Gp,a

(e) G,a,b

(£) Gp,a,b

(g) -G

(h) —-Gp

(i) -G,a

(j) -Gp,a

(k) -G,a,b

(1) -G p,a,b
Forms (a) through (f) start test program execution

with symbolic features enabled, while forms (g) through (1)
are identical in function, but disable the symbolic features
of SID. In particular, form (a) starts test program
execution from the program counter (PC) given in the machine
state of the program under text (see the "X"“ command for
Machine state display). In this case, no breakpoints are
set in the test program. Form (b) is similar to (a), but
initializes the test program's PC to p before’ starting
execution. Again, no breakpoints are set in the test
program. Similar to (a), form (c) starts execution from the
current value of PC but sets a breakpoint at location a.
The test program receives control and runs in real-time
until the address a is encountered. Note that control will
return to SID upon encountering a pass point or RST 7, as
described above.

Upon encountering the breakpoint address a, the break

---
_Page 25_

SID Users Guide Section 3

address is printed at the console in the form:
\*a 4S

where s is the first symbol in the table which matches
address a, if it exists. Note that the temporary breakpoint
at address a is automatically cleared when SID returns to
command mode (see the "P" command for permanent
breakpoints).

Form (d) combines the functions of (b) and (c): the
test program PC is set to the address p and a temporary
breakpoint is set at location a. As above, the breakpoint
is cleared when location a is encountered. It should be
noted that an immediate breakpoint will always occur if p =
a. If this is desired, however, the operator can use the
trace function to single step past the current address,
followed by a G command (see the "T" command for actions of
the trace facility).

Form (e) extends the breakpoint facility by allowing
two temporary break addresses at a and b. Program execution
begins at the current PC and continues until either address
a or b is encountered. Both temporary break addresses are
cleared when SID returns to command mode. Form (f) is
Similar to (e), except the initial value of PC is set to
location p before starting the test program.

It should be noted that the instruction at a
breakpoint address is not executed when the G command is
used. Suppose, for example, that a subroutine named TYPEOUT
is located at address §392 in a test program, consisting of
the machine code:

TYPEOUT:
6302 MOV C,A
0303 MVI C,2

8385 JMP 6005

Suppose further that the overator is testing a program which
makes calls on the TYPEOUT subroutine where a break address
is to be set. The command:

G,.TYPEOUT
is entered by the operator. Test program execution proceeds
from the current PC value and stops when the TYPEOUT
subroutine is reached, with the breakpoint message

\*§302 .TYPEOUT

indicating that control has returned from the test program
to the SID debugger. At this point the program counter of
the test program is at location 8302 (i.e., .TYPEOUT), and
the instruction at this location has not yet been executed.

---
_Page 26_

SID Users Guide Section 3

The operator can execute through the TYPEOUT subroutine
using any of the commands G, T, or U. One useful command in
this situation is

nm

G,

which continues execution from 9382, and sets a breakpoint
at the topmost stacked element (given by ""“"). Since the
topmost stacked element must be the subroutine’ return
address, this particular G command has the effect of
executing the TYPEOUT subroutine, with a break upon return
to the instruction following the original call to TYPEOUT.

Command forms (g) through (1) correspond directly to
functions (a) through (f), except that vass points are not
displayed until the corresponding pass counters reach 1 (see
the "P" command for details of intermediate pass point
display).

Note that the essential difference between the G
command and the U (Untrace) command is that execution
proceeds unmonitored in real-time with the G command, while
each instruction is executed in single-step mode when the U
command is used. Fully-monitored execution under the JU
command has the advantage that the operator can regain
control at any point in the test program execution.
However, execution time of the test program is seriously
degraded in Untrace mode since automatic breakpoints are set
and cleared following each instruction.

Examples of valid G commands are:

G100
G160,103
G.CRLF,.PRINT, #1624
G@JMPVEC+=1,.ENDC,.ERRC
G,.ERRSUB
G,.ERRSUB,+30
-G100,+19,+10

3.6, The Hexadecimal Value (H) Command.
The H command is used to perform hexadecimal

computations including number base conversion operations.
The forms of the H command are:

(a) Ha,b
(b) Ha
(c) 4A

Form (a) computes the hexadecimal sum and difference using
the two operands, resulting in the display:

ssss dddd

where ssss is the sum atb, and dddd is the difference a-b,

---
_Page 27_

SID Users Guide Section 3

ignoring overflow and underflow conditions.

Form (b) is used to perform number and character
conversion, where a is a symbolic expression. The display
format in this case is:

hhhh #ddddd 'c‘ .s

where hhhh is the four digit hexadecimal value of a, #ddddd
is the (up to) six digit decimal value of a, c is the ASCII
value of a when a is graphic, and s is the first symbol in
the table which matches the value a, when such a symbol
exists. Assume, for example, that the symbol GAMMA is
located at address 8108, as in previous examples. The H
commands shown to the left below result in the displays
shown to the right below:

COMMAND RESULTING DISPLAY
HO,1 Q001 FFFF
H41 0041 #65 ‘A’
H100 @18@ #256 .GAMMA
H. GAMMA @100 #256 .GAMMA
H=GAMMA @G01 #1
H@GAMMA @201 #513
HPF+@GAMMA @100 #256 .GAMMA
H'A' @041 #65 ‘a!
H'A'+=GAMMA 9042 #66 '‘'B!

Command form (c) prints tne complete list of symbols
along with their corresponding address values. The list is
printed from the first to last symbol loaded, and can be
aborted during typeout by depressing any keyboard character,

3.7. The Inout Line (I) Command.

When testing programs which run in the CP/M
environment, it is often useful to simulate the command line
which is normally prepared by the CCP upon program load.
The form of the I command is:

Icccce ... ccc

where the sequence of c's represent ASCII characters which
would normally follow the test program name in the CCP
command line. For example, the CP/M "DUMP" program is
normally started in CCP command mode by typing:

DUMP X,.COM

which causes the CCP to search for and load the DUMP.COM
file, and pass the file name “X.COM" as a parameter to the
DUMP program, In particular, the CCP initializes two
default file control blocks, along with a default command
line which contains the characters following the DUMP
command.

---
_Page 28_

SID Users Guide Section 3

In order to trace and debug a program such as_ DUMP,
the SID program would normally be invoked by typing:

SID DUMP.COM

which loads the command file containing the DUMP machine
code. If the symbol table is available, the SID invocation
would be:

SID DUMP.COM DUMP,SYM

In either case, SID loads the DUMP program and prompts’ the
console for a command. In order to simulate the CCP's
command line preparation, the operator would then type:

IX.COM

where the "I" denotes the Input command, which is followed
by the simulated command line. The operator may then
commence the debug run with default areas properly setup.

The I command specifically initializes the default
file control block in low memory, labelled DFCB1l, which is
normally located at 905C. The file control block which is
initialized by the I command is complete in the sense that
the program can simply address DFCBl and perform and_ open,
make, or delete operation without further initialization.
As a convenience, a second file name is initialized at
location DFCB2, which is at address DFCB1+0019
(hexadecimal). It is the programmer's responsibility to
move the second drive number, file name, and file type _ to
another region of memory before performing file operations
at DFCB1 since the 16-byte region at DFCB2 will be
immediately overwritten by any file operation. Further, the
default buffer, labelled DBUFF, is initialized to contain
the entire command line with a preceding blank character.
In a standard CP/M system, the DBUFF area is assumed to. be
located start at 9886 and end at OOFF. Note, however, that
the I command restricts the simulated CCP command line to 63
characters since SID's line buffer is used in the
simulation.

Given an I command of the form:
I dl:f1.tl d2:£2.t1

where dl: and d2: are (optional) drive identifiers, fl and
f2 are (up to eight character) file names, and tl and t2 are
(up to three character optional) file types, two default
file control block names are prepared in the form:

DFCB1: dl' f1' tl' 98 88 88 BB

DFCB2: d2' £2' t2' 868 OH BH OB
08 (current record field)

---
_Page 29_

SID Users Guide Section 3

If dl: is empty in the original command line, then di' = 9@-
(which automatically selects the default drive), otherwise
if dl =A, B, C, or D, then dit = @1, 92, 63, or 64,
respectively, which properly initializes the file control
block for automatic disk selection, Field f1' is
initialized to the ASCII file name given by fl, padded to an
eight character field with ASCII blanks. Similarly, tl' is
initialized to the ASCII file typve, padded with blanks ina
field of length three. Lower case alphabetics in dl, fl,
and tl are translated to upper case in dl’, fl‘, and tl',
respectively. Names which exceed their respective length
fields are truncated on the right. Finally, the extent
field is zeroed in preparation for a BDOS call to open or
make the file.

The second default file control block given by d2, f2,
and t2 is prepared in a similar fashion and stored starting

at location @@6C. Note that the current record field at
location 9@7C is also initialized to 99. If any of the

fields f1, tl, £2, and t2 are not included in the command
line, their corresponding fields in the default file control
blocks are filled with blanks.

Ambiguous references which use the "“\*" or “"?®
character are processed in the same manner as in the CCP:
the "\*" symbol in a name or type field causes the field to

be right-filled with "“?" characters. The inout lines’ shown
below illustrate the default area initialization which takes
place for various unambiguous and ambiguous file names. The
areas shown to the right give the hexadecimal values which
begin at the labelled addresses, where ASCII values A, B, C,
and D have the hexadecimal values 41, 42, 43, and 44,
respectively. Further, the special characters ":", ".",
“\*" and "?" have the ASCII encoded values 3A, 2E, 2A, and
3F, while an ASCII space has the hexadecimal value 29;

COMMAND LINE DEFAULT DATA AREA INITIALIZATION

I DFCB1l: 09
26 26 28 28 2EA2@ 29 20
20 20 28 O08 O68 OO BO
DFCB2: @@
20 28 20 28 28 28 28 28
20 26 20 08 OH OO Ob

DBUFF: @8 6¥

---
_Page 30_

SID Users Guide Section 3

I A.B DFCB1: We
41 20 20 20 20 20 20 28
42 20 20 09 O08 OB BB
DFCB2: @B
20 20 20 20 20 28 20 20
20 20 20 60 88 BD BO

DBUFF: 865 20 20 41 2E 42 @@

TA:B.C b:d.e DFCB1: @1
42 20 20 28 28 28 28 20
43 26 280 68 88 8H BB
DFCB2: 92
44 28 286 28 20 28 28 29
45 20 280 08 68 0H BO

DBUFF: @B 41 3A 42 2E 43 290
42 3A 44 26 45 60

I AA\*.B?C D: DFCB1L: 00
41 41 3F 3F 3F 3F 3F 3F
42 3F 43 68 OB OH BB
DFCB2: 64
26 26 20 28 26 28 28 28
28 28 20 O08 BH BH BB

DBUFF: #C 208 20
41 41 2A 2E 42 3F 43
20 44 3A 00

Note that the I command is used in conjunction with
the R command to read program files and symbol tables after
SID has initially loaded. Details of the use of I in this
Situation are given with the R command which follows.

Additional valid I commands are given below:

I x.dat
Ix.inp y.out
Ta:x.inp b:y.out $-p
ITEST.COM
I TEST.HEX TEST.SYM

3.8. The List Code (L) Command.
The L command disassembles machine code in the memory
of the machine, with symbolic labels and operands placed in

the appropriate fields, where possible. The form of the L
command are:

---
_Page 31_

SID Users Guide Section 3

(a) Ls
(bo) Ls,f€
(c) L

(d) --Ls
(e) -Ls,f
(f) <-L

Form (a) list disassembled machine code starting at
symbolic location s for 1/2 CRT screen (12 lines). Form (b)
specifies an exact range for disassembly: s specifies the
starting location, and f£ gives the final disassembly
location. Form (c) is similar to (a), but disassembles from
the last listed, assembled (see the A command), traced (see
the T and U commands), or break address (see the G and P
commands). Since form (c) also lists 1/2 CRT screen, it is
often used following form (a) to continue the disassembly
process through another segment of the program. Forms (qd)
through (£) parallel (a) through (c), but disable the
symbolic features of SID. In particular, the minus prefix
prevents any symbol lookup operations during the
disassembly.

The format of the L command output is:
SSSSS:
aaaa opcode operand .ttttt

where “sssss:" represent a symbol which labels the program
location given by the hexadecimal address aaaa, when the
symbol exists. The “opcode" field gives the 8888 mnemonic
for the instruction at location aaaa, and the "operand”
field, when present, gives the hexadecimal values which
follow the opcode in memory. The symbol "“.ttttt" is printed
when the instruction references a memory address which
matches a symbol in the table. Note that instructions may
directly reference memory through their operand fields
(e.g., CALL, JMP, LDA, LHLD), while other instructions imply
a memory address (e.g., STAX B, LDAX D). Instructions which
reference memory, such as INR M, are listed with the memory
operand in the form:

opcode M =hh
where "opcode" is the memory referencing instruction, and hh
is the hexadecimal value contained in the memory address

given by the HL register pair before the operation takes
place.

When the operation code at the list address is not a
valid 8988 mnemonic, the output form is:

??= hh

where hh is the hexadecimal value of the invalid operation
code.

---
_Page 32_

SID Users Guide Section 3

Several valid L commands are listed below.

L190
L#1024, #1034
L.CRLF
L@ICALL, +30
~L,PRBUFF+=1,+'A'

3.9. The Move Memory (M) Command.

The M command allows the operator to move blocks’ of
data values from one area of memory to another. The form of
the M command is:

Ms,h,a

where s is the start address of the move operation, h is the
high (last) address of the move, and d is the starting
destination address to receive the data. Data moves one
byte at a time from the start address to the destination
address. Each time a byte value is moved, the start and
destination addresses are incremented by one. The move
process terminates when the start address increments past
the final f address. The command:
M100,1FF,3000

for example, replicates the entire block of memory from 9199
through @1FF at the destination area from 3909 through 30FF
in memory. The data block from 46199 through O1FF remains
intact.

Note that data areas may overlap in the move process:
the command

M10@,1FF,101
shows an instance where the value at location 9100 is

propagated throughout the entire block from 9101 through
O1FF.

A number of valid M commands are listed below:
M-100,FFDO,109
M.X,+=Z,.Y
M.GAMMA,+FF,.DELTA
M@ALPHA+=X,+#59,+100
3.18. The Pass Counter (P) Command.

The P command allows the operator to set and clear
“nass points" and "pnass counts" in the program under test.

---
_Page 33_

SID Users Guide Section 3

The forms of the P command are:

(a) Pp
(b) Pp,c
(c) P
(d) -Pp
(e) -P

A "pass point" is a program location to monitor during
execution of the test program. Similar to a temporary
breakpoint (see the G command), a pass point causes SID to
stop execution of the test program each time an active pass
point is reached. Unlike a temporary breakpoint, a pass
point is not automatically cleared each time it is reached
during execution. Further, unlike a temporary breakpoint, a
pass point break occurs after the instruction as the pass
address is executed. In this way, the operator can _ simply
continue the execution of the test program under control of
a G command until the next pass point is executed, or until
a temporary breakpoint is reached.

Each pass point can have an optional “pass count"
which defaults to the value 1. The pass count enhances this
facility by allowing several passes through a pass point
before the break actually occurs. In particular, a pass
count in the range 1-FF (decimal 1 through 255) can be
associated with a particular pass point. Each time a pass
point is executed, itS corresponding pass count is
decremented. The decrementing process proceeds until the
pass count reaches 1, at which time the break address is
printed and execution of the test program stops. When a
pass count reaches 1, the pass point becomes a permanent
break address which halts execution each time the
instruction is executed, Note that a pass count does not
change once it has reached l.

Form (a) sets a pass point at address p with a pass
count of 1, causing address p to become a permanent
breakpoint. Form (b) is similar, except that the pass count
is initialized to c. Up to eight distinct pass points can
be actively set at any particular time. Form (c) displays
these active pass points in the format:

cc pppo .sssss

where cc is the hexadecimal value of the pass count which is
currently associated with the pass address pppp, and = sssss
is a symbol which matches the address pppp, if such a symbol
exists.

Form (d) clears the pass point at address p, while
form (e) clears all active pass ovoints. Note that the
command:

Pp,@

---
_Page 34_

SID Users Guide Section 3

is equivalent to form (d).

Each time a vass point is encountered, SID prints’ the
pass information in the format:

cc PASS pppp .sssss

where cc is the current pass count at pass point pppp (cc is
decremented when greater than 1). As above, the symbol
sssss corresponding to address pppp is printed when
possible.

The special command forms "-G" and "-U" can be used to
disable the intermediate pass trace as the counters are
decremented down to 1. Suppose, for example, the TYPEOUT
subroutine is a part of a program under test, as shown in
the G command above. The command:

P.TYPEOUT, #30

is issued by the operator. The effect of this particular P
command is to set a paSS point at the location labelled by
“TYPEOUT” which is assumed to exist in the symbol table.
The pass count is set to decimal 34, which allows the pass
point to execute 38 times before a breakpoint is taken.
Given that the pass point at TYPEOUT is in effect, the
command:

G
Starts execution of the test program with no temporary
breakpoint. Bach time the pass point is executed, the pass
trace:

1E PASS @3@2 .TYPEOUT
(register trace)
1D PASS @302 .TYPEOUT
(register trace)
1C PASS 9302 .TYPEOUT
{register trace)

@1 PASS 6302 .TYPEOUT
(register trace)
\*303

where the “register trace" shows the state of the CPU
registers before the "MOV C,A" at TYPEOUT is executed (see
the "X" command for register display format). Note that the
final breakpoint address is 0383, which follows the "MOV"
instruction at the pass address 8302. The operator can
depress any keyboard character during the pass point trace,
and SID will immediately stop execution following the
instruction at the pass point address. If instead, the
command

---
_Page 35_

SID Users Guide Section 3

-G

had been issued above, the intermediate pass traces would
not appear at the console. In this particular case, only
the final trace:

@1 PASS @302 .TYPEOUT
(register trace)
\* 303

is printed. Although the intermediate pass traces are not
displayed, the operator can abort execution by depressing a
keyboard character: if an intermediate pass point is
encountered with trace disabled, SID aborts execution and
returns control to the keyboard.

Temporary breakpoints can also be set while ovass
points are in effect. That is, commands such as

Ga,b Ga,b,c G,b G,b,c

can be issued which intermix with the permanent breakpoints
which are set with the P command. Note, however, that
permanent breakpoints override the temporary breakpoints
which are given by b and c when they occur at the same
address. Further, T and U command can be used to trace
sections of the test program while permanent breakpoints are
in effect. In this case, the pass counts decrement as
described above, with a break taken when the count reaches

Valid P commands are shown below:
P1O00,FF
P.BDOS
P@ICALL+34,#20
-P.CRLF

3.11. The Read Code/Symbols (R) Command,

The R command is used in conjunction with the I
command to read program segments, symbol tables, and utility
functions into the transient program area. The forms of the
R command are:

(a) R
(b) Rd

The I command is first used to set the file names which will
be involved in the read operation. Form (a) reads’ the
program and/or symbol table given by the I command without
applying an offset to the load addresses. Form (b) adds the
displacement value d to each program load address and/or
symbol table address. Note that this addition takes place
without overflow checks so that negative bias values can be

---
_Page 36_

SID Users Guide Section 3

aoplied. As a simple case, the usual initiation of SID:
SID X.COM

could be replaced by the sequence of commands:

SID Starts SID without a test program
IX,.COM Initialize the inout line
R Read the test program to memory

The response from SID in this case iS exactly the same as
the normal initialization, with the “NEXT PC END" message as
described in Section l. .

A program and symbol file can be read by preceding the
R command with an I command of the form:

I x.y u.v
where x.y is the program to load, and u.v is the symbol
table file. Note that y is usually the type "COM", x is
usually the same as u, and v is usually the type "SYM".
Thus, a typical command sequence of this form would be

IDUMP.COM DUMP.SYM

R
which reads the DUMP.COM program file into the Transient
Program Area, and loads the symbol table with the
information given by DUMP.SYM. Programs with file type

"HEX" load into the locations specified in the Intel
formatted hexadecimal records, while programs with file type
“UTL" are assumed to be SID utility functions which load and
relocate automatically. All other file types are assumed
absolute, and load starting at the base of the transient
area. Utility functions automatically remove any existing
symbol information when they relocate, but in all other
cases the symbol load operations are cumulative. In
particular, the special input form:

I\* u.v
R

skips the program load since there is an asterisk in the
program name position, and loads only the symbol table file.
Thus, a sequence of the above form could be used to load the
symbol tables for selective portions of a large program
which was initially developed in small modules.

Suppose, for example, that a report generation program
has been developed using MAC, which consists of the several
modules:

---
_Page 37_

SID Users Guide Section 3

IOMOD,.ASM I/O Module

SORT.ASM File Sorting Module
MERGE.ASM File Merge Module
FORMAT. ASM Report Format Module
MAIN. ASM Main Program Module
DATA.ASM Common Data Definitions

Suppose further that each module has been sevarately
assembled using MAC, resulting in several “HEX" and "SYM"
files corresponding to the individual program segments. The
program segments have been brought together using SID to
form a memory image by typing the sequence of commands:

SID Start the SID program
ITOMOD.HEX Initialize IOMOD

R Read I/O Module
ISORT. HEX Initialize SORT

R Read Sort Module
IMERGE.HEX Initialize MERGE

R Read Merge Module
IFORMAT.HEX Initialize FORMAT

R Read Format Module
IMAIN.HEX Initialize MAIN

R Read Main Module
IDATA. HEX Initialize DATA Area
R Read Initialized Data

Following this sequence, the Transient Program Area contains
the complete memory image of the report generation o»program,
Suppose the information printed following the last R command
is:

NEXT PC END
1B3E 9100 8E8O@

which indicates that the high memory address is 1B3E. Using
the H command:

H1B

the operator finds that 1B (hexadecimal) pages is the same
as 27 (decimal) pages. At this point, the operator returns
to CCP mode by typing either a control-C (warm start), or
"G6" command, which leaves the memory image intact. The
command:

SAVE 27 REPORT.COM
is then issued to create a memory image file on _ the
diskette, The operator then re-enters SID using a command
of the form:

SID REPORT.COM

to load the entire module for testing. Individual portions

---
_Page 38_

SID Users Guide Section 3

of the report generator can then be symbolically accessed by
selectively loading symbol tables from the original modules.
For example, the MAIN and SORT modules could be debugged by
subsequently loading the corresponding symbol information:

I\* MAIN.SYM

R
I\* SORT.SYM
R
which readies the symbol information for subsequent

debugging. Individual segments of the report generator are
then tested and reassembled. If an error is found in the
SORT module, for example, the SORT.ASM file is edited to
make necessary changes, and the module is reassembled with
MAC, resulting in new "HEX" and "SYM" files for the SORT
module only. Given that enough “expansion” area has been
provided following the SORT module, SID is reinitiated and
the SORT module is included:

SID REPORT.COM
ISORT.HEX SORT.SYM
R

which overlays the changed SORT module in the original
report generator memory image. The operator may then load
addition symbol tables by typing I and R commands such as:

I\* MAIN.SYM
R
I\* DATA.SYM
R

in order to access symbols in the SORT, MAIN, and DATA
modules.

Note that several symbol table files can be
concatenated using the PIP program (see the "CP/M Features
and Facilities" manual for PIP operation) in command mode.

For example, the PIP command:
PIP NOBUGS.SYM=IOMOD.SYM,SORT.SYM,MERGE.SYM,FORMAT.SYM

creates a file called NOBUGS.SYM which holds the symbols for
IOMOD, SORT, MERGE, and FORMAT. The SID command:

SID REPORT.COM NOBUGS.SYM

loads the memory image for the report generator, along with
the symbol tables for these particular modules. Additional
symbol files can then be selectively loaded using I and R
commands. The symbol file for the entire memory image can
then be constructed using the PIP command:

PIP REPORT.SYM=NOBUGS.SYM,MAIN.SYM,DATA.SYM

---
_Page 39_

SID Users Guide Section 3

which allows the operator to type
SID REPORT.COM REPORT.SYM

in order to load the memory image for the report generator,
along with the entire symbol table. Recall, however, that
the symbol table is always searched in load-order, and thus
symbol names which are the same in two module must be
distinguished using qualified symbolic names (see Section
1).

As mentioned above, form (b) allows a displacement
value d to be added to each program address and symbol
value. The displacement value has no effect, however, when
the program is a SID utility (file type "UTL"“). The
commands

IDUMP.HEX DUMP.SYM

R196
for example, cause the DUMP program to be loaded 189088
(hexadecimal) locations above its normal origin, with
properly adjusted symbol addresses. Note that the bias

value can be any symbolic expression, and thus the command:
R-289

first produces a (two's complement) negative number which is
added to each address. Since overflow from a 16-bit counter
is ignored, this R command has the effect of loading the
vrogram 29@ (hexadecimal) locations below the normal load
address, with symbol addresses biased by this same amount.

Error reporting during the R command is limited to the

standard “?" response, which indicates that either the
program or symbol file does not exist, or the program or
symbol file is improperly formed. Similar to the SID

startup messages, the response
SYMBOLS

occurs following program load, and appears before the symbol
load. Thus, a "?" error before the SYMBOLS’ response
indicates that the error occurred during the program load,
while the “?" error after the SYMBOLS message indicates that
an error occurred during the symbol file load operation.
The exact position of a symbol file error can be found by
subseguently using the H command to view the portion of the
symbol table which was actually loaded.

3.12, The Set Memory (S) Command.

The S command allows the operator to enter data into
Main memory. The forms of the S command are:

---
_Page 40_

SID Users Guide Section 3

(a) Ss
(b) SWs

Form (a) allows data to be entered at location s in byte
(8-bit) or character string mode, while form (b) is used to
store word (16-bit) mode data items. In either case, the
SID program proceeds to prompt the console with successive
addresses, starting at location s, along with the data item
presently located at that address. As each line prompt
occurs, the operator has the option of typing a_ single
carriage return or typing a symbolic expression (followed by
a carriage return) which is evaluated and becomes the new
data item at that location. If a single carriage return is
typed, then the data element at that location remains
unchanged. The S command terminates whenever an invalid
data item is detected, or when the operator types a single
“." followed by a carriage return. Form (a) allows single
byte data, and produces the standard "?" when a double byte
value is entered with a non-zero high order byte. In
addition, form (a) also allows long ASCII string data to be
entered in the format:

“ccecce . . .cccecec

where the sequence of c's represent graphic ASCII characters
to be entered at the prompted location. No translation from
lower to upper case takes place during entry. Further, the
next vromoted address is automatically set to the first
unfilled location following the input string.

A valid input sequence following the command:
S104
is shown below, where the SID prompt is given on the left,

and the overator's input lines are shown to the right, where
"cr" denotes the carriage return key.

SID PROMPT OPERATOR INPUT
@106 C3 34cr

8181 24 #254cr

@192 CF cr

0103 4B "ASCIIcr
@1088 6E =X+5cr

8199 &2 'S'or

@10A D4 ecr

A valid double byte input sequence following the
command

SW. X+#30

is shown below:

---
_Page 41_

SID Users Guide Section 3

SID PROMPT OPERATOR INPUT
2980 @86D 44Fecr

2302 4F32 @GAMMAcr
2304 33E2 cr

2306 FFIl ~X+=1-#290cr
2308 348F -cr

3.13. The Trace Mode (T) Command.

The T command allows the operator to single or
multiple step a test program while viewing the CPU registers
as they change. In addition, the T command can be used in
conjunction with SID utilities to collect test program data
for later display (see the section entitled "SID
Utilities"). The forms of the T command are:

(a) Tn
(b) T
(c) Tn,c
(d) T,c

(e) -T (with options a - qd)
(f) TW (with options a - d)
(g) -TW (with options a- d)

Form (a) traces program execution from the current value of
the program counter PC (see the "X" command for PC value as
well as the format of the CPU state display). Form (b) is
the trivial case of (a), with an assumed single step count
of n= 1. In either case, the SID program displays the
register state, along with the decoded instruction (assuming
"A" is not in effect) before each instruction is executed.
For example, the command:

T4
traces four program steps, producing the format:

(register state 1) opcode 1

label:
(register state 2) opcode 2
label:
(register state 3) opcode 3
label:

(register state 4) opcode 4 \*bbbb

showing the register state before each corresponding
operation code is executed. Each operation code is written
in the same format as in the L and xX commands, with
interspersed symbolic operands decoded wherever ovossible.
The interspersed labels show program addresses when they
occur in the flow of execution. The final break address,
denoted by “\*bbbb" above, shows the value of the program
counter after opcode 4 is executed. The CPU state can

---
_Page 42_

SID Users Guide Section 3

optionally be displayed at this point by typing the single
character "X" command,

Forms (c) and (d) are used only in conjunction with
the SID utilities, and automatically perform a CALL c after
each instruction executes. The value of c corresponds to a
utility entry address for data collection. Details of the
use of these forms are given in sections which follow.
Note, however, that form (d) is equivalent to (c) with a
Single step count of n= 1.

Forms given by (e) parallel (a) through (e), but the
preceding minus sign disables the symbolic features of SID.
In particular, neither the symbolic operands nor the
symbolic labels are decoded in the trace process. This
option increases the operation of SID slightly in trace mode
when large symbol tables are present.

Forms given by (f) parallel (a) through (d), but
perform a "trace without call" function. It is often
useful, for example, to trace mainline program code, but not
trace into the subroutines which are called from _ the
Mainline execution. The TW command performs this function
by running the test program in real time whenever a
subroutine is entered, returning to fully traced mode upon
return to the current subroutine level. If a return
operation takes place at the current level (i.e., a RET is
executed in fully traced mode), then tracing continues at
the encompassing subroutine or mainline program level. For
example, suppose the mainline and subroutine structure shown
below exists in a particular program:

MAINLINE SUBROUTINE 1 SUBROUTINE 2 ... SUBROUTINE n

. 8 Sl: MOV A,C S2: MOV A,D Sn: MOV A,L

CALL Sl oe 6 eo 8 oe e

MOV B,C CALL $2 eo 8 eo 8

MOV C,D MOV C,E CALL $3... MOV C,L

woe MOV D,E MOV 0D,H MOV D,L

JMP @080 RET RET RET
Suppose further that the test program is stopped within
subroutine Sl before the call to subroutine 82. The
command:

T#199

would have the effect of tracing from Sl through S2, S3, and
so-forth until level Sn is encountered. Although this’ form
of the trace could be useful, it is often more enlightening
to trace only at a particular subroutine level, and view the
effects of the subroutine levels above Sl. In this manner,
an offending subroutine is often easily discovered without
tracing non-essential program flows. If instead, the

---
_Page 43_

SID Users Guide Section 3

command:
TWH100

is typed while at subroutine level Sl, all subsequent levels
from S2 and beyond are executed in real time as if a "G"
command had been performed at each CALL within Sl. Upon
executing the RET instruction within Sl, tracing resumes at
the mainline level. Any subroutine calls following CALL Sl
at the main level are not subsequently traced.

Forms given by (g) parallel (a) through (d), but
disable the symbolic features of SID in the same manner as
form (e).

It should be noted that SID allows tracing up to’ Read
Only Memory (ROM) program code. At the point ROM is
entered, SID stops the trace operation, and runs the ROM
code in real time. An automatic breakpoint is set which
intercepts program control when ROM code is exited. The
assumption, however, is that ROM code was entered via a
subroutine call (CALL or RST n), or via a PCHL or JMP

instruction. In any case, the return address following the
ROM execution is taken as the topmost address in the test
program's stack. Note further that SID does not trace

execution of calls through the BDOS code, since these
operations are often guite lengthy, and may occassionally
require real time operation to perform various disk
functions. Thus, entry to the BDOS is intercepted by SID,
and resumed following completion of the BDOS function.

Tracing can be aborted at any time by depressing a
keyboard character. Do not use the RST instruction to
terminate trace functions.

valid trace commands are shown below:

T100
T#30,.COLLECT
~TW=1,3E03

3.14. The Untrace Mode (U) Command.

The U command is similar to the T command given above,
except that the CPU register state is not displayed at each
step. Instead, the test program runs fully monitored so
that program execution can be aborted at any time, or for
the collection of data for a SID utility function. The
forms of the U command parallel the T command:

---
_Page 44_

SID Users Guide Section 3

(a) Un

(b) JU

(c) Un,c

(d) U,c

(e) -U (with options a -~ d)

(£) UW (with options a - d)
(g) -UW (with options a - d)

Forms (a) through (d) perform the analogous functions of the
"tT" command forms (a) through (d), without displaying the
register state at each step. Forms given by (e) differ from
the T command, however: instead of disabling the symbolic
features, command forms

-Un -U -Un,c -U,c

disable the intermediate pass point display (see the "Pp"
command), until the corresponding pass counts reach l.

Forms given by (f) correspond to the "“T" command
exactly, except that the trace display is disabled. In this
case, the current subroutine level is run fully monitored,
but higher subroutine levels run in real time.

Forms given by (g) are similar to (f), but disable the
pass point display, as described above.

Similar to the T command, execution can be aborted in
untrace mode by depressing any keyboard character. The
break address is displayed, and control returns to SID
command mode.

Valid U commands are given below:
UFFFF
U#19000,.COLLECT
UW=GAMMA, .COLLECT
3.15. The Examine CPU State (X) Command.
The X command allows the operator to examine and alter

the CPU state of the program under test. The forms of the X
command are:

(a) xX
(b) Xf£
(c) Xr

Form (a) displays the entire CPU state in the format:
CZMEI A=aa B=bbbb D=dddd H=hhhh S=ssss P=pppp op sym

where

C, 2, M, E, and I represent the true or false conditions of

---
_Page 45_

SID Users Guide Section 3

the CPU carry, zero, minus, even parity, and interdigit
carry, respectively. If the position contains a "“-" then
the corresponding flag is false, otherwise the flag letter
is printed. The byte value aa is the value of the A
register, while the double byte values bbbb, dddd, hhhh,
ssss, and pppp, give the 16-bit values of the BC, DE, UHL,
Stack Pointer, and Program Counter, respectively. The field
marked "“op“ gives the decoded mnemonic instruction at
location pppp, unless "-A" is in effect, in which case the
hexadecimal value of the operation code is orinted. The
"sym" field contains a decoded operand, when vossible.
Refer to the L command for the format of the symbolic
instruction decoding. The single letter "X" command might
result in a display of the form:

C-M-- A=93 B=34EF D=2000 H=334E S=4323 P=8100 LDA 0223 .Q

which, for example, indicates that the carry and minus flags
are true, while the zero, even parity, and interdigit carry
flags are false. Further, the A register contains 63, while
the B, C, D, E, H, and L registers contain the hexadecimal
values 34, EF, 26, 08, 33, and 4E, respectively. The value
of the Stack Pointer register is 4323, and the Program
Counter is at location @19@0. The next instruction to
execute at location 69198 is an accumulator load (LDA) from
location 9233. Further, the first symbol in the table which
matches address $233 is Q.

Form (b) allows the operator to change the state of
the CPU flags. In this case, f must be one of the condition
code letters C, 2, M, E, or I. The present state of the
flag is displayed (either the flag letter if true, or a "“-"
if false). The operator can optionally type a single
carriage return, which leaves the flag in its present state,
Or may type alto set the flag true, or a @ to reset’ the
flag to false. Given that the carry flag is true, for
example, the command

XC
produces the SID response

Cc
followed by a space, indicating that the carry is currently
set, awaiting possible change by the operator. Enter a
Carriage return to leave the flag set, or a @ to reset the
carry to false. Similarly, if the zero flag is false, the
command

XZ

produces the SID response

---
_Page 46_

SID Users Guide Section 3

indicating that the zero flag is false. Enter a carriage
return if the state iS to remain unchanged, or a1 to set
the zero flag to true.

Form (c) allows alteration of the individual CPU
registers, where r is one of the register names A, B, D, H,
S, or P. In this case, the current content of the register
is displayed, and the console is prompted for input. If the
Operator types a single carriage return, the data value
remains unchanged. Otherwise, the symbolic expression typed
by the operator is evaluated and becomes the new value of
the register. Only byte values are acceptable when the "XA"
form is used, while double byte values are accepted in the
remaining forms. Note that the BC, DE, and HL registers
must be altered as a pair. The SID interaction shown below
is typical when the A register is altered:

XA 03 45cr

where the "XA" is typed by the operator, the "93" is printed
by SID as the value of the A register, and "45" is typed by
the operator as a replacement for A's value. The "cr"
represents the carriage return key in this example, and in
the examples which follow. The following interactions with
SID provide additional examples in the format described
above:

XB 34EF cr (data remains unchanged)
XD 2088 2388 (D changes to 23)
XH 334E .GAMMA
XS 4323 @STKPTR+#100

---
_Page 47_

4, SID UTILITIES.

SID Utilities are special programs which operate with
SID to provide additional debugging facilities. As
described in Section 1., a SID Utility is loaded by
initially typing

SID x.UTL

where x is the name of a utility program, described in the
sections which follow. Upon initiation, the utility program
loads, relocates, and prompts the console for any necessary
parameters. The operator then collects necessary program
test data (using the U or fT command), and displays the
information uSing a call to the utility display subroutine.
The mechanisms for system initialization, data collection,
and data display are given in detail below.

4.1. Utility Operation.

A particular SID utility loads into memory in much the
Same manner aS a normal test program. The utilities,
however, automatically move themselves into high memory,
occupying the region directly below the SID program, as
described in Section 1. The utility load operation can be
accomplished by simply typing the utility name with the SID
command as shown above, or can be loaded during the SID
execution, as described in the I and R- commands. Recall,
however, that all existing symbol information is removed
when the utility loads, and must therefore be reinitialized
if required for the debugging run.

Normally, a SID utility has three primary entry
points: one for utility (re)initialization, called INITIAL,
one for data collection, called COLLECT, and one for data
display, called DISPLAY. After loading, the utility sets up
these symbols in the table, and types the entry point
addresses in the format:

~INITIAL = iilii
-COLLECT = cccce
-DISPLAY = dddd

where iiii, cccc, and dddd are the hexadecimal addresses of
the three entry points. Note, however, that the three
symbolic names are equivalent to these three addresses.

Following initial sign on, the utility may prompt the
console for additional debugging parameters, After the
interaction is complete, the operator may use the I and R
commands to load test programs and symbol tables in order to
proceed with the debug session.

During the debug run, data collection takes place by
running the test program in monitored mode using the U or T

---
_Page 48_

SID Users Guide Section 4

commands. Either of the commands
UFFFF,.COLLECT or UFFFF,cccc

direct the SID program to run the test program from _ the
current Program Counter, for a maximum of 65535 (FFFF
hexadecimal) steps, with a call to the data collection entry
point of the utility program. Each instruction breakpoint
sends information to the utility program, where it is
tabulated for later display. Note that in this particular
case, the operator would most likely stov the untrace mode
by depressing the return key before the sequence of 65535
steps completes.

Following a series of data collection operations, the
utility DISPLAY entry point can be called to print the
accumulated data. Either of the command forms which follow
accomplish this function:

C.DISPLAY or Cdddd

The operator may then resume the data collection process, as
described above, followed by additional display operations.

At any point, the operator can reinitialize the
utility by typing either

C.INITIAL or Ciiii

which causes reinitialization of the utility tables. The
utility may then prompt for additional parameters to
complete the reinitialization process.

Note that loading and executing more than one utility
function during a debugging session may produce
unpredictable results.

The functions of the SID utilities are presented
individually in the remaining sections,

4.2. The HIST Utility.

The HIST Utility creates a histogram (bar graph) of
the relative frequency of execution in selected program
segments of a program under test. The purpose of the HIST
utility is to allow the operator to monitor "hot spots" in
the test program where the program is executing most
frequently.

After initial signon, as described in the previous
section, the HIST utility prompts the input console with

TYPE HISTOGRAM BOUNDS

The operator must respond with two symbolic expressions,

---
_Page 49_

SID Users Guide Section 4

separated by a comma:
1111,hhhh

where 1111 is the lowest address to monitor, and hhhh is the
highest address. In order to collect histogram information,
the operator must use one of the command forms

Tn,c T,c TWn,c TW,c -Tn,c -T,Cc -TWn,c -TW,c
Un,c U,c UWn,c UW,c -Un,c -U,c -UWn,c -UW,c

where c is either .COLLECT, or the address corresponding to
the COLLECT entry point. Although all of these commands are
optional, the single form

Un, .COLLECT
is nearly always used since the trace output is disabled,
the test program is fully monitored, and data collection
takes place at each program step,

Following a series of data collection operations, the
histogram is displayed by typing

C.DISPLAY or Cdddd

and the histogram is printed in the format:

HISTOGRAM:
ADDR RELATIVE FREQUENCY, MAXIMUM VALUE = mmmm
aaaa KKKKK
bbbb AKKK KKK
ecce RKEKKKKKESK
XRRX KEKKKKKKKEE
VVVY RKREKEKKKEKKEKEKKEKKEKEKKEKKKEKKEKKKKKKRKKKKKKKKKEKKKEKE
Z2Z22Z kKaAKKKK

where addresses aaaa through zzzz span the range from. the
low to high address range given in the initialization of
HIST. The maximum value mmmm is the largest number of
instructions accumulated at any of the displayed addresses,
and the asterisks represent the bar graph of relative
instruction freguencies, scaled according to the maximum
value mmmm., The address range is automatically scaled over
64 difference address slots (aaaa, bbbb, ... ,2222, above),
with a maximum of 64 asterisks in any particular bar of the
graph.

Given the above display, for example, the "hot spot"
is around the address range xxxx to zzzz. In this case, it

would be worthwhile reinitializing the HIST utility by
typing

C.INITIAL or Ciilii

---
_Page 50_

SID Users Guide Section 4

The HIST initialization prompt and response should then be
TYPE HISTOGRAM BOUNDS xxxx,2222Z

The operator may then rerun the test program using the
command

UFFFF, .COLLECT

After leaving enough time for the test program to reach
"steady state," the operator then interrupts program
execution by typing a return during the monitored execution.
The display function is then reinvoked to expand the region
between xxxx and zzzz, resulting in a more refined view of
the frquently executed region.

The L command can subsequently be used to determine
the exact instructions which are most frequently executed.
If possible, the sequence of instructions can be somewhat
improved, with an overall improvement in program
performance.

4,3. The TRACE Utility.

The TRACE utility is used to obtain a backtrace of the
instructions which lead to a particular break address in a
program under test. A program may have an error condition,
for example, which arises from a sequence of instructions
which are difficult to find under normal testing. In this
case, TRACE can be used to collect program addresses as _ the
test program executes, and display these addresses and
instructions in most recent to least recent order when
requested by the operator. Normal invocation of SID with
the TRACE utility is:

SID TRACE.UTL

with the normal utility response:

INITIAL = iiii
COLLECT = cccc
DISPLAY = dddd

In this case, the TRACE utility also prints the message:
READY FOR SYMBOLIC BACKTRACE
which indicates that the assembler/disassembler portion of
SID is present, and will be used to disassemble instructions
when the backtrace is requested.
The operator may then proceed to load a test program

with optional symbol table. The DUMP program, for example,
could be loaded by subsequently typing:

---
_Page 51_

SID Users Guide Section 4

IDUMP.COM DUMP.SYM
R

with the usual "NEXT PC END" response indicating that the
test program is loaded. At this point, the SID debugger is
executing in high memory, along with the TRACE utility. The
test program is present in low memory, ready for execution.

The simplest backtrace is obtained by typing one of
the U or T command forms shown with the HIST utility. In
particular, a U command of the form:

U#588,.COLLECT

executes 588 (decimal) program steps, and then automatically
stops program execution. The operator may then obtain a
backtrace to the stop address by typing:

C.DISPLAY

which causes TRACE to display the label, address, and
mnemonic information in the form:
label-255:
addr-255 opcode-255 sym-255
label -254:
addr-254 opcode-254 sym-254
label-253:
addr-253 opcode-253 sym-253
label-@98:
addr-@00 opcode-080 sym-O00

where label-255 down through label-888 represent the decoded
symbolic labels corresponding to addresses given by addr-255
down through addr-#0@8, when the symbolic labels exist.
Opcode-255 down through opcode-#@90 represent the mnemonic
operation codes corresponding to the backtraced addresses,
and sym-255 down through sym-@@8 denote’ the symbolic
operands corresponding to the operation codes, when the
symbols exist. The operation codes are displayed in the
same format as the list command. Note that in this display,
the most recently executed instruction is at location
addr-255, while the least recently executed instruction is
at location addr-@0@, TRACE will account for up to 256
instructions, which accumulate in T or U_ mode, The
accumulated instructions are not affected by the DISPLAY
function, but are cleared by a call to reinitialize:

C.INITIAL
Full benefit of the TRACE utility requires concurrent
use of TRACE with pass points (see the "P" command). In

particular, pass points are first set at program locations
which are of interest in the backtrace. The program is then

---
_Page 52_

SID Users Guide Section 4

run to an intermediate location where the test begins. At
this intermediate test point, the U command is used to
execute the test program in fully monitored mode, with data
collection at the COLLECT entry point of TRACE. Upon
encountering one of the pass points in U mode, program
execution breaks, and the operator regains control in SID
command mode. The DISPLAY function of TRACE is then invoked
to obtain the required backtrace information.

As an example of this process, suppose the DUMP
program is in memory with the TRACE utility, as shown above.
Suppose further that the operator wishes to view the actions
of the DUMP program on the first call to BDOS (i.e., the
first call from DUMP to the CP/M Basic Disk Operating
System, through location 9005). Assuming the symbol table
is loaded, the operator first types:

P.,BDOS
which sets a pass point at the BDOS entry, with
corresponding pass count = 1. The operator then executes

DUMP in monitored mode, collecting data at each instruction:
UFFFF,.COLLECT

The untrace count of FFFF (65535) instructions is, of
course, too many in this case, but the assumption is that
the DUMP program will stop at the BDOS call before the
instruction count is exceeded ( if it does not, the operator
can depress any keyboard character to force a program stop).
In this case, the DUMP program executes only ae few
instructions before the BDOS call, resulting in the break
information:

@1 PASS 8895 .BDOS
~ZEI A=88 B=@814 D=985C H=9000 S=9249 P=0005 IMP CCDF
\*CCDF

showing the pass count 1, pass address 6685, symbolic
location BDOS, register state, and break address. Since
execution to this point was monitored, and data was
collected, the TRACE function can be invoked:

C.DISPLAY

which results in the display:

---
_Page 53_

SID Users Guide Section 4

BDOS :
0605 IMP CCDF
@1CA CALL 6865 ~BDOS
@1C8 MVI C,OF
@1c5 LxI D,@@5C .FCB
@1C2 STA Q@g7C ~FCBCR
‘SETUP:
@1Cl XRA A
@18A CALL @1Cl ~SETUP
@197 LXI SP,9257 .STKTOP
8164 SHLD 6215 ~OLDSP
81983 DAD SP
6180 LXI H, 9009

Note that in this particular case, only 11 instructions were
executed before the BDOS call, and thus the full 256
instruction capacity had not been exceeded. In fact, the
backtrace shown above gives the complete history of the DUMP
execution, from the first instruction at address 9100. The
operator may then proceed to execute the DUMP program
further by simply typing:

UFFFF,.COLLECT
with a break at the following call on BDOS. Given that the
program execution is to stop on the 2@th call on BDOS, the
operator can type the pass command:
P.BDOS, #290
to set the pass count at 20 (decimal). The command:

UFFFF, .COLLECT

can be entered if intermediate passes are to be traced.
Alternatively, the command:

-UFFFF,.COLLECT
can be typed to disable intermediate traces. In either
case, execution stops at the 28th BDOS call, and the
operator can enter the display command:
C.DISPLAY

to view the trace to this particular BDOS call.

Note that long typeouts can be aborted by typing any
keyboard character during the display. Further, the ctl-S
key freezes the display during output. Finally, recall that
"C.DISPLAY" can be issued any number of times to reproduce
the backtrace since the command does not clear the TRACE
buffer.

The TRACE utility can also be used when the

---
_Page 54_

SID Users Guide

disassembler module is not
instruction addresses are
Mnemonics are not included.
commands shown below
disassembler module,
its symbol table:

SID

-A
ITRACE.UTL
R
IDUMP.COM

In this case,

ou —-A ty

The backtrace information is
format:

addr-255 addr-254

addr-247 addr-246

addr-007 addr-006

loads
followed by the

Section 4

present. In this case, the
listed in the trace, while the
For example, the sequence of

the TRACE utility without the
DUMP program without

Load the SID Program
Remove the Disassembler
Ready the TRACE Utility
Read the TRACE Utility
Load the DUMP Program

the TRACE utility prints the sign on message:

IN EFFECT, ADDRESS BACKTRACE

subsequently displayed in the
addr~-253 . . . addr-248
addr-245 . . . addr-24@
addr-005 . . . addr-090

---
_Page 55_

5. SID SAMPLE DEBUGGING SESSIONS.

This section contains several examples of SID
debugging sessions. The examples are based upon a “bubble
sort" of a list of byte values. The bubble sort program is
first listed in its first undebugged form. A series of
test, edit, and reassembly processes are shown which result
in a final debugged program. In each case, the operator
interaction with CP/M, ED, MAC, or SID is shown in normal
type, while comments on each of the processes are given
alongside in italics.

The dialogue which follows contains the following
seguence of operations:

(1) TYPE SORT.PRN Lists initial SORT program
(2) TYPE SORT.SYM Shows the SORT symbol table
(3) TYPE SORT.HEX Shows the SORT HEX file
(4) SID SORT.HEX SORT.SYM lst debugging session
(5) ED SORT.ASM lst re-edit of SORT program
(6) MAC SORT lst reassembly of SORT
(7) TYPE SORT.SYM Shows new symbol table
(8) SID SORT.HEX SORT.SYM 2nd debugging session
(9) ED SORT.ASM 2nd re-edit of SORT program
(18) MAC SORT 2nd reassembly of SORT
(11) SID SORT.HEX SORT.SYM 3rd debugging session
(12) ED SORT.ASM 3rd re-edit of SORT
(13) MAC SORT 3rd reassembly of SORT
(14) LOAD SORT Create a COM file for SORT
(15) SID SORT.COM SORT.SYM 4th debugging session
(16) SID SORT.COM SORT.SYM Re-entry to SID for debugging
(17) SID SORT.COM SORT.SYM Re-entry to SID for debugging
(18) SID SORT.COM SORT.SYM Re-entry to SID for debugging
(19) ED SORT,ASM 4th re-edit of SORT
(2@) MAC SORT 4th reassembly of SORT
(21) SID SORT.HEX SORT.SYM 5th debugging session
(22) ED SORT.ASM 5th re-edit of SORT
(23) MAC SORT 5th reassembly of SORT
(24) SID SORT.HEX SORT.SYM 6th debugging session
(25) ED SORT.ASM 6th (last) re-edit of SORT
(26) MAC SORT S+S 6th (last) reassembly

Following the debugging sessions, the final corrected SORT
program is given in its debugged form.

Three separate debugging sessions are then shown which
use the HIST and TRACE utilities to monitor the execution of
the tested SORT program. The operations shown here include:

(27) SID HIST.UTL Load the HIST Utility
(28) SID TRACE.UTL Load the TRACE Utility
(29) SID Load SID, TRACE follows

As a final example, a simple program which calls’ the

---
_Page 56_

SID Users Guide Section 5

BDOS is listed, followed by a single debugging session. The
purpose of this particular example is to show the actions of
SID when subroutines are traced, followed by calls on the
CP/M BDOS. The operations in this case are:

(30) TYPE IO.PRN

List the IO program
(31) SID IO.HEX IO.SYM

Enter SID for debugging

---
_Page 57_

SID Users Guide

(1) SORT. PRN

0000 =

0100 213801
0103 3601
0105 213901
0108 3600

O10A 3A6201
O10D BE
Q10E C21901

O11 213801
0114 7E
0115 B7

0116 C30000

0119 5F
011A 1600
O11C 215A01
Ol1F 19
0120 7E
0121 23
0122 BE
0123 0A3101

0126 CA3101

0129 4€
012A 77
0128 2B
012¢ 71

012D 213801
0130 34

0131 213901
0134 34
0135 C30A01

O13A

REBOOT

SORT:

COMP:

STOP:

SW:
I:

STACK:

015A O503040A08L IST:
08 N:

SORT PROGRAM IN CP/M ASSEMBLY LANGUAGE
ELEMENTS OF ‘LIST' ARE PLACED INTO
DESCENDING ORDER USING BUBBLE SORT

ORG 100H BEGINNING OF TPA

EQU OOOOH  ;CP/M REBOOT LOCATION
LXI 4, SW

MVI M1 sSW = 1

LXI HyI “INDEX TO SORT LIST
MVI M0 sI1 = 0

COMPARE I WITH ARRAY SIZE
3HL ADORESS INDEX I

LDA N sLENGTH OF VECTOR
CMP M ;CHECK FOR N=I
JNZ CONT ;CONTINUE IF UNEQUAL

END OF ONE PASS THROUGH LIST

LXI H,SW 3NO SWITCHES?
MOV AM ;FILL A WITH SW
ORA A ;SET FLAGS

END OF SORT PROCESS, REBOOT
JMP REBOOT ;RESTART CCP

CONTINUE THIS PASS
ADDRESSING I, SQ LOAD LIST(I)

MOV E,A ;LOW(I) TO € REGISTER
MVI 0,0 sHIGH(I) = 0

LXI H,LISt ;BASE OF LIST

DAD D ;ADDR LIST(I)

MOV A.M sLIST(I) IN A REGISTER
INX H sADDR OF LIST(I+1)

CMP M sLIST(1) :LIST(I+1)

Jc INCI ;SKIP IF PROPER ORDER

CHECK FOR LIST(I) = LIST(I+1)
JZ INCI ;SKIP IF EQUAL

ITEMS ARE OUT OF ORDER, SWITCH

MOV CM ;OLD LIST(I+1) TO C
MOV MA sNEW LIST\*I+1) TO M
DCX H ;ADDR LIST(I)

MOV M,C sNEW LIST(I) TOM
LXI H,SW ;SWITCH COUNT [S SW
INR M 3SW = SWo+ 1

; INCREMENT INDEX I

LXI Ht

INR M sf = 1+il

JMP COMP ;TO COMPARE I WITH N-1
DATA AREAS

oS 1 ; SWITCH COUNT

oS 1 ; INDEX

DS 32 316 LEVEL STACK

DB 5,3,4,10, 8,130,10,4

oe $-LIST ;LENGTH OF LIST

Section 5

---
_Page 58_

SID Users Guide

(2) TYPE SORT.SYM

OLOA COMP OL19 CONT 0139 I 0131 INCI O15A LIST
0162 .N 0000 REBOOT 0100 SORT OL5A STACK 0116 STOP
0138 SW

SORT .HEX

TYPE

@) 10010000213801360121390136003A6201BEC21997
:10011000012138017EB7C300005F 1600215A011982
: 100120007€23BEDA3101CA31014E772871213801AD
:080130003421390134C30A0136
:09015A000503040A088 20A0408E6
:0000000000

(4) SID SORT.HEX SORT.SYM Start SID with HEX and SYM files
SID VERS 1.4
SYMBOLS

NEXT PC END . .
0163 0100 55B7 Next free address is 163, Program Counter is 100

\#0.LIST,+=N-1 and end of TPA is 55B7
O15A: 05 03 04 OA 08 82 ...... . age . ;
0160: 0A 04 .. Display initial list of items to sort

\#G,.STOP Execute test program until "STOP" symbol address encountered

\*O116

. STOP Now at the STOP address, examine data list:

\#D.LIST,+=N-1
O15A: 05 03 04 OA 08 82 ...... Hasn't changed!
0160: 0A 04 ..

where is the program counter?
100 reset PC back to beginning and try again with trace on:

A=01 B=0000 0=0008 H=0138 $=0100 P=0100 LXI H,0138 .SW
A=01 B=0000 D=0008 H=0138 $=0100 P=0103 MVI M,O1 .SW Swe!
A=01 B=0000 D=0008 H=0138 S=0100 P=0105 LXI H,0139 .I  yx@
A=01 B=0000 D=0008 H=0139 $=0100 P=0108 MVI M,00 .I

A=01 8=0000 D=0008 H=0139 S=0100 P=O10A LDA 0162 .N N=?

A=08 8=0000 D=0008 H=0139 $=0100 P=O010D CMP M=00 I

A=08 B=0000 D=0008 H=0139 S=0100 P=O10E JNZ 0119 .CONT

No, so compare
LIST(), LIST(i+1)

5A .LIST

A=08 B=0000 D=0008 H=0139 $=0100 P=0119 MOV €
A=08 B=0000 0=0008 H=0139 $=0100 P=O11A MVI OD
A=08 B=0000 0=0008 H=0139 $=0100 P=Olic LXI H
A=08 820000 D=0008 H=015A $=0100 P=O11F DAD D
A=08 B=0000 D=0008 H=0162 S=0100 P=0120 MOV A,M .N What's this?
A=08 B=0000 D=0008 H=0162 S=0100 P=0121 INX H Why did we
A=08 B=0000 0=0008 H=0163 $=0100 P=0122 CMP M=58 fetch N?
A=08 B=0000 D=0008 H=0163 $=0160 P=0123 JC 0131 . INCI

A=08 B=0000 0=0008 H=0163 $=0100 P=0131 LXI H,0139 .I
Looks like we've discovered a bug! We have entered at "CONT"
with N in the accumulator, rather than I, which is expected!

(5) ED SORT.ASM Back to the editor to make the changes
\#A Bring all the text into memory
\*V Enter Verify mode for line numbers, then find the place to change

Ll:
28:
28:
28:
28:
28:
28:
29:
29:

\*FADDRESSING
\*OLT

: ADDRESSING I, SQ LOAD LIST(I)  petete the Line

\*KT

MOV E,A ;LOW(I) TO £ REGISTER

\*T

LDA I ;LOAD I TO A REGISTER Insert the
ctl-Z change

\*E Terminate the editing session

Section 5

---
_Page 59_

SID Users Guide Section 5

MAC SORT
CP/M MACRO ASSEM 2.0
OO1H USE FACTOR
END OF ASSEMBLY

Re-assemble the SORT program

f .
(7) TYPE SORT.SYM Here's the symbol table:
O10A COMP 0119 CONT 013¢ I 0134 INCI 0150 LIST
0165 N 0000 REBOOT 0100 SORT 015D STACK 0116 STOP
0138 SW

SID SORT.HEX SORT.SYM

SID VERS 1.4 ;
SYMBOLS Let's try again, load the HEX and SYM files
NEXT PC END

0166 0100 55B7

\#P STOP Set a “pass point" at STOP to prevent reboot
\#G Start (unmonitored) execution

O01 PASS 0116 .STOP We made it to the STOP label, check values

we--- A=7C B=0008 D=0081 H=013B $=0100 P=0116 JMP 0000 .REBOOT
\*0000 .REBOOT

\#H=N What's the value of the byte variable N?
0082 #130 130? Very strange! How did that happen?
\#0.LIST,+7 Oh well, let's look at the data values:

015D: 03 04 05 ...

0160: 08 OA OA 04 08 ..... They are almost sorted, looks like we have

some trouble near the end of the vector,
\#1SORT HEX let's reload the machine code and try
\#R again:
NEXT PC END
0166 0100 55B7
\#XP
P=0100 Program counter remains at 0100, what
ap are the active pass points?
01 0116 .STOP The one at STOP remains set, let's also
\#P SORT ,FF monitor the SORT loop point, but not
\#G break right away.
FF PASS 0100 .SORT Here's the first time through SORT
----- A=7C B=0008 D=0081 H=0138 $=0100 P=0100 LXI H,913B .SW
O01 PASS 0116 .STOP It stopped immediately! It doesn’t look good!
won-- A=79 8=0008 0=0081 H=013B $=0100 P=0116 JMP 0000 .REBOOT
\*0000 .REBOOT We know there should have been several loops
\#1 SORT HEX through the SORT label, since the data is
aR unordered. Let's try again -- reload the code
NEXT PC ENO (note that the reload is necessary here, since
onoe 0100 55B7 the data is initialized in the code area).
01 0116 .STOP What active pass points exist?
FE 0100 .SORT Wait a minute -- referring back to the
\#60 original listing, it appears that the code

preceding the STOP label ts incomplete:

there should be a conditional jump back to

the SORT label - maybe that's why the program
never makes it back!

---
_Page 60_

SID Users Guide Section 5

(8)e0 SORT.ASM Oh well, back to the editor for a
\*HAY quick fix. Append all text (#A), and
lL: \*FSTOP: enter Verify mode (V). Then find STOP,
24: \*OLT
24: STOP: UMP REBOOT ;RESTART CCP
24: \* Go up one line (-)
23: 3 END OF SORT PROCESS, REBOOT
23: \*I and enter insert mode (I)
23: JNZ CONT sCONTINUE IF NOT EQUAL
24: 3; ctl-Z, and "return"
25: €£
26: wait, I forgot the ctl-Z, now [ve got the E command in

26: \*- my input buffer. Type the ctl-Z, go back up one line,
25: € delete the E, then end the edit

25: \*KT
25: 5 END OF SORT PROCESS, REBOOT
25: \*—€ OK, we made the change, now re-assemble
MAC SORT Invoke the macro assembler with SORT as input.
CP/M MACRO ASSEM 2.0

OO1H USE FACTOR
END OF ASSEMBLY

OF SORT.HEX SORT.SYM Here we go again, I sure hope this is the

D VERS 1.4 last time (but it probably isn't),
SYMBOLS
NEXT PC END
0169 0100 55B7
\#P SORT, FF Set a pass point at sort, with a high count.
P.STOP also set a pass point at STOP with count 1, this
\#P will stop the first time through

FF 0100 .SORT
01 0119 .STOP

\#G Execute the test program

FF PASS 0100 .SORT First time through SORT label:

-one- A=00 8=0000 D=0000_ H=0000 S=0100 P=0100 LXI H,O13E .SW
O1 PASS 0119 .STOP Stopped again! Arrggh!

~Z-E- A=00 B=006A D=0007 H=013E S$=0100 P=0119 JMP 0000 .REBOOT
\*0000 .REBOOT

Let's look at some values:

H=N
0008 #8 = .
\#D.LIST,+2N N=8, looks better than last time
0160: O01 Q1 03 04 04 05 07 08 08 ......... These values look a bit
\#ISORT.HEX strange?! Try again:
\#R
NEXT PC END
0169 0100 5587
\#0.LIST,+=N-1 Machine code reloaded, display inittal values:
0160: 05 03 04 OA 08 82 OA 04 ........
\#L CONT
CONT: Let's take a look at the process of switching
Ql1C LOA O13F .I two data items - the code appears down below
5130 wt 500 the "CONT" label, so we'll disassemble a
0122 LxXI HlO160 .LIsPO tO” OF Me Program.
0125 DAD O
0126 MOV A,M
0127, INX H
0128 CMP M
0129 JC 0137 .INCI
012¢ JZ 0137 .INCI
O12F MOV C,M Here's where the switch occurs, let's set a pass
ioe point here and watch the data addresses:

FE 0100 .SORT
Q1 0119 .STOP
FF O12F

---
_Page 61_

SID Users Guide

\#G

FE PASS 0100 .SORT

-Z-E- A=00 B=006A
FF PASS Q12F

----1 A=05 B=006A
FE PASS 012F

----I A=05 B=0003
FD PASS O12F

-~--I A=0A B=0004
FC PASS 012F

---E- A=82 B=0008
ye

\#-P
\#ISORT .HEX
\#R
NEXT PC END
0169 0100 5587
\#L.CONT+5

0121 NOP

Here's the first pass through SORT

0=0007 H=O13E S=0100 P=0100 LXI H,O13E .SW
Switching at address 161, looks OK!
0=0000 H=0161 S=0100 P=O12F MOV C,M
Switching at 162, looks good.
D=QO0L H=0162 S=0100 P=012F MOV C,M
164 is the next to switch, looks good.
0=0003 H=0164 S=0100 P=012F MOV C,M
166 is probably the next one.
D=0005 H=0166 S=0100 P=012F MOV C,M
So what's wrong? This section of
code seems to work.

Clear all the pass points, and reload
the machine code for another test.

0122 LXI H,0160 .LIST

0125 OAD 0D
0126 MOV A,M
0127 INX H
0128 CMP M

0129 JC 0137.
- INCI

ol2c JZ 0137

Q12F MOV C,M

0130 MOV M,A

0131 OCX 4H
\#G,129

\#0129

\#T10
----]

----]

----I

----]

8=0000
B=0000
8=0000
8=0003
8=0003
B=0003
----1 A=05 8=0003
----I A=05 8=0003
\*0137 . INCI ‘
\#D.LIST,+7

rPrPrPrPprrp
OO09000
Hannan

--~-1

Here's the code where the element

switching occurs, let's watch the

rogram switch the first element:
inci ’

OK, here we are, ready to test and

switch, if necessary.
020000 H=0161 S=0100 P=0129 JC 0137 .INCI
D=0000 H=0161 S=0100 P=012C JZ 0137 .INCI
D=0000 H=0161 S=0100 P=012F MOV C,M
0=0000 H=0161 S$=0100 P=0130 MOV M,A
D=0000 H=0161 $=0100 P=0131 DCX H
0=0000 H=0160 $=0100 P=0132 MOV M,C .LIST
0=0000 H=0160 $=0100 P=0133 LXI H,O13E .SW
0=0000 H=013E $=0100 P=0136 INR M=Ol .SW

Well, that went nicely - elements switched, SW=1

0160: 03 05 04 OA 08 82 OA 04 ........

\#H=1
Q000 .REBOOT #0
\#G,.INCI

\*0137 .INCI
\#0.LIST,+7

The data looks good at this point.
Proceed to the INCI label

Here we are, let's look at the data:

0160: 03 05 04 OA 08 82 0A 04 ........

\#H=1
0000 .REBOOT #0
\#T
----- A=05 8=0003
\*013A
\#G,.INCI

\*Q137 INCI
\#0.LIST,+=1
0160: 03 94 ..
\#T

~--E- A=05 B=0004
\*O13A
\#G,.INCI

\*0137 J INCI
\#0.LIST,+=1

0160: 03 04 05 ...
\#G,.SORT,.STOP

\*0119 .STOP
\#0.LIST, +=]

Looks good, trace past the label and break

0=0000 H=013E $=0100 P=0137 LXI H,O13F .I
Go to the INCI label again.

Here we are (again), how's the data?

Looks good, proceed past INCI

0=0001 H=013E S=0100 P=0137 LXI H,O13F .I
And loop again...

Here we are (again), how's the data?

Looks good, this is getting monotonous, let's
go for it! Stop at either SORT or STOP

Egad! Here we at the the STOP label. Why
aren't we making it back to SORT?

0160: 01 01 03 04 04 05 07 08 08 .........

\#

Tsk! Tsk! The data's messed up again.

Section 5

---
_Page 62_

SID Users Guide

\#1SORT.HEX
\#R
NEXT PC END
0169 0100 55B7
\#1136 ,+3
0136 INR M
INCI:
O13A
\#G,136

\*0136
\#0.LIST, +21
0160: 03

\#U

----I A=05 8=0003 0=0000 H=013£ $=0100 P=0136 INR

\*0137
\#P136
4G

INC!

1 PASS 0136

----I A=05 B=0004 0=0001 H=013E S=0100 P=0136

\*O137 J INCI
\#0. LIST, +51
9160: 03 04 ..
\#S.N

01638 08 4
0169 OA .

\#6

OL PASS 0136

----1 A20A B=0008 0=0003 H=013E S=0100 P=0126 INR

\*0137 INCI
\#0.LIST,+=1
0160: 03 04 05
\#H=SW

0004 #4

\#TFEFF

waece A=0A B=0008
<---> A=0A 8=0008
-a--- A=0A 8=0008

A=04
-2-ET A=04
-2-E1 A=04
oZ-E] A=04
-Z-E] A=04
A=04

LXE oH,

A=0A 8=0008
B=0008
8=0008
B=0008
8=0008
B=0008
8=0008

B=0008
Very interesting!

Let's reload and try again.

Here's where the switcn count is incremented
O13F VI

Execute the program and break
at SW =SWil

Look at data values:

Use U to move past break address

M=O1 .SW
It's actually easier to use the pass point feature
if we want to view the action of the INR M,
since the P command stops execution after the
pass point is executed,

INR M202 .SW
SW = 2, looks good.

Data values look good.

Let's change N to a smaller value so the program
doesn't loop so many times: 4 is a good number.
End input with "."

"GO" to pass point

Here we are, switch value is incremented:
M=03 .SW
Stopped at next instruction.

08 .... Data values so far.

SW value at this point is 4.

Let's watch it run for a few steps:
0=0003 H=013E S=0100 P=0137 LXI
D=0003 H=O13F $=0100 P=013A INR
D=0003 H=O013F S=0100 P=013B JMP

H,O13F VI
M=Q3 .1
QLOA .COMP

0=0003 H=O13F
0=0003 H=013F
D=0003 H=013F
D=0003 H=013F
D=0003 H=013E
0=0003 H=O013E
0=0003 H=O13E

S=0100
$=0100
$=0100
$=0100
$=0100
5=0100
3=0100

P=O10A LDA
P=Q10D CMP
P=O10E JNZ
P=O111 LXI
P=O114 MOV
P=O115 ORA
P=O0126 JNZ

0168 .N
M=04 JI
QLIC .CONT
H,O13E .Sh
A.M SW

A

O11LC . CONT

0=0003 H=O13E S=0100 P=011C

We seem to be

LDA O13F JI

Fe 1 wr ’ a
Let's GO BP US the Sa an eT hen SORT.

ED SORT.ASM
\*#AVFORA

22; \*OLT

22:

22: \*

23:

23: \*SCONT!ZSORT!ZOLT

23:
23: \*
24:
24: \*
25:3
25: \*E

This is a simple change: append all text, enter line
verify mode, find "ORA" and make the change:
ORA A ;SET FLAGS

"return to move down one line
CONT CONTINUE IF NOT EQUAL
Substitute SORT for CONT

JNZ

JNZ SORT ;CONTINUE IF NOT EQUAL
"return’” ta move down another line
"return" again.

END OF SORT PROCESS, RE8COT
End the edit

Section 5

---
_Page 63_

SID Users Guide Section 5

MAC SORT
CP/M MACRO ASSEM 2.0
0169 Call out MAC for another assembly.
QOLH USE FACTOR
END OF ASSEMBLY

AD SORT
(14).0 D SOR Just for a little variation, we'll create a

FIRST ADDRESS 0100 SORT.COM file for testing under SID.
LAST ADDRESS 0168
BYTES READ 0047
RECOROS WRITTEN O1

SID SORT.COM SORT.SYM

SIO VERS 1.4 Back to SID, using the COM and SYM files
SYMBOLS

NEXT PC ENO

0180 0100 55B7

\#P STOP Set a pass point at STOP to prevent reboot

\#0.LIST,+=N-1 Here's the original data:
0160: 05 03 04 OA 08 82 OA 04 ........
\#G Unmonitored GO
Oops! We didn’t get control back, there must
be an infinite loop - we can get control back by

63K CP/M VERS 1.3 foreing a front panel RST 7 (interrupt 7),
or simply bail-out with a cold start.

(18)... SORT.COM SORT.SYM

SID VERS 1.4 Let's start again, but be a little more selective

SYMBOLS in setting breakpoints.

NEXT PC END

0180 0100 55B7

\#P STOP Set a pass point at STOP, as before

\#P.SORT,FF and one at SORT with a pass count of 255,

\#-G GO with pass trace disabled.

01 PASS 0100 Stopped with 255 passes through SORT - tog many!

----- A=O1 8=006A D=OOFF H=013E S=0100 P=0100 LXI H,Olse

\*0103

\#D0.LIST,+=N-1 How's the data?

0160: 03 .

\#H=N Hmmm... looks like N was destroyed.

0000 .REBOOT #0

\#H=1

0000 .REBOOT #0

\#6, .COMP There's a good possibility that we're running off
the end of the LIST vector into the variable N,

\*OLOA .COMP let's stop at the COMP label and watch the end test.

\#75

aeons A=01 B=006A D=COFF H=013F S=0100 P=010A LDA 0168 .N
----- A=00 B=O006A D=OOFF H=013F $=0100 P=0100 CMP M=00 .1
-Z-EI A=00 B=006A D=OOFF H=013F S=0100 P=O10E JNZ O11C .CONT
-Z-E1 A200 8=006A D=OOFF H=013F S$=0100 P=O111 LXI H,013E .SW
-2-E] A=G0 B=006A D=OOFF H=013E S=0100 P=0114 MOV A,M .SW
\*O115 Hey, this isn’t going to work! We'll be comparing

\#G0 LIST(N-1) with LIST(N), but the last LIST element is

at LIST(N-1), Let's try a quick fix.

---
_Page 64_

SID Users Guide Section 5

OF SORT.COM SORT.SYM
ID VERS 1.4 Let's re-enter SID with a clean memory

SYMBOLS image, and look at the machine code
NEXT PC END below the "COMP" label.
0180 0100 55B7
\#L COMP
COMP :
Q10A LDA 0168 .N Here's the reference to N - let’s change this
0100 CMP M to N-I with a "hot patch” in memory, to see
Q1OE JNZ O11C .CONT if it works, then we'll go back to the
O111 LXI H,013E .SW original source program and make the
0114 MOV A,M necessary changes. We're not using the area
\#A1OA of memory starting at 0200, so patch a jump
O10A JMP 200 over the LDA instruction, and fix-up some
0100 patch code.
\#A200
0200 LDA .N Replace the LDA instruction which now has JMP 200,
0203 DCR A N-1I in accumulator (N better be 2 or larger!)
0204 CMP M and compare with memory (HL addresses 1),
0205 JNZ .CONT jump to CONT if continuing, otherwise
0208 JMP 111 jump back to the next instruction in sequence
0208 after the patch.
\#P205 , FF Set a pass point to watch the JNZ take place
\#P STOP and catch any returns to the CCP.
iy Set a pass point at the patch return address.
! Reduce the siz i
0168 08 4 size of N for this test to 4.
0169 00 .
\#6 Everything is ready, let's go...
FF PASS 0205 First pass through the patch code:
-+-EI A=03 B=0000 D=0000 H=O13F S=0100 P=0205 JNZ O11C .CONT
FE PASS 0205 Went to CONT that time, second pass:
~---] A=03 8=0003 D=0000 H=013F S=0100 P=0205 JNZ O11C .CONT
FD PASS 0205 Went to CONT again, next pass:
----I A=03 B=0004 0=0001 H=013F S=0100 P=0205 JNZ O1IC .CONT
FC PASS 0205 And so-forth:
-Z-EI A=03 8=0004 D=0002 H=O13F S=0100 P=0205 JNZ OL1IC .CONT
FF PASS Ol1l Must be the end of one cycle:
-Z-E1 A=03 B=0004 D=0002 H=013F S=0100 P=0111 LXI H,OL3E .SW
FB PASS 0205 Now back through the patch code:

-~-EI A=03 B=0004 0=0002 H=013F S=0100 P=0205 JNZ OL11C .CONT
FA PASS 0205

----I A=03 B=0004 D=0000 H=013F S=0100 P=0205 JNZ O11C .CONT
FQ PASS 0205

----I A=03 8=0004 9=0001 H=013F S=0100 P=0205 JNZ OLIC .CONT
F8 PASS 0205

-Z-EI A=03 8=0004 0=0002 H=013F S=0100 P=0205 JNZ O11C .CONT
FE PASS O111

-Z~ET A=03 B=0004 D=0002 H=013F $=0100 P=O111 LXI H,O13E .SW

\*O114 This is getting monontonous again, so
\#0.LIST,+=N-1 | oe

push the "return" key to stop the action.
Q .
160: 03 04 05 0A \*\*\*\* Pata looks good, run in monitored mode:

-UFFFF ;

-Z-EI A=03 B=0004 0=0002 H=013E $=0100 P=0114 MOV A,M
\*013B Push the "return key to abort early.
\#H=N Value of N is still 4 (that's nice!)

0004 #4 Value of I is currently 2. This program
\#H=I should have stopped, but didn’t for some
0002 #2 reason.

---
_Page 65_

SID Users Guide Section 5

SID SORT.COM SORT.SYM
SID VERS 1.4 Let's try another approach. Suppose we
yy, . ul
see END SPrStOMEALS TEGHY EB oF Ra well! set
0180 0100 55B7 LIST(0) = 0, LIST(1) = 1
\#5.N
0168 08 2
0169 00.
\#S LIST
0160 05 0
0161 03 1
0162 04 .

Pp. STOP Things are ready to go, run completely traced:

\#TFFFF
----- A=00 8=0000 D=0000 H=0000 $=0100 P=0100 LXI H,013E .SW
w---- A=00 B=0000 D=0000 H=013E $=0100 P=0103 MVI M,O1 .SW
~---- A=00 B=0000 D=0000 H=013E S=0100 P=0105 LXI H,OL3F .I
----- A=00 B=0000 D=0000 H=013F S=0100 P=0108 MVI ™M,00 .I

----- A=00 B=0000 D=0000 H=O13F S$=0100 P=O10A LDA 0168 .N
nern- A=02 B=0000 D=0000 H=013F S=0100 P=010D CMP M=00 .I
----I A=02 B=0000 D0=0000 H=013F S=0100 P=O10E JNZ O11C .CONT

----I A=02 B=Q000 D=0000 H=013F $=0100 P=O11C LDA O13F .I
----] A=00 B=0000 0=0000 H=013F S=0100 P=O1L1F MOV E,A

----I A=00 B=0000 0=0000 H=013F S=0100 P=0120 MVI D,00

----I A=00 B=0000 D=0000 H=Q13F S=0100 P=0122 LXI H,0160 .LIST
----[ A=00 B=0000 0=0000 H=0160 S$=0100 P=0125 DAD 0D

----I A=00 B=0000 D=0000 H=0160 S=O0100 P=0126 MOV A,M .LIST
----1 A200 B=0000 0=0000 H=0160 S=0100 P=0127 INX H

----] A=00 B=0000 0=0000 H=0161 S=0100 P=0128 CMP M=O1

C-ME- A=00 B=0000 D=0000 H=0161 $=0100 P=0129 JC 0137 .INCI
INCI: Not switched!

C-ME- A=00 B=0000 D=0000 H=0161 $=0100 P=0137 LXI H,O13F .I
C-ME- A=00 8=0000 D=0000 H=013F S=0100 P=013A INR M=00 .I
C---- A=00 B=0000 D=0000 H=013F S=0100 P=0138 JMP 010A .COMP

C---- A=00 820000 D=0000 H=013F S=0100 P=010A LDA 0168 .N
C---- A=02 8=0000 D=0000 H=013F S=0100 P=010D CMP M=01 I
----I A=02 8=0000 D=0000 H=013F $=0100 P=Q10E JNZ O11C .CONT

----I A=02 8=0000 D=0000 H=013F S=0100 P=O11C LDA O13F .I
----I As01 B=0000 D=0000 H=013F S=0100 P=O11F MOV £,A

----I A=01 8=0000 D=0001 H=013F S=0100 P=0120 MVI 0,00

~---I A=01 8=0000 D=0001 H=013F S=0100 P=0122 LXI H,0160 .LIST
----I A=01 B=0000 0=0001 H=0160 S=0100 P=0125 DAD

)
----[ A01 B=0000 0=0001 H=0161 S=0100 P=0126 MOV A,M
~---I A=01 B=0000 D=0001 H=0161 S=0100 P=0127 INX H
----I A=01 8=0000 D=0001 H=0162 S=Q100 P=0128 CMP M=04
C-M-- A=01 B=0000 D=0001 H=0162 S=0100 P=0129 JC 0137 .INCI

INCI: Not switched (again)!

C-M-- A=01 8=0000 + 001 H=0162 $=0100 P=0137 LXI H,O13F .[
C-M-- A=01 B=0000 0=0001 H=013F S$=0100 P=013A INR M=Ol I
C---- A=01 8=0000 D=0001 H=013F S=0100 P=0138 JMP O10A .COMP

C---- A=01 B=0000 D=0001 H=013F S$=0100 P=010A LDA 0168 .N
C---- A=02 B=0000 D=0001 H=013F S=0100 P=010D CMP M=02 .I
~Z-EI A=02 B=0000 0D=0001 H=013F S=0100 P=O10E JNZ O11C .CONT
-Z-E1 A=02 B=0000 D=0001 H=013F S=0100 P=O111 LXI H,O13E .SW
-Z-ET A=02 B=0000 D=0001 H=013E£ $=0100 P=0114 MOV A,M .SW
-Z-EI A=01 B=0000 0=0001 H=013E S=0100 P=0115 ORA A

----- A=01 B=0000 D=0001 H=013E $=0100 P=0116 JNZ 0100 .SORT
SORT: No items were switched - SW not set to Q!

----- A=01 B=0000 D=0001 H=013E S=0100 P=0100 LXI H,O13E .SW

---
_Page 66_

SID Users Guide Section 5

Or SORT. ASM

SH AVESORT: 1ZOLT Back to the editor- change the

entry code to initialize SW

8: SORT: LX! H, SW
8: \*-
7: \*2
9: MVI M1 ;SW = 1
9: \*2S1!Z0!ZOLT
9: MVI M,O ;SW = 0
9: \*-
8: SORT: LXI H, SW
8: \*1
8: MVI A,l
9: STA SW ;SW = 1 FIRST TIME THRU
10:
10: \*€

(20) SORT

ate MACRO ASSEN 2.0 Re-assemble, again

OOLH USE FACTOR
END OF ASSEMBLY

Qs SORT.HEX SORT.SYM
ID VERS 1.4 We've fixed the SW initialization problem, which

SYMBOLS should halt the program at the proper time, but

NEXT PC END we may still have a problem with the end of
O16E 0100 5587 LIST test (remember that "hot patch"?).
\#0.LIST,+=N Here's the initial data:
0165: 05 03 04 OA 08 82 OA 04 08 .........
\#G, STOP

GO, unmonitored to the STOP (how's that for
\*OLIE .STOP confidence? ),
\#D.LIST,+=N We made it, here's the data:

0165: 03 04 04 05 08 08 OA OA OB 7B 82 ...........

0170: £6 . Data is sorted in ascending order, but there's too
\#1SORT.HEX much of it! We still have the problem that N is
\#R altered during execution.

vee 0180 oe Let's reload and make sure we know what the
\#P SORT Be9 HP Bass point at SORT, check N

\#G

O1 PASS 0105 .SORT Here's the first pass through SORT:
~Z-E- A=OQ1 8=0004 D=000A H=0143 S=0100 P=0105 LXI H,0143 .SW

MeN Break at 0108, check value of N:
eens #8 OK initially, continue the execution with C.

OL PASS 0105 .SORT We have passed through the data once:
----- A=75 B=002A D=007A H=0143 S=0100 P=0105 LXI H,0143 .SW

\*0108

\#H=N

007B #123 '.' N has been altered, which we expected, since we
\#ITSORT HEX are testing LIST(N-1I) against LIST(N) and performing
\#R a switch if unordered.

NEXT PC END

O16E 0100 5587 Let's reload and scope in on the problem:

\#G,.INCI Stop at the point where I becomes [ + I:

QL PASS 0105 .SORT Oops! T he initial pass point is still set.
wone- A=01 8=002A D=007A H=0143 S$=0100 P=O105 LXI H,O0143 .SW

free Clear all pass points.

\#G,. INCI Now, try again:

\*O13C .INCI Stopped at first entry to INCI, check value of N:
\#H=N N is still 8, looks good.

0008 #8 ;

\#G, CONT Go to the CONT label, then stop at INCI.

\*O0121 .CONT

\#G,.INCI

---
_Page 67_

SID Users Guide

Section 5

\*O13C INCI Back at INCI now. Check value of N
\#H=N
0008 48 Remains at 8. If we keep this up, we'll be typing
\#P.INCI,6 break addresses all day. We can run the next few passes
\#-G through INCI automatically by setting a pass count fuse 6
in this case), then run with -G to disable intermediate
Ol PASS 013¢ traces. We now stop 6 iterations later:
~--E- A=82 B=0004 0=0006 H=0143 $=0100 P=013C LXI H,0144
\*O13F
\#H=N Check N: remains at 8, then
0008 #8 check I to compare passes: [=0,1,2,3,4,5,6 has been
\#N=] executed. We are now about to set [= 7, but the test
0006 #6 at COMP is "JNZ" which allows execution one too many
times (which we already know about).
(22) ED SORT.ASM
\*#AY Back to the editor, change the end of LIST test
1: \*FLDA to compare I with N-I rather than N.
17: \*OLT
17: LDA N ;LENGTH OF VECTOR
17: \* "return" to go to next line
18: CMP M ;CHECK FOR N=I
18: \*I Insert the instruction before the "CMP” opcode.
18: DCR A ;N-l IN A REGISTER
19: ; (NOTE THAT N MUST BE 2 OR LARGER)
20: etl-Z
20: \*FXI Now a little clean-up work - there is a typo in
49: \*OT a comment line at address 012A in the listing:
49: MOV MA sNEW LIST#I\*-C-Of(!ZOLT
49: MOV MA sNEW LIST(I+1) TO M Looks better now.
49: \*F32 , : .
6a. \*OLT We are not using the 8080 stack, so get rid of it.
64: DS 32 316 LEVEL STACK
64; \*2KT
64: ;
64: \*E

(23) use so

Complete the edit.

A RT
CP/M MACRO ASSEM 2.0

OL4F
OO1H USE FACTOR
ENO OF ASSEMBLY

Or SORT .HEX SORT.SYM
I

DO VERS 1.4
SYMBOLS
NEXT PC END
O14F 0100 558F
\#0.LIST,+=N

9146: 05 03 04 OA 08 82 OA 04 08

\#G,STOP

?
\#G,.STOP

\*O11F .STOP
\#0.LIST,+5N

0146: 03 04 04 05 08 OA OA 82 08

\#H=N
0008 #8
\#G0

Re-assembie the source program.

Back to SID - this shouid be the fast time!

Inittal data:

Ok, ok. Let's try it with an "address reference” to
the label STOP:

That's better, now look at the data:
hooray! It's finally sorted.

Is N ok? Yes, it's still 8,
Hold it! The data is in ascending order, but it is

supposed to be in descending order! This will
be an easy fix.

---
_Page 68_

SID Users Guide Section 5

(25) ED SORT.ASM
\#A

\*T
; SORT PROGRAM IN CP/M ASSEMBLY LANGUAGE

x

; ELEMENTS OF ‘LIST’ ARE PLACED INTO

\*

; DESCENDING ORDER USING BUBBLE SORT
\*SDES!ZASC!ZOLT

; ASCCENDING ORDER USING BUBBLE SORT
\*SCC!ZC!Z0LT

; ASCENDING ORDER USING BUBBLE SORT

\*E Took care of that problem.
(28) uae SORT $+S
CP/M MACRO ASSEM 2.0
O14F Re-assemble with the symbol table option.

OO1H USE FACTOR
END OF ASSEMBLY

At this point, we have checked-out this particular SORT program using this
particular set of data items. This does not, of course, mean that the program is fully
debugged. There could be cases which are not tested properly since we have not
included all boundary conditions (the data items 00 and FF, for example, should be
included). Further, there are program segments which could be incorrect, but which
have no negative effects on the program. The initialization of SW to the value 1
before the label SORT, for example, does not affect the program, but is superfluous.
We now have a program which appears to work, but must undergo further tests before
it is considered a production program.

---
_Page 69_

SID Users Guide

SORT PROGRAM IN CP/M ASSEMBLY LANGUAGE
ELEMENTS OF ‘LIST! ARE PLACED INTO
ASCENDING ORDER USING BUBBLE SORT

0100 ORG 100H  ; BEGINNING OF TPA
0000 = REBOOT —QU OOOOH —;CP/M REBOOT LOCATION
0100 3E01 MVI Al
0102 324401 STA SW :SW = 1 FIRST TIME THRU
0105 214401 SORT: ~—LKI H, SW
0108 3600 MVI M0 ;SW = 0
Q10A 214501 LXI HI SINDEX TO SORT LIST
0100 3600 MVI M0 iI = 0
; COMPARE I WITH ARRAY SIZE
COMP:  ;HL ADORESS INDEX I
O1OF 3A4E01 LDA ON ;LENGTH OF VECTOR
0112 30 ocR A “N-1 IN A REGISTER
; (NOTE THAT N MUST BE 2 OR LARGER)
OL13 BE CMP OM sCHECK FOR NEI
0114 22201 INZ CONT — ;CONTINUE IF UNEQUAL
: END OF ONE PASS THROUGH LIST
O117 214401 LXI H,SW ;NO SWITCHES?
O11A 7E MOV AM ZFILL A WITH SW
0118 87 ORA OA SSET FLAGS
O11C C20501 JNZ SORT — ;CONTINUE IF NOT EQUAL
; END OF SORT PROCESS, REBOOT
OliF C30000 STOP: += JMP._—=—s« REBOOT =; RESTART CCP
; CONTINUE THIS PASS
CONT:
0122 3A4501 LDA I ;LOAD I TO A REGISTER
0125 5F MOV «EA sLOW(I) TO E REGISTER
0126 1600 MVI 0,0 sHIGH(I) = 0
0128 214601 LXI H,LIST ;BASE OF LIST
0128 19 DAD OD sADOR LIST(1)
012 7E MOV AM ;LIST(I) IN A REGISTER
0120 23 INX =H sAODOR OF LIST(I+1)
O12E 3& CMP M SLIST(1) :LIST(1+1)
Q12F DA3001 JC INCL  ;SKIP IF PROPER ORDER
CHECK FOR LIST(I) = LIST(I+1)
0132 CA3001 Jz INCE  ;SKIP IF EQUAL
; ITEMS ARE OUT OF ORDER, SWITCH
0135 4€ MOV CM ;OLD LIST(I+1) TO c
0136 77 MOV MSA ;NEW LIST(I+1) TOM
0137 28 DCX H ;ADDR LIST(I)
0138 71 MOV M,C ;NEW LIST(I) TO M
0139 214401 LXI H,SW  ; SWITCH COUNT IS SW
013¢ 34 INR M sSW= SW + 1
INCI:  ; INCREMENT INDEX I
0130 214501 LXI Ht
0140 34 INR M sls 141
0141 C30FO1 IMP COMP =; TO COMPARE I WITH N-1
; DATA AREAS
0144 Sw: DS 1 ;SWITCH COUNT
0145 t: DS 1 ; INDEX
0146 O503040A08LIST: 08 5,3,4,10, 8,130,10,4
O14E 08 N: 08 $-LIST ;LENGTH OF LIST
O14F END
O1OF comp 0122 CONT 0145 I 0130 INCI
O14E N 0000 REBOOT «0105 SORT OIF STOP oad a

Section 5

---
_Page 70_

SID Users Guide

SID HIST.UTL Start SID with the HIST utility

@

Section 5

D YERS 1.4
TYPE HISTOGRAM BOUNDS 100,200 Monitor 0100 through 0200.
INITIAL = 5221
COLLECT = 5224 Entry point addresses in HIST.
DISPLAY = 5227
Fy ORT HEX SORT. SYM Load the SORT program with symbols.
SYMBOLS Program loaded, now loading symbols.
NEXT PC END
0600 0100 5187
\#P STOP Permanent break at STOP address.
\#P SORT, 3 Execute to "steady state" conditions by
\#-G passing the SORT label three times before break.

"-G"" prevents intermediate pass traces.
O1 PASS 0100

A=02 8=0004 020006 H=013F $=0100 P=0100 LXI H,O13F
We're now at the third pass through SORT.
Remove the pass point at SORT, run monitored

rom this point for OF FF steps collect data.
$20100 P=0103 MVI MOL (SW

Stopped after OFF Fsteps, display collected data:

\#-P SORT

\#UFFF, COLLECT
----- A=02 B=0004 D=0006 H=013
\*0127

\#C DISPLAY

HISTOGRAM:

ADDR

O10C

RELATIVE FREQUENCY, LARGEST VALUE = 0309

wake

kK

FIO IOI OIC IO I IOI I tk most frequently executed address:
FRM KKK IK HK KK ERK KEKE KKK KEKE KEK KERR KK KEKE KEKE KR EEKRKK ERK

ak

KKK KKK

WERK KKEKK KKK

WIR II KKK IKK IK KR KKK KKK KKK KKK ERA KK KEE KEK KKK KEK IK
KAI KEKE KKK KKK KK KEEN KEE KKK ERE

KIA K IK KIRK IKK EHR REN K KKK KKK ERK KIK KK RE KKK EK KE
keke

O11C
012c
013¢

KM KKK KKK RE KKK KKK KARR KER KEEK
KKAEKKEAKRAKKAEKKEKK

0200 \*
What's happening around the most frequently executed address?
B,BE30
Ol1D .CONT
H,013F .SW
A,M

A

0100 .SORT

\#L10C
010c LXI-
OlOF JNZ
Oll2 LXI
0115 MOV
Q116 ORA
OL1L7 NZ
STOP:
O11A
\#L1Ic
Ollc
CONT:
011D
0128 INX
0129 CMP
Ol2A JC
012D JZ
\#G0

This is where the end of LIST test takes place,

so it is reasonable that this segment of code would
be executed heavily. We could improve performance
by reducing the length of this segment. The value

of N-1 could, for example, be maintained in register
C throughout the computations, while the value of

JMP OO00 .REBOOT! Could be kept in register E, with 00 in D.

NoP There is also heavy execution around location 01iC.

E,A
0,00
H,0161 .LIST
Q

A,M

H

LDA
MOV
MVI
LXI
DAD
MOV

vi This is where we go on each element comparison

whether we switch elements or not.

M

WINCT
- INCI

---
_Page 71_

SID Users Guide Section 5

(28) TRACE .UTL Load the TRACE utility with SID.
I

D VERS 1.4
INITIAL = 5321 ;
COLLECT = 5324 TRACE entry points.
DISPLAY = 5327
READY FOR SYMBOLIC BACKTRACE Indicates that assembler/disassembler is present.
\#ITSORT.HEX SORT.SYM Ready the SORT program and symbol table.
\#R Load program and symbols to memory.
SYMBOLS
NEXT PC END
0600 0100 5287
\#P STOP Permanent break at the STOP label.
\#P.CONT,3 Pass through CONT three times before stopping.
\#UFFFF, COLLECT Untrace mode, print intermediate pass points.

----- A200 B=0000 D0=0000 H=0000 $=0100 P=0100 LXI H,O13F .SW
03 PASS 011D .CONT

----{f A=07 B=0000 D=0000 H=0140 S$=0100 P=011D LDA 0140 VI
Q2 PASS 011D .CONT

---£1 A=07 8=0003 0=0000 H=0140 $=0100 P=011D LDA 0140 .I
Q1 PASS O110 .CONT

---EI A=07 B=0004 D=0001 H=0140 $=0100 P=011D LDA 0140 .I

\*0120 Stopped on the third pass.

\#C. DISPLAY Display the backtrace from CONT.
BACKTRACE:
CONT: Most recently executed instruction.

Q11D LDA 0140 .t
O10F JNZ O11D .CONT
Ol0E CMP M
0100 OCR A

COMP:
O10A LDA 0169 .N
013c JMP O1OA .COMP
0138 INR M

INCI:
0138 LXI
Q137 INR
0134 LXI
0133 MOV
0132 DCX
0131 MOV
0130 MOV
0129 JZ
012A JC
0129 cMP
0128 INX
0127 MOV
0126 DAD
0123 «LXI
Q121 MMVI
0120 MOV

morOrrtTzToonszsrzAsTtses

011D LDA 0140 .I

OlOF JNZ O11D .CONT
QlOE CMP M
0100 OCR A

COMP Least recently executed instruction.
O1OA LDA 0169 .N (aborted with "return")

---
_Page 72_

SID Users Guide

OF

1D VERS 1.4
\#-A
\#I TRACE .UTL
\#R
INITIAL = 5921
COLLECT = 5924
DISPLAY = 5927
"A" IN EFFECT, ADDRESS BACKTRACE
\#ISORT.HEX SORT. SYM
\#R
SYMBOLS
NEXT PC END
0600 0100 5887
\#P , STOP
\#P.CONT,3

\#-UFFFF, COLLECT

A=00 8=0000 D=0000
Ql PASS O11D

~--EI A=07 B=0004 0=0001

Section 5

Start SID without loading any programs.

Remove assembler/disassembler package.
Ready the TRACE utility.
Read the TRACE package to memory.

TRACE entry point addresses.

No assembler/disassembler present.
Ready the SORT program
Read to memory.

Permanent break at STOP address,

pass point at CONT with pass count 3

Run monitored, collect data, no intermediate
H=0000 $=0100 P=0100 21 O13F pass information.

H=0140 $=0100 P=011D 3A 0140

\*0120 Stopped on third pass through CONT
\#0. DISPLAY
BACKTRACE: most recent addresses
O11D O10F O1OE 0100 910A 013C 0138 0138
0137 0134 0133 0132 0131 0130 012D 012A
0129 0128 0127 0126 0123 0121 0120 0110
O1OF O10E O100 OLOA O13C 0138 0138 0137
0134 0133 0132 0131 0130 012D 012A 0129
0128 0127 0126 0123 0121 0120 O11D O10F
O1GE O10D O10A 0108 0105 0103 0100 least recent address.
\#G0
(30) 10. PRN
3 SIMPLE BOOS OUTPUT PROGRAM
0100 ORG 1QQH ;BEGINNING OF TPA
0000 = REBOOT EQU OO00H sREBOOT ENTRY POINT
0005 = BO0S EQU O0005H ;800S ENTRY POINT
0002 = CONOUT EQU 2 ;CONSOLE QUTPUT #
0100 315401 LXI SP,STACK;LOCAL STACK
0103 C31501 JMP START ; START EXECUTION
WRCHAR: ;WRITE CHARACTER FROM REGISTER A
0106 OE02 MYT C,CONOUT; CONSOLE OUTPUT #
0108 5F MOV E,A ;CHARACTER TO E
0109 C30500 JMP BD0S ;RET THROUGH BDOS
WRMSG: ;WRITE MESSAGE STARTING AT HL ‘TIL 00
O010C 7E MOV AJM ;NEXT CHARACTER
0100 B7 ORA A 300?
O1OE C8 RZ ;RETURN IF SO
QLOF cDO601 CALL WRCHAR ;QTHERWISE WRITE IT
0112 c30CcO01 JMP WRMSG ;FOR ANOTHER CHARACTER
START: ;BEGINNING OF MAIN PROGRAM
0115 212A01 LXI H,WALLAMSG ;PART 1 OF MESSAGE
0118 cDpOCOL CALL WRMSG WRITE IT
O11B 212A01 LXI H,WALLAMSG s;PART 2 OF MESSAGE
O11E cod0col CALL WRMSG ;WRITE IT
0121 213001 LXI H, WASHMSG ;PART 3 OF MESSAGE
0124 cpocol CALL WRMSG
0127 ¢30000 STOP: MP REBOOT ;STOP THE PROGRAM
; DATA AREAS
WALLAMSG:
012A 57414¢04C41 DB "WALLA '
WASHMSG:
0130 57415348 BB "WASH?
0134 dS 32 ;16 LEVEL STACK
STACK :
0154 END

---
_Page 73_

SID Users Guide

SID IO.HEX IO.SYM
Load the test program using the HEX and SYM files.

) VERS 1.4

SYMBOLS

NEXT PC EN
0134 0100 55
\#G, .WRMSG

\*O010C .WRMSG

\#7100

\*0109

\#G, .WRMSG

W
\*Q10C
\#TW100
-7-£-
-7-E-
~7-E-
~7-£-
~7-£-
STOP:
-7-£-

REBOOT:

-7-£-
\*7A03
\#

/WRMSG

A=00
A=00
A=00
A=00
A=00

A=00
A=00

Ag
GO from 0100 to the first call on WRMSG
Now trace from the WRMSG subroutine:
B=Q000 D=0000 H=012A S$=0152 P=010C MOV A,M .WALLAMSG
B=0000 D=0000 H=012A S=0152 P=0100 ORA A
B=0000 0=0000 H=012A $=0152 P=O10E RZ
8=0000 0=0000 H=012A S=0152 P=O10F CALL 0106 .WRCHAR First
call to WRCHAR
B=0000 D=0000 H=012A S=0150 P=0106 MVI C,02 with 57 (=""w")
B=0002 D=0000 H=012A $=0150 P=0108 MOV E,A
B=0002 D=0057 H=012A S=0150 P=0109 JMP 0005 .BDOS
Call to BDOS
B=0002 0=0057 H=012A $=0150 P=0005 JMP 55AA Function # 2,
B=0002 D=0057 H=012A $=0150 P=55AA JMP 5CA4 Character "Ww"
B=0002 D=0057 H=O012A S=0150 P=5CA4 XTHL
B=0002 D=0057 H=O0112 S=0150 P=5CAS SHLD 6052 (SID code to
B=0002 0=0057 H=0112 S=0150 P=5CA8 XTHL intercept call)
B=0002 D=0057 H=012A S$=0150 P=5CA9 JMP 6E06W = first character
B=0000 0=0200 H=793B S$=0152 P=0112 JMP O10C .WRMSG now we're
back to our
B=0000 0=0200 H=7938 S$=0152 P=010C MOV A,M program, with
B=0000 0=0200 H=793B S=0152 P=010D ORA A another CALL.
B=0000 0=0200 H=793B S=0152 P=OQ10E RZ
B=0000 0=0200 H=793B8 $=0154 P=011B LXI H,O12A .WALLAMSG
B=0000 D=0200 H=012A S=0154 P=OQ11E CALL O10C .WRMSG
B=0000 0=0200 H=012A S=0152 P=010C MOV A,M .WALLAMSG
8=0000 D=0200 H=012A S=0152 P=010D ORA A
B=0000 0=0200 H=012A S=0152 P=OQ10E RZ
B=0000 020200 H=012A S=0152 P=O10F CALL 0106 .WRCHAR
B=0000 0=0200 H=012A S=0150 P=0106 MVI C,02
B=0002 D=0200 H=012A S=0150 P=0108 MOV E,A abort with “return”
GO, skip traces
Should be ALLA ..., what happened?
Trace without call:
B=0000 D=0200 H=7938 S=0152 P=010C MOV A,M
B=0000 0=0200 H=7938 S=0152 P=0100 ORA A
B=0000 D=0200 H=793B S=0152 P=O10E RZ
B=0000 D=0200 H=793B S$=0154 P=0121 LXI H,0130 .WASHMSG
B=Q000 0=0200 H=0130 S$=0154 P=0124 CALL OLOC .WRMSGW
Called WRMSG, printed another "W" and stopped!
8=0000 0=0200 H=793B $=0154 P=0127 JMP 000 .REBOOT
abort with "return” so we can restart.
B=0000 0=0200 H=793B $=0154 P=0000 JMP 7A03

It appears that the WRMSG subroutine is not saving the HL
register pair, nor is HL being incremented on each loop.

Section 5

---
_Page 74_

SID Users Guide Section 5

\#AL0F

Q10F JMP 200 We'll put a "hot patch" at the end of the WRMSG

0112 subroutine to save the HL pair, call the WRCHAR

\#4200 subroutine, restore the HL pair, then increment HL.

0200 PUSH H We're not using the region above 200, so place patch

0201 CALL .WRCHAR in this region.

0204 POP H

0205 INX H

0206 JMP .WRMSG

\#G100, .WRMSG Ok, now restart the program and stop at the first call
to WRMSG.

\*O10C .WRMSG Here we are. HL addresses the message to print, which

\#0 is the default display address following a breakpoint:

012A: 57 41 4C 4C 41 20 WALLA= message to print.
0130: 57 41 53 48 56 45 52 53 20 31 2E 34 24 31 00 02 WASHVERS 1.4$1..

\#TW100 Trace without calls: shows only the activity in WRMSG.
eecee A=00 B=0000 D=0000 H=012A $=0152 P=010C MOV A,M .WALLAMSG

wore A=57 8=0000 D=0000 H=012A S$=0152 P=010D ORA A first character
----- A=57 B=0000 0=0000 H=012A S=0152 P=010E RZ is 57 = "Ww"
aren A=57 B=0000 D=0000 H=012A S=0152 P=O10F JMP 0200 Now in patch
-a--- A=57 B=0000 D=0000 H=012A S=0152 P=0200 PUSH H area.

a---- A=57 B=0000 D=0000 H=012A S=0150 P=0201 CALL 0106 .WRCHARW = character
-Z-£- A=00 B=0000 0=0200 H=7938 $=0150 P=0204 POP H

-Z-£- A=00 B=0000 0=0200 H=012A S=0152 P=0205 INX H Move to next
-2-E- A=00 B=0000 D=0200 H=012B S=0152 P=0206 JMP O10C .wRMSG Character
WRMSG: Looping back.
-Z-E£- A=00 8=0000 0=0200 H=012B S=0152 P=010C MOV A,M

-Z-E- A=41 B=0000 D=0200 H=0128 $=0152 P=010D ORA A

---E- A=41 B=0000 D=0200 H=012B S=0152 P=O10E RZ

---E- A=41 8=0000 D=0200 H=012B $=0152 P=O10F JUMP 0200

---£- A=41 8=0000 D=0200 H=0128 $=0152 P=0200 PUSH H Here's the next
---E- A=41 B=0000 0=0200 H=0128 $=0150 P=0201 CALL 0106 .WRCHARA Character
-Z-E- A=00 B=Q000 D=0200 H=793B S$=0150 P=0204 POP H (=" A")

-Z-E- A=00 B=0000 0=0200 H=012B S=0152 P=0205 INX H
-Z-E- A=00 B=0000 D=0200 =012C S=0152 P=0206 JMP O1OC .WRMSG

WRMSG

-Z-E- A=00 8=0000 0=0200 H=012C S=0152 P=Q10C MOV A,M

\*O10D Abort with "return"
\#P.STOP Set a permanent break at STOP, then GO from

\#6100 the beginning of the program:

WALLA WASHVERS 1.4$1WALLA WASHVERS 1.4S1WASHVERS 1.4$1

QO1 PASS 0127 .STOP Things look better, but "00" byte missing on messages.

-Z-E- A=00 8=0000 D=0200 H=013E S=0154 P=0127 JMP 0000 .REBOOT
\*0000 -REBOOT

\#5. WALLAMSG+4 Place a 00 byte at the end of each meSssage.

O12E 41 (leave this value, 41 = "A" in WALLA)

Q12F 20 0 (changed to 00 from blank)

0130 57 .

\#S WASHMSG+4 Place 00 byte at the end of the second message.

0134 56 0

0135 45 .

\#6100 Break at STOP remains set, GO from the beginning.
WALLAWALLAWASH Looks good, we now have enough information to

Ol PASS 0127 .STOP go back and change the source program using ED.

-2-E- A=00 B=0000 050200 H=0134 S=0154 P=0127 JMP 9000 .REBOOT
\*0000 .REBOOT
\#60

595-2549 69

