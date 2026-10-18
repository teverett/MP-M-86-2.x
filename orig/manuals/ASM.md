# ASM

> OCR text extracted from `ASM.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `ASM.pdf`.

---

---
_Page 01_

UP DIGITAL RESEARCH

Post-Office Box 579, Pacific Grove, California 93950, (408) 649-3896

CP/M ASSEMBLER (ASM)
USER'S. GUIDE

COPYRIGHT (ce) 1976, 1978

DIGITAL RESEARCH

---
_Page 02_

Copyright. (ce) 1976, 1978 by Digital Research. All rights
reserved. No part of this publication may be reproduced,
transmitted, transcribed, stored in a retrieval system, or
translated into any language or computer language, in any
form or by any means, electronic, mechanical, magnetic,
optical, chemical, manual or otherwise,without the prior
written permission of Digital Research, Post Office Box 579,
Pacifie Grove, California 93950.

Disclaimer

Digital Research makes no representations or warranties with
respect to the contents hereof and specifically disclaims any
implied: warranties of merchantability or fitness for any
particular purpose. Further, Digital Research reserves the
right to revise this publication and to make changes from
time to time in the content. hereof without obligation of
Digital Research to notify any person of ‘such revision or
changes.

---
_Page 03_

WW DIGITAL RESEARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

CP/M ASSEMBLER (ASM)
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

Section

Se

INTRODUCTION SSFCeeeceeeees ee seeeeeseecaeeeseeoeeseoneonueneeed
PROGRAM FORMAT @@eeaeesceeovevsenaeneecaneneoevnvnecoeoevnaaeaeaeeneoeneoce|)s
FORMING THE QPERAND @eeeneeaneeeonooneoeseenseneenooneenonneeenes
3.1. Labels @eeeeevseececeaeseeoeseeeneeeoeaesceeeeeoeceoneeocenn
3.2. Numeric Constants @eeeecaooceoaee Cees eseeeecaeaaoneaaeeeed
3.36 Reserved Words @eeeeeeeveevneeaeeeeeeoeeneseaeae ee nceene@
3.4.6 String Constants @eesaeaeaeceaoceaoneeceoeooeoeneoenseaeoeaeneaneeees
3.5. Arithmetic and Logical Operators weccccccccccs
3.6. Precedence of Operators eeeneaeaeaeoeevnevneon an eeceaece
ASSEMBLER DIRECTIVES @eeseeecevoevseeoe eens eeseeeeoaeseeaeenaeoeceanene
4.1. The ORG Directive eeeoevoeoeeaeeaevooeeaesocaeo oe seeeeoaeened
4.2. The END Directive eeeeeoeeeaaeseeeeeeoeeeeneeseeeeed
4.3. The EQU Directive TARR RR RRR RRERRERERERRARRARRERRER ES
4.4. The SET Directive eeseoaseeceoeaecancoonaeaaeeoeaseeneeaese
4.5. The IF and ENDIF Directives eee¢ee20260860068606086
4.6. The DB Directive eseeaeseoeoceecnegesnen2nesneeeeoeneaneceece

4.7. The DW Directive eeeeeee onc eecesneonoeecesnesnenenenand 1

OPERATION CODES eesoevseeovoeveeeeeeceseseoanvneeenouvues eee e8e08
5.l. Jumps, Calls, and Returns eeevseeaeaeeoeoeseeeoeeoeaoeneod
5-2. Immediate Operand Instructions wecccecccccccce
5.3. Increment and Decrement Instructions .ecccoces
5-4. Data Movement InStructionS ceccccccccccccccccce
505. Arithmetic Logic Unit Operations eseeveeeaoeeevn
5.6. Control Instructions @eeeeecae cen eeoeoeeaeneagene eed ene
ERROR MESSAGES @®@eeeoaceaeeeeee eee eeeeeeeeeeeoee eee eee cede
A SAMPLE SESSION eseeeavoaszseoe eons eeoeceeenceeneoooeae ceeee ened

i

WO OOH OD U1 om & & DF

---
_Page 06_

---
_Page 07_

CP/M Assembler Users Guide

1. INTRODUCTION.

The CP/M assembler reads assembly language source files from the diskette,
and produces 8888 machine language in Intel hex format. The CP/M assembler is
initiated by typing

ASM filename
or
ASM filename .parms

In both cases, the assembler assumes there is a file on the diskette with the
name
filename .ASM

which contains an 8680 assembly language source file. The first and second
forms shown above differ only in that the second form allows parameters to be
passed to the assembler to control source file access and hex and print file
destinations.

In either case, the CP/M assembler loads, and prints the message
CP/M ASSEMBLER VER n.n

where n.n is the current version mumber. In the case of the first command,
the assembler reads the source file with assumed file type “ASM” and creates
two output files

filename .HEX
and
filename .PRN

the “HEX” file contains the machine code corresponding to the original program
in Intel hex format, and the "PRN" file contains an annotated listing showing
generated machine code, error flags, and source lines. If errors occur during
translation, they will be listed in the PRN file as well as at the console

The second canmand form can be used to redirect input and output files
from their defaults. In this case, the “parms" portion of the command is a
three letter group which specifies the origin of the source file, the
destination of the hex file, and the destination of the print file. The form
is

filename .plp2p3
where pl, p2, and p3 are single letters

pl: A,B, eee, Y designates the disk name which contains

---
_Page 08_

the source file
p2: A,B, eee, Y designates the disk name which will re-
ceive the hex file
Z skips the generation of the hex file
p3: A,B, eee, Y designates the disk name which will re-
ceive the print file

X places the listing at the console
Z skips generation of the print file
Thus, the command
ASM X.AAA

indicates that the source file (xX.ASM) is to be taken from disk A, and that
the hex (X.HEX) and print (X.PRN) files are to be created also on disk A,
This form of the command is implied if the assembler is run from disk A. That
is, given that the operator is currently addressing disk A, the above command
is equivalent to

ASM X
The command

ASM X.ABX
indicates that the source file is to be taken from disk A, the hex file is
placed on disk B, and the listing file is to be sent to the console. ‘The
command

ASM X.BZZ
takes the source file from disk B, and skips the generation of the hex and
print files (this command is useful for fast execution of the assembler to
check program syntax).

The source program format is compatible with both the Intel 8889 assembler
(macros are not currently implemented in the CP/M assembler, however), as well
as the Processor Technology Software Package #1 assembler. That is, the CP/M
assembler accepts source programs written in either format. There are certain
extensions in the CP/M assembler which make it somewhat easier to use. These
extensions are described below.

2 PROGRAM FORMAT,

An assembly language program acceptable as input to the assembler consists
of a sequence of statements of the form

line# label operation operand =; comment

where any or all of the fields may be present in a particular instance. Each

---
_Page 09_

cembly language statement is terminated with a carriage return and line feed
(the line feed is inserted automatically by the ED program), or with the
character "!" which is a treated as an end-of-line by the assembler (thus,
multiple assembly language statements can be written on the same physical line
if separated by exclaim symbols).

The line# is an optional decimal integer value representing the source
program line number, which is allowed on any source line to maintain
compatibility with the Processor Technology format. In general, these line
numbers will be inserted if a line-oriented editor is used to construct the
original program, and thus ASM ignores this field if present.

The label field takes the form

identifier
or
identifier:

and is optional, except where noted in particular statement types. The
identifier is a sequence of alphanumeric characters (alphabetics and numbers),
where the first character is alphabetic. Identifiers can be freely used by
the programmer to label elements such as program steps and assembler
directives, but cannot exceed 16 characters in length. All characters are
significant in an identifier, except for the embedded dollar symbol ($) which
can be used to improve readability of the name. Further, all lower case
alphabetics become are treated as if they were upper case. Note that the “:"
following the identifier in a label is optional (to maintain compatibility
between Intel and Processor Technology). Thus, the following are all valid
instances of labels

x xy long$name
Xs yxl: longer Snamed$data:
X1Y2 X1x2 X234S 5678$9012$3456:

The operation field contains either an assembler directive, or pseudo
Operation, or an 8886 machine operation code. The pseudo operations and
Machine operation codes are described below.

The operand field of the statement, in general, contains an expression
formed out of constants and labels, along with arithmetic and logical
operations on these elements. Again, the complete details of properly formed
expressions are given below.

The camment field contains arbitrary characters following the “;" symbol
until the next real or logical end-of-line. These characters are read,
listed, and otherwise ignored by the assembler. In order to maintain
compatability with the Processor Technology assembler, the CP/M assembler also
treat statements which begin with a "\*" in column one as comment statements,
which are listed and ignored in the assembly process. Note that the Processor

---
_Page 10_

Technology assembler has the side effect in its operation of ignoring the
characters after the operand field has been scanned. This causes an ambiguous
situation when attempting to be compatible with Intel’s language, since
arbitrary expressions are allowed in this case. Hence, programs which use
this side effect to introduce comments, must be edited to place a ";" before
these fields in order to assemble correctly.

The assembly language program is formulated as a seguence of statements of
the above form, terminated optionally by an END statement. All statements
following the END are ignored by the assembler.

3. FORMING THE OPERAND.

In order to completely describe the operation codes and pseudo operations,
it is necessary to first present the form of the operand field, since it is
used in nearly all statements. Expressions in the operand field consist of
simple operands (labels, constants, and reserved words), combined in properly
formed subexpressions by arithmetic and logical operators. The expression
computation is carried out by the assembler as the assembly proceeds. Each
expression must produce a 16-bit value during the assembly. Further, the
number of significant digits in the result must not exceed the intended use.
That is, if an expression is to be used in a byte move immediate instruction,
then the most significant 8 bits of the expression must be zero, The
restrictions on the expression significance is given with the individual
instructions,

3.1. Labels.

As discussed above, a label is an identifier which occurs on a particular
statement. In general, the label is given a value determined by the type of
statement which it precedes. If the label occurs on a_ statement which
generates machine code or reserves memory space (e.g, a MOV instruction, or a
DS pseudo operation), then the label is given the value of the program address
which it labels. If the label precedes an EQU or SET, then the label is given
the value which results from evaluating the operand field. Except for the SET
statement, an identifier can label only one statement.

When a label appears in the operand field, its value is substituted by the
assembler, This value can then be combined with other operands and operators
to form the operand field for a particular instruction.

3.2. Numeric Constants.

A numeric constant is a 16-bit value in one of several bases. The base,
called the radix of the constant, is denoted by a trailing radix indicator.
The radix indicators are

B binary constant (base 2)
O octal constant (base 8)

---
_Page 11_

Q octal constant (base 8)
D decimal constant (base 19)
H hexadecimal constant (base 16)

Q is an alternate radix indicator for octal numbers since the letter O is
easily confused with the digit 4. Any numeric constant which does not
terminate with a radix indicator is assumed to be a decimal constant.

A constant is thus composed as a sequence of digits, followed by an
optional radix indicator, where the digits are in the appropriate range for
the radix. That is binary constants must be composed of @ and 1 digits, octal
constants can contain digits in the range 9 - 7, while decimal constants
contain decimal digits. Hexadecimal constants contain decimal digits as well
as hexadecimal digits A (16D), B (11D), C (12D), D (13D), E (14D), and F
(15D). Note that the leading digit of a hexadecimal constant must be a
decimal digit in order to avoid confusing a hexadecimal constant with an
identifier (a leading @ will always suffice). A constant composed in this
manner must evaluate to a binary number which can be contained within a 16-bit
counter, otherwise it is truncated on the right by the assembler. Similar to
identifiers, imbedded "S" are allowed within constants to improve their
readability. Finally, the radix indicator is translated to upper case if a
lower case letter is encountered. The following are all valid instances of
numeric constants

1234 1234D 1100B 1111$88600$1111$9000B
1234H OFFEH 33770 =. 33$ 77$220
33770 = 6fe3h 1234d ffffh

3.3. Reserved Words.

There are several reserved character sequences which have predefined
meanings in the operand field of a statement. The names of 8088 registers are
given below, which, when encountered, produce the value shown to the right

A 7
B V4)
Cc 1
D 2
E 3
H 4
L 5
M 6
SP 6
PSW 6

(again, lower case names have the same values as their upper case
eguivalents). Machine instructions can also be used in the operand field, and
evaluate to their internal codes. In the case of instructions which require
operands, where the specific operand becomes a part of the binary bit pattern

---
_Page 12_

of the instruction (e.g, MOV A,B), the value of the instruction (in this case
MOV) is the bit pattern of the instruction with zeroes in the optional fields
(e.g, MOV produces 46H).

When the symbol "S$" occurs in the operand field (not imbedded within
identifiers and numeric constants) its value becomes the address of the next
instruction to generate, not including the instruction contained withing the
current logical line.

3.4. String Constants.

String constants represent sequences of ASCII characters, and are
represented by enclosing the characters within apostrophe symbols (°). All
strings must be fully contained within the current physical line (thus
allowing "!" symbols within strings), and must not exceed 64 characters in
length. The apostrophe character itself can be included within a string by
representing it as a double apostrophe (the two keystrokes °"), which becomes
a single apostrophe when read by the assembler. In most cases, the string
length is restricted to either one or two characters (the DB pseudo operation
is an exception), in which case the string becomes an 8 or 16 bit value,
respectively. Two character strings become a 16-bit constant, with the second
character as the low order byte, and the first character as the high order
byte.

The value of a character is its corresponding ASCII code. There is no
case translation within strings, and thus both upper and lower case characters
can be represented. Note however, that only graphic (printing) ASCII
characters are allowed within strings. Valid strings are

mn? °

A AB
“Walla Walla Wash.”

“She said “’Hello’” to me.”
“I said "Hello" to her.’

¢

3.5. Arithmetic and Logical Operators.

_ The operands described above can be combined in normal algebraic notation
using any cambination of properly formed operands, operators, and
parenthesized expressions. The operators recognized in the operand field are

a
a

b unsigned arithmetic sum of a and b
unsigned arithmetic difference between a and b
unary plus (produces b)
unary minus (identical to @ - b)
unsigned magnitude multiplication of a and b
unsigned magnitude division of a by b

b remainder after a/b
logical inverse of b (all @’s become l1’s, l’s
become @°s), where b is considered a 16-bit value

zoo
SzNer+i+
om w omomememes

---
_Page 13_

a AND b_bit-by-bit logical and of a and b

a OR b bit-by-bit logical or of a and b

a XORb- bit-by-bit logicl exclusive or of a and b

a SHL b the value which results from shifting a to the
left by an amount b, with zero fill

a SHR b- the value which results from shifting a to the
right by an amount b, with zero fill

In each case, a and b represent simple operands (labels, numeric
constants, reserved words, and one or two character strings), or fully
enclosed parenthesized subexpressions such as

10+28 18n+370 Ll /3 (L2+4) SHR 3
(‘a° and 5fh) + “@° (“B’+B) OR (PSW+M)
(1+(2+c)) shr (A-(B+i))

Note that all canputations are performed at assembly time as 16-bit unsigned
operations. Thus, -] is computed as @1 which results in the value #ffffh
(i.e., all 1°s). The resulting expression must fit the operation code in
which it is used. If, for example, the expression is used in a ADI (add
immediate) instruction, then the high order eight bits of the expression must
be zero. As a result, the operation "ADI -1" produces an error message (-l
becomes O@ffffh which cannot be represented as an 8 bit value), while "ADI (-1l)
AND @FFH" is accepted by the assembler since the “AND” operation zeroes the
high order bits of the expression.

3.6. Precedence of Operators.

AS a convenience to the programmer, ASM assumes that operators have a
relative precedence of application which allows the programmer to write
expressions without nested levels of parentheses. The resulting expression
has assumed parentheses which are defined by the relative precedence. The
order of application of operators in unparenthesize expressions is listed
below. Operators listed first have highest precedence (they are applied first
in an unparenthesized expression), while operators listed last have lowest
precedence. Operators listed on the same line have equal precedence, and are
applied from left to right as they are encountered in an expression

\* / MOD SHL SHR
-+
NOT
AND
OR XOR

Thus, the expressions shown to the left below are interpreted by the assembler
as the fully parenthesize expressions shown to the right below

ax\*b+ta (a\* b) +c
a MOD b \* c SHL a ((a MOD b) \* c) SHL GA

---
_Page 14_

a OR b AND NOI c +d SHLe a OR (b AND (NOT (c + (d SHEL e))))

Balanced parenthesized subexpressions can always be used to override the
assumed parentheses, and thus the last expression above could be rewritten to
force application of operators in a different order as

(a OR b) AND (NOT c) +d SHLe
resulting in the assumed parentheses
(a OR b) AND ((NOT c) + (d SHL e))

Note that an unparenthesized expression is well-formed only if the expression
which results from inserting the assumed parentheses is well-formed.

4. ASSEMBLER DIRECTIVES.

Assembler directives are used to set labels to specific values during the
assmbly, perform conditional assembly, define storage areas, and specify
starting addresses in the program. Each assembler directive is denoted by a
“pseudo operation” which appears in the operation field of the line. The
acceptable pseudo operations are

ORG set the program or data origin

END end program, optional start address
EQU numeric "equate"

SET numeric "set"

IF begin conditional assembly

ENDIF end of conditional assembly

DB define data bytes

DW define data words

DS define data storage area

The individual pseudo operations are detailed below
4.1. The ORG directive.
The ORG statement takes the form
label ORG expression

where “label” is an optional program label, and expression is a 16-bit
expression, consisting of operands which are defined previous to the ORG
statement. The assembler begins machine code generation at the location
specified in the expression. There can be any number of ORG statements within
a particular program, and there are no checks to ensure that the programmer is
not defining overlapping memory areas. Note that most programs written for
the CP/M system begin with an ORG statement of the form

ORG 10H

---
_Page 15_

which causes machine code generation to begin at the base of the CP/M
transient program area. If a label is specified in the ORG statement, then
the label is given the value of the expression (this label can then be used in
the operand field of other statements to represent this expression).

4.2. The END directive.

The END statement is optional in an assembly language program, but if it
is present it must be the last statement (all subseguent statements are
ignored in the assembly). The two forms of the END directive are

label END
label END expression

where the label is again optional. If the first form is used, the assembly
process stops, and the default starting address of the program is taken as
6000. Otherwise, the expression is evaluated, and becomes the program
Starting address (this starting address is included in the last record of the
Intel formatted machine code “hex” file which results from the assembly).
Thus, most CP/M assembly language programs end with the statement

END 190H

resulting in the default starting address of 1@@H (beginning of the transient
program area).

4.3. The EQU directive.

The EQU (equate) statement is used to set up synonyms for particular
numeric values. the form is

label EQU_ expression

where the label must be present, and must not label any other statement. The
assembler evaluates the expression, and assigns this value to the identifier
given in the label field. The identifier is usually a name which describes
the value in a more human-oriented manner. Further, this name is used
throughout the program to “parameterize” certain functions. Suppose for
example, that data received from a Teletype appears on a particular input
port, and data is sent to the Teletype through the next output port in
seguence. The series of equate statements could be used to define these ports
for a particular hardware environment

TTYBASE EQU- 16H 7BASE PORT NUMBER FOR TTY
TTYIN EQU) TTYBASE ;TTY DATA IN
TTIYOUT EQU TTYBASE+1;TTY DATA OUT

At a later point in the program, the statements which access the Teletype
could appear as

---
_Page 16_

IN TTYIN ;READ TTY DATA TO REG-A

OUI TTYOUT ;WRITE DATA TO TTY FROM REG-A

making the program more readable than if the absolute i/o ports had been
used. Further, if the hardware environment is redefined to start the Teletype
communications ports at 7FH instead of 16H, the first statement need only be
changed to

TTYBASE EQU 7JEH sBASE PORT NUMBER FOR TTY
and the program can be reassembled without changing any other statements.
4.4, The SET Directive.
The SET statement is similar to the EQU, taking the form
label SET expression

except that the label can occur on other SET statements within the program.
The expression is evaluated and becomes the current value associated with the
label. Thus, the EQU statement defines a label with a single value, while the
SET statement defines a value which is valid from the current SET statement to
the point where the label occurs on the next SET statement. The use of the
SET is similar to the EQU statement, but is used most often in controlling
conditional assembly.

4.5. The IF and ENDIF directives.

The IF and ENDIF statements define a range of assembly language statements
which are to be included or excluded during the assembly process. The form is

IF expression
statement#1
statement#2
statement#n
ENDIF

Upon encountering the IF statement, the assembler evaluates the expression
following the IF (all operands in the expression must be defined ahead of the
IF statement). If the expression evaluates to a non-zero value, then
statement#1 through statement#n are assembled; if the expression evaluates to
zero, then the statements are listed but not assembled. Conditional assembly
is often used to write a single “generic” program which includes a mumber of
possible run-time environments, with only a few specific portions of the
program selected for any particular assembly. The following program segments
for example, might be part of a program which communicates with either a
Teletype or a CRT console (but not both) by selecting a particular value for
TTY before the assembly begins

---
_Page 17_

TRUE EQU OFFFFH DEFINE VALUE OF TRUE
FALSE EQU NOI TRUE DEFINE VALUE OF FALSE
TTY EQU) TRUE sTRUE IF TTY, FALSE IF CRI
TTYBASE EQU 10H ;BASE OF TTY I/O PORTS
CRIBASE EQU 29H ;BASE OF CRI I/O PORIS

IF TTY sASSEMBLE RELATIVE TO TTYBASE
CONIN EQU- TTYBASE s;CONSOLE INPUT
CONOUT EQU  TTYBASE+1] ;CONSOLE OUTPUT

ENDIF
IF NOI TTY ASSEMBLE RELATIVE TO CRIBASE
CONIN BQU- CRIBASE s;CONSOLE INPUT
CONOUT EQU  CRIBASE+1 ;CONSOLE OUTPUT

ENDIF

IN CONIN ;READ CONSOLE DATA

OUT CONOUT ;WRITE CONSOLE DATA

In this case, the program would assemble for an environment where a Teletype
is connected, based at port 16H. The statement defining TTY could be changed
to

TTY

BQU FALSE

and, in this case, the program would assemble for a CRT based at port 20H.

4.6. The DB Directive.

The DB directive allows the programmer to define initialize storage areas
in single precision (byte) format. The statement form is

label DB e#l, e#2, ..., e#n

where e#1 through e#n are either expressions which evaluate to 8-bit values
(the high order eight bits must be zero), or are ASCII strings of length no
greater than 64 characters. There is no practical restriction on the nunber
of expressions included on a single source line. The expressions are
evaluated and placed sequentially into the machine code file following the
last program address generated by the assembler. String characters are
Similarly placed into memory starting with the first character and ending with
the last character. Strings of length greater than two characters cannot be
used as operands in more complicated expressions (i.e., they must stand alone
between the canmas). Note that ASCII characters are always placed in memory
with the parity bit reset (@). Further, recall that there is no translation
from lower to upper case within strings. The optional label can be used to
reference the data area throughout the remainder of the program. Examples of

---
_Page 18_

valid DB statements are

data: DB 0,1,2,3,4,5
DB data and @ffh,5,3770,1+2+3+4
signon: DB ‘please type your name’,cr,1f,@
DB AB’ SHR 8, °C’, “DE” AND 7FH

4.7. The DW Directive.

The DW statement is similar to the DB statement except double precision
(two byte) words of storage are initialized. The form is

jabel DW) e#l, e#2, ..e, ef#n

where e#1 through e#n are expressions which evaluate to 16-bit results. Note
that ASCII strings of length one or two characters are allowed, but strings
longer than two characters disallowed. In all cases, the data storage is
consistent with the 888 processor: the least significant byte of the
expression is stored forst in memory, followed by the most significant byte.
Examples are

doub: DW @ffefh,doub+4,signon—$ ,255+255
DW ‘a’, 5, ‘ab’, ‘CD’, 6 shl 8 or 1lb

4.8. The DS Directive.

The DS statement is used to reserve an area of uninitialized memory, and
takes the form

label DS expression

where the label is optional. The assembler begins subsequent code generation
after the area reserved by the DS. MThus, the DS statement given above has
exactly the same effect as the statement

label: EQU S$ sLABEL VALUE IS CURRENT CODE LOCATION
ORS $+expression sMOVE PAST RESERVED AREA

5e OPERATION CODES.

Assembly language operation codes form the principal part of assembly
language programs, and form the operation field of the instruction. In
general, ASM accepts all the standard mnemonics for the Intel 8080
microcomputer, which are given in detail in the Intel manual “8880 Assembly
Language Programming Manual.” Labels are optional on each input line and, if
included, take the value of the instruction address immediately before the
instruction is issued. The individual operators are listed breifly in the

---
_Page 19_

following sections for completeness, although it is understood that the Intel
manuals should be referenced for exact operator details. In each case,

e3 represents a 3-bit value in the range e-7
which can be one of the predefined registers
A, B, C, D, E, H, L, M, SP, or PSW.

e8 represents an 8-bit value in the range @-255
el6 represents a 16-bit value in the range @-65535

which can themselves be formed from an arbitrary combination of operands and
operators. In some cases, the operands are restricted to particular values
within the allowable range, such as the PUSH instruction. These cases will be
noted as they are encountered.

In the sections which follow, each operation codes is listed in its most
general form, along with a specific example, with a short explanation and
special restrictions.

5el. Jumps, Calls, and Returns.
The Jump, Call, and Return instructions allow several different forms

which test the condition flags set in the 8888 microcomputer CPU. The forms
are

JMP e16 JMP Ll Jump unconditionally to label
JNZ el6 JMP L2 Jump on non zero condition to label
JZ el6 JMP 166H Jump on zero condition to label
JNC el6 JNC L1+4 Jump no carry to label

JC el6 JC L3 Jump on carry to label

JPO el6 JPO $+8 Jump on parity odd to label

JPE el6 JPE L4 Jump on even parity to label

JP el6 JP GAMMA Jump on positive result to label
JM el16 JM al Jump on minus to label

CALL e16 CALL Sl Call subroutine unconditionally
CNZ «el6 CNZ S2 Call subroutine if non zero flag
CZ sel 6 CZ 100H Call subroutine on zero flag
CNC «el6 CNC S1+4 Call subroutine if no carry set
cc el 6 cc OS3 Call subroutine if carry set
CPO el6 CPO $+8 Call subroutine if parity odd
CPE el6 CPE S4 Call subroutine if parity even

CP el6 CP GAMMA Call subroutine if positive result
CM el6 CM bil$c2 Call subroutine if minus flag

RST e3 RST @ Programmed "restart", equivalent to
CALL 8\*e3, except one byte call

---
_Page 20_

Sede

A828

RC

EE:

Return from subroutine
Return if non zero flag set
Return if zero flag set
Return if no carry

Return if carry flag set
Return if parity is odd
Return if parity is even
Return if positive result
Return if minus flag is set

Immediate Operand Instructions.

Several instructions are available which load single or double precision
registers, or single precision memory cells, with constant values, along with
instructions which perform immediate arithmetic or logical operations on the
accumulator (register A).

5e36

Instructions are provided

MVI

ADI
ACI
suT
SBI
ANI
XRI
ORI
CPI

e3,e8

eg
e8
e8
e8
e8
e8
eg
e8

e3,e16

MVI B,255

_ ADI 1

ACI OFFH

SUI L+ 3

SBI L AND 11B

ANI $ AND 7FH

XRI 1111$8000B
ORI L AND 1+]

CPI ‘a’

LXI B,1@@H

Move immediate data to register A, B,
C, D, E, H, L, or M (memory)

Add immediate operand to A without carry
Add immediate operand to A with carry
Subtract from A without borrow (carry)
Subtract from A with borrow (carry)
Logical "and" A with immediate data
“Exclusive or“ A with immediate data
Logical “or" A with immediate data
Compare A with immediate data (same
as SUI except register A not changed)

Load extended immediate to register pair
(e3 must be eguivalent to B,D,H, or SP)

Increment and Decrement Instructions.

in the 8088 repetoire for incrementing or

decrementing single and double precision registers. The instructions are

5.4.

INR e3

DCR e3

INX

DCX

e3
e3

INR E

DORA

INX SP

DCX B

Single precision increment register (e3
produces one of A, B, C, D, E, H, L, M)
Single precision decrement register (e3
produces one of A, B, C, D, E, H, L, M)
Double precision increment register pair
(e3 must be equivalent to B,D,H, or SP)
Double precision decrement register pair
(e3 must be equivalent to B,D,H, or SP)

Data Movement Instructions.

---
_Page 21_

Instructions which move data
memory are given below

MOV e3,e3

LDAX
STAX
LHLD
SHLD

LDA
STA
POP

PUSH

IN
OUT
XTHL
PCHL
SPHL
XCHG

e3
e3
el6
el6
el6
el6
e3
e3

e8
e8g

MOV A,B

LDAX B
STAX D
LHLD Ll
SHLD L5+x
LDA Gamma
STA X3-5
POP PSW
PUSH B

IN @
OUT 255

from memory to the CPU and from CPU to

Move data to leftmost element from right-
most element (e3 produces one of A,B,C
D,E,H,L, or M). MOV M,M is disallowed
Load register A from computed address
(e3 must produce either B or D)

Store register A to computed address
(e3 must produce either B or D)

Load HL direct from location el6 (double
precision load to H and L)

Store HL direct to location e16 (double
precision store from H and L to memory)
load register A from address el6

Store register A into memory at el6
Load register pair from stack, set SP
(e3 must produce one of B, D, H, or PSW)
Store register pair into stack, set SP
(e3 must produce one of B, D, H, or PSW)
Load register A with data from port e8
Send data from register A to port e8
Exchange data from top of stack with HL
Fill program counter with data from HL
Fill stack pointer with data from HL
Exchange DE pair with HL pair

5.5. Arithmetic Logic Unit Operations.

Instructions which act upon the single precision accumulator to perform

arithmetic and logic operations are

ADD

ADC
SUB

e3

e3
e3

e3

e3
e3
e3
e3

ADD B
apc 6L
SUB H
SBB 2
ANA 141

ORA B

Add register given by e3 to accumulator
without carry (e3 must produce one of A,
B, C, D, E, H, or L)

Add register to A with carry, e3 as above
Subtract reg e3 from A without carry,

e3 is defined as above

Subtract register e3 from A with carry,
e3 defined as above

Logical “and" reg with A, e3 as above
"Exclusive or" with A, e3 as above
Logical “or” with A, e3 defined as above
Compare register with A, e3 as above
Decimal adjust register A based upon last
arithmetic logic unit operation
Complement the bits in register A

Set the carry flag to l

---
_Page 22_

CMC

DAD e3

Complement the carry flag

Rotate bits left, (re)set carry as a side
effect (high order A bit becomes carry)
Rotate bits right, (re)set carry as side
effect (low order A bit becomes carry)
Rotate carry/A register to left (carry is
involved in the rotate)

Rotate carry/A register to right (carry
is involved in the rotate)

DAD B Double precision add register pair e3 to
HL (e3 must produce B, D, H, or SP)

5.6. Control Instructions.

The four remaining instructions are categorized as control instructions,

and are listed below

6. ERROR MESSAGES.

Halt the 8888 processor
Disable the interrupt system
Enable the interrupt system
No operation

When errors occur within the assembly language program, they are listed as
single character flags in the leftmost position of the source listing. The
line in error is also echoed at the console so that the source listing need
not be examined to determine if errors are present. The error codes are

D

Data error: element in data statement cannot be
Placed in the specified data area

Expression error: expression is ill-formed and
cannot be computed at assembly time

Label error: label cannot appear in this context
(may be duplicate label)

Not implemented: features which will appear in
future ASM versions (e.g., macros) are recognized,
but flagged in this version)

Overflow: expression is too complicated (i.e., too
many pending operators) to computed, simplify it

Phase error: label does not have the same value on
two subsequent passes through the program

---
_Page 23_

R Register error: the value specified as a register
is not compatible with the operation code

Vv Value error: operand encountered in expression is
improperly formed

Several error mesSage are printed which are due to terminal error
conditions

NO SOURCE FILE PRESENT

NO DIRECTORY SPACE

SOURCE FILE NAME ERROR

SOURCE FILE READ ERROR

OUTPUT FILE WRITE ERROR

CANNOT CLOSE FILE

A SAMPLE SESSION.

The file specified in the ASM command does
not exist on disk

The disk directory is full, erase files
which are not needed, and retry

Improperly formed ASM file name (e.g., it
is specified with "?" fields)

Source file cannot be read properly by the
assembler, execute a TYPE to determine the
point of error

Output files cannot be written properly, most
likely cause is a full disk, erase and retry

Output file cannot be closed, check to see
if disk is write protected

The following session shows interaction with the assembler and debugger in
the development of a simple assembly language program.

---
_Page 24_

ASM SORT, asseuble SogT.Asm

CP/M ASSEMBLER - VER 1.8

b15c Wext Free addvess
@O3H USE FACTOR %of
END OF ASSEMBLY

able used 00 To FF Clutcadecises )

BIR SORT. \*,
SORT ASH source file ot elit

SORT BAK louckup frm last ede

SORT PRN eects Ceantains tile Characters)
SORT HEX wachme code file

AYTYPE SORT. PRND

SORT PROGRAN
START AT THE

waclune code. locatan i

IN CP/M ASSEMBLY LANGUAGE
BEGINNING OF THE TRANSIENT PROGRAM AR

6160 & . ORG 1@6H
geurraied waching Code
@10@ 2146612 SORT: LI H, SU ;ADBRESS SWITCH TOGGLE
0163 3661 MY I mM, t i;SET TO 1 FOR FIRST ITERATION
6165 214761 LKI H, I SADBDRESS INDEX
8188 3668 MY I M, 0 i] = 6
; COMPARE I WITH ARRAY SIZE
@10A 7E COMP: MOY ALM >A REGISTER = 1
B18B FEBS CFI N-1 ;CY SET IF I ¢ (N19)
@16D D21901 JHC CONT ;CONTINUE IF I <= (N-2)
3 END OF ONE PASS THRGUGH DaTaA
@110 2144661 LKI H, Si sCHECK FOR ZERO SWITCHES
6113 7EB7C20801 MOV ALM! ORA A! JNZ SORT 5 END GCF SORT IF SUVsO
é
0118 FF RST 7 ;GQ TO THE DEBUGGER INSTEAD OF REE
; uncaldl ou tiNuE THIS Pass
3 ANDBRESSING 3, $0 LOAD AVC TD INTO REGISTERS
0119 SFI6BO2148CORT: MOV &€.A! MYI 3B,@! LKXI HH. AV! BAD D! DAD D
6121 4&€792346 MOY CM! MOY 8, C! INX H! MOV BLM
; LOW GRDER BYTE IN A AND C, HIGH ORDER BYTE IN B
; HOV H AND L TO ADDRESS AVCI41)
@125 23 1KX H
i COMPARE VALUE WITH REGS CONTAINING AYLI
0126 965778239E SUB M! MOV DBD,A! MOV ADB! INK H! SBB A sSUBTRACT
i BORROW SET IF AVCI+1) > AVEITD
@12B DA3FA1 Je INCI ;SKIP IF IN PROPER ORDER
; CHECK FOR EQUAL VALUES
@It2E B2CAZSF OI GORA DBD! vB INCI «SKIP IF AVETD = AVOCT41) 18

---
_Page 25_

6132 S67@2B5E MOV D.M! MOY M.B! DCX H! MOV EDM

6136 712B722873 MOV MeC! DCK H! MOV M.D! DCX H! MOV HE
i
; INCREMENT SWITCH COUNT
@13B 21460134 LKI HSU! INR Mf
i INCREMENT I |
O13F 21470134C3INCI;  LKI HOI! INR M! JMP COMP
; DATA DEFINITION SECTION
0146 86 SU; DB @ ; RESERVE SPACE FOR SWITCH COUNT
8147 1: DS i ;SPACE FOR INDEX
6148 O5SGQ64Q01EAV: Dw 5,180.36, 5G, 28, 7, 1808, 390,108, -32767
GQ0A = EGU ($-AV)72 ;COMPUTE N INSTEAD OF PRE
@15C — equale value. END
A>TYPE SORT. HEX ,

: 1001606@621466136612147G136067EFEGSD2196146 .
1106116@62146617EB7C2G0G1F FSF 1666214861198 ..

: 10012606194E79234623965778239EDASFOIBZCAA? octane cocle in
: 100136003F015676285E712B7228732146813421C7 HEX Tara

: 0701400647013 4C 36801 006E

: 1001480605066 480150832601480078@GES8632C61BB

: 04615880664 0G6618GBE

: 6866666006

A>DDT SORT. HEX » start ddjuq un

16K DBT VER 1.6

Meer eb dealt addons (no alret om EU? sheet

-XP)
P=6008 160. Clhamge PC to (00
‘ i éss3e bot wal
Ww
-UFFFF > urctvace ar 65535 Steps snout

COZGMBEOIG A=06 B=G686 D=8G00 H=@aG0 S=G106 P=8100 LX! H,6146+8180
“T10) Avace (0, steps

COZGHBEGIO A=81 B2G600 D=8000 H=@146 S=G180 P=@166 LXI H,6146
COZGNBEGIO A=O61 B=0G086 D=8006 H=G146 S$=@16@ P=0143 MVI 4,61
COZGMBEC16 A=861 B=8006 D=68660 H=6146 S=01GG P=#41@5 LXI H,6147
COZGMBEOIO A=61 B=6060 D=8600 H=G147 S=4146 P=@1G3 M¥I M,.6@
COZ6MBEGI@ A=61 B=0086 D=6008 H=G147 S=G@146 P=G160R MOV ALM
CGZOMBEG1G6 A=00 B=G008 D=0666 H=0147 S=0166 F=91HB CPI 99
CIZOMIEC@1G A=66 B=4088 D=08006 H=0147 S=@160 P=G1GB JNC 6119
CIZOMLEGIG A=66 B=8600 D=6606 H=0147 S=G@10G P=@1160 LXI H,.G146
CIZOMIEGI® A=80 B=B8800 B=8006 H=@146 S=016G P=@113 MOY ALM
CIZGMIE@IO A=61 B=G008 D=8066 H=@146 S=@166 P=@114 ORA A
COZOMBEG@I® A=81 B=6086 D=8606 H=6146 S=@160 P=8115 JNZ 6169
COZGMBE@IO A=61 B=8000 D=6008 H=@146 S=@1G06 P=0100 LXI HH, 6146
COZGMBEGIBG A=B61 B=B088 D=8B06 H=0146 S=O180 P=0103 MVI M.@1
COZOMBEGI@ A=O61 B=8000 D=8808 H=0146 S=@1G6 P=@1@5 LXI H.6147
C6OZOMBEGI1G A=61 B=0000 B=8606 H=9147 S=@100 P=8148 MVI 4M, 4@
COZOMNBEGIS A=81 B=8008 D=B8G06 H=0147 S=0106 P=816H MOV ALM\*B816B

JC 119) change to a jung on Carry sere at J 19

---
_Page 26_

- XP
Py

P=010B 100, Veset Prosvam county back + beginning of Program

y
“118, trace exeuston

COZGHBE O16
COZOMBEGIG
COZEMBESIG
CGOZONBEGIG
COZGMBE SIG
COZEMBEQI4
CIZOMIESIO
CiZOMiEGI®@
CIZ@M1IEOI4G
CiZOMIEOIS
CIZOMIEGIB
COZOMIEGIG
COZGMIEGI@
COZOMIE@IO
COZOMIESIO
CEOZOMIEGI®G
L166,

8108 LXI
B193 = MVI
6195 LXI
86198 MYI
Bi9A MOV
816B CPI
816Gb uC
8116 LX!
6113 MOV
8it4 ORA
B1i15 JN2Z
61168 RST
B119 MOV
BilA AVI
B81iC Lx!

- Ghovt (st wrth rabact
“G18, start proyau. from

\#0127 ” sloprel uth au external iter rupt 7 From sont pa

A=60
A=66
A=68
A=06
A=68
A=68
A=06
A=66
A=@86
A=66
A=@86
A=08
A=@6
R=88
A=65
A=65

H, 8146
MH, @i
H,0147
M, 68
A.M

H,6146
a.m

A

6?

E.A
D,.8@
H, 6148

“bor (Ou steps

B=6006
B=6066
B=6086
B=6666
B=6686
B=06668
B=60600
B=@686 f=
B=66880
B=66800
5=G088
B=G088
B=@002
B=@605
B=6085
B=6085

b=8606
b=6606
D=B006
D=6686
D=6066
D=6006
D=6608
BOG8
D=8606
D=6606
B=6660
D=8008
D=8G08
D=6086
D=8006
D=8086

H=@147
H=6146
H=@146
H=@147
H=0147
H=6147
H=@147
H=@147
H=6147
H=@147
H=0148
H=O148
H=@143
H=8148
H=6148
H=@6149

(St some code

From (OOK

ligt move

~T45 look at loop ing Pronvam ww trace mode 7

COZONBEOIO
COZGMBEGIO
COZOMBEGIO
COZGMBEBI1SB
-B148

61468 65 66
8156 32 66
BiéB BB BO

A&=38 B=8064
A=38 B=6064
R=86 B=8064
R=86 B=6664

67 08 14 86
64 68 64 66
8@ 98 08 68

D=68086
b=3866
D=3806
D=3806

H=@156
H=0156
H=@156
H=@157

1E 69

60 66 96%

tid

P=168
P=8103
P=@1@65
P=8108
P=@1604
P=@16B
P=@1&D
P=@1139
Pe@iin
P=@1L1C
P=@11F
P=@12@
Psa@izt
PeQiz2
P=@123
P=@124

LX!
MVI
LXI
MYVI
MOV
CPI

MOV
MYT
LXI
BAD
DAD
HOY
OV
INX
MOV

fuctomartie
breakpowt

ie
H, 8146 . oe
M,@1 w

H,B147 of
M, GO ait
asM

99 4
@119

EA

b,6@
H,@148

crc om
>
Oy

B.M\*8125

curvert PC (012SH) aud Yu in veal time +o 16H

nel (Prosyaue Was
loopms |
$=@100@ P=6127 MOV D.A
$=0@106 P=@128 MOV A.B
S=@10@0 P=0129 INK H
S=@1@@ P=@12A SBR M\*G128B

a deca ts sorted, but Program doesnt stp .

2C @1 EB G3 B1 8G BB 20 G6 BO 2.
06 66 GG G8 26 BO

D.Bo.......02.

---
_Page 27_

-Gp, return +o CP/M
DDT SORT. HEX 5 reload the memoy imace

16K DOT VER 1.86

NEXT PC
B15C 8606
xP

P=2008 1805 set PC +o beginning of Progra
“Li6D, lt bad Rcode

819D JNC a119
6116 LXI H,8146

~ abort [et with ruloout
-A1ODD assemble new opcode
10D JC 119,

e118,

“L160, \yot starts sechon of Prosam.

6106 LXI HH, 8146
B103 MMVI M,61
61635 LXI #H,6147
8198 MV¥1l M.69

- aloovt (ist with rubsut a .
-A1az F) Chawye ‘cayetelr” iuctralieatum 4p OD
6133 MVI 4M, @»

B195y
nc vehurn to CP/M with otl-c (GP woks as well)

SAVE 1 SORT.COM, savé 1 pore (296 bytes, Fram A00H 40 4FFH) on disk wa Case

. we heave to veload later
A>DDT SORT. COM, vetavt DDT wreh
Saved memory wane
16K DUT VER 1.6

NEXT PC . .
6266 6106 CoM" file always storts wrth address (OOH

“Gy runthe Pronvaw from PC=I00H
e a)
"9118 programmed sp Cast 7) Ouesurtered
-~BL48
esr taen properly <arted

8148 B65 866 67 6 14 86 IE La. La

6156 32 860 64 66 64 86 ZC 01 EB 83 B61 8G BG 26 BO6 6B 2D.D.-.........
8166 66 66 68 GB BB BB 6H BG 6H GH 46GB 4O BB GB BB BB

8178 86 68 86 88 GB BB 66 B68 BB BE BB BE BY BB 4B BB

-GB) return to 07 /M aI

---
_Page 28_

ED SORT. ASH, moke Chanaes +p on ginal Programe
ch? . ;
en OC20TY Fuad net a"

avi 4,8 i1 = @
k= °
Pg one line xe tent HI ;ADDRESS INDEX
\*~) ve avether (ne.
HVI md ;SET TO 1 FOR FIRST ITERATION
“Ky kell Woe God type wordt line
aT ;ADDRESS INDEX
HvI mM, a ;ZERO SW
\*t
LXI Hod FADDRESS INDEX
«Nunc oT,
JNC\*T)
CONT ;CONTINUE IF I <= (N-2)
\*-2D01 Ll
uc Bont ;CONTINUE IF I <= (N-2)
\*E Source trem disk A
? LE lag dust st

ASM SORT. naz stig pr Ale

CP/M ASSEMBLER - VER 1.8

aise wat addvas ty assoule
@G3H USE FACTOR
END OF ASSEMBLY

ODT SORT. HEX , fest projram changes

16K DOT VER t.@
NEXT PC

G15C B6&G6
~G1bB)

\*O116
~ D148)

a dada sorte

0148 BS 86 B67 66 14 B68 IE 66

8156 32 66 64 BB 64 B@ 2C Bl E8 a3 a1 69 66 66 66 60 2.0.D.,..........

8166 00 G60 66 86 U6 88 OG GH BD 6H 6G 6H GH GH BH HG

~ abot wth rubat

“66, relura to CP/M ~ provaua Checks Ot.

---
_Page 29_

sl =

pO RRS SSS OT
=

or ees

epee

a se :

---
_Page 30_

sl =

pO RRS SSS OT
=

or ees

epee

a se :


