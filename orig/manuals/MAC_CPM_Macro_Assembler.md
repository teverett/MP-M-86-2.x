# MAC CPM Macro Assembler

> OCR text extracted from `MAC_CPM_Macro_Assembler.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `MAC_CPM_Macro_Assembler.pdf`.

---

---
_Page 001_

Copyright (ec) 1977 by Digital Research. All rights
reserved. No part of this publication may be repro-
duced, transmitted, transcribed, stored in a retrieval
system, or translated into any language or computer
language, in any form or by any means, electronic,
mechanical, magnetic, optical, chemical, manual or
otherwise, without the prior written permission of
Digital Research, Post Office Box 579, Pacific Grove,
California 93950.

This manual is tutorial in nature, however, and thus
permission is granted to reproduce or abstract the
example programs shown in enclosed figures for the
purposes of inclusion within the reader's programs.

Disclaimer

Digital Research makes no representations or war-
ranties with respect to the contents hereof and specifi-
cally disclaims any implied warranties of merchant-
ability or fitness for any particular purpose. Further,
Digital Research reserves the right to revise this
publication and to make changes from time to time
in the content hereof without obligation of Digital
Research to notify any person of such revision or
changes.

---
_Page 002_

---
_Page 003_

Table of Contents

MACRO ASSEMBLER OPERATION UNDER CP/M
PROGRAM FORMAT

FORMING THE OPERAND

3.1. Labels

Numeric Constants

Reserved Words

String Constants

Arithmetic, Logical, and Relational Operators
Precedence of Operators

o ee e e e
oe wre
. °e

ASSEMBLER DIRECTIVES
1. The ORG Directive

2 The END Directive

.3 The EQU Directive

eS The SET Directive

.o. The IF, ELSE, and ENDIF Directives

-6. The DB Directive

7 The DW Directive

8 The DS Directive

-9. The PAGE and TITLE Directives

-10. A Sample Program using Pseudo Operations

Pr POP RS bb PP

OPERATION CODES

).1. Jumps, Calls, and Returns

5.2. Immediate Operand Instructions

5.3. Inerement and Decrement Instructions
5.4 Data Movement Instructions

5.5 Arithmetic Logic Unit Operations

5.6 Control Instructions

AN INTRODUCTION TO MACRO FACILITIES

INLINE MACROS

7.1. The REPT-ENDM Group
2. The IRPC-ENDM Group
3. The IRP-ENDM Group
4 The EXITM Statement
s) The LOCAL Statement

DEFINITION AND EVALUATION OF STORED MACROS
1. The MACRO-ENDM Group

2 Macro Invocation

3 Testing Empty Parameters

4. Nested Macro Definitions

-o.  Redefinition of Macros

6 Recursive Macro Invocation

7 Parameter Evaluation Conventions

8 The MACLIB Statement

---
_Page 004_

PLICATIONS OF MACROS
Special Purpose Languages
Machine Emulation
Program Control Structures

AP
9.1
9.2
9.3
9.4 Operating Systems Interface

ASSEMBLY PARAMETERS
DEBUGGING MACROS
SYMBOL STORAGE REQUIREMENTS

ERROR MESSAGES

---
_Page 005_

Foreword

The CP/M macro assembler, called MAC, reads assembly language statements
from a diskette file and produces a "hex" format object file on the diskette suitable
for processing in the CP/M environment, and is upward compatible from the standard
CP/M non-macro assembler (see the Digital Research manual entitled "CP/M Assembler
(ASM) User's Guide"), The facilities of MAC include assembly of Intel 8080 micro-
computer mnemonics, along with assembly-time expressions, conditional assembly, page
formatting features, and a powerful macro processor which is compatible with the
standard Intel definition (MAC implements the mid-1977 revision of Intel's definition,
which is not compatible with previous versions). In addition, MAC will accept most
programs prepared for the Processor Technology Software #1 assembler, normally
requiring only minor modifications.

The macro assembler is supplied on a CP/M non-system diskette, along with a
number of standard library files. The macro assembler requires approximately 12K of
machine code and table space, along with an additional 2.5K of I/O buffer space.
Since the BDOS portion of CP/M is coresident with MAC, the minumum usable memory
size for MAC is approximately 20K. Any additional memory adds to the available
symbol] table area, thus allowing larger programs to be assembled.

Upon receiving the MAC diskette, you should follow the steps given below

(a) place the MAC diskette into drive B, with a CP/M system diskette in
drive A. Copy the MAC.COM to drive A from drive B using PIP (see the CP/M
Features and Facilities Guide for PIP operation).

(b) Copy the SAMPLE.ASM program from drive B to drive A using the PIP
program.

(e) Remove the MAC diskette from drive B, and retain the diskette for future
backup (there are a number of "LIB" files which may be useful at a later time).

(d) Type "MAC SAMPLE" to execute the macro assembler (see Figure 1).
The macro assembler should load and print the signon message. Upon completion, the
final program address is printed, followed by the "use factor" which indicates that the
assembly is complete.

(e) Type the "SAMPLE.PRN" and "SAMPLE.SYM" files, and compare with
Figure 1 to ensure that the assembier is executing properly, thus completing the MAC
test.

This manual is organized in three major sections. The first section describes
the simple assembler facilities of MAC which involve 8080 mnemonic forms, expressions,
and conditional assembly, similar to the discussion found in the ASM User's Guide. If
you are familiar with ASM, you may wish to skip over the first section, and start
reading Section 6. The second portion of this manual, beginning with Section 6,
describes the MAC macro facilities in some detail. Again, if you are familiar with
macros, you may wish to briefly skim these sections, and refer primarily to the examples
to get the "flavor" of the MAC facility. Section 10 discusses macro applications,
where common macro forms and programming practices are discussed. Again, it is
useful to skim the examples and refer back to the explanations for detailed discussions
of each program.

---
_Page 006_

---
_Page 007_

1. MACRO ASSEMBLER OPERATION UNDER CP/M

The user must first prepare a source program containing assembly language
statements using the ED program under CP/M (see the Digital Research manual "CP/M
Context Editor (ED) User's Guide"), and then submit the assembly language file for
processing under MAC. Although the user may specify certain options (described -under
"Assembly Parameters"), the usual invocation of MAC is simply

MAC filename

where "filename" corresponds to the assembly language file which was prepared using
ED, with an assumed (and unspecified) file type of "ASM." Upon completion of the
translation process, MAC leaves a file called "filename.HEX" containing the machine
code in Intel hexadecimal format which can subsequently be loaded (see the LOAD
command in the "CP/M Features and Facilities" manual), or tested under the CP/M
debugger (see the "CP/M Dynamic Debugging Tool (DDT) User's Guide"). In addition
to the HEX file, MAC also prepares a file named "filename.PRN" which contains an
annotated source listing, along with a file called "filename.SYM" which contains a
sorted list of symbols defined in the program.

Figure 1 provides an example of the output from MAC for a sample assembly
language program which is stored on the diskette under the name SAMPLE.ASM. The
macro assembler is executed by typing "MAC SAMPLE" followed by a carriage return.
Upon completion, the PRN, SYM, and HEX files will appear as shown in the figure.
The assembler listing file (PRN) includes a 16 column annotation at the left which
shows the values of literals, machine code addresses, and generated machine code.
Note that an equal sign (=) is used to denote literal values (see the EQU directive)
to avoid confusion with machine code addresses. In all cases, output files contain tab
characters (ASCII control-I) wherever possible in order to conserve diskette space. Tab
positions are assumed to be placed at every eight columns of the output line.

---
_Page 008_

OE02
1E3F
CD0500
cg

Figure 1,

Source Program (SAMPLE.ASM)

org 100h ;transient program area

equ 0005h ;bdos entry point

equ 2 ;write character function
enter with ecep's return address in the stack
write a single character (?) and return

mv i e,wehar s;write character function

mv i e,'?! ;character to write

eall bdos swrite the character

ret sreturn to the eep

end 100h ;start address is 100h

Assembler Listing file (SAMPLE. PRN)

ORG
BDOS
WCHAR

en en

100H ; TRANSTENT PROGRAM AREA

EQU 0005H ;BDOS ENTRY POINT

EQU 2 ;WRITE CHARACTER FUNCTION
ENTER WITH CCP'S RETURN ADDRESS IN THE STACK
WRITE A SINGLE CHARACTER (?) AND RETURN

MVI C,WCHAR ;WRITE CHARACTER FUNCTION
MVI E,'?' ; CHARACTER TO WRITE

CALL BDOS ;WRITE THE CHARACTER

RET ;RETURN TO THE CCP

END 100H ;START ADDRESS IS 100H

Assembler Sorted Symbol (SAMPLE.SYM)

0005 BDOS 0002 WCHAR

Assembler "Hex" Output file (SAMPLE.HEX)

:080100000E021E3FCD0500C9EF
:00010000FF

Sample ASM, PRN, SYM, and HEX Files from MAC.

---
_Page 009_

2. PROGRAM FORMAT

A program acceptable as input to the macro assembler consists of a sequence
of statements of the form

line# label operation operand comment

where any or all of the elements may be present in a particular statement. Each
assembly language statement is terminated by a carriage return and line feed (the line
feed is inserted automatically by the ED program when the file is prepared), or with
the character "!" which is treated as an end of line by the assembler. Thus, multiple
assembly language statements can be written on the same physical line if separated
by exclamation marks.

Statement elements are delimited by a sequence of one or more blank or tab
characters. Tab characters are preferred since the program element alignment is
automatically maintained in the output line at every eighth column, without requiring
extra blanks in the file. This not only conserves source file space, but also reduces
the listing file size since the tab characters are included in the PRN file. The tab
characters are not actually expanded until the file is printed or typed at the console.

The line# is an optional decimal integer value representing the source program
line number, which is allowed on any source line in case the program is prepared with
a line editor which uses line numbers at the beginning of each statement. In all cases,
the optional line# is ignored by the assembler.

The label field takes the form
identifier or identifier :

and is optional, except where noted in particular statement types. The identifier is
a sequence of alphanumeric characters (alphabeties, question marks, commercial atsigns,
and numbers) where the first character is alphabetic (including "2" and "@"). Identifiers
ean be freely used by the programmer to label elements such as program steps and
assembler directives, but cannot exceed 16 characters in length. All characters are
significant in an identifier, except for the embedded dollar sign ($) which can be used
to improve readability of the name. Further, all lower case alphabetics are treated
as if they are upper case in an identifier. Note that the ":" following the identifier
in a label is optional (to maintain compatibility between the Intel and Processor
Technology versions). Thus, the following are all valid instances of labels

Xx xy long$name

x? xyl: longer$named$data
x1x2 @123: 2??@@abcDEF
Gamma @GAMMA ?ARE$WE$HERE?
X234$5678$9012$3456:

The operation field contains an assembler directive (pseudo operation), 8080
machine operation code, or a macro invocation with optional parameters. The pseudo
operations and machine operation codes are described below, while the macro calls are
delayed for later discussion.

---
_Page 010_

The operand field of the statement, in general, contains an expression formed
from constant and label operands, with arithmetic, logical, and relational operations
upon these operands. Again, the complete details of properly formed expressions are
given in sections which follow.

The comment field is denoted by a leading ";" character, and contains arbitrary
characters until the next real or logical end of line. These character are read, listed,
and otherwise ignored in the assembly process. In order to maintain compatibility
with other assemblers, MAC also treats statements which begin with a "\*" in the first
position as comment lines.

The assembly language program is thus a sequence of statements of the above
form, terminated optionally by an END statement. All statements following the END
are ignored by the assembler.

---
_Page 011_

3. FORMING THE OPERAND

In order to completely describe the operation codes and pseudo operations, it
is necessary to first present the form of the operand field, since it is used in nearly
all statements. Expressions in the operand field consist of simple operands (labels,
constants, and reserved words), combined into properly formed subexpressions by
arithmetic and logical operators. The expression computation is carried out by the
assembler as the assembly proceeds. Each expression produces a 16-bit value during
the assembly. Further, the number of significant digits in the result must not exceed
the intended use. That is, if an expression is to be used in a byte move immediate
(see the MVI instruction), the absolute value of the operand must fit within an 8-bit
field. The restrictions on the expression significance are given with the individual
instructions.

3.1. Labels.

As discussed above, a label is an identifier which occurs on a particular statement.
In general, the label is given a value determined by the type of statement which it
precedes. If the label occurs on a statement which generates machine code or reserves
memory space (e.g., a MOV instruction or a DS pseudo operation), then the label is
given the value of the program address which it labels. If the label precedes an EQU
or SET, then the label is given the value which results from evaluating the operand
field. In the case of a macro definition, the label is given a text value (i.e., a
sequence of ASCII characters) which is the body of the macro definition. With the
exception of the SET and MACRO pseudo operations, an identifier can label only one
statement.

When a (non-macro) label appears in the operand field, its 16-bit value is
substituted by the assembler. This value can then be combined with other operands
and operators to form the operand field for a particular instruction. When a macro
identifier appears in the operation field of the statement, the text which is stored as
the value of the macro name is substituted in place of the name. In this case, the
operand field of the statement contains "actual parameters" which are substituted for
"dummy parameters" in the body of the macro definition. The exact mechanisms for
definition, invocation, and substitution of macro text are given in later sections.

3.2. Numeric Constants.

A numeric constant is a 16-bit value in one of several number bases. The base,
called the radix of the constant, is denoted by a trailing radix indicator. The radix
indicators are:

binary constant (base 2)

octal constant (base 8)

octal constant (base 8)
decimal constant (base 10)
hexadecimal constant (base 16)

mMoUoow

Q is an alternate radix indicator for octal numbers since the letter O is easily confused
with the digit 0. Any numeric constant which does not terminate with a radix indicator
is assumed to be a decimal constant.

---
_Page 012_

A constant is thus composed as a sequence of digits, followed by an optional
radix indicator, where the digits are in the appropriate range for the radix. That is,
binary constants must be composed of 0 and 1! digits, octal constants can contain digits
in the range 0 - 7, while decimal constants contain decimal digits. Hexadecimal
constants contain decimal digits as well as hexadecimal digits A through H (corresponding
to the decimal numbers 10 through 15). Note, however, that the leading digit of a
hexadecimal constant must be a decimal digit in order to avoid confusing a hexadecimal
constant with an identifier (a leading 0 will always suffice). A constant composed in
this manner will produce a binary number which can be contained within a 16-bit
counter, truncated on the right by the assembler. Similar to identifiers, imbedded "$"
symbols are allowed within constants to improve their readability. Finally, the radix
indicator is translated to upper case if a lower case letter is encountered. The
following are all valid instances of numeric constants:

1234 1234D 1l00B 111$0000$111$0000B
1234H OFFFEH 33770 33$77$22Q
33770 Ofe3h 1234d Offffh

3.3. Reserved Words.

There are several reserved character sequences which have predefined meanings
in the operand field of a statement. The names of 8080 registers are given below
which, when encountered, produce the corresponding value.

symbol value symboi value
A 0

Cc ]

E 3

L 5

SP 6

~]
Z2xm08
DM & bo

PSW

Again, lower case names have the same values as their upper case equivalents. Machine
instructions can also be used in the operand field, and result in their internal codes.
In the case of instructions which require operands, where the specific operand becomes
a part of the binary bit pattern of the instruction (e.g., MOV A,B), the value of the
instruction is the bit pattern of the instruction with zeroes in the optional fields. For
example, the statement

LXI H,MOV

assembles an LXI H instruction with an operand equal to 40H (which is the value of
the MOV instruction with zeroes as operands).

When the symbol "$" appears in the operand field (not imbedded within identifiers
and numbers), its value becomes the address of the beginning of the current instruction.
For example, the two statements

Xt JMP X
and
JMP $

both produce a jump instruction to the current location. As an exception, the "$"
symbol at the beginning of a logical line can introduce assembly formatting instructions
(see "assembly parameters").

---
_Page 013_

3.4, String Constants.

String constants represent sequences of graphic ASCH characters, and are
represented by enclosing the characters within apostrophe symbols (’). All strings must
be fully contained within the current physical line, with the "!" character within strings
treated as an ordinary string character. Each individual string must not exceed 64
characters in length, otherwise an error is reported. The apostrophe character itself
ean be included within a string by representing it as a double apostrophe (the two
keystrokes "), which become a single apostrophe when read by the assembler.

Note that particular operation codes may require that the string length be no longer
than one or two characters. The LXI instruction, for example, will accept a character
string operand of one or two characters, while the CPI instruction will accept only a
one character string. The DB instruction, however, allows strings of length zero
through 64 characters in its list of operands. In the case of single character strings,
the value becomes the 8-bit Ascii code for the character (without case translation),
while two character strings produce a 16-bit value, with the second character as the
low order byte, and the first character as the high order byte. The string constant
'A' for example, is equivalent to 41H, while the two character string 'AB' produces the
16-bit value 4142H. The following strings are valid in various MAC statements:

"A' ‘AB! Tab! ‘fel ™ ‘she said "hello™

There is one special case which must be considered inside string constants. As
discussed in later sections, the character "&" can be used to cause evaluation of dummy
arguments within macro expansions when they occur inside of string quotes. The exact
details of the substitution process will be given in the discussion of macro definition
and call statements.

3.5. Arithmetic, Logical, and Relational Operators.

The operands described above can be combined in normal algebraic notation
using any combination of properly formed operands, operators, and parenthesized
expression. The operators recognized by MAC in the operand field are given below.
In general, the letters a and b represent operands which are treated as 16~bit unsigned
quantities in the range 0-65535. All arithmetic operators (+, -, \*, /, MOD, SHL, and
SHR) produce a 16-bit unsigned arithmetic result, the relational operators (EQ, LT, LE,
GT, GE, and NE) produce a true (OFFFFH) or false (0000H) 16-bit result, and the
logical operators (NOT, AND, OR, and XOR) operate bit-by-bit on their operand(s)
producing a 16-bit result of 16 individual bit operations. The HIGH and LOW functions
alway produce a 16-bit result with a high order byte which is zero.

atb produces the arithmetic sum of a and b, +b is b

a-b produces the arithmetic difference between a and b, -b is 0-b
a\*b is the unsigned magnitude multiplication of a by b

a/b is the unsigned magnitude division of a by b

a MOD bD is the remainder after division of a by b

a SHL b produces a shifted left by b, with zero right fill

a SHR b produces a shifted right by b, with zero left fill

NOT b is the bit-by-bit logical inverse of b

a EQ b produces true if a equals b, false otherwise

---
_Page 014_

LT b produces true if a is less than b, false otherwise

LE b produces true if a is less or equal to b, false otherwise
GT b produces true if a is greater than b, false otherwise

GE b produces true if a is greater or equal to b, false otherwise
AND b produces the bitwise logical AND of a and b

OR b produces the bitwise logical OR of a and b

XOR b produces the logical exclusive OR of a and b

HIGH b is identical to b SHR 8 (high order byte of b)

LOW b is identical to b AND OFFH (low order byte of b)

om mm © ©

In general, all computations are performed curing the assembly process as 16-bit unsigned
operations, as described above. The resulting expression must fit the operation code
in which it is used. For example, the expression used in an ADI (add immediate)
instruction must fit into an 8-bit field, and thus the high order byte must be zero.
If the computed value does not fit the field, the assembler produces a value error for
that statement. As an exception to this rule, 8-bit values which would normally be
considered "negative" are allowed in 8-bit fields under the following conditions: if the
program attempts to fill an 8-bit field with a 16-bit value which has all I's in the high
order byte, and the "sign bit" is set, then the high order byte is truneated and no
error is reported. This particular condition arises when a negative sign is placed in
front of a constant. The value -2, for example, is defined (and computed) as 0-2
which produces the 16-bit value OFFFEH, where the high order byte (OFFH) contains
extended sign bits which are all I's, while the low order byte (QFEH) has the sign bit
set. Thus, the following instructions do not produce value errors in MAC:

ADI -l ADI -15 ADI -127 ADI -128 ADI OFF80H
while the following instructions do produce value errors:
ADI 256 ADI 32768 ADI -129 ADI OFF7FH

The special operator NUL is used in conjunction with macro definition and
expansion operations, and must be the last operator in the operand field, preceding
only a single operand. The use and effects of the NUL operator are delayed until the
discussion of macros.

Expressions can generally be formed from simple operands such as labels, numeric
constants, string constants, and machine operation codes, or fully enclosed parenthesized
expressions such as:

10+20, 10H+37Q, Li/3, (L2 + 4) SHR 3, ('a' and 5fh) + '0'
(BB' + B) OR (PSW + M), (1 + (2+C)) shr (A-(B + 1)), (HIGH A) SHR 3

where blanks and tabs are ignored between the operators and operands of the expression.
3.6. Precedence of Operators.
As a convenience to the programmer, MAC assumes that operators have a
relative precedence of application which allows expressions to be written without nested

levels of parentheses. The resulting expression has assumed parentheses which are
defined by this relative precedence. The order of application of operators in

---
_Page 015_

unparenthesized expressions is listed below. Operators listed first have highest prece-
dence, and are applied first in an unparenthesized expression. Operators listed last
have lowest precedence, and are applied last. Operators listed on the same line have
equal precedence, and are applied from left to right as they are encountered in an
expression:

\* / MOD SHL SHR
+

EQ LT LE GT GE NE
NOT
AND
OR XOR
HIGH LOW

Thus, the expressions shown below are equivalent:

a \* b + @ produces (a \* b) +e
a+b \* @ produces a + (b \* e)
a MOD b \* ec SHL d produces ((a MOD b) \* ec) SHL D
a OR b AND NOT c¢ + d SHL e produces a OR (b AND (NOT (ec + (d SHL e))))

Balanced parenthesized subexpressions can always be used to override the assumed
parentheses, and thus the last expression above could be rewritten to force application
of operators in a different order as shown below:

(a OR b) AND (NOT ¢) + d SHL e
resulting in the assumed parentheses:
(a OR b) AND (NOT c) + (d SHL e))

Note that an unparenthesized expression is well-formed only if the expression which
results from inserting the assumed parentheses is well-formed.

As a notational convenience, the following are equivalent:

LT
LE
EQ
NE
GE
GT

VV AWAA
fv

---
_Page 016_

4. ASSEMBLER DIRECTIVES

Assembler directives are used to set labels to specific values during assembly,
perform conditional assembly, define storage areas, and specify starting addresses in
the program. Each assembler directive is denoted by a pseudo operation which appears
in the operation field of the statement. The acceptable pseudo operations are given
below.

ORG sets the program or data origin

END terminates the physical program

EQU performs a numeric "equate"

SET performs a numeric "set" or assignment
IF begins conditional assembly

ELSE is an alternate to a previous IF

ENDIF marks the end of conditional assembly
DB defines data bytes or strings of data
DW defines words of storage (double bytes)
DS reserves uninitialized storage areas
PAGE defines the listing page size for output
TITLE enables pages titles and options

In addition to those listed above, there are several pseudo operations which are used
in conjunction with the macro processing facilities. Specifically, the MACRO, EXITM,
ENDM, REPT, IRPC, IRP, LOCAL, and MACLIB operations are reserved words, and
are fully described in separate sections which deal with macro processing. The
non-macro pseudo operations are detailed below.

4.1. The ORG Directive.
The ORG statement takes the form
label ORG expression

where "label" is an optional program label (i.e., an identifier followed by an optional
""), and "expression" is a 16-bit expression consisting of operands which are defined
previous to the ORG statement. The assembler begins machine code generation at
the location specified in the expression. There can be any number of ORG statements
within a particular program, and there are no checks to ensure that the programmer
is not redefining overlapping memory areas. Note that most programs written for
CP/M begin with an "ORG 100H" statement which causes machine code generation to
begin at the base of the CP/M transient program area.

If a label is specified in the ORG statement, then the label takes on the value
given by the expression, which is the next machine code address to assemble. This
label ean then be used in the operand field of other statements to represent this
expression.

4.2. The END Directive.
The END statement is optional in an assembly language program, but if present

it must be the last statement. All statements following the END are ignored. The
two forms of the END statement are:

---
_Page 017_

label END
label END expression

where the label is optional. If the first form is used, the assembly process stops, and
the default starting address of the program is taken as 0000. Otherwise, the expression
is evaluated and becomes the program starting address. This starting address is included
in the last record of the Intel format machine code "hex" file which results from the
assembly. Thus most CP/M assembly language programs end with the statement

END  100H

resulting in the default starting address of 100H, which is the beginning of the transient
program area.

4.3. The EQU Directive.

The EQU (equate) statement is used to name synonyms for particular numeric
values. The form is

label EQU_ expression

where the label must be present, and must not label any other statement. The
assembler evaluates the expression and assigns this value to the identifier given in the
label field. The identifier is usually a name which describes the value in a more
human-oriented manner. Further, this name can be used throughout the program as
a parameter for certain functions. Suppose, for example, that data received from a
Teletype appears on a particular input port, and data is sent to the Teletype through
the next output port in sequence. The series of equate statements that could be used
to define these ports for a particular hardware environment are shown below.

TTYBASE EQU_ 10H ;BASE TTY PORT
TT YIN EQU TTYBASE ;TTY DATA IN
TTYOUT EQU TTYBASE+1] ;TTY DATA OUT
At a later point in the program, the statements which access the Teletype could appear
as:
IN TTYIN ;sREAD TTY DATA TO A
OUT TTYOUT ;sWRITE DATA FROM A
making the program more readable than if the absolute I/O port addresses had been
used. If the hardware environment is later redefined to start the Teletype communica-
tions ports at 7FH instead of 10H, the first statement need only be changed to:
TTYBASE EQU T7FH ;BASE PORT NUMBER FOR TTY
and the program can be reassembled without changing any other statements.
4.4. The SET Directive
The SET statement is similar to the EQU, taking the form

label SET expression

---
_Page 018_

except that the label, taken as a variable name, can occur on other SET statements
within the program. The expression is evaluated and becomes the current value
associated with the label. Thus, unlike the EQU statement where a label takes on a
single value throughout the program, the SET statement can be used to assign different
values to a name at different parts of the program. In particular, the SET statement
gives the label a value which is valid from the current SET statement to the point
where the label occurs on the next SET statement. The use of SET is similar to the
EQU, except that SET is used more often to control conditional assembly within macros.

4.5. The IF, ELSE, and ENDIF Directives.

The IF, ELSE, and ENDIF directives define a range of assembly language
statements which are to be included or excluded during the assembly process. The IF
and ENDIF statements alone can be used to bound a group of statements to be
conditionally assembled, as shown below:

IF expression
statement#l
statement#2

statement#n
ENDIF

Upon encountering the IF statement, the assembler evaluates the expression following
the IF (all operands in the expression must be defined ahead of the IF statement). If
the expression produces a non-zero value then statement#1 through statement#n are
assembled. If the expression evaluates to a zero value then the statement are listed
but not assembled.

Conditional assembly is often used to write a single "generic" program which
includes a number of possible alternative subroutines or program segments, where only
a few of the possible alternatives are to be included in any given assembly. Figures
2a and 2b give an example of such a program. Assume that a console device (either
a Teletype or CRT) is connected to an 8080 microcomputer through I/O ports. Due
to the electronic enviroment, the "current loop" Teletype is connected through ports
10H and NH, while the "RS-232" CRT is connected through ports 20H and 21H. The
program continually loops, reading and writing console characters. A single program
is shown which, when the condition is properly set, produces a program which operates
with either a Teletype (TTY is TRUE), or with a CRT (TTY is FALSE), but not both.
Figure 2a shows an assembly for the Teletype environment, while Figure 2b shows the
assembly for a CRT-based system. Note that the leftmost 16 columns are left blank
by the assembler when statements are skipped due to a false condition.

The ELSE statement can be used as an alternative to an IF statement, and must
occur between the IF and ENDIF statements. The form is:
IF expression
statement#]

statement#2

statement#n

---
_Page 019_

CP/M MACRO ASSEM 2.0

FFFF
FFFF

= TRUE
= FALS
= TTY

= TTYB
= CRTB

= CONI

\#001

EQU
E  EQU
EQU
ASE EQU
ASE EQU
IF
TITLE
N  EQU

= CONOUT EQU

CON!

ENDIF

IF

TITLE
N EQU

CONOUT EQU

ENDIF

DB10 ECHO: IN

D311
C30000

Figure 2a.

OUT
JMP
END

Teletype Echo Program

OFFFFH ;DEFINE "TRUE"

NOT TRUE;DEFINE "FALSE"

TRUE ;SET TTY ON

10H ;BASE OF TTY PORTS

20H ;BASE OF CRT PORTS

TTY ; ASSEMBLE TTY PORTS
'Teletype Echo Program'
TTYBASE ;CONSOLE INPUT
TTYBASE+1 ;CONSOLE OUT

NOT TTY ;ASSEMBLE CRT PORTS

'CRT Echo Program'

CRTBASE ;CONSOLE IN

CRTBASE+1 ;CONSOLE OUT

CONIN ;READ CONSOLE CHARACTER
CONOUT ;WRITE CONSOLE CHARACTER
ECHO

Conditional Assembly with TTY "True."

---
_Page 020_

CP/M MACRO ASSEM 2.0

FFFF

= TRUE
= FALS
= TTY

= TTYB
= CRTB

CONI

\#001

EQU
E  EQU
EQU
ASE EQU
ASE EQU
IF
TITLE
N  EQU

CONOUT EQU

= CONI

ENDIF

TITLE
N EQU

= CONOUT EQU

ENDIF

DB20 ECHO: IN

D321
C30000

Figure 2b.

CRT Echo Program

OFFFFH ;DEFINE "TRUE"

NOT TRUE;DEFINE "FALSE"

FALSE ;SET CRT ON

10H ;BASE OF TTY PORTS

20H ;BASE OF CRT PORTS

TTY ; ASSEMBLE TTY PORTS
"Teletype Echo Program'
TTYBASE ;CONSOLE INPUT
TTYBASE+1 ;CONSOLE OUT

NOT TTY ;ASSEMBLE CRT PORTS

‘CRT Echo Program!

CRTBASE ;CONSOLE IN

CRTBASE+1 ;CONSOLE OUT

CONIN ;READ CONSOLE CHARACTER
CONOUT ;WRITE CONSOLE CHARACTER
ECHO

Conditional Assembly with TTY "False."

---
_Page 021_

ELSE
statement#ntl
statement#nt2

statement#m
ENDIF

If the expression produces a non-zero (true) value, then statements | through n are
assembled, as before. In this case, however, statements n+l through m are skipped in
the assembly process. When the expression produces a zero value (false), statements
1 through n are skipped, while statements n+] through m are assembled. As an example,
the conditional assembly shown in Figure 2 could be rewritten as shown in Figure 3a.

Properly balanced IF's, ELSE's, and ENDIF's can be completely contained within the
boundaries of outer encompassing conditional assembly groups. The structure outlined
below shows properly nested IF, ELSE, and ENDIF statements:

IF exp#]
group#]

IF exp#2
group# 2
ELSE
group#3
ENDIF
group#4
ELSE
group#5

IF exp#3
group#6
ENDIF
group#7
ENDIF

where group | through 7 are sequences of statements to be conditionally assembled,
and exp#1 through exp#3 are expressions which control the conditional assembly. If
exp#1 is true, then group#1 and group#4 are always assembled, and groups 5, 6, and
7 will be skipped. Further, if exp#l and exp#2 are both true, then group#2 will also
be included in the assembly, otherwise group#3 will be included. If exp#1 produces a
false value, groups 1, 2, 3, and 4 will be skipped, and groups 5 and 7 will always be
assembled. If under these circumstances, exp#3 is true then group#6 will also be
included with 5 and 7, otherwise it will be skipped in the assembly. A _ structure
similar to this is shown in Figure 3b, where literal true/false values are used to show
conditional assembly selection.

Conditional assembly of this sort can be nested up to eight levels (i.e., there
can be up to eight pending IF's or ELSE's with unresolved ENDIF's at any point in the
assembly), but usually becomes unreadable after two or three levels of nesting. The
nesting level restriction also holds, however, for pending IF's and ELSE's during macro
evaluation. Nesting level overflow will produce an error during assembly.

4.6. The DB Directive.

The DB directive allows the programmer to define initialized storage areas in
single precision (byte) format. The statement form is

---
_Page 022_

CP/M MACRO ASSEM 2.0

FFFF

= TRUE
= FALS
= TTY

\#001

EQU
E EQU
EQU

= TTYBASE EQU
= CRTBASE EQU

IF
TITLE

CONIN  EQU
CONOUT EQU

= CONI

ELSE
TITLE
N EQU

= CONOUT EQU

ENDIF

DB20 ECHO: IN

D321
C30000

Figure 3a.

OUT
JMP
END

Conditional

CRT Echo Program

QFFFFH ;DEFINE "TRUE"

NOT TRUE;DEFINE "FALSE"

FALSE ; SET CRT ON

10H ;BASE OF TTY PORTS

20H ;BASE OF CRT PORTS

TTY ; ASSEMBLE TTY PORTS

"Teletype Echo Program'

TTYBASE ;CONSOLE INPUT

TTYBASE#+1 3;CONSOLE OUT
; ASSEMBLE CRT PORTS

"CRT Echo Program!

CRTBASE ;CONSOLE IN

CRTBASE?+1 ;CONSOLE OUT

CONIN ;READ CONSOLE CHARACTER
CONOUT ;WRITE CONSOLE CHARACTER
ECHO

Assembly Using "ELSE" for Alternate.

---
_Page 023_

tt

FFFF

TRUE EQU OFFFFH ;DEFINE "TRUE"
FALSE _EQU NOT TRUE :DEFINE "FALSE"
IF FALSE
MVI A,l
IF TRUE
MVI A,2
ELSE
MV I A,3
ENDIF
MVI A,4
ELSE
0000 3805 MVI A,5
IF TRUE
0002 3806 MV 1 A,6
ELSE
MVI A
ENDIF
0004 3508 MVI A,8
ENDIF
0006 END

Figure 3b. Sample Program using Nested IF, ELSE, and ENDIF

---
_Page 024_

label DB e#], e#2,..., efn

where the label is optional, and e#1 through e#n are either expressions which produce
8-bit values (the high order eight bits are zero, or the high order nine sign bits are
one's), or are ASCII strings of length no greater than 64 characters each. There is
no practical restriction on the number of expressions included on a single source line.
The expressions are evaluated and palced sequentially into the machine code following
the last program address generated by the assembler. String characters are similarly
placed into memory starting with the first character and ending with the last character.
Strings of length greater than two characters cannot be used as operands in more
complicated expressions (i.e., they must stand alone between the commas). Note that
ASCII characters are always placed in memory with the high order (parity) bit reset
to zero. Further, recall that there is no translation from Jower to upper case within
strings. The optional label can be used to reference the data area throughout the
program. Examples of valid DB statements are:

data: DB 0,1,2,3,4,5,6
DB data and Offh,5,377Q,1+2+3+4
signon: DB ‘please type your name:',cr,|f,0

DB 'AB' SHR 8, 'C’, 'DE' AND 7FH
DB HIGH data, LOW (signon GT data)

4.7. The DW Directive.

The DW statement is similar to the DB statement except double precision (two
byte) words of storage are initialized. The form is:

label DW e#l, e#2,..., e#n

where the label is optional, and e#1] through e#n are expressions which produce 16-bit
values. Note that Ascii strings of length one or two characters are allowed, but
strings longer than two characters are disallowed. In all cases, the data storage is
consistent with the 8080 processor: the least significant byte of the expression is
stored first in memory, followed by the most significant byte. The following DW
statements are examples of properly formed statements:

doub: DW Offefh, doub+4,signon-$,255+255
DW ‘a’, 5, 'AB', 'CD', doub LT signon
4.8. The DS Directive.

The DS statement is used to reserve an area of uninitialized memory, and takes
the form:

label DS expression
where the label is optional. The assembler begins subsequent code generation after

the area reserved by the DS. Thus, the DS statement given above has exactly the
same effect as the statement sequence:

---
_Page 025_

label: EQU $ ;CURRENT CODE LOC
ORG $texpression ;sMOVE PAST AREA

4.9. The PAGE and TITLE Directives.

The PAGE and TITLE pseudo operations give the programmer control over the
output formatting which is sent to the PRN file (or directly to the printer device).
The forms for the PAGE statement are:

PAGE
and
PAGE expression

If the PAGE statement stands alone, as in the first case above, the output page is
ejected to the top of form (i.e., an ASCII control-L (form feed) is sent to the output
file). The form feed is sent after the statement with PAGE has been printed, thus
the PAGE command is often issued directly ahead of major sections of an assembly
language program, such as a group of subroutines, to cause the next statement to
appear at the top of the following printer page.

The second form of the PAGE command is used to specify the output page size.
In this case, the expression which follows the PAGE pseudo operation determines the
number of output lines to be printed on each page. If the expression is zero, there
are no page breaks, and the print file is simply a continuous sequence of annotated
output lines. If the expression is non-zere, then the page size is set to the value of
the expression, and form feeds are issued to cause page ejects when this count is
reached for each page. The assembler initially assumes that

PAGE 56

is in effect, thus producing a page eject at the beginning of the listing, and at each
96 line increment.

The TITLE directive takes the form:
TITLE string-constant

where the string-constant is an ASCII string, enclosed in apostrophes, which does not
exceed 64 characters in length. If a TITLE pseudo operation is given during the
assembly, each page of the listing file is prefixed with the title line, preceded by a
standard MAC header. The title line thus appears as:

CP/M MACRO ASSEM n.n_ #ppp string-constant

where n.n is the MAC version number, ppp is the page number in the listing, and
string-constant is the string given in the TITLE pseudo operation. MAC initially
assumes that the TITLE operation is not in effect. When specified, the title line,
along with the blank line which follows the title, are not included in the line count
for the page. Normally, no more than one TITLE statement is included in a particular
program. Similarly, no more than one PAGE statement with the expression option is
normally included.

---
_Page 026_

If a TITLE statement is included, and the symbol table is being appended to
the PRN file (see "assembly parameters"), then the SYM file also contains the specified
title at the beginning of the symbol listing, with page breaks given by either the
default or specified value of the PAGE statement.

4.10 A Sample Program using Pseudo Operations.

Figure 4 demonstrates the various pseudo operations available in MAC. The
sample program, called "typer,” is intended to operate in the CP/M environment by
performing the simple function of selecting one of three messages for output at the
console. This program is created using the ED program, then assembled using MAC,
and then placed into "COM" file format using the CP/M LOAD function. Given that
these steps have been accomplished, typer is executed at the console command processor
level of CP/M by typing one of the commands:

typer a
typer b
typer ¢

to select message A, B, or C for printing. The typer program loads under the CCP,
and jumps to the label START where the 8080 stack is initialized. The typer program
then prints its "signon" message, which would appear as:

'typer' version 1.0

The program then retrieves the first character typed at the console following the
command "typer" which should be one of the letters A, B, or C. If one of these
letters is not specified, then typer "reboots" the CP/M system to give control back
to the CCP. If a valid letter is provided, typer selects one of the three messages
(MESS@A, MESS@B, or MESS@C) and prints it at the console before returning to CP/M.

Note that the TITLE and PAGE statements are used to produce a title at the
beginning of each page (form feeds were necessarily suppressed here), with a page size
of 20 lines, excluding the title lines. A number of EQU statements are used at the
beginning to improve readability of the program. Note that the exclaim symbol (!) is
used throughout the program to allow several simple assembly language statements on
the same line. Although multiple statements make the program more compact, they
often decrease the overall readability of the source program. Note also that the
program terminates without the END statement, which is only necessary if a starting
address is specified. The END statement is often included, however, to maintain
compatibility with other assemblers.

The DB statements labelled by SIGNON contain simple strings of characters, as
well as expressions which produce single byte values. The DW statement following
TABLE defines the base address of each string (corresponding to A, B, and C). Finally,
the DS statement at the end of the program reserves space for the stack defined
within the typer program.

---
_Page 027_

O00A
005C
000D
000A

010A
O10E

011B
O11E

CP/M MACRO ASSEM 2.0 #001 Typer Program

otk tho

tow ow

C31201

7EB7C8

SFOEO2E5
CD0500E1
23C30301

31C101
213701
CD0301

3A5D00
D641
FE03
D20000

TITLE 'Typer Program'
PAGE 33
; PRINT THE MESSAGE SELECTED BY THE INPUT COMMAND A,B, OR C
VERS EQU 10 ; VERSION NUMBER N.N
BOOT EQU 0000H ;REBOOT ENTRY POINT
BDOS EQU 0005H ;BDOS ENTRY POINT
TFCB EQU 005CH ;DEFAULT FILE CONTROL BLOCK (GET A,B, OR C)
WCHAR EQU 2 ;WRITE CHARACTER FUNCTION
CR EQU 0DH ;CARRIAGE RETURN CHARACTER
LF EQU OAH ;LINE FEED CHARACTER
STKSIZ EQU 16 ;SIZE OF LOCAL STACK (IN DOUBLE BYTES)
ORG 100H ;sORIGIN AT BASE OF TPA
JMP START ; JUMP PAST THE MESSAGE SUBROUTINE

WMES SAGE:
;WRITE THE STRING AT THE ADDRESS GIVEN BY HL 'TIL 00
MOV A,M! ORA A! RZ ;RETURN IF AT 00
MOV E,At MVI C,WCHAR! PUSH H ;READY TO PRINT
CALL BDOS! POP H ;CHARACTER PRINTED, GET NEXT
INX H! JMP WMESSAGE

START: ;ENTER HERE FROM THE CCP, RESET TO LOCAL STACK
LXI SP, STACK ;SET TO LOCAL STACK
LXI H, SIGNON -WRITE THE MESSAGE
CALL WMESSAGE ;'TYPER' VERSION N.N

LDA TFCB+1 ;GET FIRST CHAR TYPED AFTER NAME
SUI TA! ; NORMALIZE TO 0,1,2

CPI TABLEN “COMPARE WITH THE TABLE LENGTH
INC BOOT sREBOOT IF NOT VALID

COMPUTE INDEX INTO ADDRESS TABLE BASED ON A'S VALUE

Figure 4. "Typer" Program Listing (Part A).

---
_Page 028_

012B
012C
012D
O12E
012F

014A

014D

01Al

CP/M MACRO ASSEM 2.0 #002 Typer Program

oF
214D01

CD0301
C30000

°
bd

SIGNON:
2774797065
312E30
0DO0A00

TABLE:
9301670182
= TABLEN

7468697320MESS@A:
796F752073MESS@B:
7468697320MESS@C:

:

STACK:

MOV
MV I
LXI
DAD
DAD
MOV
INX
MOV ;HIGH ORDER MESSAGE ADDRESS TO DE
XCHG ;READY FOR PRINTOUT

CALL WMES SAGE ;MESSAGE WRITTEN TO CONSOLE

JMP BOOT ;REBOOT, GO BACK TO CCP LEVEL

; LOW ORDER INDEX

; EXTENDED TO DOUBLE PRECISION
ABLE ;BASE OF THE TABLE TO INDEX

;SINGLE PRECISION INDEX

; DOUBLE PRECISION INDEX

; LOW ORDER BYTE TO E

toh

Cr a AS

=
maAmaAanwmraA

=

DATA AREAS

DB '''typer'' version '
DB VERS/10+'0', '.', VERS MOD 10 +'0'!
DB CR,LF,O0 ;END OF MESSAGE

;OF MESSAGE BASE ADDRESSES
DW MESS@A , MESS@B , MESS@C
EQU ($-TABLE) /2 ; LENGTH OF TABLE

DB "this is message a',CR,LF,0
DB ‘you seleeted b this time',CR,LF,0
DB "this message comes out for ¢c',CR,LF,0

DS STKSIZ\*2 ;RESERVES AREA FOR STACK

Figure 4. "Typer" Program Lisitng (Part B).

---
_Page 029_

5. OPERATION CODES

Operation codes, found in the operation field of the statement, form the principal
components of assembly language programs. In general, MAC accepts all the standard
mnemonics for the Intel 8080 microcomputer, which are given in detail in the Intel
manual "8080 Assembly language Programming Manual." Labels are optional on each
input line and, if included, take the value of the instruction address immediately before
the instruction is issued by the assembler. The individual operators are listed briefly
in the following sections in order to be complete, although it is understood that the
Intel documents should be referenced for exact operator details. In the discussion
which follows, the operation codes are placed into categories for discussion purposes,
followed by a sample assembly which shows the hexadecimal codes produced for each
operation. The following notation is used throughout the discussion:

e3 represents a 38-bit value in the range 0-7, which usually
takes one of the predefined register values A, B, C, D,
H, L, M, SP, or PSW.

e8 represents an 8-bit value in the range 0-255 (recall
that signed 8-bit values are also allowed in the range
-128 through +127)

e16 represents a 16-bit value in the range 0-65535

where e3, e8, and el6 can themselves be formed from an arbitrary combination of
operands and operators in a well-formed expression. In some cases, the operands are
restricted to particular values within the range, such as the PUSH instruction. These
eases will be noted as they are encountered.

o.1. Jumps, Calls, and Returns.

The jump, call and return instructions allow several different forms, as shown
in Figure 5. In some cases, the condition flags are tested to determine whether or
not the jump, call, or return is to be taken. The forms are shown below.

JMP el6 JNZ e16 JZ el6
JNC el6 JC el6 JPO el6
JPE ei6 JP el6 JM el6

The call instructions are:

CALL el6 CNZ el6 CZ el6
CNC el6 CC el6 CPO el6
CPE el6 CP el6 CM el6

Thre return instructions are:

RET RNZ RZ
RNC RC RPO
RPE RP RM

The restart instruction takes the form:

---
_Page 030_

000C
OO0F

001B
OO1E
002A
002D

003A
003B
003C
003D
003E
003F

CP/M MACRO ASSEM 2.0 #001 8080 JUMPS, CALLS, AND RETURNS

C31B00
C25C00
CA0001
D21F00
DA4142
E21700
EA0DDO0
F24100
FAI1BO0

CD36 00
C43800
CC0001
D43A00
DC0000
E43200
EC0900
F44100
FC4100

EO

Figure 5,

LIL:

Sl:

D4
GAMMA :

TITLE ‘8080 JUMPS, CALLS, AND RETURNS'

JUMPS ALL REQUIRE A 16 BIT OPERAND

JMP Ll ; JUMP UNCONDITIONALLY TO LABEL
JINZ L1+'A'  ;JUMP ON NON ZERO TO LABEL

JZ 100H ; JUMP ON ZERO CONDITION TO LABEL
JNC L1t+4 ; JUMP ON NO CARRY TO LABEL

JC "AB! ; JUMP ON CARRY TO LABEL

JPO $+8 ; JUMP ON PARITY ODD TO LABEL

JPE L1/2 ; JUMP ON EVEN PARITY TO LABEL

JP GAMMA ; JUMP ON POSITIVE RESULT TO LABEL
JM LOW L1 ;JUMP ON MINUS TO LABEL

CALL OPERATIONS ALL REQUIRE A 16-BIT OPERAND

CALL Sl ;CALL SUBROUTINE UNCONDITIONALLY
CNZ Sit+x ;CALL SUBROUTINE IF NON ZERO FLAG
CZ 100H ;CALL SUBROUTINE IF ZERO FLAG

CNC S1+4 ;CALL SUBROUTINE IF NO CARRY FLAG
CC S1 MOD 3;CALL SUBROUTINE IF CARRY FLAG
CPO $+8 ;CALL SUBROUTINE IF PARITY ODD
CPE S1-$ ;CALL SUBROUTINE IF PARITY EVEN
CP GAMMA ;CALL SUBROUTINE IF POSITIVE

CM GAM$MA ;CALL SUBROUTINE IF MINUS FLAG

PROGRAMMED RESTART (RST) REQUIRES 3-BIT OPERAND
(RST X IS EQUIVALENT TO CALL X\*8)

RST 0 ;"RESTART" TO LOCATION 0
RST Xt+1

RETURN INSTRUCTIONS HAVE NO OPERAND

RET ;RETURN FROM SUBROUTINE
RNZ ;RETURN IF NON ZERO

RZ sRETURN IF ZERO FLAG SET
RNC ;RETURN IF NO CARRY FLAG
RC ;RETURN IF CARRY FLAG SET
RPO ;RETURN IF PARITY IS ODD
RPE ;RETURN IF PARITY IS EVEN
RP ;RETURN IF POSITIVE RESULT
RM ;RETURN IF MINUS FLAG SET
EQU 2

END

Assembly showing Jumps, Calls, Returns, and Restarts.

---
_Page 031_

RST e3

and performs exactly the same function as the instruction "CALL e3\*8" except that
it requires only one byte of memory for the instruction.

Figure 5 shows the hexadecimal codes for each instruction, along with a short
comment on each line which describes the function of the instruction.

5.2. Immediate Operand Instructions.

Several instructions are available which load single or double precision registers
or single precision memory cells with constant values, along with instructions which
perform immediate arithmetic or logical operations on the accumulator (register A).
The "move immediate" instruction takes the form:

MVI e3,e8
where e3 is the register to receive the data given by the value e8. The expression
e3 must produces a value corresponding to one of the registers A, B, C, D, E, H, L,
or the memory location M which is addressed by the HL register pair.

The "accumulator immediate" operations take the form:

ADI e8 ACI e8& SUI e8 SBI e8
ANI e8 XRI e8 ORI e8 CPI e8

where the operation in always performed upon the accumulator using the immediate
data value given by the expression e8.

The "load extended immediate" instructions take the form:
LXI e3,e16
where e3 designates the register pair to receive the double precision value given by
e16. The expression e3 must produce a value corresponding to one of the double

precision register pairs B, D, H, or SP.

Figure 6 shows the use of the accumulator immediate operations in an assembly
language program, along with a short comment describing the use of each instruction.

d.3. Increment and Decrement Instructions.

Instructions are provided in the 8080 repetoire for incrementing or decrementing
single and double precision registers. The instruction forms for single precision registers
are:

INR e3 DCR e3
where e3 produces a value corresponding to one of the registers A, B, C, D, H, L, or

M (corresponding to the byte value at the memory location addressed by HL). The
double precision instructions are:

---
_Page 032_

CP/M MACRO ASSEM 2.0 #001 IMMEDIATE OPERAND INSTRUCTIONS
TITLE ‘IMMEDIATE OPERAND INSTRUCTIONS!

. MVI USES A REGISTER (3BIT) OPERAND AND 8-BIT DATA
0000 O6FF MVI B,255  ;MOVE IMMEDIATE A,B,C,D,E,H,L,M

; ALL REMAINING IMMEDIATE OPERATIONS USE A REGISTER
0002 C601 ADI 1 ;ADD IMMEDIATE TO A W/O CARRY
0004 CEFF ACI OFFH ;ADD IMMEDIATE TO A WITH CARRY
0006 D613 SUI L1+3 ; SUBTRACT FROM A W/O BORROW (CARRY)
0008 DE10 SBI LOW L1 ;SUBTRACT FROM A WITH BORROW (CARRY)
000A E602 ANI $ AND 7 ;LOGICAL "AND" WITH IMMEDIATE DATA
000C EE3C XRI 1111$00B;LOGICAL "XOR" WITH IMMEDIATE DATA
OOO0E F6FD ORI -3 ;LOGICAL "OR" WITH IMMEDIATE DATA
Ll:
0010 END

Figure 6. Assembly using Immediate Operand Instructions.

CP/M MACRO ASSEM 2.0 #001 INCREMENT AND DECREMENT INSTRUCTIONS
TITLE ‘INCREMENT AND DECREMENT INSTRUCTIONS '

; INSTRUCTIONS REQUIRE REGISTER (3-BIT) OPERAND
0000 1c INR E -BYTE INCREMENT A,B,C,D,E,
0001 3D DCR A ;BYTE DECREMENT A,B,C,
0002 33 INX SP -16-BIT INCREMENT B,D,
0003 0B DCX B ;16-BIT DECREMENT B,D,
0004 END

H,L,M
D,E,H,L,M
H,SP
H,SP

Figure 7. Assembly containing Increment and Decrement Instructions.

---
_Page 033_

INX e3 DCX e3

where e3 must be equivalent to one of the double precision register pairs B, D, H, or
SP.

Figure 7 shows a sample assembly language program which uses both single and
double precision increment and decrement operations.

9.4. Data Movement Instructions.

A number of 8080 instructions are placed in this category which move data
from memory to the CPU and from the CPU to memory. A number of register to
register move operations are also included. The single precision "move register"
instruction takes the form:

MOV e3,e3'

where e3 and e3’ are expressions which each produce one of the single precision
registers A, B, C, D, E, H, L, or M (corresponding to the memory location addressed
by HL). In all eases, the register named by e3 receives the 8-bit value given by the
register expression e3'. The instruction is often read as "move to register e3 from
register e3'."| The instruction "MOV B,H" would thus be read as "move to register B
from register H." Note that the instruction MOV M,M is not allowed.

The single precision load and store extended operations take the form:
LDAX e3 STAX e3

where e3 is a register expression which must produce one of the double precision
register pairs B or D. The 8-bit value in register A is either loaded (LDAX) or stored
(STAX) from/to the memory location addressed by the specified register pair.

The load and store direct instructions operate either upon the A register for
single precision operations, or upon the HL register pair for double precision operations,
and take the forms:

LHLD el6 SHLD e16 LDA el6 STA e16

where e1l6 is an expression produces the memory address to obtain (LHLD, LDA) or
store (SHLD, STA) the data value.

The stack pop and push instructions perform double precision load and store
operations, with the 8080 stack as the implied memory address. The forms are:

POP e3 PUSH e3

where e3 must evaluate to one of the double precision register pairs PSW, B, D, or
H.

The input and output instructions are also found in this category, even though
they receive and send their data to the electronic environment which is external to
the 8080 processor. The input instruction reads data to the A register, while the
output instruction sends data from the A register. In both eases, the data port is

---
_Page 034_

CP/M MACRO ASSEM 2.0 #001 DATA/MEMORY /REGISTER MOVE OPERATIONS
TITLE "DATA/MEMORY /REGISTER MOVE OPERATIONS’
THE MOV INSTRUCTION REQUIRES TWO REGISTER OPERANDS

(3-BITS) SELECTED FROM A,B,C,D,E,H, OR M (M,M INVALID)
0000 78 MOV A,B ;MOVE DATA TO FIRST REGISTER FROM SECOND

on Om ON

3 LOAD/STORE EXTENDED REQUIRE REGISTER PAIR B OR D
0001 OA LDAX B ; LOAD ACCUM FROM ADDRESS GIVEN BY BC
0002 12 STAX D ; STORE ACCUM TO ADDRESS GIVEN BY DE

; LOAD/STORE DIRECT REQUIRE MEMORY ADDRESS
0003 2A1900 LHLD D1 ; LOAD HL DIRECTLY FROM ADDRESS D1
0006 221B00 SHLD D1+2 ; STORE HL DIRECTLY TO ADDRESS D1+2
0009 3A1900 LDA D1 ; LOAD THE ACCUMULATOR FROM D1
000C 326400 STA Di SHL 2;STORE THE ACCUMULATOR TO D1 SHL 2

; PUSH AND POP REQUIRE PSW OR REGISTER PAIR FROM B,D,H
OO00F Fl POP PSW ;LOAD REGISTER PAIR FROM STACK
0010 C5 PUSH B ; STORE REGISTER PAIR TO THE STACK

; INPUT/OUTPUT INSTRUCTIONS REQUIRE 8-BIT PORT NUMBER
0011 DBO6 IN X+2 ;READ DATA FROM PORT NUMBER TO A
0013 D3FE OUT OFEH ;sWRITE DATA TO THE SPECIFIED PORT

; MISCELLANEOUS REGISTER MOVE OPERATIONS
0015 E83 XTHL ; EXCHANGE TOP OF STACK WITH HL
0016 ES PCHL ;PC RECEIVES THE HL VALUE
0017 F9 SPHL ;SP RECEIVES THE HL VALUE
0018 EB XCHG ; EXCHANGE DE AND HL

. END OF INSTRUCTION LIST

0019 Di: DS 2 : DOUBLE WORD TEMPORARY
001B DS 2 s ANOTHER TEMPORARY
0004 = X EQU 4 : LITERAL VALUE

001D END

Figure 8. Assembly Using Various Register/Memory Moves.

---
_Page 035_

given by the data value which follows the instruction:
IN e8 OUT e8

Various instructions are a part of the instruction set which transfer double
precision values between registers and the stack. These instructions are:

XTHL PCHL SPHL XCHG

Figure 8 lists these instructions in an assembly language program, along with a short
comment on the use of each instruction.

9.0. Arithmetic Logic Unit Operations.

A number of instructions are included in the 8080 set which operate between
the accumulator and single precision registers, including operations upon the A register
and carry flag. The accumlator/register instructions are:

ADD e3 ADC e3 SUB e3 SBB e3
ANA e3 XRA e3 ORA e3 CMP e3

where e3 produces a value corresponding to one of the single precision registers A,
B, C, D, E, H, L, or M, where the M "register" is the memory location addressed by
the HL register pair.

The acecumulator/carry operations given below operate upon the A register, or
carry bit, or both.

DAA CMA STC CMC
RLC RRC RAL RAR

The actual function of each instruction is listed in the comment line shown in Figure
9,

The last instruction of this group is the double precision add instruction which
performs a 16-bit addition of a register pair (B, D, H, or SP) into the 16-bit value in
the HL register pair, producing the 16-bit (unsigned) sum of the two values which is
placed into the HL register pair. The form is:

DAD e3

5.6. Control Instructions.

The four remaining instructions in the 8080 set are categorized as control
instructions, and take the forms:

HLT DI EI NOP

and are used to stop the processor (HLT), enable the interrupt system (EI), disable the
interrupt system (DI), or perform a "no-operation" (NOP).

---
_Page 036_

000A
000B
000C
000D
0O0E
000F

CP/M MACRO ASSEM 2.0 #001 ARITHMETIC LOGIC UNIT OPERATIONS
TITLE "ARITHMETIC LOGIC UNIT OPERATIONS '

ASSUME OPERATION WITH ACCUMULATOR AND REGISTER,
WHICH MUST PRODUCE A, B, C, D, E, H, L, ORM

er. ee

ADD
ADC
SUB
SBB
ANA
XRA
ORA
CMP

;ADD REGISTER TO A W/O CARRY
;ADD TO A WITH CARRY INCLUDED
; SUBTRACT FROM A W/O BORROW

; SUBTRACT FROM A WITH BORROW
;LOGICAL "AND" WITH REGISTER
; LOGICAL "XOR" WITH REGISTER
; LOGICAL "OR" WITH REGISTER

;COMPARE REGISTER, SETS FLAGS

=
+
maHAmrnMoanr

; DOUBLE ADD CHANGES HL PAIR ONLY
DAD B ;DOUBLE ADD B,D,H,SP TO HL

; REMAINING OPERATIONS HAVE NO OPERANDS
DAA ;DECIMAL ADJUST REGISTER A USING LAST OP
CMA ;COMPLEMENT THE BITS OF THE A REGISTER
STC ;SET THE CARRY FLAG TO 1
CMC ;COMPLEMENT THE CARRY FLAG
RLC ;8-BIT ACCUM ROTATE LEFT, AFFECTS CY
RRC ;8-BIT ACCUM ROTATE RIGHT, AFFECTS CY
RAL 39-BIT CY/ACCUM ROTATE LEFT
RAR ;9-BIT CY/ACCUM ROTATE RIGHT

Figure 9. Assembly Showing ALU Operations.

---
_Page 037_

6. AN INTRODUCTION TO MACRO FACILITIES

The fundamental difference between the Digital Research "ASM" and "MAC"
assemblers is that ASM provides only the fundamental facilities for assembling 8080
operation codes, while MAC includes a powerful macro processing facility. In particular,
MAC implements the industry standard Inte] macro definition, which includes the
following pseudo operations.

MACRO definitions allow groups of instructions to be stored and substituted in
the source program, as the macro names are encountered. Definitions and invocations
(macro "ealls") can be nested, symbols can be constructed through concatenation (using
the special "&" operator), and locally defined symbols can be created (using the LOCAL
pseudo operation). Macro parameters can be formed to pass arbitrary strings of text
to a specific macro for substitution during expansion. In addition, the MACLIB (macro
library) feature allows the programmer to define a particular set of macros, equates,
and sets for automatic inclusion in a program. A macro library can contain an
instruction set for another central processor, for example, which is not directly supported
by the MAC built-in mnemonics. The macro library may also include general purpose
input/output macros which are used in various programs which operate in the CP/M
environment to perform peripheral or diskette I/O functions.

IRPC, IRP, and REPT pseudo operations provide repetition of source statements
under contro! of a count or list of characters or items to be substituted each time
the statements are re-read by the assembler. This feature is particularly useful in
generating groups of assembly language statements with similar structure, such as a
set of file control blocks where only the file type is changed in each statement.

In order to illustrate the power of a macro facility, consider the macro library
shown in Figure 10, which is assumed to reside in a diskette file called "MSGLIB.LIB."
This macro library contains macro definitions which have standard instruction sequences
for program startup, message typeout, and program termination. The program shown
in Figure 11 provides an example of the use of this macro library. The assembly
shown in Figure 11 lists both the macro calls and the statements in the macro expansions
which generate machine code. The statements which are marked by '+' in Figure 11
are generated from the macro calls, while the remaining statements are a part of the
ealling program.

As an introduction to MAC features, the macro invocation
ENTCCP 10

in Figure 11 shows a specific expansion of ENTCCP (enter from CCP) which is defined
in the macro library given in Figure 10. The macro call causes MAC to retrieve the
definition (i.e., the text between MACRO and ENDM in Figure 10) and substitute this
text following the macro call in Figure 11. This particular macro performs the following
function: upon entry to the program from the CCP, the stack pointer (SP) is saved
into a variable called "GENTSP" for later retrieval. The stack pointer is then reset
to a local area for the remainder of the program execution. The size of the local
stack is defined by the macro parameter which is named in the macro definition as
SSIZE (see Figure 10), and filled-in at the call with the value 10. The result is that
the ENTCCP macro reserves space for a local stack of SSIZE=10 double bytes (2\*10
bytes) and, after setting up the stack, branches around this reserved area to continue
the program execution.

---
_Page 038_

SIMPLE MACRO LIBRARY FOR MESSAGE TYPROUT

REBOOT EQU 088K ;WARM START ENTRY POINT
TPA EQU 8100H sTRANSIENT PROGRAM AREA
BDOS EQU @0Q05H sSYSTEM ENTRY POINT
TYPE EQU 2 :WRITE CONSOLE CHARACTER FUNCTION
CR EQU @DH sCARRIAGE RETURN
LF EQU GAH ;LINE FEED
a
H MACRO DEFINITIONS
CHROUT MACRO s;WRITE A CONSOLE CHARACTER FROM REGISTER A
MVI C,TYPE ¢;TYPE FUNCTION
CALL BDOS :;ENTER THE BDOS TO WRITE THE CHARACTER
ENDM
,
TYPEOUT MACRO ?MESSAGE sTYPE THE LITERAL MESSAGE AT THE CONSOLE
LOCAL PASTSUB ;;.JUMP PAST SUBROUTINE INITIALLY
JMP PASTSUB
MSGOUT: ¢:THIS SUBROUTINE IS USED TO PRINT THE MESSAGE STARTING AT HL “TIL 64
MOV E,M ssNEXT CHARACTER TO E
MOV A,E 3:TO ACCUM TO TEST FOR @@
ORA A 33=80?
RZ 3;RETURN IF END OF MESSAGE
INX H s ;OTHERWISE MOVE TO NEXT CHARACTER AND PRINT
PUSH H ?:;SAVE MESSAGE ADDRESS
CHROUT
POP H :;RECALL MESSAGE ADDRESS
JMP MSGOUT :3;FOR ANOTHER CHARACTER
PASTSUB:

,

se
oe

REDEFINE THE TYPEOUT MACRO AFTER THE FIRST INVOCATION

TYPEOUT MACRO ? 2MESSAGE

LOCAL TYMSG 7;LABEL THE LOCAL MESSAGE
LOCAL PASTM

LXI H,TYMSG ;;ADDRESS THE LITERAL MESSAGE
CALL MSGOUT ;;CALL THE PREVIOUSLY DEFINED SUBROUTINE
JMP PASTM
7? INCLUDE THE LITERAL MESSAGE AT THIS POINT
TYMSG: DB “FROM CONSOLE: &??MESSAGE’,CR,LF,@
3 ARRIVE HERE TO CONTINUE THE MAINLINE CODE
PASTM: ENDM
TYPEOUT <?MESSAGE>
ENDM
c
ENTCCP MACRO SSIZE ;ENTER PROGRAM FROM CCP, RESERVE 2\*SSIZE STACK LOCS
LOCAL START +;AROUND THE STACK
LXI H,@
DAD SP 3;SP VALUE IN HL
SHLD @ENTSP ;;:ENTRY SP
LXI SP,@STACK;;SET TO LOCAL STACK
IMP START
IF NUL SSIZE
DS 32 :;DEFAULT 16 LEVEL STACK
ELSE
DS 2\*SSIZE
ENDIF
@STACK: :;LOW END OF STACK
@ENTSP: DS 2 ;;ENTRY SP
START: ENDM
RETCCP MACRO ;RETURN TO CONSOLE PROCESSOR
LHLD @ENTSP 3;;RELOAD CCP STACK
SPHL
RET ?;BACK TO THE CCP
ENDM
,
ABORT MACRO sABORT THE PROGRAM
IMP REBOOY
ENDM

we

END OF MACRO LIBRARY

Figure 10. A Sample Macro Library.

---
_Page 039_

CP/M MACRO ASSEM 2.8 #661 SAMPLE MESSAGE OUTPUT MACRO

TITLE “SAMPLE MESSAGE OUTPUT MACRO”

MACLIB MSGLIB ;INCLUDE THE MACRO LIBRARY

0129 ORG TPA sORIGIN AT THE TRANSIENT AREA
: USE THE MACRO LIBRARY TO TYPS TWO MESSAGES

ENTCCP 18 ;ENTER PROGRAM, RESERVE 1@ LEVEL STACK

0100+218006 LXI H,@

8103+39 DAD SP

01044222101 SHLD @ENTSP

8107+312191 LXI SP,@STACK

819A+C32381 JMP ??@001

G1 98D+ DS 2\*10

@121+ @ENTSP; DS 2
TYPEOUT <THIS IS THE FIRST MESSAGE>

91234+C33461 JMP ??B0B2

O126+5E MOV E,M

6127+B7 ORA A

@128+C8 R2

6129423 INX H

O12A+E5 PUSH H

012B+HEG2 MVI C,TYPE

612D+CDO586 CALL BDOS

9136481 POP H

61314+C326A1 gMpP MSGOUT

01344+213DA1 LXI H,2??6003

8137+CD2661 CALL MSGOUT

@13A4+C36761 JMP ??6004

813D+46524F4D207??7?0803; DB “FROM CONSOLE: THIS IS THE FIRST MESSAGE’ ,CR,LF,@
TYPEOUT <THIS IS THE SECOND MESSAGE>

01674217081 LXI H,??60085

®16A+CD2661 CALL MSGOUT

Al6D+C39BG1 JMP 2??0006

8170+46524F4D267274085: DB “FROM CONSOLE: THIS IS THE SECOND MESSAGE’ ,CR,LF,@
TYPEOUT <THIS IS THE THIRD MESSAGE>

819B+21A4@81 LXI H,??0687

B19E+CD2661 CALL MSGOUT

G1A14+C3CE61 JMP ??0008

B1A44+46524F 4020770807: DB “PROM CONSOLE: THIS IS THE THIRD MESSAGE’ ,CR,LF,@
RETCCP ;RETURN TO THE CONSOLE COMMAND PROCESSOR

61CE+2A2161 LHLD @ENTSP

BIDL+F9 SPHL

@1D2+C9 RET

G1D3 END

Figure 11. A Sample Assembly using the MACLIB Facility.

---
_Page 040_

Consider also the special macro statements which are used in Figure 10 within
the body of the ENTCCP macro. The "local" statement defines the label START which
is used within the macro body. Generally, each LOCAL statement causes the macro
assembier to construct a unique symbol (starting with "??") each time it is encountered,
Thus, multiple macro calls reference unique labels which do not interfere with one
another. To continue the example, ENTCCP also contains a conditional assembly
statement which uses the "NUL" operator, which is used to test whether a macro
parameter has been supplied or not. In this case, the ENTCCP macro could be invoked
by:

ENTCCP

with no actual parameter, resulting in a default stack size of 32 bytes. If this seems
confusing, don't be concerned at this point because the individual sections which follow
give exact details and examples.

The TYPEOUT macro provides a more complicated example of macro use. Note
that this macro contains a redefinition of itself within the macro body. That is, the
structure of TYPEOUT is:

TYPEOUT MACRO  ?MESSAGE
TYPEOUT MACRO  ??MESSAGE
ENDM
ENDM

where the outer definition of TYPEOUT completely encloses the inner definition. The
outer definition is active upon the first invocation of TYPEOUT, but upon completion,
the nested inner definition becomes active.

In order to see the use of such a nested structure, consider the purpose of the
TYPEOUT macro. Each time it is invoked, TYPEOUT prints the message sent as an
actual parameter at the console device. The typeout process, however, can be easily
handled with a short subroutine. Upon the first invocation, we would like to inelude
the subroutine "inline," and then simply call this subroutine on subsequent invocations
of TYPEOUT. Thus, the outer definition of TYPEOUT defines the utility subroutine,
and then redefines itself so that the subroutine is called, rather than including another
copy of the utility subroutine.

It should be noted that macro definitions are stored in the symbol table area
of the assembler and thus each macro reduces the remaining free space. As a result,
MAC allows "double semicolon" comments which indicate that the comment itself is
to be ignored and not stored with the macro. Thus, comments with a single semicoion
are stored with the macro and appear in each expansion while comment with two
preceding semicolons are listed only when the macro is defined.

Figure 11 gives three examples of TYPEOUT invocations, with three messages

which are sent as actual parameters. Note that the LOCAL statement causes a unique
label to be created (220002) in the place of "PASTSUB," which is used to branch around

---
_Page 041_

the utility subroutine which is included inline between addresses 0126H and 0133H.
The utility subroutine is then called, followed by another jump around the console
message which is also included inline. Note, however, that subsequent invocations of
TYPEOUT use the previously included utility subroutine to type their messages. Again,
this may seem confusing, but it is worthwhile studying this example before continuing
into the exact details of macro definition and invocation in order to gain some insight
into macro facilities.

It should also be noted that, although the example shown here concentrates all
macro definitions in a separate macro library, it is often the case that macros are
defined in the mainline (.ASM) source program. In fact, many programs which use
macros do not use the external macro library facility at all.

There are many applications of macros which will be examined throughout the
remainder of this manual. Specifically, macro facilities can be used to simplify the
programming task by "abstracting" from the primitive assembly language levels. That
is, the programmer can define macros which provide more generalized functions that
are allowed at the pure assembly language level, such as macro languages for a given
applications (see Section 10), improved control facilities, and general purpose operating
systems interfaces. The remainder of this manual first introduces the individual macro
forms, then presents several uses of the macro facilities in realistic applications.

---
_Page 042_

---
_Page 043_

7. INLINE MACROS

The simplest macro facilities involve the REPT (repeat), IRPC (indefinite repeat
character), and IRP (indefinite repeat) macro groups. All these forms cause the
assembler to repetively re-read portions of the source program under control of a
counter or list of textual substitutions. These groups are listed below in increasing
order of complexity.

7.1. The REPT-ENDM Group.

The REPT-ENDM group is written as a sequence of assembly language statements
starting with the REPT pseudo operation, and terminated by an ENDM pseudo operation.
The form is:

label: REPT expression
statement-1
statement-2

statement-n
label: ENDM

where the labels are optional. The expression following the REPT is evaluated as a
16-bit unsigned count of the number of times that the assembler is to read and process
statements 1 through n which are enclosed within the group.

Figure 12 shows an example of the use of the REPT group. In this case the
REPT-ENDM group is used to generate a short table of the byte values 5, 4, 3, 2,
and 1. Upon entry to the REPT, the value of NXTVAL is 5 which is taken as the
repeat count (even though NXTVAL changes within the REPT). Note that the macro
lines which do not generate machine code are not listed in the repetition, while the
lines which do generate code are listed with a "+" sign after the machine code address.
Full macro tracing is optional, however, using assembly parameters, as discussed in a
later section.

In general, if a label appears on the REPT statement, its value is the first
machine code address which follows. This REPT label is not re-read on each repetition
of the loop. The optional label on the ENDM is re-read on each iteration and thus
constant labels (not generated through concatenation or with the LOCAL pseudo
operation) will generate phase errors if the repetion count is greater than 1.

Properly nested macros, including REPT's, can occur within the body of the
REPT-ENDM group. Further, nested conditional assembly statements are also allowed,
with the added feature that conditionals which begin within the repeat group are
automatically terminated upon reaching the end of the macro expansion. Thus, IF and
ELSE pseudo operations are not required to have their corresponding ENDIF when they
begin within the repeat group (although the ENDIF is allowed).

7.2. The IRPC-ENDM Group.

Similar to the REPT group, the IRPC-ENDM group causes the assembler to
re-read a bounded set of statements, taking the form

---
_Page 044_

0005 =

0100 DB
0102 FE
0104 D2
0107 21
010A 5F
010B 16
010D 19
QO10E 7E
010F D3
0111 C3

0005 #

0114+05
0115+04
0116+03
0117+02
0118+01

CP/M MACRO ASSEM 2.0 #001 SAMPLE REPT STATEMENT

on ON ON OM

MAXVAL
00 RLOOP :

NXTVAL
TABLE:

NXTVAL

ORG 100H ;BASE OF TRANSIENT AREA

TITLE ‘SAMPLE REPT STATEMENT'

THIS PROGRAM READS INPUT PORT 0 AND INDEXES INTO A TABLE
BASED ON THIS VALUE. THE TABLE VALUE IS FETCHED AND SENT
TO OUTPUT PORT 0

EQU 5 ; LARGEST VALUE TO PROCESS
IN 0 ;READ THE PORT VALUE
CPI MAXVAL ;TOO LARGE?
JNC RLOOP ; IGNORE INPUT IF INVALID
LXI H,TABLE ;ADDRESS BASE OF TABLE
MOV E,A ;LOW ORDER INDEX TO E
D,0 ;HIGH ORDER 00 FOR INDEX
D ;HL HAS ADDRESS OF ELEMENT
MOV A,M ;FETCH TABLE VALUE FOR OUTPUT
0 ;SEND TO THE OUTPUT PORT AND LOOP
RLOOP ;FOR ANOTHER INPUT
GENERATE A TABLE OF VALUES MAXVAL,MAXVAL-1,...,1
SET MAXVAL ;START COUNTER AT MAXVAL
REPT NXTVAL
DB NXTVAL ;FILL ONE (MORE) ELEMENT
SET NXTVAL-1;;AND DECREMENT FILL VALUE
ENDM
DB NXTVAL ;FILL ONE (MORE) ELEMENT
DB NXTVAL ;FILL ONE (MORE) ELEMENT
DB NXTVAL ;FILL ONE (MORE) ELEMENT
DB NXTVAL ;FILL ONE (MORE) ELEMENT
DB NXTVAL ;FILL ONE (MORE) ELEMENT

Figure 12. A Sample Program Using the REPT Group.

---
_Page 045_

label: IRPC identifier,character-list
statement-1
statement-2

statement-n
label: ENDM

where the optional labels obey the same conventions as in the REPT-ENDM group.
The "identifier" is any valid assembler name, not including embedded "$" separators,
and "character-list" denotes a string of characters, terminated by a delimiter (space,
tab, end-of-line, or comment).

The IRPC controls the re-read process as follows: the statement sequence is
read once for each character in the character-list. On each repetition, a character
is taken from the character-list and associated with the controlling identifier, starting
with the first and ending with the last character in the list. Thus, an IRPC header
of the form

IRPC ?X,ABCDE

re-reads the statement sequence which follows (to the balancing ENDM) a total of
five times, once for each character in the list "ABCDE." On the first iteration, the
character "A" is associated with the identifier "?X" and on the fifth iteration the
letter "E" is associated with the controlling identifier.

On each iteration, the macro assembler substitutes any occurrence of the
controlling identifier by the associated character value. Using the above IRPC header,
an occurrence of "?X" in the bounds of the IRPC-ENDM group is replaced by the
character "A" on the first iteration, and by "E" on the last iteration.

The programmer can use the controlling identifier to construct new text strings
within the body of the IRPC by using the special "concatenation" operator, denoted
by an ampersand (&). Again using the above IRPC header, the macro assember would
replace "LAB&?X" by "LABA" on the first iteration, while "LABE" would be produced
on the final iteration. The concatenation feature is most often used to generate unique
label names on each iteration of the IRPC re-read process.

Note, however, that the controlling identifier is not normally substituted within
string quotes, since the controlling identifier could quite possibly occur as a part of
a quoted message. Thus, the macro assembler performs substitution of the controlling
identifier when it is either preceded and/or followed by the ampersand operator.
Further, recall that all alphabetics outside string quotes are translated to upper case,
while no case translation occurs within string quotes. This requires that the controlling
identifier be not only preceded or followed by the concatenation operator within strings,
but must also be typed in upper case.

Figure 13 illustrates the use of the IRPC-ENDM group. Figure 13a shows the
original assembly language program, before processing by the macro assembler. Note
that the program is typed in both upper and lower case. Figure 13b shows the output
from the macro assembler, with the lower case alphabetics translated to upper case.
Three IRPC groups are shown in this example. The first IRPC uses the controlling
identifier "reg" to generate a sequence of stack push operations which save the double
precision registers BC, DE, and HL. Again note that the lines generated by this group
are marked by a "+" sign following the machine code address.

---
_Page 046_

3 eonstruect a data table

save relevant registers

bi

enter: irpe reg,bdh
push reg 3;3save reg
endm

; initialize a partial ascii table
irpe ec, 1Ab$?@

data&ke: db "&C!
endm

wo

3 restore registers

irpe reg,hdb

pop reg s;;recall reg
endm

ret

end

Figure 13a. Original (.ASM) File with IRPC Example.

; CONSTRUCT A DATA TABLE
;

3 SAVE RELEVANT REGISTERS
ENTER: IRPC REG, BDH

PUSH REG ;;SAVE REG
ENDM

0000+C5 PUSH B

0001+D5 PUSH D

0002+E5 PUSH H

;
INITIALIZE A PARTIAL ASCII TABLE
IRPC C, 1AB$2@

°

DATA&C: DB '&C!
ENDM
0003+31 DATA1: DB ryt
0004+41 DATAA: DB ‘A!
0005+42 DATAB: DB 'B!
0006+24 DATA$: DB '$!
0007+3F DATA?: DB 191

0008+40 DATA@: DB ‘@'
RESTORE REGISTERS
IRPC —- REG, HDB

POP REG 3;RECALL REG
ENDM

0009+E1 POP H

000A+t+D1 POP D

000B+C1 POP B

000C C9 RET

000D END

Figure 13b. Resulting (.PRN) file with IRPC Example.

---
_Page 047_

The second IRPC shown in Figure 13 uses the controlling identifier "C" to
generate a number of single byte constants with corresponding labels. It is important
to observe that although the controlling variable was typed in lower case (see Figure
13a), it has been translated to upper case during assembly. Further, note that the
string '&C' occurs within the group and, since the controlling variable is enclosed in
string quotes, it must occur next to an ampersand operator and be typed in upper case
for the substitution to occur properly. On each iteration of the IRPC, a label is
constructed through concatenation, and a "DB" is generated with the corresponding
character from the character-list.

It should be pointed out that substitution of the controlling identifier by its
associated value could cause infinite substitution if the controlling identifier is the
same as the character from the character-list. For this reason, the macro assembler
performs the substitution and then moves along to read the next segment of the
program, rather than re-reading the substituted text for another possible occurrence
of the controlling identifier. Thus, an IRPC of the form

IRPC C,1AC$?@

would produce
DATAC: DB 'C!

in place of the DB statement at the label DATAA in Figure 13b.

The last IRPC of Figure 13 is used to restore the previously saved double
precision registers, and performs the exact opposite function from the IPRC at the
beginning of the program.

One special case does occur, however, when the character-list is empty (i.e.,
when no characters occur following the "identifier," portion of the IRPC header). In
this case, the group of statements is read once, and any occurrence of the controlling
identifier is deleted when it is read (i.e., it is replaced by the "null string").

7.3. The IRP-ENDM Group.

The IRP (indefinite repeat) is similar in function to the IRPC, except that the
controlling identifier can take on a multiple character value. The form of the IRP
group is

label: IRP _ identifier, acl-1,el-2,...,el-n2
statement-1
statement-2

statement-m
label: ENDM

where the optional labels obey the conventions of the REPT and IRPC groups. The
identifier controls the iteration as follows. On the first iteration, the character-list
given by "cl-1" is substituted for the identifier wherever the identifier occurs in the
bounded statement group (statements 1 through m). On the second iteration, el-2
becomes the value of the controlling identifier. Iteration continues in this manner

---
_Page 048_

until the last character-list, denoted by ecl-n, is encountered and processed. Substitution
of values for the controlling identifier is subject to the same rules as in the IRPC
(note rules for substitution within strings and concatenation of text using the ampersand
operator "&"), One should also note that controlling identifiers are always ignored
within comments.

Figure 14 gives several examples of IRP groups. The first occurrence of the
IRP in Figure 14 is a typical use of this facility to generate a "jump vector" at the
beginning of a program or subroutine. The IRP assigns label names (INITIAL, GET,
PUT, and FINIS) to the controlling identifier "?LAB" and produces a jump instruction
for each label by re-reading the IRP group, substituting the actual label for the formal
name on each iteration.

The second occurrence of the IRP group in Figure 14 points out substitution
conventions within strings (for both IRPC and IRP groups). The controlling identifier
"IS" takes on the values "A-ROSE" and "?" on the two iterations of the IRP group,
respectively. Note that the controlling identifier is replaced by the character-lists in
the two cases "&IS" and "IS&" inside the string quotes since they are both adjacent
to the ampersand operator. Note further that "is&" is not replaced because the
controlling identifier is typed in lower case, and there is no automatic translation to
upper case within strings. The occurrences of "IS" within the comments are not
substituted,

The last IRP group shows the effects of an empty character-list. The value of
the controlling identifier becomes the null string of symbols and, in the cases where
"2X" is replaced, produces the statement

DB "

which produces no machine code, and is therefore not listed in the macro expansion.
The three statements

DB '?x' DB '?X' DB '&!

appear in the expansions because the "?x" is typed in lower ease (and thus is not
replaced), the '?X' does not appear next to an ampersand in the string (and is thus
not replaced), while in the last case only one of the double ampersands is absorbed in
the '&k&?X&' string. In this last case, the two ampersands which surround "?X" are
removed since they occur immediately next to the controlling identifier within the
string.

Recall that substitution rules outside of string quotes and comments is much
less complicated: the controlling identifier is replaced by the current character-list
value whenever it occurs in any of the statements within the group. Further, the
ampersand operator can be placed before or after the controlling identifier to cause
the preceding or following text to be concatenated.

The actual forms for the character-lists (cl-1 through el-n) are more general
than stated here. In particular, bracket nesting is allowed as well as escape sequences
to allow delimiters to be ignored. The exact details of character-list forms are
discussed in the macro parameter sections.

---
_Page 049_

0000+C30C00
0003+C34300
0006+C34600
0009+C34900

°

>
INITIAL:
000C 211200
O000F C35100
CHRS :

0012+412D524F53
0022+412D524F53
0032+3F20495320
0038+3F2069736E

0043 C35100 GET:
0046 C35100 PUT:

0049 C35100 FINIS:

004C+3F78
004E+3F58
0050+26
ENDCASE :
0051 C9

CREATE A "JUMP VECTOR" USING THE IRP GROUP

IRP ?LAB ,< INITIAL,GET, PUT,FINIS ”
JMP 2? LAB ;;GENERATE THE NEXT JUMP
ENDM

JMP INITIAL

JMP GET

JMP PUT

JMP FINIS

INDIVIDUAL CASES

LXI H,CHRS

JMP ENDCASE

IRP IS, <A-ROSE, ? >

DB 'aIS IS I[S&! 31S IS &IS
DB "'aIS isn''t is&'

ENDM

DB "A-ROSE IS A-ROSE' ;I1S IS &IS
DB 'A-ROSE isn''t is&'!

DB '? JS ? ;IS IS &IS
DB '? isn''t is&!

JMP ENDCASE

JMP ENDCASE

JMP ENDCASE

IRP 2K, <>

DB 1x!

DB 2X!

DB "RIX!

DB "&?X&!

DB "Ra? XA!

ENDM

DB 19x!

DB 2x!

DB '&!

RET

END

Figure 14. A Sample Program Using IRP.

---
_Page 050_

7.4. The EXITM Statement.

The EXITM pseudo operation can occur within the body of a macro and, upon
encountering the EXITM statement, the macro assembler aborts expansion of the current
macro level. The EXITM pseudo operation occurs in the context

macro-heading
statement-1

label: EXITM

statement-n
ENDM

where the label is optional, and "macro-heading" denotes the REPT, IRPC, or IRP
group heading as described above. The EXITM statement can also be used with the
MACRO group, as discussed in later sections.

In order to be useful, the EXITM statement normally occurs within the scope
of a surrounding conditional assembly operation. If the EXITM occurs in the scope of
a false conditional test, the statement is ignored and macro expansion continues. If
the EXITM occurs within the scope of a true conditional, the expansion stops at the
point where the EXITM is encountered. Assembly statement processing continues after
the ENDM of the group aborted by the EXITM statement.

Two examples of the EXITM statement are shown in Figure 15. This figure
shows two IRPC's used to generated "DB" statements which do not exceed eight
characters in length. These IRPC's might occur within the context of another macro
definition, such as in the generation of CP/M file control block (FCB) names. In both
eases, the variable "LEN" is used to count the number of filled characters. If the
count ever reaches eight characters, the EXITM statement is assembled under a true
condition, and the IRPC stops expansion.

The first IRPC generates the entire string "SHORT" since the length of the
character-list is less than eight characters. Each evaluation of "LEN = 8" produces
a false value and the EXITM is skipped. Thus, this IRPC terminates normally by
exhausing the character-list through its five repetitions.

The second IRPC stops generation at the eighth character of the list
"LONGSTRING" when the conditional "LEN EQ 8" produces a true value (note that "="
and "EQ" are equivalent operators), resulting in assembly of the EXITM statement.
The EXITM causes immediate termination of the expansion process.

The second IRPC also contains a conditional assembly without the balancing
ENDIF. In this case, the ENDIF is not required since the conditional begins within
the macro body. The ENDM serves the dual purpose of terminating unmatched IF's
as well as marking the physical end of the macro body.

---
_Page 051_

0000 #

0000+53
0001+48
0002+4F
0003+52
0004+54

0000 #

0005+4C
0006+4F
0007+4E
0008+47
0009+53
000A+54
000B+52
000C+49

000D

on om on on on]

zZ
fo

Z
ea
i)

on en en em ony

enw

SAMPLE USE OF THE EXITM STATEMENT WITH THE IRPC MACRO

THE FOLLOWING IRPC FILLS AN AREA OF MEMORY WITH AT MOST
EIGHT BYTES OF DATA:

SET 0
IRPC  N,SHORT
DB "SN!
SET LEN+1
IF LEN = 8
EXITM ;STOP MACRO IF AREA IS FULL

; INITIALIZE LENGTH TO 0

DB 'S'
DB 'H'
DB ‘O!'
DB 'R!
DB a

THE FOLLOWING MACRO PERFORMS EXACTLY THE SAME FUNCTIONS AS
SHOWN ABOVE, BUT ABORTS EXPANSION WHEN LENGTH EXCEEDS 8

SET 0 ; INITIALIZE LENGTH COUNTER
IRPC N, LONGSTRING

DB '&N'

SET LEN+1

IF LEN EQ 8

DB 'L!
DB ‘OQ!
DB 'N'
DB 'G'
DB 'S'
DB 'T!
DB 'R!
DB 'y!

END

Figure 15. Use of the EXITM statement in Macro Processing.

---
_Page 052_

7.5. The LOCAL Statement.

It is often useful to "generate" labels for jumps or data references which are
unique on each repetition of a macro. This facility is available through the LOCAL
statement, which takes the form

macro-heading
label: LOCAL id-1,id-2,. . .,id-n

ENDM
where the label is optional, "macro-heading" is a REPT, IRPC, or IRP heading as
discussed above (or a MACRO heading as discussed in following sections), and id~1
through id-n represent one or more assembly language identifiers which do not contain
embedded "$" separators. The LOCAL statement must occur within the body of a
macro definition. Although MAC allows the LOCAL statement to appear anywhere

within the macro body, it should appear immediately following the macro header to
be compatible with the standard Intel macro facility.

The action of the assembler upon encountering the LOCAL statement is to
create a new name of the form

??nnnn

for association with each identifier in the LOCAL list, where nnnn is a four digit
decimal value, assigned in ascending order starting at 0001. Whenever one of the
identifiers in the list is encountered, the corresponding created name is substituted in
its place. Substitution occurs according to the same rules as the controlling identifier
in the IRPC and IRP groups.

The user should avoid the use of labels which begin with the two characters
"22" so that no conflicting names will accidentally occur. Further, symbols which
begin with "??" are not normally included in the sorted symbol list at the end of
assembly (see "assembly parameters” to override this default). Lastly, a total of 9999
LOCAL labels can be generated in any assembly, and an overflow error will occur if
more generations are attempted.

Figure 16a shows an example of a program which uses the LOCAL statement
to generate both data references and jump addresses. This program uses the CP/M
disk operating system to print a series of four generated messages, as shown in the
output from the program in Figure 16b. The program begins with "equates" which
define the disk system primary entry point, along with names for the non graphic
ASCII characters CR and LF (carriage return and line feed). The REPT statement
which follows contains a LOCAL statement with the identifiers X and Y which are
used throughout the body of the REPT group. On the first iteration, X's value becomes
220001 which is the first generated label, while Y's value becomes ??0002. Note that
the substitution for X and Y within the generated strings follows the rules stated for
controlling identifiers in previous sections. Upon completion, four messages are
generated along with four CALL's to the PRINT subroutine. At each call to PRINT,
the message address is present in the DE register pair. The subroutine loads the "print
string" function number into register C (C = 9) and calls the disk system to print the
string value.

---
_Page 053_

0005 = BDOS
000D = CR
000A = LF

;

;

X:

Y:
0100+C31E01
0103+7072696E74?20001:
011E+110301 220002:
0121+CD9101
0124+C34201
0127+7072696E74?7?0003:
0142+112701 220004:
0145+CD9101
0148+C36601
014B+7072696E74?70005:
0166+114B0i 220006:
0169+CD9101
016C+C38A01
O016F+7072696E74?20007:
O18A+116F01 220008:
018D+CD9101
0190 C9

>
0191 OEN9 PRINT:
0193 CD0500
0196 C9

ORG
EQU
EQU
EQU

100H ;BASE OF THE TRANSIENT AREA

5 ;BDOS ENTRY POINT
0DH ;CARRIAGE RETURN (ASCII)
0AH ;LINE FEED (ASCII)

SAMPLE PROGRAM SHOWING THE USE OF 'LOCAL'

REPT
LOCAL

MV I
CALL
RET
END

Figure 16a. Assembly

Figure 16b.

print
Print
print
print

4 ;REPEAT GENERATION 4 TIMES
X,Y ;;GENERATE TWO LABELS

Y ; JUMP PAST THE MESSAGE
"print x=&X, y=&¥',CR,LF,'$'

D,X ;READY PRINT STRING

PRINT

220002 ;JUMP PAST THE MESSAGE
‘print x=?2?0001, y=?20002',CR,LF,'$"'
D,?70001 ;READY PRINT STRING
PRINT

220004 ;JUMP PAST THE MESSAGE
‘print x=?20003, y=?20004',CR,LF,'$!
D,?20003 ;READY PRINT STRING
PRINT

220006 ;JUMP PAST THE MESSAGE
‘print x=?20005, y=??0006',CR,LF,'$'
D,?20005 ;READY PRINT STRING
PRINT

220008 ;JUMP PAST THE MESSAGE
'print x=?20007, y=?20008',CR,LF,'$'
D,?20007 ;READY PRINT STRING
PRINT

C,9
BDOS

Program using the LOCAL Statement.

x=?20001, y=?70002
x=?20003, y=??20004
x=?20005, y=?20006
x=220007, y=?20008

Output from Program of Figure 16a.

---
_Page 054_

Upon completion of the program, control returns to the console command
processor (CCP) for further operations. This particular program uses the default stack
which is passed by the CCP (approximately 16 levels are available). Although this
example is primarily intended to show operation of the LOCAL statement, the reader
may wish to consult the CP/M Interface Guide to determine BDOS interface conventions
in order to follow this example completely.

---
_Page 055_

8. DEFINITION AND EVALUATION OF STORED MACROS

The "stored macro" facility of MAC allows the programmer to name a sequence
of assembly language "prototype" statements for selective inclusion at various places
throughout the assembly process. Macro parameters can be supplied in various forms
at the point of expansion which are substituted as the prototype statement are re-read.
These parameters are generally used to tailor the individual macro expansion for a
particular case.

Although similar in coneept to subroutine definition and call, macro processing
is purely textual manipulation at assembly time. That is, macro definitions causes
source text to be saved in the assembler's internal tables, and any particular expansion
involves manipulation and re-reading of the saved text. These concepts will become
clear as the individual macro forms are discussed.

In general, macro features can be combined in various ways to greatly enhance
the facilities which are available to the programmer. Specifically, the programmer
can easily manipulate generalized data definitions, macros can be defined for generalized
operating systems interface, simplified program control structures can be defined and
non standard instruction sets (such as the Z-80) can be supported. Finally, well designed
macros for a particular application can achieve a measure of machine independence.
All of these notions will be covered in the sections which follow.

8.1. The MACRO-ENDM Group.

The prototype statements for a stored macro are given in the macro body
enclosed by the MACRO and ENDM pseudo operations, taking the general form

macname MACRO d-1,d-2,. . .,d-n
statement-l
statement-2

statement-m
label: ENDM

where the "macname" is any non conflicting assembly language identifier, d-1 through
d-n constitutes a (possibly empty) list of assembly identifiers without imbedded "$"
separators and statements-1 through m are the macro prototype statements. The
identifiers denoted by d-1 through d-n are called "dummy parameters" for this particular
macro and, although they must be unique among themselves, can generally be identical
to any program identifiers outside the macro body without causing a conflict. The
prototype statements may contain any properly balanced assembly language statements
or groups, including nested REPT's, IRP's, IRPC's, MACRO's and IF's.

The prototype statements are read and stored in the assembler's internal tables
under the name given by "macname," but are not processed until the macro is expanded.
The expansion process is given in the following section.

As before, the label preceding the ENDM is optional.

8.2. Macro Invocation.

The macro text which is stored through a MACRO-ENDM group can be brought
out for processing through a statement of the form

---
_Page 056_

label: macname a-la-2, ... ,a-n

where the label is optional, and macname has previously occurred as the identifier on
a MACRO heading. The "actual parameters” a-1 through a-n are sequences of characters,
separated by commas and terminated by a comment or end of line.

Upon recognition of the macname, the assembler first "pairs-off" each dummy
parameter in the MACRO heading (d-1 through d-n) with the actual parameter text
(a-1 througn a-n) by associating the first dummy parameter with the first actual
parameter (d-1 is paired with a-1), the second dummy is associated with the second
actual, and so forth until the list is exhausted. If more actuals are provided than
dummy parameters then the extras are ignored, If fewer actuals are provided then
the extra dummy parameters are associated with the empty string (i.e., a text string
of zero jength). It is important to realize at this point that the value of a dummy
parameter is not a numeric value, but is instead a textual value consisting of a sequence
of zero or more ASCII characters.

After each Gummy parameter is assigned an actual textual value, the assembler
re-reads and processes the previously stored prototype statements and substitutes each
occurrence of a dummy parameter by its associated actual textual value, according to
the same rules as the controlling identifier in an IRPC or IRP group.

Figures 17 and 18 provide examples of macro definitions and invocations. Figure
17 begins with the definition of three macros, called SAVE, RESTORE, and WCHAR.
The SAVE macro contains prototype statements which save the principal CPU registers
(PUSH PSW, B, D, and H), while the RESTORE macro restores the principal registers
(POP H, D, B, and PSW). The WCHAR macro contains the statements necessary to
write a single character at the console using a CP/M BDOS call.

Note that the occurrence of the SAVE macro definition between MACRO and
ENDM causes the assembler to read and save the PUSH's, but does not assemble the
statements into the program. Similarly, the statements between the RESTORE MACRO
and corresponding ENDM are saved, as are the statements between the WCHAR MACRO
and ENDM group. The fact that the assembler is reading the macro definition is
indicated by the blank columns in the leftmost 16 columns of the output listing.

Referring to Figure 17, note that machine code generation starts following the
invocation of the SAVE macro. The prototype statements which were previously stored
are re-read and assembled, with a "+" between the machine code address and the
generated code to indicate that the statements are being recalled and assembled from
a macro definition. Note that the SAVE macro has no dummy parameters in the
definition and thus there are no actual parameters required at the point of invocation.

The invocation of SAVE is immediately followed by an expansion of the WCHAR
macro. The WCHAR macro, however, has one dummy parameter, called CHR, which
is listed in the macro definition header. This dummy parameter represents the character
to pass to the BDOS for printing. In the first expansion of the WCHAR macro, the
actual parameter "H" becomes the textual value of the dummy parameter CHR. Thus,
the WCHAR macro expands with a substitution of the dummy parameter CHR by the
value H. Note that the use of CHR is within string quotes and thus must be typed
in upper case and preceded by the ampersand operator. Following the reference to

WCHAR, the prototype statements are listed with the "+" sign to indicate that they
are generated by the macro expansion.

---
_Page 057_

0100 ORG 100H ;BASE OF TRANSIENT AREA
0005 = BDOS EQU 5 3;BDOS ENTRY POINT
0002 = CONOUT EQU 2 ; CHARACTER OUT FUNCTION
SAVE MACRO ;SAVE ALL CPU REGISTERS
PUSH PSW
PUSH B
PUSH D
PUSH H
ENDM
>
RESTORE MACRO ;RESTORE ALL REGISTERS
POP H
POP D
POP B
POP PSW
ENDM
bd
WCHAR MACRO - CHR ; WRITE CHR TO CONSOLE
MVI C, CONOUT 3;CHAR OUT FUNCTION
MV I E, '&CHR' ;;CHAR TO SEND
CALL BDOS
ENDM
bd
; MAIN PROGRAM STARTS HERE
SAVE ;SAVE REGISTERS UPON ENTRY
0100+FS PUSH PSW
0101+C5 PUSH B
0102+D5 PUSH D
0103+E5 PUSH H
WCHAR H ;SEND 'H' TO CONSOLE
0104+0E02 MV I C, CONOUT
0106+1E48 MV I E,'H'
0108+CD0500 CALL BDOS
WCHAR I ;SEND 'I' TO CONSOLE
010B+0E02 MV I C,CONOUT
010D+1E49 MVI E,'I'
010F+CD0500 CALL BDOS
RESTORE ;RESTORE CPU REGISTERS
0112+E1 POP H
0113+D1 POP D
0114+Cl1 POP B
O115+F1 POP PSW
0116 C9 RET ;RETURN TO CCP
0117 END
Figure 17. Example of Macro Definition and Invocation.

a1

---
_Page 058_

The second invocation of WCHAR is similar to the first except that the dummy
parameter CHR is assigned the textual value I, causing generation of a MVI E,T for
this case.

After the listing of the second WCHAR expansion, the RESTORE macro is
invoked, causing generation of the POP statement to restore the register state. The
RESTORE is followed by a RET to return to the CCP following the character output.

This particular program thus performs the simple function of saving the registers
upon entry, typing the two characters "HI" at the console, restoring the registers, and
then returns to the Console Command Processor. One should note that the SAVE and
RESTORE macros are used here for illustration, and are not required for interface to
the CCP since all registers are assumed invalid upon return from a user program.
Further, this program uses the CCP's stack throughout, which is only eight levels deep.

Figure 18 shows another macro for printing at the console. In this case, the
PRINT macro uses the operating system call which prints the entire message starting
at a particular address until the "$" symbol is encountered. The PRINT macro has a
slightly more complicated structure: two dummy parameters must be supplied in the
invocation. The first parameter, called N, is a count of the number of carriage-return
line-feeds to send after the message is printed. The second parameter, called MESSAGE,
is the ASCII string to print which must be passed as a quoted string in the invocation.
The LOCAL statement within the macro generates two labels denoted by PASTM and
MSG. When the macro expands, substitutions will occur for the two dummy parameters
by their associated actual textual values, and for PASTM and MSG by their sequentially
generated label values. The macro definition contains prototype statements which
branch past the message (to PASTM) which is included inline following the label MSG.
The message is padded with N pairs of carrriage-return line-feed sequences, followed
by the "$" which marks the end of the message. The string address is then sent to
the BDOS for printing at the console.

There are two invocations of the PRINT macro included in Figure 18. The
invocation sends two actual parameters: the textual value 2 is associated with the
dummy N, followed by a quoted string which is associated with the dummy parameter
MSG. Note that the second actual parameter includes the string quotes as a part of
the textual value. Note also that the generated message is preceded by a jump
instruction, and followed by N = 2 carriage-return line-feed pairs.

The second invocation of the PRINT macro is similar to the first, except that
the REPT group is executed N = 0 times, resulting in no generations of the carriage-
return line-feed pairs.

Similar to Figure 17, the program of Figure 18 uses the Console Command
Processor's eight level stack for the BDOS calls. When the program executes, it types
the two messages, separated by two lines, and returns to the CCP.

8.3. Testing Empty Parameters.

Before continuing the discussion of macro definition and invocation, it is necessary
to discuss a particular operator, called the NUL operator, which is specifically designed
to allowing testing of null parameters (i.e., actual parameters of length zero). The

---
_Page 059_

0005 = BDOS
0009 = PMSG
000D = CR
000A = LF

PRINT
a3
MSG:
PASTM:

0100+C31E01
0103+5468652072270002:
0119+0D0A
011B+0D0A
011D+24
O11E+110301 220001:
0121+0E09
0123+CD0500
0126+C34001
0129+6D61696E6C??0004:
013F+24
0140+112901 220003:
0143+0E09
0145+CD0500
0148 C9

Figure

ORG 100H ;BASE OF THE TPA

EQU 5 ;BDOS ENTRY POINT
EQU 9 ;PRINT 'TIL $ FUNCTION
EQU 0DH ;CARRIAGE RETURN

EQU 0AH ;LINE FEED

MACRO N,MESSAGE

PRINT MESSAGE, FOLLOWED BY N CRLF'S
LOCAL PASTM,MSG

JMP PASTM 3; JUMP PAST MSG

DB MESSAGE ;; INCLUDE TEXT TO WRITE
REPT N ;;REPEAT CR LF SEQUENCE
DB CR, LF

ENDM

DB '$! 3 ;MESSAGE TERMINATOR
LXI D,MSG 3 ;MESSAGE ADDRESS

MV I C,PMSG ;;PRINT FUNCTION
CALL BDOS

ENDM

PRINT 2,'The rain in Spain goes'
JMP 220001

DB 'The rain in Spain goes'

DB CR, LF

DB CR, LF

DB '¢$!

LXI D,??0002

MV I C,PMSG

CALL BDOS

PRINT 0,'mainly down the drain.'
JMP 220003

DB "mainly down the drain.'

DB '¢$!

LXI D,??0004

MV I C,PMSG

CALL BDOS

RET

18. Sample Message Print-out Macro.

---
_Page 060_

NUL operator is used in an expression as a unary operator, and produces a true value
if its argument is of length zero and a false value if the argument has length greater
than zero. Thus, the operator appears in the context of an arithmetic expression as:

. NUL argument

where the ellipses (. . .) represent an optional prefixing arithmetic expression, and
"argument" is the operand used in the NUL test. Note that the NUL differs from
other operators since it must appear as the last cperator in the expression. This is
due to the fact that the NUL operator "absorbs" all remaining characters in the
expression until the following comment or end of line is found. Thus, the expression

& GT Y AND NUL XXX

is valid since NUL absorbs the argument XXX (producing a false value) in the scan
for the end of line. The expression

X GT Y AND NUL

is also valid, however, since the argument following the NUL is empty, thus causing
NUL to return a true value since the end of line is immediately encountered in the
sean. Intervening blanks and tabs are ignored in this scanning process. The expression

X GT Y AND NUL M + Z)

is somewhat deceiving, but nevertheless valid even though it appears as if it is an
unbalanced expression. In this case, the argument following the NUL operator is the
entire sequence of characters "M + Z)" which is absorbed by the NUL operator in
scanning for the end of line. The value of "NUL M + Z)" is "false" since the sequence
is not empty.

Figure 19 gives several examples of the use of NUL in a particular program.
In the first case, NUL returns true since there is an empty argument following the
operator. Thus, the "true case" is assembled (as indicated by the machine code to
the left), and the "false case" is ignored. Similarly, the second use of NUL in Figure
19 produces a false value since the argument is non-empty. Both uses of NUL, however,
are contrived examples, since NUL is really only useful within a macro group, as shown
in the definition of the NULMAC macro.

NULMAC consists of a sequence of three conditional tests which demonstrate
the use of NUL in checking empty parameters. In each of the tests, a "DB" is
assembled if the argument is not empty, and skipped otherwise. Six invocations of
NULMAC follow its definition, giving various combinations of empty and non-empty
actual parameters.

In the first case, NULMAC has no actual parameters and thus all dummy
parameters (A, B, and C) are assigned the empty sequence. As a result, all three
conditional tests produce false results since both A and B are empty, and B&C
concatenates two empty sequences, producing an empty sequence as a result.

The second invocation of NULMAC provides only one actual parameter (XXX)
which is assigned to the dummy parameter A, while B and C are both assigned the

---
_Page 061_

0000 7472756520

on

0009 7878782069

IF

DB
ELSE
DB
ENDIF

IF

DB
ELSE
DB
ENDIF

NULMAC MACRO

on

0017+61203D2058

0029+62203D2058
003B+6263203D20

004F+61203D2058
0061+6263203D20

0075+6263203D20

0089+6263203D20
009C

Figure 19.

IF

DB
ENDIF
IF

DB
ENDIF
IF

DB
ENDM

NULMAC
NULMAC
DB
NULMAC
DB

DB
NULMAC
DB

DB
NULMAC
DB
NULMAC
NULMAC
DB

END

NUL
"true ease!

‘false ease!

NUL XXX

"xxx is nul'

"xxx is not nul'
A,B,C

NOT NUL A

‘a = &A is not nul'

NOT NUL B
"b = &B is not nul'

NOT NUL B&C
"be = &B&C is not nul!

‘a = XXX is not nul'

b = XXX is not nul'
"be = XXX is not nul'

'a = XXX is not nul'
"be = YYY is not nul'

bd]
be = YYY is not nul'

be = titt is not nul’

Sample Program using the NUL Operator.

---
_Page 062_

empty sequence. Thus, only the "DB" for the first conditional test is assembled.

The third case is similar to the second, except that the actual parameters for
A and C are omitted. Thus, the second and third conditionals both test "NOT NUL
XXX" which is true since B has the value XXX, and B&C produces the value XXX as
well.

The fourth invocation of NULMAC skips the actual parameter for B, but supplies
values for both A and C. Thus, the first and third test result in true values, while
the second conditional group is skipped.

The fifth invocation provides an actual parameter only for C. As a result, only
the third conditional is true, since B&C produces the sequence YYY.

The sixth invocation produces exactly the same result as the first, since all
three actual parameters are empty.

The final expansion of NULMAC in Figure 19 shows a special case of the NUL
operator. The expression

NUL ''

(where the two apostrophes are in juxtaposition) produces the value true even though
there are two apostrophe symbols on the line following NUL and before the end of
line. Note that the value of A is the empty string in this case, while the value
assigned to both B and C consists of the two apostrophe characters side-by-side, which
is treated as a quoted string of length zero (even though it is a sequence of two
characters!). In this last expansion, the first conditional produces a false value since
A is associated with the empty sequence. The second conditional, however, evaluates
the form

NOT NUL ''

which is the specia) case of NUL applied to a length zero quoted string (not a length
zero sequence, however). Because of the special treatment of the length zero quoted
string, this expression also produces a false result. The third conditional, however,
must be considered carefully: the original expression in the macro definition takes
the form

NOT NUL B&C
with B and C both associated with the sequence of length two given by two adjacent
apostrophes. Thus, the macro assembler examines

NOT NUL '! '&''

or, after concatenation,
NOT NUL '''!

where the four apostrophes are juxtaposed. Considering only the four adjacent

apostrophes, the macro assembler considers this a quoted string which happens to
eontain a single apostrophe, since double apostrophes within strings are always reduced

---
_Page 063_

to a single apostrophe. As a result, the test produces a true value and the conditional
segment is assembled. If this all seems confusing, that's because it is. Fortunately,
these cases are very specialized, and are included here for completeness. Under normal
circumstances, the NUL operator is used only to test for missing arguments, as shown
in later examples (see Figure 22 for a particular case).

8.4. Nested Macro Definitions.

The MAC assembler allows the programmer to include nested macro definitions,
which take the form

macl MACRO macl-list
mac2 MACRO mac2-list
ENDM
ENDM
where "maci" is the identifier corresponding to the outer macro, and "mac2" is an
identifier corresponding to an inner nested macro which is wholly contained within the
outer macro. In this case, "maci-list" and "mac2-list" correspond to the dummy

parameter lists for macl and mac2, respectively. As before, labels are allowed on
the ENDM statements.

Recall that the statements contained within a macro definition are "prototype"
statements which are read and stored by the assembler, but not evaluated as assembly
language statements until the macro is expanded. Thus, in the form shown above,
only the macl macro can is available for expansion, since the assembler has stored
but not processed the body of maci which contains the definition of mac2. That is,
mac2 cannot be expanded until macl is first expanded revealing the definition of mac2.

Properly balanced imbedded macros of this form can be nested to any level,
but cannot be referenced until their encompassing macros have themselves been
expanded.

Figure 20 gives a practical example of nested macro definition and expansion.
This particular program writes characters to either the CP/M console device or the
currently assigned list device, according to the value of the LISTDEV flag which is
set for the assembly. If the LISTDEV flag is true, then the assembly sends characters
to the listing device, otherwise the console is used for output. In either case, the
macro OUTPUT is produced which sends a single character to whatever device is
selected.

For purposes of illustration, the macro SETIO is used to construct the OUTPUT
macro. Note in Figure 20 that the OUTPUT macro is wholly contained within the
SETIO macro and, as a result, remains undefined until SETIO expands. Upon encountering
the invocation of SETIO, the macro assembler reads the prototype statements within
SETIO and, in the process, constructs the definition of the OUTPUT macro. Since
LISTDEV is true for this assembly, the OUTPUT macro becomes defined as

a7

---
_Page 064_

FFFF

FFFF

°
,

BDOS 5

CONOUT EQU 2 ;WRITE TO CONSOLE
LISTOUT EQU 5

ORG 100H ;BASE OF THE TPA

FALSE EQU 0000H ; VALUE OF FALSE
TRUE

EQU NOT FALSE ;VALUE OF TRUE
LISTDEV IS TRUE IF LIST DEVICE IS USED
FOR OUTPUT, AND FALSE IF CONSOLE IS USED

;
LISTDEV EQU TRUE

EQU ;BDOS ENTRY POINT

;sWRITE TO LIST DEVICE

bd
SETIO MACRO ;SETUP "OUTPUT" MACRO FOR LIST OR CONSOLE

bi
OUTPUT MACRO CHAR

0100+1E2A
0102+0E05
0104+CD0500

0107+1E31
0109+0E05
010B+CD0500

010E+1E32
0110+0E05
0112+CD0500
0115 C9

Figure 20.

MVI E,CHAR ;;READY THE CHARACTER FOR PRINTING
IF LISTDEV
MV I C,LISTOUT

MVI C,CONOUT
CALL BDOS

OUTPUT '\*!

SETIO ;SETUP THE IO SYSTEM
MVI E,'\*!

MV I C,LISTOUT
CALL BDOS
OUTPUT ‘1!

MVI E,'1'

MVI C, LISTOUT
CALL BDOS
OUTPUT '2!

MVI E,'2'

MVI C, LISTOUT
CALL BDOS

Sample Program showing a Nested Macro Definition.

---
_Page 065_

OUTPUT MACRO CHAR

MVI E,CHAR
MVI C,LISTOUT
CALL BDOS
ENDM

Note that the SETIO macro itself uses this newly created OUTPUT macro in its last
prototype statement to print a single "\*" at the selected device.

Following the invocation of SETIO, the invocations of OUTPUT are recognized
since its definition has been entered in the process of reading the prototype statements
of SETIO. These invocations send the characters "1" and "2" to the list device,
respectively.

8.5. Redefinition of Macros.

It is often useful to redefine the prototype statements of a particular macro
after the initial prototype statements have been entered. This is often simply a
particular case of the previous section, where the inner nested macro carries the same
name as the encompassing macro definition. Although this feature may seem somewhat
frivolous, there is one particular case where macro redefinition is extremely useful:
if the macro uses a subroutine then the subroutine can be included on the first expansion
and simply called in any remaining expansions. Thus, if the macro is never invoked
then the subroutine is not included in the program.

Figure 21 shows an example of macro redefinition. In this case, the macro
MOVE is defined which is intended to move byte values from a starting "source address"
to a target "destination address" for a particular number of bytes. The three dummy
parameters denote these three values: SOURCE is the starting address, DEST is the
destination address, and COUNT is the number of bytes to move (a constant in the
range 0-65535). The actions of the MOVE macro, however, are sufficiently complicated
that they should be performed through a subroutine, rather than inline machine code
each time MOVE is expanded.

Examining the structure of MOVE in Figure 21, note that it contains a properly
nested redefinition of MOVE, taking the general form:

MOVE MACRO SOURCE,DEST,COUNT

@QMOVE subroutine
MOVE MACRO 28,?D,?C
call to QMOVE

ENDM
invocation of MOVE
ENDM

The action of the assembler upon encountering the first invocation of MOVE is to
begin reading the prototype statements. Note, however, that the first expansion of
the MOVE includes the subroutine for the actual move operation, labelled by QMOVE
so that there is no name conflict (with a branch around the subroutine). MOVE then
redefines itself as a sequence of statements which simply call the out-of-line subroutine
each time it expands. In fact, the last statement of the original MOVE macro is an

---
_Page 066_

MOVE

@MOVE :

x?

PASTSUB:

MOVE

0100+C30E01
0103+79
0104+B0
0105+C8
0106+7E
0107+12
0108+23
0109413
010A+0B
010B+C30301
010E+212701
0111+114001
0114+010500
0117+CD0301

011A+210030
011D+110010
0120+010015
0123+CD0301

0126 C9

0127 6865726520X1:
0140 7878787878xX2:

ORG 100H ;BASE OF TPA

MACRO SOURCE , DEST , COUNT

MOVE DATA FROM ADDRESS GIVEN BY 'SOURCE'

TO ADDRESS GIVEN BY 'DEST' FOR 'COUNT' BYTES
LOCAL PASTSUB ;;LABEL AT END OF SUBROUTINE

JMP PASTSUB ;;JUMP AROUND INLINE SUBROUTINE
;; INLINE SUBROUTINE TO PERFORM MOVE OPERATION
HL IS SOURCE, DE IS DEST, BC IS COUNT

MOV A,C ;;LOW ORDER COUNT

ORA B 3;ZERO COUNT?

RZ 3;STOP MOVE IF ZERO REMAINDER
MOV A,M 3;;GET NEXT SOURCE CHARACTER
STAX D ;;PUT NEXT DEST CHARACTER

INX H ;;ADDRESS FOLLOWING SOURCE
INX D ;;ADDRESS FOLLOWING DEST

DCX B 3 ; COUNT=COUNT-1

JMP @Q@MOVE ;;FOR ANOTHER BYTE TO MOVE
ARRIVE HERE ON FIRST INVOCATION - REDEFINE MOVE
MACRO 28,?D,?C ;;CHANGE PARM NAMES
LXI H,?S ;;ADDRESS THE SOURCE STRING
LXI D,?D ;;ADDRESS THE DEST STRING

LXI B,?C 3; PREPARE THE COUNT

CALL @QMOVE 3;MOVE THE STRING

ENDM

CONTINUE HERE ON THE FIRST INVOCATION TO USE
THE REDEFINED MACRO TO PERFORM THE FIRST MOVE
MOVE SOURCE , DEST , COUNT

ENDM

MOVE X1,X2,5 ;MOVE 5 CHARS FROM XI TO X2
JMP 220001

MOV A,C

ORA B

RZ

MOV A,M

STAX D

INX H

INX D

DCX B

JMP @QMOVE

LXI H,X1

LXI D,X2

LX]! B,5

CALL @QMOVE

MOVE 3000H,1000H,1500H ;BIG MOVER
LXI H,3000H

LXI D,1000H

LXI B,1500H

CALL @MOVE

RET ;RETURN TO THE CCP
DB "here is some data to move'
DB "xxxxxwe are!'

Figure 21. Sample Program showing Macro Redefinition.

---
_Page 067_

invocation of the newly defined version. As indicated by this example, once a macro
has started expansion, it will continue to completion (or until EXITM is assembled),
even if it redefines itself.

It is important to note the use of ?S, ?D, and ?C in the above example. The
innermost MOVE macro uses the same sequence of three parameters for the source,
destination, and count. The dummy parameter names must differ, however, since they
would be substituted by their actual values if they were the same. This is due to the
fact that the inner MOVE macro is wholly contained within the outer macro and thus
parameter substitution takes place irregardless of the context.

Macro storage is not reclaimed upon redefinition, however, since the macro
assembler performs two passes through the source program and saves any preceding
definitions for the second pass scan.

8.6. Recursive Macro Invocation.

A "recursive" macro x has the property that its prototype statements contain
invocations of macros which, in turn, invoke macros which eventually lead back to an
invocation of x. A particular case of recursion, called "direct recursion," occurs when
x invokes itself, as shown in the form below:

macname MACRO d-1, ..., d-n
macname a-l,..., am
ENDM
Although this form is similar to the embedded macro definition discussed in the previous
section, note that "macname" is being expanded within its own definition, rather than
being redefined. Recursion is only useful, however, in the presence of conditional
assembly where various tests are made which prevent infinite recursion. In fact,

recursion is only allowed to sixteen levels before returning to complete the expansion
of an earlier level.

Figure 22 shows a situation where (indirect) recursive macro invocation is useful.
The macro WCHAR writes a character to the console device using the general-purpose
operating system macro CBDOS (call BDOS). CBDOS acts as an interface between
the program and the CP/M system by performing the system function given by FUNC,
with optional "information address" INFO. In particular, CBDOS loads the specified
function to register C, then tests to see if the INFO argument has been supplied (using
the NUL operator). If supplied, INFO is loaded to the DE register pair. After register
setup, the BDOS is called, and the macro has completed its expansion.

Assume, however, that CBDOS has the additional task of inserting a carriage-
return line-feed before writing messages in the particular case that operating system
function 9 (write buffer until "$") has been specified. In this ease, CBDOS uses the
WCHAR macro to send the carriage-return line-feed. Note, however, that the WCHAR
macro, in turn, uses CBDOS to send the character resulting in two activations of
CBDOS at the same time. The assembler holds the initial invocation of CBDOS until
the WCHAR macro has completed, then returns to complete the initial CBDOS expansion.

An important observation in the presence of recursion is that the values of the
dummy parameters are saved at each successive level of recursion, and restored when

---
_Page 068_

000D
000A

0100+0E02
0102+116800
0105+CD0500

0108+0E02
010A+116900
010D+CD0500

0110+0E02
0112+110D00
0115+CD0500
0118+0E02
011A+110A00
011D+CD0500
0120+0E09
0122+112901
0125+CD0500
0128 C9

MSGADDR :

0129 616E64206C

Figure 22,

ORG 100H ;BASE OF TRANSIENT AREA
SAMPLE PROGRAM SHOWING RECURSIVE MACROS

EQU 0005H  ;ENTRY TO BDOS
EQU 2 ;CONSOLE CHARACTER OUT
EQU g ;PRINT MESSAGE 'TIL $
EQU 0DH ;CARRIAGE RETURN

EQU 0AH sLINE FEED

MACRO CHR

WRITE THE CHARACTER CHR TO CONSOLE
CBDOS CONOUT,CHR ;;CALL BDOS
ENDM

MACRO -- FUNC, INFO

GENERAL PURPOSE BDOS CALL MACRO

FUNC IS THE FUNCTION NUMBER,

INFO IS THE INFORMATION ADDRESS OR NUL
CHECK FOR FUNCTION 9, SEND CRLF FIRST IF SO

IF FUNC=MSGOUT

PRINT CRLF FIRST

WCHAR CR

WCHAR LF

ENDIF

NOW PERFORM THE FUNCTION

MV I C, FUNC

INCLUDE LXI TO DE IF INFO NOT EMPTY
IF NOT NUL INFO

LX! D, INFO

ENDIF

CALL BDOS

ENDM

WCHAR ‘h! s;SEND "H" TO CONSOLE
MV I C,CONOUT

LXI D,'h!

CALL BDOS

WCHAR tq sSEND 'I' TO CONSOLE
MV I C,CONOUT

LXI D,'i!

CALL BDOS

CBDOS MSGOUT ,MSGADDR ;SEND MESSAGE
MV I C,CONOUT

LXI D,CR

CALL BDOS

MV I C, CONOUT

LXI D,LF

CALL BDOS

MV I C,MSGOUT

LXI D,MSGADDR

CALL BDOS

RET ; TERMINATE PROGRAM
DB ‘and lois$'!

END

Sample Program showing a Recursive Macro.

---
_Page 069_

that level of recursion is re-instated. In particular, re-entry into a macro expansion
through recursion does not destroy the values of dummy arguments held by previous
entry levels.

8.7. Parameter Evaluation Conventions.

There are a number of options which the programmer can exercise in the
construction of actual parameters, as well as in the specification of character-lists
for the IRP group. Although an actual parameter is simply a sequence of characters
placed between parameter delimiters, these options allow overrides where delimiter
characters themselves to become a part of the text. In general, a parameter x occurs
in the context:

label: maecname <...,%X%,...?

where "macname" is the name of a previously defined macro, and the preceding label
is optional. The elipses ". . ." represent optional surrounding actual parameters in the
invocation of macname. In the case of an IRP group, the occurrence of a character-list
x would be

label: IRP id) ...,X%,...

where the label is again optional, and the elipses represent optional surrounding
character-lists for substitution within the IRP group where the controlling identifier
"id" is found. In either case, the statements could be contained within the scope of
a surrounding macro expansion. Hence, dummy parameter substitution could take place
for the encompassing macro while the actual parameter is being scanned.

The macro assembler follows the steps shown below in forming an actual
parameter or character-list:

(a) leading blanks and tabs (control-I) are removed if they occur in front of x.
After this "deblanking" has occurred,

(b) the leading character of x is examined to determine the type of scan
operation which is to take place;

(c) if the leading character is a string quote (apostrophe), then x becomes the
text up through and ineluding the balancing string quote, using the normal string
scanning rules: double apostrophes within the string are reduced to a single apostrophe,
and upper case dummy parameters adjacent to the ampersand symbol are substituted
by their actual parameter values. Note that the string quotes on either end of the
string are included in the actual parameter text.

(d) If instead the first character is the left broken bracket "<" then the bracket
is removed, and the value of x becomes the sequence of characters up to, but not
including, the balancing right broken bracket ">" which does not become a part of x.
In this case, left and right broken brackets may be nested to any level within x, and
only the outer brackets are removed in the evaluation. Quoted strings within the
brackets are allowed, and substitution within these strings follows the rules stated in
(c) above. Note that left and right brackets within quoted strings become a part of
the string, and are not counted in the bracket nesting within x. Further, the delimiter

---
_Page 070_

characters comma, blank, semicolon, tab, and exclaim become a part of x when they
occur within the bracket nesting.

(e) If the leading character is a percent (%), then the sequence of characters
which follows is taken as an expression which is evaluated immediately as a 16-bit
value. The resulting value is converted to a decimal number and treated as an ASCII
sequence of digits, with left zero suppression (0-65535).

(f) If the leading character is neither a quote nor a left bracket nor a percent,
the (possibly empty) sequence of characters which follow, up to the next comma, blank,
tab, semicolon, or exclaim symbol, becomes the value of x.

There is one important exception to the above rules: the single character
escape, denoted by an up-arrow, causes the macro assembler to read the immediately
following special (non alphabetic) character as a part of x without treating the character
as significant. The character which follows the up-arrow, however, must be a blank,
tab, or visible ASCII character. The up-arrow itself can be represented by two up
arrows in succession. If the up-arrow directly precedes a dummy parameter, then the
up-arrow is removed and the dummy parameter is not replaced by its actual parameter
value. Thus, the up-arrow can be used to prevent evaluation of dummy parameters
within the macro body. Note that the up-arrow has no special significance within
string quotes, and is simply included as a part of the string.

Evaluation of dummy parameters in macro expansions must also be considered,
although this topic has been presented throughout the previous sections. Generally,
the macro assembler evaluates dummy parameters as follows:

(a) If a dummy parameter is either preceded or followed by the concatenation
operator (&), then the preceding and/or following "&" operator is removed, the actual
parameter is substituted for the dummy parameter, and the implied delimiter is removed
at the position(s) the ampersand occurs.

(b) Dummy parameters are replaced only once at each occurrence as the
encompassing macro expands. This prevents the "infinite substitution" which would
occur if a dummy parameter evaluated to itself.

In summary, parameter evaluation follows these rules:

leading and trailing tabs and blanks are removed

quoted strings are passed with their string quotes intact
nested brackets enclose arbitrary characters with delimiters

a leading percent symbol causes immediate numeric evaluation
an up-arrow passes a special character as a literal value

an up-arrow prevents evaluation of a dummy parameter

the "&" operator is removed next to a dummy parameter
dummy parameters are replaced only once at each occurrence

ee eH Ke He &

Figures 23, 24, and 25 show examples of macro definitions and invocations which
illustrate these points. In Figure 23, for example, two macros are defined, called
MAC1 and MAC2, which each have several dummy parameters. In this case, the macro
definitions are headed by "DB" statements in order to reveal the actual values which
are passed in each case. There is a single (mainline) invocation of MACI1 with the
actual parameters

---
_Page 071_

MAC1

;

?

A:

C&1:

L&A&D

;

MAC2

;

;

?

y
0OO0O0F = xX

+ ;

+ ’
0000+492020582B
0009+6B776F7465
000E+3610

+

+ ;

+ ;
0010+49204D2049
0018+6B776F7465
001D+00 I:
GO01E+3601
0020+00 Il:
0021+00 LI16:

+ ;

+ ;

+

+ ;

+ ;

+

Figure 23.

MACRO PARAMETER EVALUATION

MACRO A,B,C,D,S
ENTERING MACRO 1;

DB '&A &B &C &D!
DB S

NOP

MVI B,1

NOP

NOP

LEAVING MACRO 1

ENDM

MACRO E,F,G,H,S
ENTERING MACRO 2:

DB '&E &F &G &H'
DB S

MV I M,H

MAC1 E,F&M,A,H,S
LEAVING MACRO 2

ENDM

EQU 15

MAC2 I >, +1, %X + 1, 'kwote!'!
ENTERING MACRO 2:

DB 'T X+1 16!
DB "kwote'

MVI M,16

MAC1 1,M,I,16,'kwote'
ENTERING MACRO 1:

DB ‘rE MI 16!

DB 'kwote'!

NOP

MVI M,1

NOP

NOP

LEAVING MACRO 1

ENDM

LEAVING MACRO 2

ENDM

END

Macro Parameter Evaluation Example.

---
_Page 072_

I, X+1, % X + 1, 'kwote’

which assocates I with E, the null sequence with F, the sequence X+1 with G, the
value 16 with H, and the literal string 'kwote' with S. MAC2 expands, filling the DB
and MVI instructions with the substituted values. Before leaving MAC2, MACI1 is
invoked with the value of E (the sequence I), the concatenation of the dummy argument
F with the sequence M (producing "M" since F's value is null), along with the literal
value A, followed by the value of H (which is 16), and terminated by the value of S
(yielding the string 'kwote’). These values are associated with MACI's dummy para-
meters. Upon expanding MAC1, the DB statements are filled-out, followed by the
substitution of A as a label (producing A's value I). The MVI instruction references
memory since B's value is M. Note that the coneatenation of C with 1 reduces to a
concatenation of A with 1 since C's value is A. The replacement of C by A constitutes
a substitution of a single occurrence of a dummy parameter, and thus the A which is
produced is not itself replaced at this point. Finally, the literal value L is concatenated
to the value of A and D to produce the label LI16.

Figure 24 illustrates the use of bracketed notation, using IRP's (indefinite repeats)
within two macros, called IRPM1, IRPM2, and IRPM3. Note that one bracket level
is removed in the first invocation of IRPM1, leaving the IRP list with one bracket
level (required in the IRP heading). Similarly, the IRPM2 invocation also eliminates
the outer bracket level, but these brackets are replaced at the IRP heading within
IRPM2. IRPM3 has three distinct dummy parameters which are reconstructed as a
single list at the IRP heading which it contains. IRPM4 shows the effect of passing
parameters through two macro invocation levels by accepting a single parameter X,
which is immediately passed along to the IRPM1 macro. Note that the invocation
requires three bracket levels: the first is removed at the invocation of IRPM4, the
second level is removed at the nested invocation of IRPM1 inside IRPM4, and the
innermost level is required at the IRP heading within IRPM1.

Figure 25 presents various combinations of bracketed actual parameters, quoted
strings, and escape sequences. The MAC1 macro has two parts: the first portion
includes a "DB" statement which shows the value of the first parameter X (if it is
not empty), and the second part produces the value of Y, if not empty. Note that
the first invocation includes a properly nested braeketed sequence for X, and an empty
parameter for Y. The second invocation sends a properly nested bracketed expression
for X which produces an empty value since no characters remain after the brackets
are removed. The second parameter includes a quoted string (‘string of pearls’) and
a hexadecimal value which becomes a part of the "DB" in MAC1.

The third invocation of MAC1 passes a bracketed expression, which includes a
quoted string (i.e., the pair of adjacent apostrophes), followed immediately by a sequence
of ASCH characters. Note that the pair of apostrophes are passed intact since they
appear as an empty quoted string. In this case, the value of Y is empty. The
remaining examples show various cases of strings and escape sequences. In particular,
one must take care in passing quoted strings which themselves contain apostrophes,
since a pair of apostrophes is considered a single apostrophe at each evaluation level
in the sequence of macro invocations. Pay particular attention to the use of the
escape character to pass an unevaluated dummy parameter from MAC2 to the MACI1
invocation.

---
_Page 073_

ITRPM1 MACRO x

73 INDEFINITE REPEAT MACRO
IRP Y,X
Y: NOP
ENDM
ENDM
IRPM1 <<ONE, TWO, THREE>>
GAG04+88 ONE: NOP
08681+00 TWO: NOP
6882+80 THREE: NOP
IRPM2 MACRO xX
IRP Y,<X>
Y: NOP
ENDM
ENDM
IRPM2 <FOUR,FIVE,SIX>
0003+06 FOUR: NOP
8884+98 FIVE: NOP
@8954+68 SIX: NOP
IRPM3 MACRO X1,X2,X3
IRP ¥,<X1,X2,X3>
Y: NOP
ENDM
ENDM
IRPM3 SEVEN , EIGHT ,NINE
9006+86 SEVEN: NOP
G397+9G0 EIGHT: NOP
OAG8+O00 NINE: NOP
IRPM4 MACRO xX
IRPM1 xX
ENDM
IRPM4 <<<TEN,ELEVEN , TWELVE>>>
0909409 TEN: NOP
GOGA+HDB ELEVEN: NOP
G0GB+00 TWELVE: NOP
BBGC END

Figure 24, Parameter Evaluation using Bracketed Notation,

---
_Page 074_

SAMPLE BRACKETED PARAMETERS, WITH ESCAPE CHARACTER

,

MAC1 MACRO
DB
IF
EXITM
ENDIF
DB
ENDM

oy

MAC1
0000+3C4C454654 DB
‘ MAC1
OG1F+737472696E DB
‘ MAC1
60304+412051554F DB
‘ MACI
0046+7269676874 DB
‘ MAC1
80574+6973207468 DB
‘ MAC1
606B+4845524520 DB
MAC2 MACRO
LOCAL
X EQU
DB
MAC]
ENDM

ow

MAC2
GQSA+= 220801 EQU
8B7E+3C DB
O087F+41584152 DB
0883+7768617427 DB

Q-

Figure 25. Examples

X,Y
“Ex
NUL Y

Pd

(ONE)

on

m

(TWO)

<<LEFT SIDE> MIDDLE <RIGHT SIDE>>
“<LEFT SIDE> MIDDLE <RIGHT SIDE>~\* ; (ONE)

<>,< string of pearls’ ,34H>
“string of pearls’,34H_ ; (TWO)

oe

<A QUOTE IS A , RIGHT?>

oe o

“A QUOTE IS A , RIGHT? : (ONE)

eoeeoee

<>,< “right, but also >
“right, but also °”’ (TWO)

chy

’ oo eee or ewe

,< is this ..

“is this °, confusing

confusing

owe

,63>
,63 2 (TWO)

<HERE IS A 7> AND A TT>

“HERE IS A > AND A 1 : (ONE)

APAR,BPAR
xX

APAR
TAPAR,BPAR

owe weave

(X+5) \*4, “what
(2??@001+5) \*4
“APAR’ =; (ONE)

“what’’s going on?’ > (TWO)

S qoing on?’

of Macro Parameter Evaluation,

---
_Page 075_

It is worthwhile examining the various parameters and their evaluations in Figure
25 to ensure that the rules for evaluation given in this section are consistent.

8.8. The MACLIB Statement.

The macro assembler allows the programmer to create and reference "macro
library" files which are external to the mainline program. The form of the macro
library reference is

MACLIB libname

where "libname" is an identifier which references a particular file "libname.LIB" which
is assumed to exist on the diskette. Macro libraries are in source program form, and
ean thus be easily created and modified by the programmer using the CP/M system
editor (ED).

In order to speed-up the assembly process, macro libraries are read only on the
first assembly pass. This places some restrictions on the use of the MACLIB statement,
as listed below:

(a) the statements included in the macro library cannot generate machine code.
For example, comments, EQU's, SET's, and MACRO definitions are allowed, while DB
statements outside macro definitions are not allowed.

(b) Macro libraries are not normally listed with the source program (although
there is an overriding parameter which can be supplied ~- see Assembly Parameters).

(c) All MACLIB statements must appear before the mainline program macro
definitions. Generally, the MACLIB statements are placed at the beginning of the
program, followed by the mainline declarations and machine code.

The principal advantage of the MACLIB feature is that the programmer can
predefine macros which enhance the facilities of the assembly language itself. For
example, the additional operations codes of the Zilog Z-80 microprocessor can be
defined in a macro library which is reference in a single statement

MACLIB Z80

which causes the assembler to read the file "Z80.LIB" from the diskette, containing
the necessary macros for Z-80 code generation. These macros can then be referenced
within the program intermixed with the usual 8080 mnemonics.

Normally, the "libname.LIB" file is assumed to exist on the currently logged
disk drive. The programmer can override this default condition using a special parameter
(L) when the macro assembler is started which redirects the ".LIB" references to a
different diskette (see Assembly Parameters).

Figures 10 and 11 show the use of the macro library facility, as introduced in

the initial macro discussion. The following sections contain additional examples of the
use of MACLIB in practical applications.

---
_Page 076_

---
_Page 077_

9. APPLICATIONS OF MACROS

The MAC assembler provides a powerful tool for microcomputer systems develop-
ment through its macro facilities. In order to demonstrate this tool, a number of
applications of macros in the solution of practical problems are described in some
detail in the following sections. Four particular applications areas are considered:
use of macros in implementation of special-purpose languages, emulation of non-standard
machine architectures, implementation of additional control structures, and operating
systems interface macros.

9.1. Special Purpose Languages.

A wide variety of microcomputer designs can be broadly classed as "controller"
applications. Specifically, the microcomputer is used as the controlling element in
sequencing and decision-making as real-time events are sampled and directed.

Typical applications of this sort include assembly line sensing and control, metal
machine control, data communications and terminal contro] functions, production in-
strumentation and testing, and traffic control systems.

In many cases, application programmers set up the sequence of operations that
the microprocessor is to carry-out in performing its particular task. In order to avoid
unnecesary details, the application programmer is not expected to know how to program
and debug microcomputer assembly language programs.

In this situation, it is useful to define a "language" through macros which suits
the particular application. The application programmer then uses these predefined
macros as the primitive language elements. If properly defined, the application language
is easily programmed, allowing considerable machine independence. That is, an applica-
tion program written for a particular microprocessor can be used with another processor
by changing the definitions of the individual macros which implement the primitive
operations. Further, the macro bodies can incorporate debugging facilities for applica-
tion development.

In order to illustrate the notion of language definition, consider the following
situation. Hornblower Highway Systems, Inc., produces "turnkey" traffic control systems
for cities throughout the country. Their hardware subsystems consist of various traffic
lights and sensors which are customized for the traffic layout in a particular city.
When Hornblower negotiates a contract, their engineers survey the intersections of the
city, and produce plans which show a configuration of their standard hardware for each
intersection, along with the "algorithms" required for traffic flow at that point.

The standard hardware items which Hornblower manufactures consist of the
following. Central and corner traffic lights which display green, yellow, and red (or
off completely), pushbutton switches for pedestrian cross requests, road "treadles" for
sensing the presence of an automobile at an intersection, and a central controller box.

The central controller box contains an 8080 microcomputer connected through
external logic to relays which control the lights, and "latches" which holds the sensor
input information. The controller box also contains a time of day clock, which changes
on an hourly basis from 0 through 23. The 8080 processor in the controller box can
be configured for any particular intersection with up to 1024 bytes of programmable

---
_Page 078_

read only memory (PROM) in 256 byte increments. Although random access memory
ean be included in the controller box, Hornblower uses only ROM when possible.

Thus, the Hornblower engineers examine the hardware requirements for each
intersection in the city, and produce a set of hardware configuration plans which
intermix the various standard components. Programs are then written and debugged
which contro] each intersection, based upon predicted traffic patterns.

The intersection of Easy St. and Maria Ave., for example, controls minimal
traffic and thus consists of a controller box with a single central light. The "algorithm"
for this intersection is to simply alternate red and green lights between Easy and
Maria, with a "bias" toward Easy St., since traffic along Easy has measured higher in
the past surveys. Thus, the green light along Easy lasts for 20 seconds, while the
green along Maria last only 15 seconds. Given this situation, the application programmer
writes the following program:

HORNBLOWER HIGHWAYS SYSTEMS, INC.

: INTERSECTION:
. EASY ST.(N-S) / MARIA AVE. (E-W)
MACLIB INTERSECT ;LOAD MACROS
;
CYCLE: SETLITE NS,GREEN
SETLITE EW,RED
TIMER 20 ;WAIT 20 SECS

; CHANGE LIGHTS
SETLITE NS, YELLOW
TIMER 3 ;WAIT 3 SECS
SETLITE NS,RED
SETLITE EW,GREEN
TIMER 15 ;WAIT 15 SECS

; CHANGE BACK
SETLITE EW,YELLOW
TIMER 3 ;WAIT 3 SECS
RETRY CYCLE

The macro library "INTERSECT.LIB" contains the macro definitions which implement
the "primitive" operations SETLITE and TIMER which set the central traffic light, and
time-out for the specified interval, respectively. Further, the RETRY macro causes
the traffic light to recycle on each light change. Note that the sequence of operations
is easy to write, and is completely machine independent.

Figure 26 gives an example of a macro library for "intersect" which assumes
the following hardware with an 8080 processor: the central traffic light is controlled
by the 8080 output port 0 (given by "light"), while the time of day clock is read from
port 3 ("clock"), Further, the north-south ("nsbits") of the central light are given by
the high order 4 bits of output port 0, while the east-west direction ("ewbits") is
specified in the low order 4 bits of output port 0. When either of these fields is set
to 0, 1, 2, or 3, the light in that direction is turned off, or set to red, yellow, or
green, respectively. Thus, the SETLITE macro in Figure 26 accepts both a direction
(NS or EW), along with a color (OFF, RED, YELLOW, or GREEN), and sets the specified
direction to the appropriate color.

---
_Page 079_

ee woe we

light
elock

J

nsbits
ewbits
;

off
red
yellow
green
setlite

+9

iffalse:

retry

e
9?

macro library for basie intersection

input/output ports for light and clock

equ 00h straffie light control

equ 03h 324 hour clock (0,1,...,23)
constants for traffie light control

equ 4 ;north souuth bits

equ 0 ;east west bits

equ 0 ;turn light off

equ 1 svalue for red light

equ 2 svalue for yellow light

equ 3 ;green light

macro dir,color
set light "dir" (ns,ew) to "color" (off,red, yellow, green)

mvi a,color shl dir&bits ss;color readied
out light 3;3;sent in proper bit position
endm

macro seconds
construet inline time-out loop

local t1,t2,t3 3;;loop entries

mvi d,4\*seconds 3;3;basie loop control
mvi b,250 33;250msece \*4 = 1 see

mv i e,182 33182\*5.5usee = Imsec

der e s;3;l cy = .5 usee

jnz t3 33+10 cy = 5.5 usee

der b s;;count 250,249...

jnz t2 ;;loop on b register

der d 3;basie loop control

jnz tl ;;loop on d register
arrive here with approximately "seconds" secs timeout
endm

macro low,high, iftrue
jump to "“iftrue" if eloek is between low and high

local iffalse ;;alternate to true case
in clock 33;read real-time clock

if not nul high 3;check high clock
epi high 33;equal or greater?

jne iffalse ;;skip to end if so

endif

epi low 3;;less than low value?

jne iftrue 3;;skip to label if not
endm

macro golabel

continue execution at "golabel"
jmp golabel

endm

Figure 26. Macro Library for Basie Intersection.

---
_Page 080_

The TIMER macro in Figure 26 uses the internal cycle time of the 8080 processor
to construct an inline timing loop, based on the value of SECONDS. Note that this
loop is not generated as a subroutine, since Hornblower prefers not to include RAM
in the controller box (subroutines require return addresses in RAM).

In addition to the basic intersection macro library, Hornblower has also defined
macro libraries for all of the optional hardware components. Figure 27a, for example,
is included when the intersection contains treadles in the street to detect automobiles,
while Figure 27b shows the macro library for pedestrian pushbuttons. In the case of
automotive treadles, the sensors are attached to input port 1 ("trinp") of the processor.
The treadles, however, require a "reset" operation which clears the latched value
through output port 1 ("trout") of the controlling 8080 processor. In any particular
intersection, the treadles are numbered clockwise from true north, labelled 0, 1, through
a maximum of 7 treadles. Each sensor and reset position of the treadle ports correspond
to one bit position, numbered from the least to most significant bit. Thus the treadle
\#0 sensor is read from bit 0 of port 1, and reset by setting bit 0 of output port 1.
Similarly, treadle #1 uses bit position 1 of input and output port 1. The TREAD?
macro is invoked to sense the presence of a latched value for treadle "tr" and, if on,
the sensor is reset with control transferring to the label given by "iftrue."

Figure 27b shows the macro library which processes pedestrian pushbuttons.
Hornblower's hardware is set up to sense the latched pedestrian switches on input port
0 ("ewinp") as a sequence 1's and 0's in the least significant positions, corresponding
to the switches at the intersection. Thus, if there are four pedestrian switches, bit
positions 0,1,2, and 3 correspond to these switches. A "1" bit in any of these positions
indicates that the pushbutton has been depressed. Unlike the automotive treadles, the
crosswalk switch latches are all cleared whenever input port 0 is read. In addition
to these macro libraries, Hornblower has defined several additional libraries which
support optional hardware manufactured by their company.

The intersection of Bumpenram Boulevard and Lullabye Lane presents a somewhat
more complicated situation. Bumpenram Blvd. earries heavy traffie in an E-W direction
to and from the center of town. Lullabye Ln., however, feeds a residential portion
of the city, running perpendicular to Bumpenram in a N-S direction. The contracting
city has specified that the traffic control should be biased toward Bumpenram Blvd.
as follows: the traffie light must remain green along Bumpenram until the treadles
along Lullabye detect the presence of automobiles or until the pedestrian switches are
pushed. At that time, the light must change to allow the traffic to move N-S through
Lullabye Ln., allowing all traffic to clear before returning to the major E-W flow
along Bumpenram Blvd. Late night traffic along Bumpenram is not very heavy, so the
city has also specified that the E-W light flashes yellow and and N-S direction flashes
red between the hours of 2 and 5 AM.

The application program created by Hornblower for the Bumpenram Blvd. and
Lullabye Ln. intersection is shown in Figure 28. Each major cycle of the traffic light
enters at "CYCLE" where the time of day is tested. If between 2 and 5, then control
transfers to "NIGHT" where the yellow/red lights are flashed in the appropriate
directions. If not between 2 and 5 AM, the switches and treadles are sampled until
N-S traffic along Lullabye Ln. is sensed. If cross traffie is detected, the lights switch
until all the traffic is through. Sampling also stops if the time of day ever reaches
2 AM.

---
_Page 081_

iffalse:

ewinp

push?

ee woe we

wo we we we

macro library for street treadles

equ O1h ;treadie input port
equ 01h ;treadle output port
macro tr,iftrue

"tread?" is invoked to check if

treadle given by tr has been sensed.

if so, the lateh is cleared and control
transfers to the label "iftrue"

local iffalse 3;;in case not set
in trinp ;;read treadie switches
ani 1 shi tr s;3;mask proper bit
jz iffalse 3;skip reset if 0
mvi a,l shi] tr s;to reset the bit
out trout ss;clear it

jmp iftrue 33;go to true label
endm

Figure 27a. Macro Library for "treadle" Control.

macro library for pedestrian pushbuttons
equ 00h ;input port for crosswalk

macro iftrue

"push?" jumps to label "iftrue" when any one
of the erosswalk switches is depressed. The
value has been latehed, and reading the port
clears the latehed values

in ewinp ;3;read the crosswalk switches
ani (1 shl ewent) - 1 3;build mask
jnz iftrue 3;;any switches set?

continue on false condition

endm

Figure 27b. Macro Library for Corner Pushbuttons.

---
_Page 082_

000C

001B
O03E

005B
O05F

OO07F

008D

00A2

00A5
00A9
00AD
OOBF
00C3
00C7
00D9

CWCNT
LULLO
LULLI

CYCLE:

SAMPLE:

SWITCH:

DONE? :

NOTDONE :

NIGHT:

INTERSECTION: BUMPENRAM BLVD / LULLABYE LN.

EQU 4 ;SET TO 4 CROSSWALK SWITCHES
EQU 0 ;NAME FOR TREADLE ZERO

EQU 1 ;NAME FOR TREADLE ONE

MACLIB INTER ;BASIC INTERSECTION
MACLIB TREADLES ; INCLUDE TREADLES
MACLIB BUTTONS ; INCLUDE PUSHBUTTONS
; ENTER HERE ON EACH MAJOR CYCLE OF THE LIGHT
CLOCK? 2,5,NIGHT ;SPECIAL FLASHING?
;NOT BETWEEN 2 AND 5 AM

SETLITE NS,RED ;RED LIGHT ON LULLABYE
SETLITE EW,GREEN ;GREEN ON BUMPENRAM

; SAMPLE THE BUTTONS AND TREADLES
PUSH? SWITCH ;ANYONE THERE?

TREAD? LULLO, SWITCH ;TREADLE 0?
TREAD? LULL1,SWITCH sTREADLE 1?
CLOCK? 2, ,NIGHT sPAST 2 AM?
RETRY SAMPLE ;TRY AGAIN IF NOT

; SOMEONE IS WAITING, CHANGE LIGHTS

SETLITE EW, YELLOW ;SLOW 'EM DOWN
TIMER 3 ;WAIT 3 SECONDS
SETLITE EW,RED :STOP 'EM
SETLITE NS,GREEN ;LET 'EM GO
TIMER 23 ;FOR AWHILE

:IS ALL THE TRAFFIC THROUGH ON LULLABYE?
TREAD? LULLO,NOTDONE ;TREADLE 0?
TREAD? LULL1,NOTDONE ;TREADLE 1?
;NEITHER TREADLE IS SET, CYCLE

RETRY CYCLE ;FOR ANOTHER LOOP
TIMER 5 ;WAIT 5 SECONDS
RETRY DONE? ;TRY AGAIN

;THIS IS NIGHTTIME, FLASH LIGHTS

SETLITE EW,OFF ;TURN OFF

SETLITE NS,OFF :TURN OFF

TIMER 1 ;WAIT WITH OFF
SETLITE EW, YELLOW ;TURN TO YELLOW
SETLITE NS,RED ;TURN TO RED
TIMER 1 ;LEAVE ON FOR 1 SEC
RETRY CYCLE ;GO AROUND AGAIN

Figure 28a. Traffie Control Algorithm using "-M" Option.

---
_Page 083_

f=)

oS

So

oS
now

0000+DB03
0002+FE05
0004+D20C00
0007+FE02
0009+D2A500

000Ct+3E10
000E+D300

0010+3E03
0012+D300

0014+DB00
0016+E60F
0018+C24100

001B+DB01
001D+E601
001F+CA2900
0022+3E01
0024+D301
0026+C34100

0029+DB01
002B+E602
002D+CA3700
0030+3E02
0032+D301
0034+C34100

0037+DB03
0039+FE02
003B+D2A500

003E+C31400

Figure 28b.

CWCNT
LULLO
LULL1

CYCLE:

SAMPLE:

Intersection Algorithm with "\*M"

INTERSECTION: BUMPENRAM BLVD / LULLABYE LN.

EQU 4 ;SET TO 4 CROSSWALK SWITCHES
EQU 0 ;NAME FOR TREADLE ZERO

EQU 1 ;NAME FOR TREADLE ONE

MACLIB- INTER ;BASIC INTERSECT ION
MACLIB TREADLES ; INCLUDE TREADLES
MACLIB BUTTONS ; INCLUDE PUSHBUTTONS

; ENTER HERE ON EACH MAJOR CYCLE OF THE LIGHT
CLOCK? 2,5,NIGHT ;SPECIAL FLASHING?

;NOT BETWEEN 2 AND 5 AM

SETLITE NS,RED ;RED LIGHT ON LULLABYE

SETLITE EW,GREEN ;GREEN ON BUMPENRAM

;SAMPLE THE BUTTONS AND TREADLES

PUSH? SWITCH ;ANYONE THERE?

TREAD? LULLO, SWITCH ;TREADLE 0?
TREAD? LULL1,SWITCH ;TREADLE 1?
CLOCK? 2, ,NIGHT ;PAST 2 AM?
RETRY SAMPLE ;TRY AGAIN IF NOT

in Effect.

---
_Page 084_

0041+3E02
0043+D300

0045+160C
0047+06FA
0049+0EB6
004B+0D
004C+C24B00
004F+05
0050+C24900
0053415
0054+C24700

0057+3E01
0059+D300

005B+3E30
005D+D300

005F+165C
0061+06FA
0063+0EB6
0065+0D
0066+C26500
0069+05
006A+C26300
006Dt+15
006E+C26100

0071+DB01
0073+E601
0075+CA7FO00
0078+3E01
007At+D301
007C+C39000

007F+DB01
0081+E602
0083+CA8D00
0086+3E02
0088+D301
008A+C39000

008D+C30000

Figure 28c.

SWITCH:

220005:
220006:
220007:

220008:
220009:
220010:

DONE? :

; SOMEONE IS WAITING, CHANGE LIGHTS

SETLITE EW, YELLOW ;SLOW 'EM DOWN
MV I A, YELLOW SHL EWBITS

OUT LIGHT

TIMER 3 ;WAIT 3 SECONDS
MV I D,4\*3

MVI B, 250

MVI C,182

DCR Cc

INZ 220007

DCR B

JINZ 220006

DCR D

INZ 220005

SETLITE EW, RED ;STOP 'EM
MV I A,RED SHL EWBITS

OUT LIGHT

SETLITE NS,GREEN ;LET 'EM GO
MV I A,GREEN SHL NSBITS

OUT LIGHT

TIMER 23 ;FOR AWHILE
MVI D,4\*23

MV I B, 250

MV I C,182

DCR Cc

INZ 220010

DCR B

INZ 220009

DCR D

INZ 220008

:1S ALL THE TRAFFIC THROUGH ON LULLABYE?
TREAD? LULLO,NOTDONE ;TREADLE 0?
IN TRINP

ANI 1 SHL LULLO
JZ 270011

MV I A,1 SHL LULLO
OUT TROUT

JMP NOTDONE

TREAD? LULL1,NOTDONE ;TREADLE 1?
IN TRINP

ANI 1 SHL LULL1

JZ 220012

MV I A,1 SHL LULLI1

OUT TROUT

JMP NOTDONE

;NEITHER TREADLE IS SET, CYCLE

RETRY CYCLE ;FOR ANOTHER LOOP
JMP CYCLE

Algorithm with Generated Instructions.

---
_Page 085_

Figure 28a shows the assembly with no macro generated lines (controlled by the
"_M" parameter - see Assembly Parameters). Although the machine code locations are
shown to the left, no 8080 machine code is listed. Figure 28b shows a segment of
this same program with machine code generation, but no 8080 mnemonics (controlled
by "\*M"), while Figure 28¢ shows another segment with normal macro generation. Note
that Figure 28a is the most readable to the application programmer, while Figures 28b
and 28¢c would be useful for macro debugging.

It should be noted that the resulting program requires no random access memory
for execution, since all temporary values are maintained in the 8080 registers. Further,
no subroutine calls take place and thus the 8080 stack is not used. Finally, the program
is less than 256 bytes, so it can be placed in a single programmable read only memory
chip for a minimum memory/processor configuration.

Macro based languages of this sort can easily incorporate debugging facilities.
In the case of Hornblower, Inc., the principal algorithms are constructed and tested
in the CP/M environment by including debugging traces within each macro. In each
case, a debug "flag" is tested and, if true, machine code is generated to trace the
operation at the console, rather than actually executing the input/output calls. Figure
29 shows the modification required to the "INTER.LIB" file to include the debugging
eode. Although only the SETLITE macro is shown, similar coding is easily included
for the remaining macros. Figure 29 includes the debug flag at the beginning of the
library (initially set FALSE), along with the appropriate equates for CP/M system calls.
If the debug flag is set to true by the application programmer, special trace calls are
included. Note, for example, that the SETLITE macro constructs a message of the
form

DIR changing to COLOR

where "DIR" and "COLOR" are the parameters sent to the macro. If debug remains
false in the application program, this trace code is not assembled.

Figure 30a shows an application program for a particular intersection where the
debug flag is set to TRUE after the macro library is included. As a result, each
macro expansion assembles a call to the CP/M operating system to trace the light
direction and color change, skipping the machine code which will eventually be assembled
to drive the actual Hornblower hardware.

The application programmer then uses CP/M to trace the operation of the
algorithm, which results in the print-out shown in Figure 30b. Each trace line
corresponds to an invocation of SETLITE with a specifie direction and color, with the
appropriate wait time between print-outs.

Upon completion of the initial debugging under CP/M, the SET statement in the
application program is removed (the ORG may be removed as well), and the program
is re-assembled. This time, the CP/M traces are not included since the debug flag
remains FALSE. As a result, the actual Hornblower hardware interface is assembled
instead. The newly assembled program is then placed into PROM in the controller
box for that intersection and tested in its target enviroment.

---
_Page 086_

;

;

true
false
debug
bdos
rehar
wbuf f
er

lf

we wo

light
elock
;

?
nsbits
ewbits
;

;

off
red

yellow
green

;
setlite

ee
,°

setmsg:

pastmsg:

we we wo we

macro library for basie intersection

global definitions for debug processing

equ
equ
set
equ
equ
equ
equ
equ

equ
equ

equ

Offffh ;value of true
not true;value of false

false sinitially false
5 sentry to ep/m bdos
1 ;read character function
9 swrite buffer function
Odh ;carriage return
Qah ;line feed
input/output ports for light and clock
00h straffie light control
03h 324 hour elock (0,1,...,23)
bit positions for traffie light control
4 ;north souuth bits
0 ;east west bits

equ

constant values for the light control

equ
equ
equ
equ

macro

;turn light off

;value for red light
;value for yellow light
;green light

WNe ©

dir,color

set light given by "dir" to color given by "color"

if
local
mvi
lxi
eall
jmp
db
db

exitm
endif
mvi
out
endm

debug ;;print info at console
setmsg,pastmsg

e,wbuff ;;write buffer funetion
d,setmsg

bdos ;;write the trace info
pastmsg

er,if

'&DIR changing to &COLOR$'!

a,color shl dir&bits 3; readied
light 33;sent in proper bit position

(remaining macros are identical to the previous figure,
but each contains trace information similar to "setlite")

Figure 29. Library Segment with Debug Facility.

---
_Page 087_

FFFF

01A9
01CB
01DD

ORG

MACLIB
DEBUG SET
CYCLE: SETLITE
SETLITE
TIMER
SETLITE
TIMER
SETLITE
SETLITE
TIMER
SETLITE
TIMER
RETRY

Figure 30a.

100H
INTER
TRUE

;READY FOR THE DEBUG RUN
;BASIC MACRO LIBRARY
;READY DEBUG TOGGLE

NS , RED
EW, GREEN

EW, YELLOW

EW, RED

NS ,GREEN

NS , YELLOW

CYCLE

Sample Intersection Program with Debug.

Figure 30b.

ehanging
changing
changing
ehanging
changing
changing
changing
changing
changing
changing

to
to
to
to
to
to
to
to
to
to

RED
GREEN
YELLOW
RED
GREEN
YELLOW
RED
GREEN
YELLOW
RED

Debug Trace Printout.

---
_Page 088_

This approach to macro based language facilities provides a simple too] for rapid
development and debugging of programs where high level languages are not available,
but a measure of machine independence is desired. The macros are easy to develop,
and the application programs are simple to write and debug.

9.2. Machine Emulation.

A second application of macro processing is found in the "emulation" of a
machine operation code set which is different from the 8080 microprocessor. In
particular, a machine architecture is selected, based upon an existing or fictitious
operation code set, and a macro is written for each "opcode," taking the general form:

op MACRO d-1,d-2, ..., dn
opeode emulation
ENDM

where "op" jis a mnemonic instruction in the emulated machine and the dummy
Parameters d-l through d-n represent the optional operands required by "op." The
"macro body" includes 8080 instructions which carry-out the operation on the 8080
microprocessor. That is, the instructions within the macro body perform the same
function as the "op" with its arguments on the emulated machine.

Upon completion of the opeode macro definitions, a program ean be written
using these opcodes, which expand to the equivalent 8080 instructions, but perform the
emulated machine operations.

In order to be specific, consider the situation encountered by Nachtflieger
Maschinenwerke, an internationally famous manufacturer and distributor of automated
machining equipment. Though incorporating microprocessors in controlling their equip-
ment, Wachtflieger expects to build a custom LSI processor for their future products.
The processor, called the KDF-10 will be used primarly as an analog sensing and control
element in a larger electronic environment. As a result, the KDF-10 word size must
accommodate digital values corresponding to analog signals of up to twelve bits. In
order to allow computations on these twelve bit values, Nachtflieger engineers are
going to allow a full 16-bit word in the KDF-10, along with a number of primitive
operations on these values. Externally, the KDF-10 will provide four analog to digital
(A-D) input "ports" which can be read by KDF-10 programs, along with four digital to
analog output ports (D-A) which can be written by the program. The KDF-10 will
automatically perform the A-D and D-A conversion at these ports.

Begin forward thinkers, the engineers at Nachtflieger have designed the KDF-10
as a "stack machine," which is similar in concept to the Hewlett-Packard HP-65 hand
held programmable calculator, where data can be loaded to the top of a "stack" of
data elements, automatically "pushing" existing elements deeper onto the stack. Similar
to the Reverse Polish Notation (RPN) of an HP-65, arithmetic on the KDF-10 will be
performed on the topmost stacked elements, automatically absorbing the stacked
operands as the arithmetic is performed. Somewhat simpler than the HP~65, the
designers settle upon the following three-character operation codes for the KDF-10:

SIZ n reserves n 16-bit elements as the maximum size of

the KDF-10 operand stack. This operation code
must be provided at the beginning of the program.

---
_Page 089_

RDM i Reads the analog signal from input port i (0,1,2, or 3)
to the top of the stack, automatically pushing any

WRM o Writes the digital value from the top of the stack
to the D-A output port given by o, (0,1,2, or 3).
The value at the stack top is removed.

DUP The top of the KDF-10 stack is duplicated.

SUM The top two elements of the KDF-10 stack are added,
both operands are removed, and the resulting sum is
placed on the top of the stack.

LSR on Performs a logical shift of the topmost stacked element
to the right by n bits (1,2, .. .,15), replacing the
original operand by the shifted result. Note that
LSR n performs a division of the topmost stacked
value by the divisor Qn

JMP a Branch directly to the program address given by the
label a.

Since the KDF-10 does not exist (except in the fertile minds of Nachtflieger
engineers), the software designers have decided to use the macro facilites of MAC to
emulate the KDF-10 using the 8080 microcomputer.

Figure 31 shows an example of a program for the KDF-10 which was processed
by MAC using the macro library defined by the Nachtflieger software group. In this
situation, the KDF-10 is connected to four temperature sensors which are attached at
strategic places on the machining equipment. The program continuously reads the four
input values from the A-D ports and computes their average value by summing and
dividing by four. This average value is then sent to D-A output port 0 where it is
used to set environmental controls.

Referring to Figure 31, the program begins by reserving a stack of 20 elements,
which is much larger than required for this application (a maximum of four elements
are actually stacked). The program then cycles following "LOOP," where the values
are read and processed. The four operations RDM 0, RDM 1, RDM 2, and RDM 3
read all four temperature sensors, placing their data values in the stack. The three
SUM operations which follow the read operations perform pairwise addition of the
temperature values, producing a single sum at the top of the stack. Since the average
value is desired, the LSR 2 operator is applied to the stack top to perform the division
by four. Finally, the resulting average is sent to the D-A port using the WRM 0
operation code. Control then transfers back to LOOP, where the entire operation is
performed again.

Since Nachtflieger designers are emulating KDF-10's using 8080's, they have
created the macro library file, called "STACK.LIB" as shown in Figure 32. A macro
is shown in this figure for each of the KDF-10 opcodes, starting with the SIZ operator.
In this case, the program origin is set (since this must be the first opcode in the
program), and the stack area is reserved. Note that double words of storage are

---
_Page 090_

012E
013A

Q13E

ee

LOOP:

0156 C32E01

Figure 31.

AVERAGE THE VALUES WHICH ARE READ FROM ANALOG
INPUT PORTS, WRITE THE RESULTING VALUE TO ALL
THE D-A OUTPUT PORTS.

MACLIB STACK ;READ THE STACK MACHINE OPCODES

SIZ
RDM
RDM
RDM
RDM

ALL
SUM
SUM
SUM

SUM
LSR
WRM
JMP

20 “CREATE 20 LEVEL WORKING STACK
0 ;READ A-D PORT 0
1 :READ A-D PORT 1
2 sREAD A-D PORT 2
3 sREAD A-D PORT 3

FOUR VALUES ARE STACKED, ADD THEM UP
;AD3+AD2
; (AD3+AD2)+AD1
; ((AD3+AD2 )+AD1) +ADO

IS AT TOP OF THE STACK, DIVIDE BY 4
2 ;SHIFT RIGHT TWO = DIV BY 4
0 ;WRITE RESULT TO D-A PORT 0
LOOP ;GO GET ANOTHER SET OF VALUES

A-D Averaging Program using "Stack Machine."

---
_Page 091_

Siz macro size
33 set "org" and create stack
loeal stack ;;label on the stack
org 100h ;;at base of TPA
1xi sp,stack
jmp stack ;;past stack
ds size\*2 ;;double precision
stack: endm
dup macro
33 duplicate top of stack
push h
endm
Ps
sum macro
35 add the top two stack elements
pop d 3;;top-1 to de
dad d 33;baek to hl
endm
?
Isr macro len
33 logical shift right by len
rept len ;;generate inline
xra a 3;;clear carry
mov a,h
rar ;;rotate with high 0
mov h,a
mov a,l
rar
mov l,a 3;back with high bit
endm
endm
>
ade0 equ 1080h 3;a-d converter 0
adel equ 1082h 3;a-d eonverter 1
ade2 equ 1084h 3;a-d converter 2
ade3 equ 1086h 3;a-d converter 3
?
dac0g equ 1090h ;d-a converter 0
dacl equ 1092h 3;d-a eonverter 1
dac2 equ 1094h 3;d-a converter 2
dac3 equ 1096h ;d-a converter 3
bd
rdm macro 2e
33 read a-d converter number "?e"
push h 3;;clear the stack
33 read from memory mapped input address
lhld adec&?e
endm
,
wrm macro 2e@
33 write d-a converter number "?ec"
shld dack&?e 3;;value written
Pop h s;restore stack
endm
Figure 32. "Stack Machine" Opeode Macros.

---
_Page 092_

reserved since a 16-bit word size is assumed. The DUP, SUM, and LSR operators
follow the SIZ macro. In each case, the KDF-10's stack top is assumed to be in the
8080's HL register pair. Further, each operation which pushes the KDF-10 stack causes
the element in the 8080 HL pair to be pushed to the 8080 memory area reserved by
the SIZ opeode.

The DUP opcode simply pushes the HL register pair to memory, since the HL
pair is not altered in the 8080 during this operation. In the case of the SUM operator,
it is assumed that the KDF-10 programmer has somehow loaded two values to the
KDF-10 stack. Thus, it must be the case that the HL registers contain the most
recently loaded value, while the 8080 memory stack contains the next-to-most recently
stacked value. The POP D operation loads the second operand to the DE pair in the
8080 CPU, then the topmost value and next to top value are added using the DAD D
operation. The resulting operand goes into the HL register pair, which is necessary
in the KDF-10 emulation, since the top of the KDF-10 stack is located in the 8080's
HL register pair.

The LSR opcode is somewhat more complicated. Since the 8080 does not support
a double precision (16-bit) right shift of the HL register pair, the values must go
through the accumulator. Thus, the LSR macro contains a REPT loop which generates
inline machine code for each right shift. The inline machine code performs the right
shift by first clearing the carry (XRA A), followed by a high order right shift by one
bit (MOV A,H followed by RAR), then by a low order bit shift (MOV A,L followed by
RAR). Note that an intermediate bit may move from the high order byte to the Jow
order byte using the carry between high and low order byte shifts.

Referring to Figure 32, the RDM and WRM operation codes are defined by
"memory-mapped" input/output operations. That is, memory locations 1080H through
1087H are intercepted external to the 8080 microprocessor and treated as external
read operations. Thus, a load from location 1080H/1081H to HL is treated as a read
from A-D device 0, rather than from random access memory. This operation is simple
to perform in the KDF-10 emulation, since all program addresses are assumed to be
below 1000H, and thus any 8080 address bus values beyond 1000H must be memory
mapped I/O. As a result, ADCO through ADC3 correspond to the locations where A-D
values 0 through 3 are obtained. Similarly, the D-A output values which are written
to locations 1090H through 1097H are intercepted as memory mapped output values
which are sent to the D-A converters rather than random access memory. The RDM
instruction is emulated by simply performing an LHLD from the appropriate memory
mapped input address (constructed through concatenation of the dummy parameter).
The HL value is first pushed, since the KDF-10 RDM opcode performs this task
automatically, then the new value is loaded into the HL register pair. The WRM
opeode definition is similar, except the value to write is assumed to reside at the top
of the KDF-10 stack (and thus appears in the 8080 HL register pair). The value is
written to the memory mapped output location, and the value is removed from the
HL pair by restoring HL from the 8080 stack.

In order to see the actual code generated by each of these macros, Figure 33
shows the same averaging program as given in Figure 31, except that the generated
8080 instructions are interspersed throughout the listing file (Figure 33 is the usual
output from MAC, while Figure 31 was generated using the parameter "-M" which
suppresses generated mnemonics). It is worthwhile cross-referencing Figures 31, 32,
and 33 to ensure that the macro expansion processes are clearly understood.

---
_Page 093_

0100+
0100+312E01
0103+C32E01
0106+

O12E+ES
012F+2A8010

0132+E5
0133+2A8210

0136+E5
0137+2A8410

013A+E5
013B+2A8610

013E+D1
013F+19

0140+D1
0141+19

0142+D1
0143+19

0144+AF
014547C
0146+1F
0147+67
0148+7D
0149+1F
014A+6F
014B+AF
014C+7C
014D+1F
014E+67
014F+7D
0150+1F
0151+6F

0152+229010
0155+E1
0156 C32E01

oo we we we

LOOP :

Figure 33,

AVERAGE THE VALUES WHICH ARE READ FROM ANALOG
INPUT PORTS, WRITE THE RESULTING VALUE TO ALL
THE D-A OUTPUT PORTS.

MACLIB
SIZ
ORG

STACK ;READ THE STACK MACHINE OPCODES
20 ;CREATE 20 LEVEL WORKING STACK
100H

SP,?70001

220001

20\*2

0 sREAD A-D PORT 0

H

ADCO

1 ;READ A-D PORT 1

H

ADC1

2 ;READ A-D PORT 2

H

ADC2

3 ;READ A-D PORT 3

H
ADC3

ALL FOUR VALUES ARE STACKED, ADD THEM UP

SUM
POP
DAD
SUM
POP
DAD
SUM
POP
DAD

SUM IS A

LSR
XRA
MOV
RAR
MOV
MOV
RAR
MOV
XRA
MOV
RAR
MOV
MOV
RAR
MOV
WRM
SHLD
POP
JMP

3 AD3 +AD2

; (AD3+AD2 ) +AD1

; ((AD3+AD2 ) +AD1 ) +ADO

4 OU OU OUD

TOP OF THE STACK, DIVIDE BY 4
;SHIFT RIGHT TWO = DIV BY 4

‘ee ~ ee “

~~

;WRITE RESULT TO D-A PORT 0

myer ro Prr Po PpPYy
Q r re FC Ff FP

:

;GO GET ANOTHER SET OF VALUES

Averaging Program with Expanded Macros.

---
_Page 094_

A particular problem arose at Nachtflieger MW, however, which had to be
rectified: although programs could be effectively written for the KDF-10 computer
using the 8080 emulation, they could not be effectively debugged. The program of
Figure 33, for example, could be tested under the CP/M debugger (see the CP/M DDT
Users Guide), but required monitoring and tracing at the 8080 machine code level. It
became clear that higher level debugging tools were necessary.

As a result, Nachtflieger designers added several "pseudo opcodes" which allow
debugging traces. The opcodes can be interspersed in the program, and selectively
enabled and disabled depending upon the debugging needs. In production, all debugging
traces would, of course, be disabled resulting only in absolute port I/O. The additional
debugging opcodes are listed below.

PRN msg _ Print the message given by "msg" at the debugging
console whenever the print trace is enabled. The
message must be enclosed in broken brackets.

DMP Print the value of the top element in the KDF-10
stack (in hexadecimal).

TRT t Set machine code trace option to true. Each time
a KDF-10 machine operation is executed, the opcode
is printed, followed by the (approximate) KDF-10
machine code address, followed by the top two
elements of the KDF-10 stack, in the format:

OPC oploc top top'

where OPC is the opcode, oploc is the location, top
is the top element, and top’ is the second to the
top element, all in hexadecimal notation.

TRF t Disable the machine code trace. Only the KDF-10
instructions which physically appear between the TRT
and TRF opcodes are shown in the trace.

TRT p Enable the print/read trace. PRN opcodes which
follow produce output at the debugging console,
and are otherwise treated as comments. Further,
RDM and WRM opcodes prompt and display data
at the debugging console.

TRF p Disable the print/read trace. Only the PRN, RDM,
and WRM instructions which physically appear
between TRT and TRF interact with the console.

The convention is also taken that the traces are initially disabled at the beginning of
the program, and must be explicitly enabled with TRT opcodes.

Figure 34 shows the averging program of Figure 31 with interspersed debugging
statements. Note that the opcodes TRT t and TRT p are executed at the beginning

---
_Page 095_

d000
G183
G12E
B1FS
@22C
$26A
@2A5
G@2A8
Q2E3
62E6

A316
A327
%33B
833E

837B
O3B1
03B4
03EE
O3F1

on on

AVERAGING PROGRAM WITH INTERSPERSED DEBUG CODE

MACLIB
SIZ
TRT
TRT
PRN

LOOP: RDM

Figure 34,

en

on

DMP
RDM
DMP
RDM
DMP
RDM
DMP
PRN

ALL FO
SUM
DMP
SUM
DMP
SUM
PRN
DMP

DSTACK

T

P

;READ THE STACK MACHINE OPCODES
;CREATE 2@ LEVEL WORKING STACK
;MACHINE CODE TRACE ON

+PRINT TRACE ON

<TRACE FOR AVERAGING PROGRAM>

;READ A-D PORT @
;WRITE TOP OF STACK
;READ A-D PORT 1
;WRITE TOP OF STACK
;READ A-D PORT 2
;WRITE TOP OF STACK
7;READ A-D PORT 3
;WRITE TOP OF STACK

<FOUR VALUES HAVE BEEN READ>

UR VALUES

<VALUES

ARE STACKED, ADD THEM UP
sAD3+AD2

;WRITE FIRST SUM

: (AD3+AD2) +AD1

sWRITE SECOND SUM

+ ((AD3+AD2) +AD1) +AD@
HAVE BEEN ADDED>

;WRITE SUM OF VALUES

SUM IS AT TOP OF THE STACK, DIVIDE BY 4

LSR
PRN
DMP
WRM
BRN
XIT

Averaging

;SHIFT RIGHT TWO = DIV BY 4

<AVERAGE VALUE CALCULATED>

q
LOOP

;WRITE AVERAGE VALUE

;WRITE RESULT TO D-A PORT @
;GO GET ANOTHER SET OF VALUES
sEMIT EXIT CODE

Program with Debuaqging Statements,

---
_Page 096_

of the program, thus enabling all trace options throughout the execution. The PRN
statement above the LOOP label prints the initial sign-on, while the DMP statements
after each read operation give the value of the A-D port. Upon completion of the
four element read, the PRN opcode is used to indicate this fact. Each SUM operator
is followed by a DMP opcode which shows the current sum. Finally, the PRN and
DMP opcodes are used to display the final average value which is being sent to D-A
port 0. The "XIT" opcode shown at the end of the program will be introduced in the
paragraphs which follow.

Figure 35 shows the execution of the averaging program under DDT. Note that
the program headings appear at the points in the program where PRN opcodes are
placed. Further, the console is prompted for input in the case of an RDM opcode
(giving the absolute memory mapped input address in decimal), while the WRM instruction
produces a "D-A OUTPUT . ." message which shows the absolute memory mapped
output address as wel] as the data which is written. The opcodes are also traced
showing the opcode mnemonic, address, and top two stacked elements. The "RDM"
trace at the beginning, for example, shows the instruction address 01AD, which is in
the range of the first RDM of Figure 34 (012E and 01EF), and is followed by the two
values 0111 (i.e., the value just read) and C21D ("garbage" value, since only one element
is stacked), The trace is easily followed at the KDF-10 level, showing each value
which is read-in, and the operations performed upon these values. Upon completion
of the debugging process under CP/M, the TRT opcodes are removed and the program
is reassembled, leaving only the 8080 instructions required in the production machine.
Nachtflieger systems engineers then take the resulting program and test its operation
in a hardware environment.

Forward thinking though they were, Nachtflieger engineers quickly realized that
the KDF-10 design had a number of deficiencies due to the paucity of arithmetic
operators and the total absence of conditional branching instructions. Further, there
was no provision for variable storage other than the stack. Thus, the KDF-11 naturally
evolved from the KDF-10, which incorporates these features. In particular, the operation
codes of the KDF-11 include:

DCL v,n Declare (i.e., reserve) storage for a variable by
the name v, with optional size n. If n is omitted,
then n = 1 is assumed. All DCL opcodes must fol-
low the XIT opeode given below.

LIT e¢ Load the value of the literal constant ec to the top
of the KDF-11 stack.

VAL v,i,e Load the value of the variable v optionally indexed by
the variable i with the optional constant offset c.
VAL V loads the value of V to the top of the stack,
VAL V,I loads the value located at the address of
V plus the index value contained in I, while
VAL V,I,3 loads the value at location V plus the
index I, plus the constant index 3. In all eases, the
value is placed at the top of the KDF-11 stack.

STO  v,i,c Similar to the VAL operator, the STO opcode stores
the value obtained from the KDF-11 stack to the

---
_Page 097_

ddt aver.hex
DDT VERS 1.4

NEXT PC
0406 0000
-g100

TRACE FOR AVERAGING PROGRAM
A-D INPUT AT 4224 111

RDM 01AD 0111 C21D

(TOP)= 0111

A-D INPUT AT 4226 222

RDM 0255 0222 O111

(TOP)= 0222

A-D INPUT AT 4228 555

RDM 0293 0555 0222

(TOP)= 0555

A-D INPUT AT 4230 444

RDM 02D1 0444 0555

(TOP)= 0444

FOUR VALUES HAVE BEEN READ
SUM 0312 0999 0222

(TOP)= 0999
SUM 0329 OBBB 0111
(TOP)= OBBB

SUM 0340 0CCC C21D
VALUES HAVE BEEN ADDED

(TOP)= 0OCC
AVERAGE VALUE CALCULATED
(TOP)= 0333

D-A OUTPUT AT 4240 0333
WRM 03DC 793B C21D
A-D INPUT AT 4224

Figure 35. Sample Execution of "Average" using DDT.

---
_Page 098_

address given by v, plus the optional index i, plus
the optional constant index given by c. The top ele-
ment of the KDF-11 stack is removed.

DIF The DIF opcode subtracts the top element of the KDF-11
stack from the next-to-top element of the stack,
and replaces both operands by their difference.

GEQ a The GEQ opcode tests the next to top element
(top') against the top of stack element (top),
and branches to the label given by "a" if top’
is greater than or equal to top. If not, program
control continues to the next opcode in sequence.

BRN a The BRN instruction replaces the JMP instruction
in the KDF-10 architecture to allow complete
separation of the KDF-11 and 8080 machines.

Figures 36a, 36b, 36c, and 36d give the macro library which was constructed by the
Nachtflieger software group for KDF-11 machine emulation. Note that over half of
the macro library implements trace and debugging functions (Figures 36a and 36b)
while the remaining components implement the KDF-11 opcodes themselves. A brief
description is given below for each major section of this macro library, called
"DSTACK.LIB," before giving an example of its use.

Figure 36a shows the first portion of the macro library. Since this portion of
the library is principally concerned with debugging functions, it begins with CP/M
system calls, function numbers, and equates for non-graphie characters, similar to the
examples given earlier. Although these values are not necessary for operation of the
KDF-11, they are necessary for the debugging functions which operate when the TRT
opcode is in effect. Following the CP/M equates, the "toggles" DEBUGT and DEBUGP
are set to false (0 value), which reflect the conditions of the debugging switches given
by TRT and TRF. When DEBUGT is true (1 value), machine operation codes are
traced. Similarly, when DEBUGP is true, PRN, RDM, and WRM operations interact
with the console.

The PRN macro shown in Figure 36a (left), for example, produces an inline
message with a call to CP/M to write the message whenever the DEBUGP toggle is
true; otherwise the PRN produces no generated code.

The UGEN macro which follows PRN in Figure 36a is invoked the first time
that the debugging subroutines are required by trace or print/read opcodes. When
invoked, the UGEN macro produces several! inline subroutines which are used throughout
the debugging process. If no trace or print/read functions are invoked during the
assembly, UGEN is not invoked and thus no inline subroutines are included for debugging.
If UGEN is invoked, the subroutines shown below are included inline:

@CH writes a single ASCII character to the console

@NB writes a single half-byte (nibble) to the console

@QHX writes a full hexadecimal byte value at the console

@AD writes a full address (double byte) value with preceding
blank

@IN reads a hexadecimal value from the console to HL

---
_Page 099_

ee
wm

we
oD

macro library for a zero address machine
Peso SeP REESE SE SESE RASS ERE SSLESA SEP E SEE LES SS

\*

begin trace/dump utilities \*

KERKKKEKKEKEKKEKEREKEKEEKEEKHEEERKEKAKERERKEKEE

egu
egu
equ
equ
equ
equ
equ
equ

set
set

macro
print message ‘pr

if

local
jmp

db
db

push

lxi
mvi

call

pop

endif
endm

macro

@0ash ;system entry

1 sread a character

2 swrite character

9 swrite buffer

108h ;transient program area
1lagh ;data area

8dh ;carriage return

Bah jline feed

B
C1)

ytrace debug set false
pprint debug set false

pr

“ at console

debugp ;;print debug on?

pmsg ,msg tzlocal message

pmsg yaround message

cr,lf yreturn carriage

“SPRS” ;;literal message

h 37Save top element of stack

d,msg ;rlocal message address

c,wbouff ;;write buffer “til $
’
,
,

om mm ws

bdos mprint it
h jrestore top of stack
jend test debugp

generate utilities for trace or dump

local

jmp

;7write

mov
mvi

jmp

;rwrite

adi
daa
aci
daa

jmp

rewrite
push

rre
tre

psub

psub ;3jump past subroutines
character in reg-a

e,a

c,wchar

bdos ;3return thru bdos

nibble in reg-a
96h

48h

@ch sreturn thru @ch

hex value in reg-a
psw 37Save low byte

Figure 36a.

<
a
nw

@in®@:

@inl:

rre
rec

ani 6fh ;
call @nb :
pop psw
ani @fh
jmp @nb

smask high nibble

s7print low nibble

;z;weite address value in hl

push h “ppsave-vaine

mvi a, ~ leading blank
call @ch ;ahead of address
pop h shigh byte to a
mov a,h
push h
call @hx
pop h
mov a,l zzylow byte

jmp @hx ggwrite low byte

om em oy

scopy back to stack
;ewroite high byte

w

;3cead hex value to hl from console

mvi a,” \* ;z;leading space

call @ch zto console

lxi h,@ pstarting value

push h ssave it for char read
mvi c,rchar ;3;read character function
call bdos 37vead to accumulator

pop h ¢3;value being built in hl
sui ;7;normalize to binary

cpi 16 37decimal?

je @inl pycarry if @,1,.
May be hexadecimal a,...,f

sui “A\*-°O°-18

cpi 16 37a through f£?
rnc spreturn with assumed cr
;zin range, multiply by 4 and add

rept 4
dad h
endm

ora 1
mov l,a
jmp ainb

ee te ow

pshift 4

zadd digit
yand replace value
3for another digit

em oe on

macro
redef to include once

endm

ugen z7generate first time

endm
RAKE TITRE AEE EKEREAREAEEEEKKEKEEEKEN

\* end of trace/dump utilities \*

Stack Machine Macro Library.

---
_Page 100_

wate fT Ne Ne

\* begin trace(only) utilities \*
(eRe RE SEER SRS EERE RSE ESSE SERS EEE RSE REO LE 2

race macro code ,mname
: trace macro given by mname,
; at location given by code
local Ppsub
ugen t7generate utilities
jmp psub
@tl: ds 2 s3temp for reg-l
at2 ds 2 ;3temp for reg-2
@tr ;;trace macro call
3 be=code address, de=message
shld atl ;7store top reg
pop h se3return address
xthl p3reg-2 to top
shld at2 ;7Store to temp
push psw ;7save flags
push le) ;;Save ret address
myvi c,wbuff ;;print buffer func
call bdos ripcint macro name
DOD h ;;code address
call @ad ;7printed
lhid atl ritop of stack
call Rad trprinted-
lhld a@t2 ;3top-1
call Qad ;7printed
pop Dsw ;;flags restored
90D da p3return address
lhid at2 rp ptop-1l
push h :;restored
push d s3return address
lnld @tl zstop of stack
ret
psub: ;;past subroutines
trace macro c,m
tt redefined trace, uses atr
local pomsg ,msg
3mo pmsg
-mSG: db cr,lf prer,lf
do “EMS” ;yMac name
pmsq:
1xi b,c ;3cOde address
lxi d,msq ;3Macro name
call Qtr y7to trace it
endm
tf back to original macro level
trace code ,mname
endm
ert macro £
tt turn on flag “f£"
debugéef set 1 s3Drint/trace on
endm
trf macro f
tf turn off flag “f"
debugéf set a p¢trace/print off
endm
?tr macro m
iG

check debugt toggle before trace

if debugt

trace 3S,m

endm
KRHA KKK KKaK KK KEKK KKK K KKK
\* end trace (only) utilities \*

Figure 36b.

we te Gusev
we}

~~ ee DD
we
’ a

@dmé:

>
ee
ow

G
a

we Qe Ue
we Be we
ue}

msg:

pmsq:
active

\* begin dump(only) utilities \*
PES R SAS SE REESE SESS S SE SESE SAE R RE RR RE RSA SS SE
macro vname ,n

dump variable vname for
n elements (double bytes)

local psub ;3past subroutines
ugen 379en inline routines
jmp psub ;;past local subroutines

;7dump utility program
de=msg address, c=element count
hl=base address to print

push n s;base address

push b ;selement count

mvi c,wbuff ;;wRite buffer func
call bdos s3message written

pop b s;¢recall count

pop a ;3recall base address
mov a,c s;end of list?

ora a

Cz pzreturn if so

dcr c ;3decrement count

mov &, ir 4 peee-—+ bt en—trew)

inx h

mov d,m simnext item fhigh)
inx h :3ready for next round
push h s3save print address
push b ;3Save count

xchg s;3:data ready

call @ad siprint item value
jmp @dmo 373for another value

37dump top of stack only

prn < (top) => 33" (TOP)=”"
oush h

call @ad ssvalue of hl
pop n ;itop restored
ret

macro ?v,?n

tcedefine dump to use ddm utility
local pmsg ,msg

special case if null parameters

if nul vname

dump the top of the stack only

call Qadt

exitm

endif

otherwise dump variable name

jmp pmsg

db er,lf szerle

db “g?Vv=S° :;message

adr Vv ;;hl=address

set i) ;;Cclear active flag
Llxi d,msg rimessage to print

if nul ?n ;;use length 1

mvi c,l

else

mvi 2,?n

endif

call adm yito perform the dumo
endm ;7end of redefinition
dmp vname ,n

endm

Rec eeESSeRE RE RE RASSAE SSSR RSE SAR EEE REPS EEE Sef
\* and dump (only) utilities, \*

Stack Machine Library (Con't).

---
_Page 101_

oes OD on UD

N
-_

om

save

.

save

active

rest

:

active

clear

active

ctive

oY

.
,

,

del

.
’

vname:

va ae
oT oon

°
e

\* begin stack machine opcodes \*
RMR MIRAMAR KKK EKER KER

set (] sactive register flag

ry
TD seen
Lis}

macro size

org tean s7set to transient area
create a stack when "xit" encountered
set size :3save for data area
Lxi sp,stack

endm

macro
check to ensure “enter” properly set up

if stack gris it present?

endif

macro s3redefine after initial reference
if active s;selement in hl

push h sisave it

endif

set 1 s7set active

endm

save

endm

macro
restore the top element

if not active

pop h s3recall to hl
endif

set ] simark as active
endm

Lom]
TJ ow ee
on D> ew oe

:
,

macro
clear the top active element
rest syensure active
set A s;cleared

endm

macro vname,size
label the declaration

if nul size

ds 2 yyone word req’d
else

ds size\*2 ;;double words
endm

macro val

load literal value to top of stack
save ssave if active
Lxi h,val sload literal

Ptr lit

endm

.
,

macro base,inx,con

load address of base, indexed by inx,
with constant offset given by con
save sspush if active

if nul inx&con

xi h,base ;;address of base
exitm rrsimple address
endif

must be inx and/or con

if nul inx

Lxi h,con\*2 ;;constant

else

ihld inx z3index to hl

dad h 3;double precision inx
if not nul con

Lxi d,con\*2 ;;double const
dad d ssadded to inx
endif s7mnot nul con
endif simul inx

lxi d,base ;;ready to add

dad da ;;basetinx\*2+con\*2
endm

macro b,i,c

get value of btitc to hl

check simple case of b only

if nul ifc

save :rpush if active
lhld b szload directly
else

"adr" pushes active registers
adr b,i,e¢ saddress in nt
mov e,m slow order byte
inx h

mov da
xchg
endif

?tr val
endm

are

thigh order byte
sback to hl

in

jtrace set?

macro b,i,c
store the value of the top of stack
leaving the top element active
if nul ise
rest
shld b
else
adr b
pop da
mov m,e
h
m

sactivate stack
;stored directly to b

svalue is in de
slow byte

inx
mov
endif
clear s3mark empty
?tr sto r3trace?
endm

shiah byte

os
Me,
.

Figure 36¢e. Stack Machine Library (Con't).

---
_Page 102_

sum

we
ot os
nD ve

Me
n
at oe

oOo
Owes
on OM on oe

c
wares
1 LQ ve

macro
rest sitestore if saved
add the top two stack elements

pop d yrtop-l to de

dad d ssback to hl

Ptr sum

endm

macro
compute difference between top elements
rest yrestore if saved

pop stop-1 to de

mov ;top-l low byte to a
sub slow order difference
mov gback to l

mov ;top-1 high byte

sbb thigh order difference
mov h,a sback to h

carry flag may be set upon return

Ptr aif

endm

o

ou
.
DOHA Do

ee ee Ok on te on ee en

macro len

logical shift right by len

rest szactivate stack
rept len :7generate inline
xra a ;7clear carry

mov a,h

tar :3rotate with high @
mov h,

mov a
rac

mov la
endm

endm

pback with high bit

macro lab

jump to lab if (top-1) is greater or

egual to (top) element.

dif scompute difference

clear -sselear active

?tr geq

jnc lab :3no carry if greater
jz lab :3zero if equal

drop through if neither

endm

macro
duplicate the top element in the stack
rest :7ensure active

Push h

Ptr dup

endm

macro addr
branch to address
jmp addr

endm

Figure 36d. Stack Machine

xit

stack:

adc?
adcl
adc2
adc3
;

dac8
dacl
dac2
dac3

:
?

rwtrace macro

oe oe ae

Library

macro
?tr xit getrace on?

jmp (1) s3restart at 0000

org data 373Start data area

ds @stk\*2 s:sobtained from “siz"
endm

RHEE AERA ARR EREEREREKEKERERKEEKEEREE

bl Memory mapped i/o section \*

REAR RRERRARERREKKKEHEERAERERRRERERERHEAE RRR

input values which are read as if in memory
equ 1688h ya-d converter
equ 1882h ya-d converter
equ 1884h 3a-d converter
equ 1086h sa-d converter

Sannm

equ 1898h sd-a converter
equ 1892h :d-a converter
equ 1094h ;d-a converter
equ 1#96h 3d-a converter

aanm

msg,adr

read or write trace with message
given by “msg" to/from "adr"

prn <msg at adr>

endm

macro ?c

read a-d converter number "“?c"

save ssclear the stack

if debugp ;;stop execution in ddt
rwtrace <a-d input >,% adc&?c

ugen srensure @in is present
call @in sivalue to hl

shld adc&?c ;;ssimulate memory input
else

read from memory mapped input address
lhld adc&?c

endif

Ptr cdm prtracing?

endm

macro ?c

write d-a converter number "?c"

rest srrestore stack

if debugp ;s:trace the output
rwtrace <d-a output>,% dac&?c

ugen stinclade subroutines
call @ad srwrite the value
endif

shld dac&?c

Ptr wrm srtracing output?
clear :7;remove the value
endm

KKK KKK KARE REAREKERRKKEKEEREKAEKKREAKKKKEK

\* end of macro library \*
RRR KKK EERE KEE KARE REEEKEKERRKRRAKEE

(Con't).

---
_Page 103_

Upon including these subroutines, UGEN then redefines itself (see lower right of Figure
36a) to an empty macro body so that the subroutines will not be included upon
subsequent invocations of UGEN. This ensures that the inline subroutines will only be
included once, and only if they are required by the debugging macros.

Referring again to Figure 36c, the SIZ macro is similar the opcode defined for
the KDF-10, except that the SIZE of the stack is saved for later declaration in the
data area (see the XIT opcode), The SAVE and REST macros are used throughout the
opcode macros to save and restore the HL register pair, based upon the ACTIVE flag.
The CLEAR macro, however, is used to mark the top element of the KDF-11 stack
as deleted.

Continuing with Figure 36c¢ (left), the DCL macro simply sets up the variable
name VNAME as a label, and follows the label by a DS which reserves the specified
number of double words. The DCL opcodes must all occur at the end of the KDF-11
program, following the XIT opcode.

The LIT opeode is emulated with a macro which first SAVEs the stack top
(possibly generating an HL push). The literal value is then loaded directly into the
HL register pair. Note that the ACTIVE flag is set upon completion of this macro,
since SAVE always marks HL as active.

The ADR macro in Figure 36e (right) is a utility macro which is used in the
VAL, STO, and DMP opcodes to build the address of a particular variable (with optional
variable and constant offsets) in the HL register pair. Based upon the optional
parameters, ADR either loads the base address directly to the HL pair, or constructs
the address using HL and DE for indexing. Thus, the invocations of ADR shown to
the left below produce the machine code to the right below.

ADR X LXI H,X
ADR X,I LHLD I
DAD H
LXI D,X
DAD D
ADR X,1,3 LHLD I
DAD H
LXI D,6
DAD D
LXI D,X
DAD D
ADR X,,3 LXI H,6
LXI D,X
DAD D

thus leaving the final address for the optionally indexed variable in the HL register
pair. Note that the code within the ADR macro could be improved slightly in the
case that a constant offset is provided. That is, the invocations to the left below
could produce the machine code shown to the right below by redefining the ADR
macro.

---
_Page 104_

ADR X,I,3 LHLD I

LXI D,X+6
DAD D
ADR X,,3 LXI H,X+6

It is a worthwhile exercise for the reader at this point to redefine ADR to generate
this improved machine code sequence.

The VAL and STO macros are shown in Figure 36c (right) which load a variable
value to the stack, or store the top of stack value to memory, respectively. Note
that ADR is used to construct the address of the variable whenever optional indexing
is specified. Otherwise, an LHLD or SHLD is used to directly access the variable.
Again, slight improvements in generated code could be obtained when only a constant
offset is provided with no variable index.

Note that the opcodes LIT, VAL, and STO all end with an invocation of the
?TR macro which, as discussed above, checks the DEBUGT flag. If true, the ?TR
macro invokes TRACE with the machine code address and opcode name for display at
the debugging console. The ?TR macro invocation produces no machine code trace
when DEBUGT is false.

Figure 36d contains a listing of the remainder of the "DSTACK.LIB" macro
library. The SUM opcode shown on the left first invokes REST to ensure that the HL
register pair contains the topmost KDF-11 element. The second to top element is
then loaded to the DE pair and added to HL, producing an active KDF-11 element in
HL. Note that ACTIVE is true at this point, since REST always leaves the flag set
to true.

The DIF opeode definition is similar to SUM, except the 8080 accumulator is
used to compute the 16-bit difference between the top two KDF-11 stacked elements.

Referring to Figure 36d (left), the LSR macro defines the KDF-11 logical shift
right operation. The REST macro is first invoked to ensure that HL is active, followed
by a repetition of the machine code required to perform a 16-bit right shift of the
HL register pair. In the case of a long shift, there will be a considerable amount of
inline machine code for the operation. Thus, it is a useful exercise for the reader to
redefine LSR so that it generates an inline subroutine to perform the shift operation
for values of LEN which are sufficiently large to warrant the subroutine call. Although
this will require a subroutine set up and call, the amount of generated code could be
reduced significantly for programs which make heavy use of the LSR operator.

The GEQ macro follows the LSR definition, and allows conditional branching to
the specified label address. GEQ begins by computing the difference between the top
two elements of the KDF-11 stack which has the side-effect of setting the 8080 carry
bit if the next to top element exceeds the top element in the KDF-11 stack. Note
that the ?TR macro eventually leads to the @TR subroutine where the status flags
(including the carry condition) are saved and restored. Otherwise, GEQ could not
generally count on the condition of the carry flag. Further, the 8080 A register
contains the least significant difference between DE and HL, hence the ORA H produces
a zero result if the difference is zero. To be complete, the KDF-11 should have a

---
_Page 105_

complete range of conditional tests, allowing tests for equality (EQL), inequality (NEQ),
less-than (LSS), greater-than (GTR), and less-than-or-equa] (LEQ). Although Nachtflieger
designers intend to include these opcodes in the KDF-12, it may be a worthwhile
exercise for the reader to implement these additional macros.

The DUP opcode in Figure 36d (bottom left) first ensures that the HL register
pair is active, then duplicates this value by pushing the HL pair to the 8080 stack,
thus emulating a KDF-11 stack push operation. Note that the HL pair is active at
the end of the DUP macro due to the invocation of REST.

The BRN and XIT macros follow GEQ in Figure 36d. The BRN macro simply
translates to a jump instruction in the 8080 while the XIT is slightly more complicated.
The XIT macro first invokes the ?TR macro to check for machine code tracing. A
"JMP 0" is then emitted corresponding to a system restart in both CP/M and the
emulated KDF-11 machine architecture. The XIT macro then produces an "ORG"
statement which restarts the assembly process in the data area of the emulated
environment (1000H, or 4096 decimal). The area reserved for the stack is then set
up (recall that the SIZ macro saves the value of SIZE), followed by the declaration
of the label "STACK" at the base of this reserved area. Referring back to Figure
36c (middle left), note that the SAVE macro includes the statement sequence

IF STACK 33is it present?
ENDIF

which ensures that both the SIZ and XIT macros have been included in the assembly.
If the XIT macro had not been included, then the label "STACK" would not appear
(unless used in the KDF-11 program), and the "IF STACK" test would produce an
undefined operand (U) error. Further, if the XIT operator had been used, but the SIZ
had not, then the statement "DS SIZ\*2" within XIT would produce an undefined operand
message. Although these tests are by no means complete, they will detect the most
common errors.

Figure 36d (right) also contains the definitions of both the RDM and WRM
opcodes, based upon the memory mapped input/output addresses defined by ADCO
through ADC3 for the A-D ports, and DACO through DAC3 for the D-A ports. The
RWTRACE (Read/Write Trace) macro is included for tracing the RDM and WRM macros
when DEBUGP is true. The MSG argument corresponds to either "A-D INPUT" for
the RDM opcode, or "D-A OUTPUT" for the WRM opcode. The ADR argument
corresponds to the absolute decimal address where the memory mapped input/output
is taking place. Thus, RWTRACE simply constructs a trace message from its two
argments and passes this message to PRN for display at the debugging console.

The RDM macro reads the port given by the argument "?C" (0,1,2, or 3). The
HL register pair is pushed, if necessary, by the SAVE macro (leaving the active flag
set for the RDM). RDM then generates an invocation of the RWTRACE macro to
produce the trace message. Note that the argument % ADC&?C produces the numeric
value of one of ADCO, ADC1, ADC2, or ADC3 which is included in the trace message.
If the % were omitted, only the name, not the value, of the input port address would ~
be printed. Following the output message, UGEN is invoked to ensure that the utility
subroutines have been included inline. The call to @IN allows the programmer to type
a hexadecimal value for the simulated A-D input value, which is subsequently stored
to memory and left in the HL register pair (with ACTIVE true). If DEBUGP is not

---
_Page 106_

set, then the RDM macro simply loads the HL register pair from the appropriate
memory mapped input location. Finally, RDM invokes ?TR to check for possible opcode
tracing.

The WRM opcode is similar to the RDM opcode, except that the REST macro
is first invoked to ensure that the HL registers contain the top element of the KDF-11
stack. This value is then displayed at the debugging console if DEBUGP is true, and
then sent to the appropriate memory mapped output location.

One particular application of the emulated KDF-11 machine shows the power
of this particular instruction set. As a small part of a machine control system, a
KDF-11 processor monitors the machine tool head motion. Nachtflieger engineers
eonnect A-D port 0 to a KDF-11 processor which reads the instantaneous velocity of
the tool head at 1 millisecond (ms) intervals. The velocity is provided at the A-D
port in micrometer (um) increments, and the processor is synchronized with the input
so that it halts until the 1 ms interval has elapsed. Nachtflieger engineers also
guarantee that the tool head is in motion for no more than 100 ms before stopping.
Thus, with no variations in velocity, if the tool moved at the constant rate of 256
um/ms over 50 intervals of 1 ms each, the total distance travelled by the tool is

256 um/ms \* 50 ms = 1280 um = 1.280 mm

During its travel, however, the instantaneous velocity of the too] head varies
according to the roughness of the cut, wear on the parts, and start/stop intervals.
Nachtflieger uses the data collected during a particular cut to monitor these factors,
and displays machine operator information in both digital and analog forms. A primary
function of the KDF-11 processor in this particular case is to collect the instantaneous
velocities during a single cut, and hold these values for analysis as the tool returns
to its starting postition. Figure 37 shows a KDF-11 program which includes the data
collection phase, as well as an analysis phase described below,

The data collection phase of Figure 37 occurs between the labels MOVE? and
COMP, while the analysis phase is found between labels COMP and ENDF. Note that
the program is bounded by the SIZ operator at the beginning, along with the XIT
operator at the end, followed by DCL opcodes which reserve data areas. This particular
program also includes debugging PRN, DMP, TRT, and TRF opcodes for checking out
the program.

Referrring to the DCL statements at the end of Figure 37, the "vector" V is
declared with length 100 (double bytes), which will hold the collected velocities, while
I and X are temporary values used during the collection and analysis phase. The
variable TOTAL is a result produced by the analysis as discussed below.

The program collects data by performing the following steps. The variable I is
first initialized to 0, corresponding to the first velocity V(0). The program then
examines the A-D input port for the first non-zero velocity, waiting for the tool head
to begin its travel. When the first non-zero velocity is read, the collection process
proceeds by storing the first value at V(0). The index value I is then moved along as
data items are read, with values placed into V(1), V(2), and so-forth, until a zero value
is read, indicating the tool has ended its travel.

Referring to Figure 37, note that the KDF-11 opcodes listed before the label
MOVE? initialize the index I by loading a literal 0 value to the KDF-11 stack, followed

---
_Page 107_

81D3
B1E8

O1E8
O21A

822A
A258
629C
829!
B2AC
B2AF
62B3
@2B5
62B8
B2BB
O2BF
g2CC
O24
O2F7

B2rAa
O31A

Ds.
<>}
>
io}

a=

COMP;

MACLIB
S12
TRI
TRY
PRN
Liv
STO
TRE

DS'TACK :STACK MACHINE SIMULATION

5@ 350 LEVEL STACK

P ;TURN ON PRN TRACE

T ;TURN ON CODE TRACE
<COMPUTATION OF TOOL TRAVEL DISTANCE
6 sINITIALIZE INDEX

I :1=6

T ;TURN CODE ‘TRACE OFF

LOOK FOR STARTING MOTION (NON ZERO VALUE)
;READ A-D CONVERTER FOR NON ZERO

RDM
sto
VAL
LIT
GEO
BRN

PRN
DMP
VAL
sto
VAL
LI
SUM
Sto
Ur
VAL
GEQ
RDM
STO
BRN

PRN
DMP

4)

x ;HOLD TEMPORARILY
x ;RELOAD FOR TEST
1 7X GEQ 1 TEST
READ +X GEQ 1?

MOVE? ;RETRY IF NOT

<STORE FIRST/NEXT VALUE>
;LOAD FIRST/NEXT VALUE

ai ;STORE TO THE ITH ELEMENT
; INCREMENT Tf

xX DPA

sI4+]

s1=Itl

38, FOR @ GTR X TEST
;ZERO VALUE READ?
sCOMPUTE DISTANCE IF @
sREAD ANOTHER DATA ITEM
;SAVE IT IN X

sTO STORE AND TEST

OMP

Pi

HexUusenx a

EAD

<VALUE ARE LOADED>

V,18

NOW COMPUTE DISTANCE TRAVELLED BY TOOL

Figure 37.

a32D
@35F
B3A3

83C8
83CC
@3DD
O3DF
O3EA
O3EC

O3EF
O3F2
O3F6
63F8
O3FB

O3FE
043A

LIe
DUP
sto
STO

GETNXT:

ENDF:

on en

DMP
DMP
DMP
LIvt
VAL
GEQ

nov
VAL
VAL
SUM
LSR
VAL
SUM
sTO

VAL
Lit
SUM
STO
BRN

PRN
DMP
VAL
WRM
xiv

i)

I

TOTAL
PRN

I

TOTAL
<V,1>,2
@

vV,t
ENDF

AT END OF INTERVAL,

V,1
vV,I,1

TOTAL

TOTAL

I

I
GETNXT

<END OF
TOTAL
TOTAL

4)

DATA AREA

DCL
DCL
DCL
DCL

Program for Tool Travel Computation.

I
x
V,188
TOTAL

sTWO ZEROES

;1=8

sTOTAL=6

<COMPUTING NEXT INTERVAL>

3;ZERO AT END
;AT END?
72 GEQ X(I)?

COMPUTE NEXT TRAPEZO

3V(1T),V(T+1)
sV(L)4+V(I+1)

7 (V(T)+V(I+1))/2
s;READY TOTAL

s TOTAL=TOTAL+TRAPEZOID
:BACK TO SUM

sI=I+1

;BACK TU I

COMPUTATION>

;LOAD FOR D-A OUTPUT
;WRITE D-A PORT

; INDEX

; TEMPORARY

; VELOCITY VECTOR
;TOTAL DISTANCE

---
_Page 108_

by a store into the variable I. In order to follow these operations, the TRT P and
TRT T traces are enabled. Note, however, that the TRF T opcode stops the machine
code trace immediately before the MOVE? label.

Following the MOVE? label, A-D port 0 is read and examined for the first non
zero value. Each time the port is read it is stored into the temporary variable X,
then reloaded and examined for a zero value. Since GEQ is the only comparison
operator in the KDF-11 machine, the test is "1 greater than or equal to X." Thus,
the branch is taken to READ whenever X is 1 or larger.

Upon encountering the READ label, the value X (just read from port 0) is stored
into V(I), where I is zero. The value of I is then incremented by loading I to the top
of the KDF-11 stack, adding 1 (LIT 1, SUM), and then storing the sum back into I.
After incrementing I, the program proceeds to check the end of the tool travel. X
is loaded to the top of the stack, and the test "0 greater than or equal to X" is
performed. If the condition is true, contro] transfers to the label COMP, where the
analysis phase begins. Otherwise, port 0 is read again and the value is stored into
the temporary X. Control then proceeds back to the READ label to store the next
velocity, and test for zero.

Before 100 intervals have elapsed, the RDM 0 produces a zero value which is
stored into X and subsequently stored into V(I), for the current value of I. Thus, when
control arrives at the label COMP, the instantaneous velocities are stored in V,
terminated by a zero. At this point, the analysis of these collected velocities can
take place.

The single function which takes place in the analysis section of Figure 37 is
the computation of the distance travelled by the tool through this interval. In particular,
Nachtflieger engineers have determined that it is sufficient to compute the distance
travelled by the tool using the "trapezoidal rule" which approximates the actual distance
by summing the average of each adjacent pair of velocites. The sums are formed as
shown below:

Vot¥y + V,+Vo pose G4 Vt a

2 2 2

where n is the last interval to sum. Thus, for example, if the velocity is constant
at 256 um/ms (which wouldn't occur in practice), then

V, = Vo =- +. = Vy = 256,
and the summing formula given above reduces to 256 \* n. Given the example above
where n = 50 ms, the above formula produces the value 1.280 mm, as given earlier.
In general, the velocity values will not be constant, hence the numerical integration
given by the trapedzoidal rule is used to obtain an approximation.

The KDF-11 instructions shown in Figure 37 between the COMP and ENDF labels
perform the numeric integration given by the trapedzoidal rule. In general, the
temporary I is used to index through the velocity vector V until the final zero value
is encountered. For each interval, the values of two adjacent velocities are summed
and divided by two. Each result is then summed into TOTAL, where the values are
accumulated until the final zero velocity is discovered.

---
_Page 109_

The opcode sequence immediately following COMP places a zero value at the
top of the KDF-11 stack, then stores this value into both the index I and the accumulating
sum given by TOTAL. Ignoring the trace opcodes, the operations following GETNXT
read the starting point of the next interval to process into the stack, using VAL V,I
(value of V, indexed by I). If 0 is greater than or equal to this value then the
computation is complete and control goes to the label ENDF. Otherwise, the value
of V(I) is loaded to the KDF-11 stack, followed by the value of V(I+1). The loaded
values are then summed (SUM) and divided by two (LSR 1), producing a value which
remains in the KDF-11 stack. TOTAL is then loaded and added to this partial sum
and the result is stored back to TOTAL. The index value I is then incremented to
the next interval and processing continues back at the loop header GETNXT.

Upon processing the final zero velocity, control reaches the ENDF label where
the distance travelled is written to D-A output port zero. The output value is sent
to external instrumentation which processes the result and displays the distance travelled
in a form which is readable by the tool operator.

Note that debugging statements have been placed throughout the program which
can be used to trace the program execution. Figure 37 also contains TRT operators
which have enabled trace code generation, and thus this particular program, although
longer than the final production version, can be used to follow execution under CP/M.

Figure 38 shows the execution of the program of Figure 37 under DDT. The
messages printed at the debugging console are a result of the PRN opcodes distributed
throughout the original program which were enabled through the TRT P opcode. Further,
the machine code trace was only enabled for the interval of two operation codes (LIT
and STO) at the beginning. In order to test this program, simple A-D values were
supplied at the console for the velocities:

V, = 100H, V = 0

0 1 3 4
Upon detecting the final 0 value, the trace of Figure 38 shows the first 10 values of
V (the last 5 elements are "garbage" values), followed by a trace of the sum operations
for each interval. In each case, the pairs of values which are being added are displayed
(using the DMP opcode), followed by their summed value, along with the running total.
Upon completion of the distance computation, the value 320H is sent to the D-A output
port and displayed at the. console.

= 120H, V. = 100H, V, = 80H, V

Upon completion of initial checks under CP/M, Nachtflieger programmers remove
the TRT and TRF statements from the KDF-11 program and reassemble producing only
the absolute input/output instructions required for machine tool control. The resulting
program, which produces much less code than the debugging version, is placed into the
equipment for further testing and evaluation.

Figure 39 is also provided as an example of the listing which is produced when
all machine code operators are traced. Although the source program listing is not
shown, it is identical to Figure 37 except that the TRF T opcode is removed. Since
the complete trace is quite extensive, only a partial execution is shown in Figure 39.

In summary, Nachtflieger MW has derived several benefits from their emulation
of the KDF series stack machines. First, there is very little cost involved in designing

---
_Page 110_

DDT INTEG.HEX
DDT VERS 1.4

NEXT PC
0465 0000
-G100

COMPUTATION OF TOOL TRAVEL DISTANCE

LIT 0139 0000 OFT77

STO 01D6 0000 0000

A-D INPUT AT 4224 0

A-D INPUT AT 4224 100
STORE FIRST/NEXT VALUE

X= 0100

A-D INPUT AT 4224 120
STORE FIRST/NEXT VALUE

X= 0120

A-D INPUT AT 4224 100
STORE FIRST/NEXT VALUE

X= 0100

A-D INPUT AT 4224 80
STORE FIRST/NEXT VALUE

X= 0080

A-D INPUT AT 4224 0
STORE FIRST/NEXT VALUE

X= 0000

VALUE ARE LOADED

V= 0100 0120 0100 0080 0000 3ECO BA11 C1iC9 5EE1 5623

COMPUTING NEXT INTERVAL
= 0000

TOTAL= 0000

V,I= 0100 0120

COMPUTING NEXT INTERVAL
I= 0001

TOTAL= 0110

V,I= 0120 0100

COMPUTING NEXT INTERVAL
= 0002

TOTAL= 0220

V,I= 0100 0080

COMPUTING NEXT INTERVAL
I= 0003

TOTAL= 02E0

V,I= 0080 0000

COMPUTING NEXT INTERVAL
I= 0004

TOTAL= 0320

V,1I= 0000 3ECO

END OF COMPUTATION

TOTAL= 0320

D-A OUTPUT AT 4240 0320

Figure 38. Sample Execution of "Distance" using DDT.

---
_Page 111_

ddt integ.hex
DDT VERS 1.4

NEXT PC
0852 0000
-x1i00

COMPUTATION OF TOOL TRAVEL DISTANCE
LIT 026E 0000 CAB1
STO 030B 0000 0000
A-D INPUT AT 128 0
RDM 0344 0000 0000
STO 0359 0000 0000
VAL 036E 0000 0000
LIT 0384 0001 0000
DIF 039D FFFF 0000
GEQ O03AF FFFF 0000
A-D INPUT AT 128 6
RDM 0344 0006 0000
STO 0355 0006 0000
VAL 036E 0006 0000
LIT 0384 0001 0006
DIF 039D 0005 0000
GEQ 03AF 0005 0000
STORE FIRST/NEXT VALUE
X= 0006

VAL 043F 0006 0000
STO 045E 016F 0000
VAL 0473 0000 0000
LIT 0489 0001 0000
SUM 049D 0001 0000
STO 04B2 0001 0001
VAL 04C7 0006 0001
A-D INPUT AT 128 0
RDM 0501 0000 0006
STO 0516 0000 0006
LIT 052B 0001 0006
DIF 0544 0005 0001
GEQ 0556 0005 0001
STORE FIRST/NEXT VALUE
X= 0000

VAL 043F 0000 0001
STO 045E 0171 0001
VAL 0473 0001 0001
LIT 0489 0001 0001
SUM 049D 0002 0001
STO 04B2 0002 0002
VAL 04C7 0000 0002
A-D INPUT AT 128
RDM 0501 0000 0000

Figure 39. Partial Listing of "Distance" with Full Trace.

---
_Page 112_

and altering their machine architecture. In fact, current prices for 8080 microcomputers
may preclude the custom LSI version of the KDF-? machine. A second advantage of
the KDF emulation is that the KDF programs are highly independent from the host
processor. That is, given that a higher performance or less expensive processor becomes
available to Nachtflieger, the existing programs can be used intact by only changing
the macro definitions for each of the KDF opcodes and reassembling using MAC or
an equivalent macro processor. Lastly, machine emulation through macro defined
operation codes offers a distinct advantage over interpretive approaches since each
opcode translates to only a few host machine operations. Interpretive execution often
involves ratios of 1000 to 20,000 emulated instructions per host instruction, while
macro based opcodes are often in a ratio of less than 10 to 1. Further, interpretive
processors usually require run-time support consisting of a predefined general-purpose
subroutine package which is ineluded for each and every program. Thus, for a wide
variety of microcomputer applications, machine emulation through macro defined op-
codes offers distinct advantages over alternative approaches.

9.3. Program Control Structures.

Macro facilities can be used to provide program control statements which
resemble those found in many high-level languages. In general, program control
statements allow boolean tests and conditional branching based upon the outcome of
the boolean test. Further, label names which would normally be provided by the
programmer as the destination of a branch are automatically generated for the particular
statement.

In the paragraphs which follow, three typical contro] statements are presented
whieh allow simple conditional grouping (WHEN-ENDW), controlled iteration (DO-
ENDDO), and case selection (SELECT-ENDSEL). In all three cases, the intention is
to define program control facilities which allow well-structured programming, resulting
in programs which are easier to write, debug, and maintain.

Two libraries are first introduced in order to provide a foundation for further
discussion. The I/O library shown in Figure 40 allows simple character input operations
along with full message output. The READ macro accepts a single character from
the console keyboard and stores this character into the variable given by the parameter
"VAR." The WRITE macro shown in Figure 40 takes an ASCII message as a parameter
and sends this message to the console output device preceded by a carriage-return
line-feed sequence. These simple I/O macros are stored on the diskette in the file
"SIMPIO.LIB" and are used in the examples which illustrate the control structures.

The second library used in the control structure examples is given in Figure 41.
Collectively, these macros define a number of boolean operations which are performed
upon 8-bit operands, providing the basic relational operations on unsigned integer values,
including:

LSS Less Than

LEQ Less Than or Equal To
EQL Equal To

NEQ Not Equal To

GEQ Greater or Equal

GTR Greater Than

---
_Page 113_

: macro library for simple i/o
bdos equ @205h sbdos entry
conin equ 1 sconsole input function
msgout equ 9 sprint message til $
cr equ @dh scarriage return
lf equ Gah sline feed
read macro var
a] read a single character into var
mvi c,conin :console input function
call bdos scharacter is ina
sta var
endm
write macro msg
se write message to console
local msgl,pomsg
jmp pmsg
msgl: db cr ,1if ::leading crlf
ab “&MSG° ;;inline message
db “s° s3;message terminator
pmsg: mvi c,msgout sprint message til §$
lxi d,msgl
call bdos
endm

Figure 49, Simple I/O Macro Library.

---
_Page 114_

tdig?

n

ee

se se ve

se Q we we TS we ~e MD we ee ot)
=e O ~e @O se Q ~e OD
Q Q Qa

we Q ~e
~e Ct
i]

fl:

Figure 41,

macro X,Y
utiltity macro to generate condition codes

if not nul x :sthen load x

lda x 23x assumed to be in memory
endif

irpce ?V,Y 73y may be constant operand
set “GE ?Y -"B" s;first char digit?
exitm s:stop irpe after first char
endm

if tdig? <= 9 esy numeric?

sui y :3yes, so sub immediate
else

lxi h,y ssy not numeric

sub m :3:S0 sub from memory

endm

macro X,Y,tl

x lss than y test,

transfer to tl (true label) if true,
continue if test is false

test? X,Y s3;set condition codes
jc tl

endm

macro x,y,tl

x less than or egual to y test
iss x,y,tl

12 tl

endm

macro x,Vv,tl

x ecual to y test

test? X,Y

|z tl

endm

macro x,y,tl

x not egual to y test
test? X,Y

jnz tl

endm

macro x,y,tl

x greater than or equal to y test
test? X,Y

jnec tl

endm

macro x,y¥,tl
x greater than v test

local fl ::false label
test? X,Y

jc fl

dcr a

jnec tl

endm

Macro Library for Simple Comparison Operations,

---
_Page 115_

In all cases, the macros accept three actual parameters, consisting of two data values
involved in the test (X and Y), along with a program label which receives control if
the boolean test produces a true value (TL). The first operand X can be a labelled
memory location containing an 8-bit value, and Y can be either a labelled 8-bit location
or a literal numeric value. If the first operand X is not supplied, then the value to
be tested is assumed to exist in the 8080 accumulator when the macro is entered.
Thus, for example, the macro invocation

LSS ALPHA,BETA,TRUECASE

compares the values stored at the labelled memory locations ALPHA and BETA (defined
by a DS or DB statement), and transfers to the program step labelled by TRUECASE
if ALPHA contains a value less than the value stored at BETA. The invocation

LSS »BETA, TRUECASE

is similar, but compares the contents of the 8080 accumulator with the value stored
at BETA. Finally, the invocation

LSS ALPHA,34,TRUECASE
compares ALPHA with the literal value 34 in the relational test.

Note that the macro TEST? is used throughout the macro library to construct
the relational test by first loading the initial operand X, if necessary. The second
operand type is then examined by executing an "IRPC" within the TEST? macro of
Figure 41 which extracts the first character of the Y operand. This first character
must be either numeric or alphabetic. If numeric, then the literal value is subtracted
from the accumulator, setting the 8080 condition codes. If the first character of Y
is non-numeric then the value is assumed to reside in memory. In this case, the HL
registers are set to the Y operand and the value at Y is subtracted from the accumulator
value. In any case, the 8080 condition codes are set as a result of the subtraction
operation. These condition codes are then used in the individual macros to produce
conditional jumps to the destination labels. These macros are collectively stored on
the diskette in a file named "COMPARE.LIB" for use in examples which follow.

Figure 42 shows an example of a program which uses both the SIMPIO and
COMPARE libraries. The purpose of this program is to successively read console
characters and print messages based upon the character which is typed. The program
begins by sending the sign-on message at the label CYCLE. A character is then read
and stored into X using the READ macro. The LSS test is used to determine if
lower-to-upper case translation is required (assuming the input is alphabetic). If X is
numerically less than 61H, which is the value of an upper case "A," then control
transfers to the label NOTRAN. Otherwise, the character is loaded to the accumulator,
the "upper case" bit is stripped from the character, and it is replaced in memory.
Following the label NOTRAN, the character is compared with the letters A, B, C, and
D. In each case, a message is typed corresponding to each letter. If one of these
four letters cannot be found, the message at ERROR is typed.

In comparing each letter, the macro NEQ is invoked with the first argument

corresponding to the character typed at the console (X), while the second argument
corresponds to the letter to match. Note that the "%" operator is used in each case

---
_Page 116_

§12B

Y13B
A135
@144

G16A
818D

91B3

81B6
Q1BE
§1D9
O1EB

G1EC
Q20E

3A11682
E65F
321162

C3AG8O1

C306@1

C30061

co

C30981

Fiqure 42,

CYCLE:

oe me

ORG 106H
MACLIB SIMPIO ;SIMPLE IO LIBRARY
MACLIB COMPARE ;COMPARISON OPERATORS

WRITE <TYPE A CHARACTER FROM A TO D >

READ x
TEST FOR LOWER CASE ALPHABETIC
LSS X,61H,NOTRAN

ARRIVE HERE IF X IS GREATER OR EQUAL TO
A LOWER CASE A (=61H), TRANSLATE

LDA x
ANI 5FH CLEAR LOWER CASE BIT
STA x :STORE BACK TO X

NOTRAN:

=e $e

NOTA:
NOTB:

NOTC:

NOW CHECK CASES

NEO X,% A’ ,NOTA
WRITE <YOU TYPED AN A>
JMP CYCLE

NEO X,%°B° ,NOTB
WRITE <YOU TYPED A B>
JMP CYCLE

NEO X,% °C” ,NOTC
WRITE <YOU TYPED A C>
JMP CYCLE

NEQ X,% D°,ERROR

WRITE <YOU TYPED A D>
WRITE <BYE™ !>
RET

WRITE <NOT AN A, B, C, OR D>
JMP CYCLE

DS 1 7;TEMP FOR CHARACTER
END

Single Character Processing using COMPARE,

---
_Page 117_

to produce the numeric value of the character. This is necessary since the TEST?
macro expects either a number or a label value in the second argument position. The
program processes characters until a "D" is typed at which time it returns to the
console command processor. The intention here is to show the use of boolean tests
used by the control structure macros which follow.

Figure 42b shows a partial expansion of the macros given in the previous example.
The first message expansion is shown, along with the READ and NEQ macros. The
listing has been abstracted, however, and does not show the macro library statements
or the remainder of the program following the NOTA label.

The macro library shown in Figures 43a and 43b, called NCOMPARE, expands
upon the basie relational macros by allowing a "false branch" option. That is, each
macro accepts four arguments: the X and Y operands, as before, as well as a "true
label" (TL) and "false label" (FL). It is assumed that either the TL or FL will be
supplied in any particular invocation of a relational operator, but not both. If the TL
is supplied, then the branch is taken if the relational operator produces a true result.
Conversely, if the TL label is absent but the FL label is supplied, then the branch to
FL is taken if the relational operation produces a false result. Thus, NCOMPARE
expands upon the COMPARE library by allowing all of the relational operation as well
as their negations. Using the NCOMPARE library, for example, the macro invocation

LSS —s-X,20, , FALSELAB

branches to the label FALSELAB if X is not less than the value 20. One should note
that the negation operations are accomplished within the NCOMPARE library by first
testing for a null TL operand and, if empty, the relational operation is reversed by
invoking the appropriate negated macro. For example, the LSS macro in Figure 43a
invokes the GEQ macro, which is equivalent to "not LSS" when the TL argument is
empty and supplies the FL argument to LSS as the TL label to GEQ. These negated
relational forms will be used within the control structures which are described below.

Figure 44a gives an example of the use of the NCOMPARE library within a
particular program. This program is similar to the previous example, but instead
checks to insure that alphabetic translation only occurs within the proper range of
lower case letters. Following the label CYCLE, the character read from the console
is compared with a lower case "a" (using the % operation to produce the equivalent
decimal value 97). Since the negated form of GEQ is used here, the label NOTRAN
receives control if X is not greater than or equa] to %'a'. If X is greater than or
equal to %'a', program flow continues to the next test in sequence where X is compared
with a lower case "z" (%'z' = decimal 122), In this case, the normal form of GTR is
used and thus control transfers to NOTRAN if X is greater than %'z' which is above
the range of lower case alphabetics. If X is between %'a’ and %'z', the character is
changed to upper case, as before, by removing the lower case bit and replacing X in
memory. Note that the indentation levels between the GEQ and GTR operations are
included for readability of the program.

Figure 44b shows the GEQ-GTR section of the program of Figure 44a with full
macro trace enabled (see Assembly Parameters). The trace in this figure shows the
transition from GEQ to the LSS operator, substituting the FL label in the place of
the TL label. Again, the macro library statements are not shown, and the listing
following the NOTRAN label is not present.

---
_Page 118_

61800+C32361
G103+8DGA

9185+5459564528

G8122+24
6123+BEB9
612541190361

G8128+CDE504

912B+AF@1
812D+CDO568
0138+321102

6133+3A1162
@136+D661
8138+DA4341

813B 3A1162
Q13E E65F
8149 32112

Q143+3A1192
0146+D641
8148+C26AG1l

G@14B+C35F@1
BL4E+ODGA

4150+594F552054

Q15E+24
G15F+GE89
8161+114E91
6164+CDO508
8167 C38081

Figure 42b,

=e Ne

e
,

CYCLE: WRITE <TYPE A CHARACTER FROM A TO D >
JMP ??GOG2
??0661: DB CR, LF
DB “TYPE A CHARACTER FROM A TO D ~
DB “$\*
2?0G02: MVI C,MSGOUT
LXI D,??0081
CALL BDOS
READ x
MVI C,CONIN ;CONSOLE INPUT FUNCTION
CALL BDOS :CHARACTER IS IN A
STA x
: TEST FOR LOWER CASE ALPHABETIC
LSS X¥,61H,NOTRAN
LDA x
SUI 61H
JC NOTRAN
: ARRIVE HERE IF X IS GREATER OR EQUAL TO
: A LOWER CASE A (=61H), TRANSLATE
LDA D4
ANI SFH :CLEAR LOWER CASE BIT
STA X :STORE BACK TO X
NOTRAN:
; NOW CHECK CASES
NEO ¥,% A’ ,NOTA
LDA x
SUI 65
JINZ NOTA
WRITE <YOU TYPED AN A>
JMP ??0004
??0003: DB CR,LF
DB “YOU TYPED AN A’
DB “$°
??B004: MVI C,MSGOUT
LXI D,??0063
CALL BDOS
JMP CYCLE
NOTA: NEQ X,%°B’°,NOTB

Partial Trace of Fig 42a with Macro Generation,

---
_Page 119_

macro library for 8-bit comparison operation

macro X,V

utiltity macro to generate condition codes

if not nul x ::then load x

lda x 77X assumed to be in memory

endif

irpe ?V,V 7y may be constant operand

tdig? set “&?Y -"G ::first char digit?
exitm :stop irpc after first char
endm

if tdig? <= 9 2:y numeric?

sui y syves, so sub immediate

else

lxi h,y ;

sub m :

endm

ow

QO.
vv
QM ea
om on 4)

eal

on

on

y not numeric
so sub from memory

e
f
°
,

)
Wn)
ares

macro x,y,tl,fl

x lss than y test,

if tl is present, assume true test

if tl is absent, then invert test

if nul tl

geq x,y,fl

else

test? X,Y s;set condition codes
jc tl

endm

en on on
oy Ob Om

leg macro x,y,tl,fl
os x less than or equal to y test
if nul tl
geq x,y,fl
else
lss x,y,tl
jz tl
endm

Figure 43a, Expanded NCOMPARE Comparison Operators,

---
_Page 120_

eal macro x,y,tl,f1
23 xX equal to y test
if nul tl
neg x,Y,f1l
else
test? X,Y
1z tl
endm
neq macro x,y,tl,fl
73 x not equal to y test
lf nul tl
eql x,y,fl
else
test? x,V
jnz tl
endm
aeq macro x,y,tl,f£1
2 x greater than or equal to y test
if nul tl
lss x,y,fl
else
test? X,Y
jne tl
endm
qtr macro x,y,tl,fl
ss x greater than y test
if nul tl
leq xX,V,fl
else
local gfl \*:false label
test? X,V
ake gfl
dcr a
jne tl
afl: endm

Figure 43b.

Expanded NCOMPARE Comparison Operators (Cont).

---
_Page 121_

A100

§12B

@13B
@14A
G14c

O14F

B17E

g19C
O1A4
O1BF

91C2
Q1CA
B1E5
O1F7

O1F8
G21A

@21D
@21E

Figure 44a,

3A1D62

E65F

321D@2

C390801

C39001

C36901

C9

C36661

CYCLE:

=e

=e

=e Se mw

ORG 1866H
MACLIB SIMPIO ;SIMPLE IO LIBRARY
MACLIB NCOMPARE;COMPARISON OPERATORS

WRITE <TYPE A CHARACTER FROM A TO D >

OTRAN:

READ x
TEST FOR LOWER CASE ALPHABETIC
GEQ X,% a’ ,,NOTRAN ;BRANCH ON FALSE
X IS GREATER OR EQUAL TO LOWER CASE A
GTR X,%° z° ,NOTRAN
LDA 4
ANI 5FH sUPPER CASE
STA x :BACK TO X
NOW CHECK CASES
NEO X,% A’ ,NOTA
WRITE <YOU TYPED AN A>
JMP CYCLE
NEQ X,%°B°,NOTB
WRITE <YOU TYPED A B>
JMP CYCLE
NEQ X,% C°,NOTC
WRITE <YOU TYPED A C>
JMP CYCLE
NEQ X,%° D°,ERROR

WRITE <YOU TYPED A D>
WRITE <BYE™ !>
RET

WRITE <NOT AN A, B, C, OR D>

JMP CYCLE
DS 1 ;TEMP FOR CHARACTER
END

Sample Program using NCOMPARE Library.

---
_Page 122_

thee eee

8133+3A1D82
+
+
+ TDIG?
+
+
BO09+# TDIG?
+
+
6136+D661
+
+
+
+
8138+DA4FA1

the et
=e

tee ete t+

613B+3A1D02
+
+
+ TDIG?
+
+
OBG1+# TDIG?
+
+
@13E+D67A
+
+
+
+
0140+DA4761
@1434+3D
81444+D24F91
+ 770003:
6147 3A1D82
@14A E65F
G14C 321D82

;
NOTRAN:

TEST FOR LOWER CASE ALPHABETIC

GEQ X,%°a°,,NOTRAN ;BRANCH ON FALSE

IF NUL

LSS X,97,NOTRAN

IF NUL NOTRAN

GEO X,97,

ELSE

TEST? X,97

IF NOT NUL X

LDA xX

ENDIF

IRPC ?Y,97

SET "gE ?Y°-"@"

EXITM

ENDM

SET “9 "=" @?

EXITM

IF TDIG? <= 9

SUI 97

ELSE

LXI H,97

SUB M

ENDM

Jc NOTRAN

ENDM

ELSE

TEST? X,97

JNC

ENDM

X IS GREATER OR EQUAL TO LOWER CASE A
GTR X,%°z\* ,NOTRAN

IF NUL NOTRAN

LEQ X,122,

ELSE

LOCAL GPL
TEST? X,122

IF NOT NUL X
LDA x
ENDIF
IRPC ?Y,122
SET “SPY = "O°
EXITM
ENDM
SET “)°-"@"
EXITM
IF TDIG? <= 9
SUI 122
ELSE
LXI H,122
SUB M
ENDM
Jc 270003
DCR A
JNC NOTRAN
ENDM
LDA x
ANI SFH sUPPER CASE
STA xX ;BACK TO X

Figure 44b, Seqment of Fig 44a with “+M" Option,

---
_Page 123_

Given the SIMPIO and NCOMPARE libraries, it is now possible to define the
first complete control structure, called the WHEN-ENDW group. The form of the
group is:

WHEN condition
statement-1
statement-2

statement-n
ENDW

where "condition" is a relational expression taking one of the forms
id,rel,id id,relynumber _ ,rel,id yre],number
and "id" is an identifier, "rel" is a relational operator (LSS, LEQ, EQL, NEQ, GEQ,
GTR), and "number" is a literal numeric value. Similar in form to the arguments of
the individual relational operators of the COMPARE library, the last two forms shown
above assume the first argument is present in the 8080 accumulator. The meaning of
the WHEN-ENDW group is as follows: the condition following the WHEN is evaluated
as a relational expression, according to the rules stated with the COMPARE library.
If the condition produces a true result, then statement-1 through statement-n are
executed. Otherwise, contro] transfers to the statement following the ENDW. Nested
WHEN-ENDW groups are allowed when they take the form:
WHEN...
WHEN...
WHEN...
ENDW
ENDW
ENDW
to arbitrary levels, where the ". . ." represent interspersed statements. Because of
the simplified implementation, nested parallel WHEN-ENDW groups are disallowed when
they take the form:
WHEN...
WHEN...
ENDW
WHEN...
ENDW

ENDW

---
_Page 124_

The implementation of the WHEN-ENDW group is based upon macros which "count"
WHEN-ENDW groups and generate branches and labels at the proper levels in the
structure.

Figure 45 shows the WHEN macro library, consisting of four macros GENWTST
(generate WHEN test), GENLAB (generate label), WHEN (beginning of WHEN group),
and ENDW (end of WHEN group). These macros, in turn, use the macros in the
NCOMPARE library shown previously and thus are assumed to exist in the user's
program as a result of a MACLIB NCOMPARE statement. Label generation is based
upon the WCNT (WHEN count) and WLEV (WHEN level) counters. WCNT is incremented
each time a WHEN is encountered, and WLEV keeps track of the number of WHEN's
which have occurred without corresponding ENDW's.

Upon encountering the first WHEN, the WCNT and WLEV counters are set to
zero, and the WHEN macro is redefined to generate the first WHEN test by invoking
GENWTST, using the relation R, operands KX and Y, and WHEN counter WCNT. Note
that the value of WCNT is passed to GENWTST rather than the characters "WCNT"
themselves. Thus, at the first invocation of GENWTST, the dummy argument NUM
has the value 0. The first argument to GENWTST, called TST, corresponds to a
relational operation (LSS through GTR) and thus is invoked automatically within the
body of GENWTST, using the negated form of the relational since the TL argument
is empty. Again referring to the body of the GENWTST macro in Figure 45, note
that the last argument, corresponding to the false label of the relational operation, is
the constructed label ENDWé&num, where num has the value 0 initially, and successively
larger values on later invocations. Each time GENWTST is invoked, it generates a
relational test and a branch on false to a generated label. It is the responsibility of
the ENDW macro to produce the appropriate balanced label when encountered in the
program,

Referring back to the body of the WHEN macro in Figure 45, the WLEV level
counter is set to the current WCNT, and the WCNT is incremented in preparation for
the next WHEN statement. Similar to nearly all macros which redefine themselves,
the outer macro definition of WHEN invokes the newly created WHEN macro before
exit.

Upon encountering the an ENDW statement in the source program, the ENDW
macro first invokes GENLAB to generate the appropriate ENDW label. The first
argument to GENLAB is the label prefix ENDW, while the second argument is the
evaluated parameter %WLEV corresponding to the current ENDW label. If only one
WHEN statement had been encountered, for example, the value of WLEV would be
zero, and thus GENLAB would produce the label ENDW0O which is the destination of
the earlier branch generated by an invocation of GENWTST. Following the invocation
of GENLAB, WLEV is decremented to account for the fact that one more destination
label has been resolved.

As an example of the use of WHEN-ENDW, Figure 46a shows a sample program
which resembles the previous character scanning function, but uses the WHEN group
in the place of simple tests and branches. As before, a single character is read from
the console and first tested for possible case conversion. The statement "WHEN
X,GEQ,61H" causes the three statements which follow to be executed when X is greater
than or equal to 61H (lower case "a") and skipped otherwise. Further, the four WHEN
groups which follow each test for the specific characters A, B, C, or D. If an "A"

---
_Page 125_

macro library for “when" construct

label generators

we we se OC] we we we

Jenwtst macro tst,x,y,num
: generate a "when" test (negated form),
: invoke macro "tst" with parameters
: x,y with jump to endw & num
tst x,y, ,endwé&num
endm
genlab macro lab,num
se produce the label “lab” & “num"
lab&num:
endm
,
: “when" macros for start and end
t
when macro xv,rel,yv
2: initialize counters first time
went set 4) ssnumber of whens
when macro K,/U,Y
genwtst r,x,y,%went
wlev set went s:snext endw to generate
went set went+l :snumber of “when"s
endm
when xv,rel,yv
endm
endw macro
23 generate the ending code for a "when"
genlab endw,%wlev
wlev set wlev-l -:count current level down
3 wlev must not go below @ (not checked)
endm

Figure 45. Macro Library for the WHEN Statement.

---
_Page 126_

0169 ORG 1@@0H
MACLIB SIMPIO ;SIMPLE IO LIBRARY
MACLIB NCOMPARE;EXPANDED COMPARE OPS

MACLIB WHEN :WHEN CONSTRUCT

D100 CYCLE: WRITE <TYPE A CHARACTER FROM A TO D >
912B READ x

; TEST FOR LOWER CASE ALPHABETIC
9133 WHEN X,GEQO,61H
813B 3A1192 LDA x
013E E65F ANI SFU ;CLEAR LOWER CASE BIT
G14G 321192 STA x s;STORE BACK TO X
0143 ENDW

; NOW CHECK CASES
0143 WHEN X,EOL,% A”
G14B WRITE <YOU TYPED AN A>
9167 C36601 JMP CYCLE
Q16A ENDW
A16A WHEN X¥,EQOL,% B’
@172 WRITE <YOU TYPED A B>
18D C30081 JMP CYCLE
D196 ENDW
A199 WHEN X,EOL,% C”
9198 WRITE <YOU TYPED A C>
91B3 C3088] JMP CYCLE
61B6 ENDW
H1B6 WHEN X¥,EOL,%°D~
Q1BE WRITE <YOU TYPED A D>
61D9 WRITE <BYE” !>
G1EB C9 RET
Q1EC ENDW
G1EC WRITE <NOT AN A, B, C, OR D>
Q@20E C38601 JMP CYCLE
6211 X: DS 1 \*TEMP FOR CHARACTER

Figure 46a. Sample WHEN Program with °-M" in Effect,

---
_Page 127_

is typed, the corresponding WHEN group is executed, and control transfers back to the
CYCLE label where another character is read from the console. If the letter D is
typed, the program responds with two messages and returns to the console command
processor.

Figure 46b shows the same program with full macro trace enabled. This particular
portion of the program shows macro processing for the first WHEN-ENDW group only,
although the remaining groups are processed in a similar fashion. It is a worthwhile
exercise for the reader to determine that the nesting rules for WHEN groups are
properly stated, and that the restriction on nested parallel groups is, in fact, necessary.

A second contro] structure, called the DOWHILE-ENDDO group takes the general
form

DOWHILE eondition
statement-1
statement-2

statement-n
ENDDO

where the "condition" and nesting rules are identical to the WHEN-ENDW group. The
DOWHILE group is similar in concept to the WHEN group, except that statements 1
through n are executed repetitively as long as the condition remains true. That is,
the condition is evaluated when the DOWHILE is encountered in normal program flow.
If the condition produces a false value, then control transfers to the statement following
the ENDDO. Otherwise, the statements within the group are executed until the ENDDO
is reached. Upon encountering the ENDDO, control transfers back to the DOWHILE
and the condition is evaluated again. Iteration continues through the group until the
condition produces a false value.

The macro library for the DOWHILE group is shown in Figure 47. In general,
the DOWHILE statement invokes the relational operator macros to produce the proper
sequence of tests and branches. Upon encountering the ENDDO, the proper label and
jump sequence is again generated. Note that the only essential difference in the
DOWHILE and WHEN groups is that the location of the DOWHILE test must be labelled
and a JMP instruction must be generated to this label at the end of each group.

Referring to Figure 47, GENDTST (generate DOWHILE test), GENDLAB (generate
DOWHILE label), and GENDJMP (generate DOWHILE jump) are all "label generators"
used in the macros which follow. Similar to the WHEN macro, DOWHILE uses the
counters DOCNT and DOLEV to keep track of the number of DOWHILE groups which
have been encountered along with the current DOWHILE level, corresponding to the
number of unmatched DOWHILE's. The DOWHILE macro first generates the entry
label DTESTn, where n is the DOWHILE count. The conditional test is then generated,
similar to the WHEN macro, with a branch on false condition to the ENDDn label
which will eventually be generated by the ENDDO macro. Finally, the DOWHILE
macro increments the DOCNT counter in preparation for the next group.

The ENDDO macro in Figure 47 first generates the JMP instruction back to the
DOWHILE test, using the GENDLAB utility macro, and then produces the ENDDn label
which becomes the target of the jump on false condition. The form of the expanded
macros for one nested level thus becomes:

---
_Page 128_

=e se te

TEST FOR LOWER CASE ALPHABETIC

WHEN X,GEO,61H

OBGO+# WCNT SET 6

+ WHEN MACRO X,R,Y

+ GENWTST R,X,Y,%WCNT

+ WLEV SET WCNT

+ WCNT SET WCNT+1

+ ENDM

+ WHEN X,GEQ,61H

+ GENWTST GEQ,X,61H,%WCNT

+ GEQ X,61H, ,ENDW@

+ IF NUL

+ LSS X,61H, ENDWO

+ IF NUL ENDWG

+ GEO ¥,61H,

+ ELSE

+ TEST? X,61H

+ IF NOT NUL X
0133+3A1102 LDA x

+ ENDIF

+ TRPC ?Y,61H

+ TDIG? SET "&?PY'°-"@"

+ EXITM

+ ENDM
OHG6+# TDIG? SET “6°-"Q°

+ EXITM

+ IF TDIG? <= 9
6136+D661 SUI 61H

+ ELSE

+ LXI H,61H

+ SUB M

+ ENDM
8138+DA4301 JC ENDWS

+ ENDM

+ ELSE

+ TEST? X,61H

+ JNC

+ ENDM

+ ENDM
OBGG+# WLEV SET WCNT
OBB1+# WCNT SET WCNT+1

+ ENDM

+ ENDM
®13B 3A11802 LDA x
G13E EG65F ANI 5FH ;CLEAR LOWER CASE BIT
8140 3211082 STA X ;STORE BACK TO X

ENDW

Figure 46b. Partial Listing of Fig 46a with "+M" Option,

---
_Page 129_

° macro library for “dowhile” construct
gendtst macro tst,x,y,num
23 generate a “dowhile” test
tst x,y,,endd&num
endm
gendlab macro lab,num
2 produce the label lab & num
23 for dowhile entry or exit
lab&num:
endm
agendjmp macro num
3 generate jump to dowhile test
jmp dtesté&num
endm
dowhile macro xv,rel,yv
23 initialize counter
docnt set 4) snumber of dowhiles

dowhile macro X,L,Y
23 generate the dowhile entry
gendlab dtest,%docnt
23 generate the conditional test
gendtst r,x,y,%docnt
dolev set docnt ssnext endd to generate
docnt set docnt+l
endm
dowhile xv,rel,yv
endm

r
enddo macro

23 generate the jump to the test
gendjmp ¢dolev

3 generate the end of a dowhile
gendlab endd,%dolev

dolev set dolev-1
endm

Figure 47, Macro Library for the DOWHILE Statement,

---
_Page 130_

DTESTO:

conditional jump to ENDDO
DTEST1:
conditional jump to ENDD1L

JMP DTEST1
ENDDI
JMP DTESTO
Figure 48a shows an example of a program which uses the DOWHILE group.
Although this program differs slightly from the previous examples, the principal function
is the same: a STOP character is first read from the console, followed by a group
of statements which repetitively execute in search for the STOP character. Two
DOWHILE groups occur within the program. The first group checks each character
typed (X) to see if it matches the STOP character. If not ('DOWHILE X,NEQ,STOP"),
the statements up through the matching ENDDO are processed. If the value of X is
the character A, then the message "YOU TYPED AN A" is sent to the console.
Otherwise, the message "NOT AN A" is typed, followed by a check to see if the STOP
character was typed. If so, the messages "STOP CHARACTER" and "BYE!" appear at
the console. In this case, control continues through the ENDW's to the ENDDO and

back to the DOWHILE header. In this case, the "DOWHILE X,NEQ,STOP" produces a
false condition, and control transfers to the "XRA A" instruction following the ENDDO.

Referring again to Figure 48a, a second DOWHILE-ENDDO group is executed
which clears the normal CRT screen size of 23 lines. This is accomplished by first
setting X to the value zero, followed by a DOWHILE group which checks the condition
"X,LSS,23" which iterates until X reaches the value 23. The WRITE statement within
the DOWHILE group produces only the carriage-return line-feed on each interation,
since the character sequence within the brackets is empty. Following the WRITE
statement, X is incremented by one, thus acting as a line counter. When X reaches
23, the "RET" statement following the matching ENDDO receives control, and the
program terminates by returning to the console processor. Note that the "DB" statement
for X provides the initial value zero so that the first DOWHILE executes at least one
time.

Figure 48b shows a portion of the program of Figure 48a, with partial macro
trace enabled. Note in particular that this trace does not show the generated labels
ENDD1 and DTEST1 since no machine code was generated on those lines (the "+M"
assembly parameter would show the labels, however). The locations of these labels
can be derived from the "hex" listing to the left by noting that the "JNC ENDD1"
produces the destination address "01FF" corresponding to the "RET" statement, while
the "JMP DTEST1" produces the address "01E2" corresponding to the "LDA X" instruction
at the beginning of the DOWHILE group.

The last contro] structure presented in this section is the SELECT-ENDSEL
group, which corresponds to the Fortran "computed GO-TO," the ALGOL "switch"
statement, and the PL/M "case" statement. The general form of the SELECT group
is

---
_Page 131_

41098

012F

4#18D
81A3
@1AD
81C9
81DB
@1DB
g1DB

G1DE
@1DF
81E2
O1EA
G1F8
Q1FB
@1FC
O1FF

@201

on

ow

om

en

om Om

AF
3200862

218802
C9

8G

om W

T

Figure 48a.

OP: DS

ORG 1064

MACLIB SIMPIO ;SIMPLE IO LIBRARY
MACLIB NCOMPARE;EXPANDED COMPARE OPS
MACLIB WHEN ;WHEN CONSTRUCT
MACLIB DOWHILE ;DOWHILE STATEMENT

WRITE <TYPE THE STOP CHARACTER: >
READ STOP
X = @ FOR THE -FIRST LOOP

DOWHILE X,NEQ,STOP LOOK FOR STOP CHARACTER
WRITE <TYPE A CHARACTER: >
READ x

WHEN X,EOL,%°A~
WRITE <YOU TYPED AN A>
ENDW

WHEN X,NEQ,%°A~

WRITE <NOT AN A>
WHEN X,EOL,STOP
WRITE <STOP CHARACTER>
WRITE <BYE™ !>
ENDW

ENDW

ENDDO

CLEAR THE SCREEN (23 CRLF’S)
XRA A

STA x :X=0
DOWHILE X,LSS,23

WRITE <>

LXI H,X

INR M \*X=X+1]
ENDDO

RET

DB 8 ;EXECUTES "DOWHILE" FIRST TIME
1 ;STOP CHARACTER

An Example using the DOWHILE Statement,

---
_Page 132_

MIDE AF
O@1DF 329962

G1E2+3A0802
@1E5+D617
G1E7+D2FFO1

B1EA+C3FQ61
GLED+@DBA
O1EF+24

G1 FO+9EB9
Q1F2+11ED61
O1F5+CDG580
G1F8 210862
@1FB 34

B1FC+C3E261
O@1FF C9

Figure 48b,

CLEAR THE SCREEN (23 CRLF’S)

=e

XRA A
STA x >X=9
DOWHILE X,LSS,23
LDA x
SUI 23
INC ENDD1
WRITE <>
JMP ??0014

??0013: DB CR,LF
DB “$°

??0014: MVI C,MSGOUT
LXI D,?2?0813
CALL BDOS
LXI H,X
INR M >X=X+4+1
ENDDO
JMP DTEST1
RET

Partial Listing of Fig 48a with Macro Generation,

---
_Page 133_

SELECT id
statement-set-0
SELNEXT
statement-set-1
SELNEXT
SELNEXT
statement-set-n
ENDSEL

where "id" is a data label corresponding to an 8-bit value in memory, and statement
set 0 through n denote groups of statement separated by SELNEXT delimiters.

The action of the SELECT-ENDSEL group is as follows: the variable given in
the SELECT statement is taken as a “case” number assumed to be in the range 0
through n. If the value is 0, statement-set-0 is executed and, upon completion of the
group, control transfers to the statement following the ENDSEL. If the variable has
the value 1, then statement-set-1 is executed. Similarly, if the variable produces a
value i between 0 and n, then statement-set-i receives control. There can be up to
255 groups of statements within each SELECT-ENDSEL group, and any number of
distinet SELECT-ENDSEL groups. Nested SELECT-ENDSEL groups are not allowed,
however. That is, a SELECT-ENDSEL group cannot occur within a statement-set
enclosed within an encompassing SELECT-ENDSEL group. As a convenience, the
variable following the SELECT can be omitted in which case the current 8080 accumu-
lator content is used to select the proper case.

Figures 49a and 49b show the SELECT macro library which implements the
SELECT-ENDSEL group. The general strategy is to count the cases as they occur,
starting with the SELECT, delimited by NEXTSEL, and terminated by ENDSEL. As
the cases occur, a case label is generated which takes the form CASEn@m where n
counts the SELECT-ENDSEL groups, and m is the case number within group n. A
jump instruction is generated at the end of each case to the label ENDSn which marks
the end of the SELECT group number n. Upon encountering the end of the group, a
"select-vector" is generated which contains the address of each case within the group,
headed by the label SELVn, where n is again the group number. Machine code is thus
generated at the SELECT entry which indexes into the select vector, based upon the
SELECT variable, to obtain the proper case address. The first statement within the
ease receives control based upon the value obtained from this vector.

The general form of the machine code generated for the first SELECT group
within a particular program (group n = 0) is:

LDA id

LXI SELVO

(index HL by id, and
load the address to HL)

PCHL
CASE0@0:
statement-set-0
JMP ENDSO
CASE0@1:
statement-set-1
JMP ENDSO

---
_Page 134_

Macro library for “select” construct

label generators

we (QQ we we we
~- ©

nslxi macro num

load hl with address of case list
lxi h,selv&num
endm

gencase macro num,elt

7? generate jmp to end of cases
if elt gt 6@
jmp ends&num s;past addr list
endif

23 generate label for this case

case&num&@&elt:
endm

genelt macro num,elt

23 qenerate one element of case list
dw case&num&@&elt
endm

qenslab macro num,elts

23 generate case list

selv&énum:

ecnt set g z;;count elements
rept elts s37g9enerate dws
genelt num,%ecnt

ecnt set ecnt+4+l
endm ssend of dw’s

3 generate end of case list label
endsénum:
endm

Figure 49a, Macro Library for SELECT Statement.

---
_Page 135_

selnext macro

os: qenerate the next case

gencase %ccnt,%ecnt
23 increment the case element count
ecnt set ecnt+l

endm

,
select macro var

3 generate case selection code
ecnt set 4) s:count “selects”
select macro V ssredefinition of select
3 select on v or accumulator contents
if not nul v
lda Vv :;load select variable
endif
genslxi %cent ssgenerate the lxi h,selv#
mov e,a ss:create double precision
mvi d,@ sv in d,e pair
dad d ¢ssingle prec index
dad a ::;double prec index
mov e,m \*:low order branch addr
inx h ::to high order byte
mov d,m eshigh order branch index
xchg \*sready branch address in hl
pehl :;;gone to the proper case
ecnt set 6 ss;element counter reset
endm

invoke redefined select the first time

select var

selnext ssautomatically select case @
endm

=e
~e

,
endsel macro

3 end of select, generate case list
gencase $ccnt,%ecnt sz:last case
genslab $ccnt,%ecnt sscase list

2? increment "select" count

cent set ecnt+4+l
endm

Figure 49b, Library for SELECT Statement (Con’t).

---
_Page 136_

CASE0@n:
statement-set-n
JMP ENDSO

SELVO:

DW CASE0@0
DW CASE0@1

DW. CASE0@n
ENDSO:

Figure 49a contains the label generators GENSLXI (generate SELECT LX0D),
GENCASE (generate case labels, GENELT (generate select vector element), and
GENSLAB (generate SELECT label). Figure 49b contains the macro definitions for
SELNEXT (select next case), SELECT, and ENDSEL. Referring to Figure 49b, the
SELECT macro begins by zeroing CCNT which counts SELECT-ENDSEL groups and
then redefines itself, similar to the WHEN and DOWHILE macros. The redefined
SELECT macro then generates the select vector indexing operation by loading the
indexing variable, if necessary, and then fetches the specific case address. Note that
no machine code is generated to check that the indexing variable is within the proper
range. The PCHL at the end of this code sequence performs the branch to the selected
ease. At the end of the redefined select macro, SELNEXT is invoked automatically
to delimit the first case in the SELECT group (otherwise SELECT would have to be
followed immediately by SELNEXT in the user program to generate the proper labels.
SELECT also zeroes the ECNT variable which counts the cases until ENDSEL is
encountered,

SELNEXT, shown at the top of Figure 49b, is invoked by the programmer to
delimit cases. The GENCASE utility macro is invoked which, in turn, generates a
JMP instruction for the previous group, if this is not group zero, and then produces
the appropriate case entry label. SELNEXT also increments the select element counter
ECNT to account for yet another case.

Upon encountering the ENDSEL, the last macro in Figure 49b, GENCASE is
again invoked to generate the JMP instruction for the last case. GENSLAB then
produces the select vector by first generating the SELVn label, followed by a list of
ECNT DW statements which have the case label addresses as operands.

Figure 50a gives an example of a simple program which uses two SELECT groups.
The first SELECT group executes one of five different MVI instructions based upon
the value of X. The second SELECT group assumes that the 8080 accumulator contains
the selector index, and executes one of three different MVI instructions. The program
of Figure 50a is used only to illustrate the generated control structures, and does not
itself produce any useful values as output. The sorted symbol table shown at the end
of the listing gives the generated label addresses for the individual cases.

Figure 50b shows a segment of the previous program with generated macro lines.
Note the case selection code following "SELECT X" and the selection vector at the
end of the listing.

Figure 50c gives a more complete trace of the SELECT-ENDSEL group, showing
the actions of the macros as they expand for the second SELECT-ENDSEL group of

---
_Page 137_

GH08
OB1A
6@1C
OG1F
GO21

A844
OG4A
AG4C

8818 CASEG@G
6629 CASE@@5

3E09
3E@1
3EG2
3E93

3E64

9033 ENDSS

Figure 5@a.

on

MACLIB
SELECT
MVI
SELNEXT
MVI
SELNEXT
MVI
SELNEXT
MVI
SELNEXT
MVI
ENDSEL

SELECT
MVI
SELNEXT
MVI
SELNEXT
MVI
ENDSEL

DS

6915 CASE@@1
8848 CASE1@G
09655 ENDS]

SELECT
x
A,@

661A CASEG9@2 @G1F CASEG@3 9024 CASEO@4
0045 CASE1@1l 964A CASE1@2 804F CASE1@3
@629 SELVG O684F SELV1 6055 X

Sample Program using SELECT with "-M +S" Options,

---
_Page 138_

6000+3A5586
80034+2129608
@BO6+5F
@897+1668
8889+19
GABA+19
6AOB+5E
9@8C+23
@80D+56
GOGSE+EB
OABF+E9
6819 3EGB

@612+C33306
0015 3E@1

$98174+C33300
OB1A 3EB2

G81C+C33308
A81F 3E43

0@4214+C333909
0824 35864

§8826+C3339808
4429+1000
§992B+1508
OB2D+1A9B
OO2F+1 FOB
00314+2408

Figure 5@b,

MACLIB
SELECT
LDA

LXI

MOV

MVI

DAD

DAD

MOV

INX

MOV
XCHG
PCHL
MVI
SELNEXT
JMP

MVI
SELNEXT
JMP

MVI
SELNEXT
JMP

MVI
SELNEXT
JMP

MVI

Segment of

SELECT

arn
ica)
tt
<
a

UMMoUUmMu\*K x
<4

ENDS@
4,3

ENDS@
A,4

ENDSA

CASE@@B
CASEG@@1L
CASE@@2
CASE@@3
CASE@@4

Fig 5@a with Mnemonics,

---
_Page 139_

te t+

6833+214F98
+

BBAG+#

O801+4

tet te eett

6646 8668

+-

+
80424+C35508

+

+

+
BEG2+8

+

+

+
OLOG+4

+

+

+

+

+
BB4F+4008

+
OOG1+#

+
665144508
+

O8B2+#

+
8653+4A00
+
6O03+#

+
+
+
OOG2+4
+

Figure

.
(remaining c

58c,

SELECT

IF NOT NUL
LDA

ENDIF

GENSLXI &CCNT
LXI H,SELV1
ENDM

SET

(indexing code similar to Fig 5@b)

ECNT 4)
GENCASE %CCNT,%ECNT
IF 0 GT 86
JMP ENDS]
ENDIF
CASE1@6@:
ENDM
ECNT SET ECNT+1
ENDM
ENDM
MVI B,B
SELNEXT
GENCASE &CCNT,%ECNT
IF 1 Gt @
JMP ENDS]
ENDIF
CASE1@1:
ENDM
ECNT SET ECNT+1
ENDM

ases are similar)

ENDSEL
GENSLAB $CCNT,%ECNT

SELV1:

ECNT SET 6
REPT 3
GENELT 1,%ECNT

ECNT SET ECNT+1
ENDM
GENELT 1,%ECNT
DW CASE1@86
ENDM

ECNT SET ECNT+1
GENELT 1,%ECNT
DW CASE1@1
ENDM

ECNT SET ECNT+1
GENELT 1,%ECNT
DW CASE1@2
ENDM

ECNT SET ECNT+1
ENDM

ENDS]:
ENDM

CCNT SET CCNT+1
ENDM

Segment of

Fig 58a with "+M" Option,

---
_Page 140_

Figure 50a. The listing has been edited to remove the case selection code, which is
listed in Figure 50b, as well as the code generated for case number 2. Figure 50c
should be cross-referenced with the SELECT macro library given in Figures 49a and
49b if confusion remains as to the actions of these macros.

It is now possible to show a complete program which uses the WHEN, DOWHILE,
and SELECT groups. Figure 51 shows a program which is similar in function to a
more complicated program which interacts with the console in executing single character
input commands. In fact, the two CP/M programs ED and DDT both take this general
form (see the ED and DDT Users Guides for details). That is, a single letter is used
to select a single action which may correspond to an edit request in the ED program,
or a debug request in DDT. Upon completion of each command, control returns back
to the main loop to accept another single letter command.

The program given in Figure 51 begins by loading the macro definitions for the
SIMPIO, NCOMPARE, WHEN, DOWHILE, and SELECT operations. Several messages
are then sent to the console device, followed by a single DOWHILE-ENDDO group
which encompasses nearly the entire program. The DOWHILE group is controlled by
the X,NEQ,%'D' test and thus continues to loop while the X character is not the letter
D. On each iteration of the DOWHILE group, a single letter is read from the console
and converted to upper case, if necessary. In order to ensure that the letter is in
the proper range of values, two WHEN groups follow which convert illegal values to
the letter E, which will subsequently produce an error response.

Following the WHEN tests in Figure 51, the character must be in the range ‘A’
through 'E'. Before indexing into the SELECT group, this value is "normalized" to the
absolute value 0 through 4 corresponding to each of the possible values. The SELECT
statement uses the value in the accumulator to select one of the five cases, producing
the appropriate response to the letters A through D, or an error response for the last
ease. Upon completion of the SELECT group, control returns to the DOWHILE where
the Jast character typed is tested against the letter D. If X is not equal to the letter
D, the iteration continues. Otherwise, the DOWHILE completes and control returns
to the console processor.

The control structures presented in this section are representative of the forms
which can be implemented. Additional facilities, such as the controlled iteration found
in Fortran DO loops, or Algol FOR loops can be implemented using essentially the
same techniques used for the WHEN and DOWHILE. Further, Subroutine parameter
mechanisms which pass actual values to subroutines for assignment to formal parameters
can also be defined with macro libraries. Note also that it would be relatively easy
to include control structures for the stack machine given in the previous section, thus
allowing machine independent programming of control structures as well as arithmetic
operations.

---
_Page 141_

G189

91989

@17C
819C

@1A4
@1AC 3ABFO2E65F
81B4

81B4
G1BC 3E4532BFO62
G1cl

G1cl
GiCC 3E4532BF@2
@1D1

@1D1 3ABF@2D641
g1D6
G1E3
B24C
O24F
G293
@2AE
Q@2BB

(2BE C9

Q2BF 88

=e 4 we

=o

e
a

Xe

Figure 51.

ORG 16@H ;BEGINNING OF TPA
MACLIB SIMPIO ;SIMPLE READ/WRITE
MACLIB NCOMPARE;COMPARISON OPS
MACLIB WHEN ;"WHEN" CONSTRUCT
MACLIB DOWHILE ;"DOWHILE” CONSTRUCT
MACLIB SELECT ;"SELECT" CONSTRUCT

USING THE CCP’S STACK, READ INPUT
CHARACTERS, UNTIL A Z IS TYPED
WRITE <SAMPLE CONTROL STRUCTURES>
WRITE <TYPE SINGLE CHARACTERS FROM>
WRITE <A TO b, It’f’LL STOP ON D>

DOWHILE X,NEQ,%°D~
WRITE <TYPE A CHARACTER: >
READ X

WHEN X,GEQ,3% A’
LDA X! ANI @5FH! STA xX :CONV CASE
ENDW

WHEN X,LSS,%°A’
MVI A,°E°! STA X ;SET TO ERROR
ENDW

WHEN X,GTR,%°E°
MVI A,’ E°! STA X :SET TO ERROR
ENDW
LDA X! SUI “A” ;NORMALIZE TO 6-4
SELECT ;BASED ON X IN ACCUM
WRITE <YOU SELECTED CASE A>
SELNEXT
WRITE <YOU SELECTED CASE B>
SELNEXT
WRITE <YOU SELECTED CASE C>
SELNEXT
WRITE <YOU SELECTED CASE D>
WRITE <SO I°°M GOING BACKT!>

SELNEXT
WRITE <BAD CHARACTER>
ENDSEL
ENDDO
RET 7;BACK TO CCP
DATA AREA
DB a 7X=9@ INITIALLY

Program uSing WHEN, DOWHILE, and SELECT,

---
_Page 142_

9.4. Operating Systems Interface.

In a general-purpose computing environment, macros are often used to provide
systematic and simplified mechanisms for programmatic access to operating system
functions. Throughout this document, the examples have shown various low-level calls
to the CP/M operating system which implement function such as single character input,
single character output, and full message output. In each case, the macros simplify
the operations by performing the low-level register set-ups and calls which perform
the function.

The purpose of this section is to introduce more comprehensive operating system
interface macros, and specifically show a sample macro library which allows simplified
diskette file operations for sequential "stream" input/output operations. The principal
macros of this library which allow file access are listed below:

FILE ~ set up a named file for subsequent disk operations
GET ~ read a single character from a specific data source
PUT - send a character to a specific data destination
FINIS - terminate file access for a specific group of files
ERASE - remove a specifie diskette file

DIRECT - search for a specific file on the diskette

RENAME - rename a specific diskette file

Before introducing the macro library which performs these functions, the operation of
each macro is described, followed by a simple example.

The FILE operation takes the form:
FILE mode,fileid,diskname,filename,filetype,buffsize,buffaddr

where the individual parameters of the FILE macro describe a particular file to be
accessed in the program. The parameter values for the FILE macro are:

mode - infile (input file),
~ outfile (output file),
- setfile (set up file name for ancillary functions),

fileid - file identifier for internal reference throughout
the program

diskname ~ disk drive name (A, B, .. .) containing the file
being accessed, or empty if the default drive is
being used

filename ~ the (up to eight character) file name of the diskette

file being accessed; if "1" or "2" is specified, then
the first or second default file name is used,
respectively

filetype - the (up to three character) file type of the file being

accessed; if "1" or "2" has been specified for the
filename parameter and an empty filetype is given,

---
_Page 143_

then the file type is taken from the selected default
file name, otherwise the type is set to blanks

buffsize - the size (in bytes) of the buffer area used for this
file; the value is rounded down to an integral
multiple of the diskette sector size; if the rounding
produces a result which is too small, or if the para-
meter is empty, then only one sector is buffered.

buffaddr - the address of the buffer area to be used during
accesses to this file; if empty, then the buffer
address is assigned automatically

The FILE statement
FILE INFILE,ZOT,A,NAMES,DAT

for example, sets up the file "NAMES.DAT" on diskette drive A for subsequent access.
Internal to the program, this file will be referenced by the name ZOT. Further, the
buffer address is assigned automatically, and the buffer size is set to one sector
(normally 128 bytes). In general, larger buffers are useful in minimizing rotational
delay on the diskette due to "missed sectors" during the file operations. If the
"NAMES.DAT" file does not exist, an error message is sent to the console, and the
program is aborted. An output file can be created using the statement

FILE OUTFILE,ZAP,B,A DDRESS, DAT, 1000

for example, which creates the file "ADDRESS.DAT" on drive B for subsequent output,
referenced internally by the name ZAP. In this case, the buffer size is set to 1000
bytes (rounded down tc 7 \* 128 = 896 bytes), and the base address of the buffer is
set automatically. The sample programs show alternative FILE options.

The GET macro invocation takes the form
GET device
where "device" specifies a simple peripheral or a diskette file defined by a previously

executed FILE statement. The GET statement reads one byte of data into the 8080
accumulator from the specified device. The possible device names are:

key ~ console keyboard input
rdr ~ reader device
fileid - previously defined file identifier given in a FILE statement

The following GET invocations perform the functions shown to the right below.

GET KEY ~- read one keyboard character

GET RDR - read one reader character (see CP/M Interface and
Alteration Guides for READER entry point definition)

GET ZOT ~- read one character from the file given by the in-
ternal name ZOT (i.e., the NAMES.DAT file if the
above FILE statement had been executed)

---
_Page 144_

The end of data can be detected in two ways: if the file contains character data,
the end of file is detected by comparing the individual characters with the standard
CP/M end of file mark which is a control-Z (hexadecimal 1AH). The GET function
also returns with the 8080 zero flag set to true if a real end of file is encountered
so that pure binary files can be read to the end of data.

The PUT macro performs the opposite function from the GET macro. The PUT
invocation takes the form:

PUT device

where "device" specifies a simple output peripheral or a diskette file defined previously
using the FILE macro. The possible device names are

econ - console display device

pun - system punch device

ist - system listing device

fileid ~ previously defined output file identifier

The following PUT invocations perform the funetions shown to the right below:

PUT CON - write the accumulator character to the console
PUT PUN - write the accumulator character to the punch
PUT LST - write the accumulator character to the list device
PUT ZAP - write the accumulator character to the file

whose internal name is ZAP (i.e., the ADDRESS.DAT
file in the above example)

Note that the character in the accumulator is preserved during the invocation so that
it may be involved in further tests or macro invocations following the PUT statement.

The FINIS statement is usec to close a file or set of files upon completion of
file access. In the case of an output file, the interna) buffers are written to disk,
and the file name is permanently recorded on the diskette for future access. The
form of the FINIS invocation is

FINIS filelist

where "filelist" is a single internal name which appeared previously in a file statement,
or @ list of such file names enclosed within broken left and right brackets, and separated
by commas. Although it is not necessary to close input files with the FINIS statement,
it is good practice, since the file close operation may be required on future versions
of the macro library. An example of the FINIS statement is:

FINIS ZAP - write all buffers for the ZAP file, and record the
file in the diskette directory; in the above example,
the ADDRESS.DAT file is closed.

The ERASE macro allows programmatic removal of a diskette file given by the
specified file identifier defined in a previous FILE statement. If the file identifier is
not used in a GET or PUT statement, then the FILE statement can have the mode

---
_Page 145_

"setfile’ which requires less program space than an "infile" or "outfile’ parameter.
Specific eases of the ERASE statement will be given in the examples which follow.
In the simple case

ERASE ZOT

however, the file NAMES.DAT would be removed from the diskette, given the previous
FILE statement which defines ZOT.

The DIRECT macro is used to search for a specific file on the diskette. Similar
to the ERASE macro, the file identifier must be previously given in a FILE statement
using one of the three possible file modes. The DIRECT invocation sets the 8080 zero
flag to false if the file is present on the diskette. In both the ERASE and DIRECT
macros, the file identifiers can reference file names and types with embedded "?"
characters, similar to the normal CP/M "DIR" command, where the question mark will
mateh any character in the file names being scanned. The macro invocation

DIRECT ZAP

for example, returns a non-zero flag if the file ADDRESS.DAT is present, and a zero
flag if the file is not present, given the original FILE statement involving the ZAP
file identifier.

The RENAME macro takes the form
RENAME new file,oldfile

where "newfile” and "“oldfile" are file identifiers which have appeared in previous FILE
statements. The rename macro changes the file name given by newfile to the file
name given by oldfile. Similar to the ERASE and DIRECT macros, the file identifiers
"newfile" and "“oldfile' must appear in previously executed FILE statements, but may
have a mode of "setfile" if they are not used in GET or PUT macros. If the drive
names for the oldfile and newfile differ, then the drive name of the newfile is assumed.
The sequence of macro invocations

FINIS ZAP ;CLOSE "ZAP"
ERASE ZOT ;sREMOVE "ZOT"
RENAME ZOT,ZAP ;CHANGE NAMES

for example, first closes the ADDRESS.DAT file on drive B, then erases the NAMES.DAT
file on drive A. The RENAME macro then changes the ADDRESS.DAT file to the
name NAMES.DAT file on drive A.

Figure 52 shows the use of the FILE, GET, PUT, and FINIS macros in a working
program. The purpose of this program is to read an input file, specified at the console
command processor level as the first file name, and translate each lower case alphabetic
character to upper case. The output is sent to the file given as the second parameter
at the command level. Given that this program has been assembled, loaded, and stored
as "CASE.COM" on the diskette, a typical execution would be

CASE LOWER.DAT UPPER.DAT

---
_Page 146_

OO5F

B1EC

Q2EA
82ED
82EF

B2F2
O2F4
O2F7
G2F9

82FC
O2FE
83089

8308C
634D
93598

=e Se Ge

ORG 1808

COPY FILE 1 TO FILE 2, CONVERT

TO UPPER CASE DURING THE COPY

AND ECHO TRANSACTION TO CONSOLE
MACLIB SEQIO ;SEQUENTIAL I/O LIB

= BOOT EQU 6000H sSYSTEM REBOOT
= UCASE EQU 5FH sUPPER CASE BITS
317683 LXI SP,STACK
; DEFINE SOURCE FILE:
; INFILE = INPUT FILE
; SOURCE = INTERNAL NAME
: (NUL) = DEFAULT DISK
; 1 = FIRST DEFAULT NAME
: (NUL) = FIRST DEFAULT TYPE
; 2089 = BUFFER SIZE
FILE INFILE,SOURCE,,1,,2009
; DEFINE DESTINATION FILE:
: OUTFILE = OUTPUT FILE
; DEST = INTERNAL NAME
; (NUL) = DEFAULT DISK
; 2 = SECOND DEFAULT NAME
; (NUL) = SECOND DEFAULT TYPE
: 2888 = BUFFER SIZE
FILE OUTFILE,DEST,,2,,20080
: READ SOURCE FILE, TRANSLATE, WRITE DEST
CYCLE: GET SOURCE
PELA CPI EOF :END OF FILE?
CA0CH3 JZ ENDCOPY ;SKIP TO END IF SO
: NOT END OF FILE, CONVERT TO UPPER CASE
FE61 CPI "a’ :;BELOW LOWER CASE "A"?
DAFEG2 JC NOCONV ;SKIP IF SO
FE7B CPI “2°41 ;BELOW LOWER CASE "Z"?
D2FEG2 JNC NOCONV ;SKIP IF ABOVE
: MASK OUT LOWER CASE ALPHA BITS
E65F ANI UCASE
NOCONV: PUT CON :WRITE TO CONSOLE
PUT DEST sAND TO DESTINATION FILE
C3EA@2 JMP CYCLE ;FOR ANOTHER CHARACTER
ENDCOPY:
FINIS DEST sEND OF OUTPUT
C36088 JMP BOOT ;BACK TO CCP
DS 32 316 LEVEL STACK
STACK:
BUFFERS:
= MEMSIZE EQU BUFFERS+@NXTB :PROGRAM SIZE
END
Figure 52. Lower to Upper Case Conversion Program.

---
_Page 147_

which causes the CASE.COM file to be loaded and executed in the transient program
area. Before execution, the console command processor passes LOWER.DAT as the
first default file name, and UPPER.DAT as the second file name (see the CP/M
Interface Guide for exact details). Referring to Figure 52, the CASE program begins
by intializing the stack pointer to a local stack area in preparation for subsequent
subroutine calls which occur within the various macros in the SEQIO macro library.
The first default file name is then taken as the SOURCE file, as defined in the first
FILE macro. The second FILE statement assigns the second default file name as an
output file with the internal name DEST. In both cases, the FILE statements open
the respective files and initialize the buffer areas consisting of 2000 bytes (rounded
down to a multiple of the sector size). Note that if the UPPER.DAT file already
exists, the second file statement removes the existing file and creates a new UPPER.DAT
file before continuing. In either case, the appropriate error messages will appear at
the console if the files cannot be accessed or created in the FILE statements.

The CASE program's main loop is shown in Figure 52 between the CYCLE and
ENDCOPY labels. Each successive character is read from the SOURCE file (in this
case, LOWER.DAT) and tested to see if the character is in the range of a lower case
"a" to lower case "z." If in this range, the character is changed to upper case. At
the NOCONV label, the (possibly translated) character in the accumulator is sent to
the console device using the "PUT CON" macro and then sent to the DEST file (in
this case, UPPER.DAT). Looping continues back to the CYCLE label where another
character is read and translated. Since the data file is assumed to consist of a stream
of Ascii characters, the end of file is detected when a control-Z is encountered. When
this character is found, control transfers to the label ENDCOPY where the DEST file
is closed using the FINIS macro. Again note that errors in writing or closing the
DEST file will produce an error message at the console, and the program execution
will be aborted immediately. Upon completion of the program, control is returned to
the console processor through a system reboot (JMP BOOT).

The SEQIO library macros assume that all file buffers are located at the end
of the user's program, as shown in Figure 52, In particular, the label BUFFERS must
appear as the last label in the user's program, and becomes the base of the buffers
allocated automatically in the FILE statements. The actual memory requirements for
the program can be cetermined using an "EQU" as shown in Figure 52, with a statement
of the form

MEMSIZE EQU BUFFERS+@NXTB

which produces the equated value 1270H at the left of the listing. In this particular
ease, the memory area beyond 1270H is not used by the program.

The macro library for SEQIO is shown in Figures 53a, 53b, 53e, 53d, and 53e,
which constitute the most comprehensive macro library shown in this manual. The
particular macro library contains an instance of nearly every macro facility available
in MAC, and thus it is useful to read and understand the operations contained in the
listing. The discussion below of SEQIO outlines the general functions of each macro,
but it is left to the reader to investigate the exact operation of the library.

The SEQIO segment shown in Figures 53a and 53b contain generally useful

equates and utility macros. The labe] FILERR at the beginning becomes the destination
of transfers upon encountering a file operation error and, since this is a SET statement,

---
_Page 148_

; sequential file i/o library

‘
filerr set agegh sreboot after error
@bdos equ @d8Sh rpdos entry point
@tficb egu @d5ch sdefault file control block
@tbuf equ a086h ;default buffer address
; bdos functions
@msg equ 9 ;send message
@opn equ 15 sfile open
@cls equ 16 ;file close
@dir equ 17 sdirectory search
@del equ 19 7file delete
@frd equ 28 ;file read operation
@fwr equ 21 ;file write operation
@mak equ 22 sfile make
@ren equ 23 ;file rename
@dma equ 26 ;set dma address
@sect equ 128 ;sector size
eof equ lah send of file
cr equ @dh scarriage return
lf equ @ah sline feed
tab equ 89h shorizontal tab
akey equ 1 pkeyboard
@con equ 2 sconsole display
@rdr equ 3 jreader
@pun equ 4 ;punch

5 slist device

@lst equ
; keywords for “file” macro
infile equ 1 sinput file

outfile equ 2 soutputfile
setfile equ 3 ;setup name only

the following macros define simple sequential
file operations:

illnam macro fc,c
77 fill the file name/type given by fe for c characters
@cnt set fod :smax length
irpe ?fe,fe ;;fill each character
a may be end of count or nul name
if @cnt=8 or nul ?fc
exitm
endif
db “S?FC° 3;£i11 one more
@cnt set @cnt-l ;;decrement max length
endm 3;0f irpe ?fc
a pad remainder
rept @ent 3:@cnt is remainder
db us :;pad one more blank
endm 330f rept
endm
filldef macro feb,?f1,?1ln
a fill the file name from the default feb
7 for length ?in (9 or 12)
local psub
jmp psub z7jump past the subroutine
@def: ;;this subroutine fllls from the tfcb (+16)
mov a,m siget next character to a
stax d ;7store to fcb area
inx h
inx d
dcr c s;3;count length down to 6
jnz @def
cet
33 end of fill subroutine
psub:
filldef macro ?feb,?£,2?1
lxi h @tfcb+?f£ preither a@tfcb or atfcb+l6
1lxi d,?fcb
mvi c,?l1 prlength = 9,12
call @def
endm
filldef fcb,?£1,?1n
endm
fillnxt macro
3t initialize buffer and device numbers
@nxtb set @ sinext buffer location
@nxtd set @lst+l simext device number
fillnoxt macro
endm
endm

Sequential File Input/Output Library.

Figure 53a.

---
_Page 149_

mm

Se se Ne se Se Ne se

llfcb macro fid,dn,fn,ft,bs,ba
fill the file control block with disk name
fid is an internal name for the file,
dn is the drive name (a,b..), or blank
fn is the file name, or blank
ft is the file type
bs is the buffer size
ba is the buffer address
local pfcb

Re 80 a se Me ne te

af set up the file control block for the file
33 look for file name = 1 or 2
@c set L ;3assume true to begin with
irpe ?c,fn szlook through characters of name
if not (°&?C" = “1° or “&?C’ = °2°)}
@c set ] ;7clear if not 1 or 2
endm
33 @c is true if fn = 1 or 2 at this point
if @c githen fn = 1 or 2
3 fill from default area
if nul ft ;;type specified?
@c set 12 3;both name and type
else
ac set 9 :;name only
endif
fFilldef fcbsafid,(fn-1)\*16,@c 337to select the feb
jmp pfcb sipast feb definition
ds @c zispace for drive/filename/type
fillnam £t,12-@c ;;series of db’s
else
jmp pfcb ripast initialized feb
if nul dn
db 8 s;use default drive if name is zero
else
db “SDN -"A“+1 z;;use specified drive
endif
fillnam fn,8 :;£111 file name
ws now generate the file type with padded blanks
fillnam ft,3 grand three character type
endif
Ecb&fid equ $-12 3;;beginning of the fcb
db g ssextent Field @@ for setfile
7 now define the 3 byte field, and disk map
ds 26 37%,x,rc,dm@,..dml15,cr fields
if fidstyp<=2 sin/outfile
? generate constants for infile/outfile
fillnxt 77@nxtb=9 on first call
if bs+8@<@sect
33 bs not supplied, or too small
@bs set @sect :;default to one sector
else
3 compute even buffer address
@bs set (bs/@sect) \*@sect
endif
3 now define buffer base address
if nul ba
ad use next address after @nxtb
fidsbuf set buffers+@nxtbd
7 count past this buffer
@nxtb set @nxtb+é@bs
else
fidsbuf set ba
endif
33 fidsbuf is buffer address
fid&adr:
dw fidéebuf
fid&siz equ’ @bs srliteral size
fidslen:
dw @bs s3buffer size
fidsptr:
ds 2 33set in infile/outfile
7f set device number
@&fid set @nxtd pinext device
@nxtd set @nxtd+l
endif 770f fidatyp<=2 test

pfcb: endm

Sequential File I/O Library (Con't).

Figure 53b.

---
_Page 150_

file macro md,fid,dn,fn,ft,bs,ba
tM create file using mode md:
73 infile = 1 input file
a outfile = 2 output file
a setfile = 3 setup fcb
a (see fillfcb for remaining parameters)
local psub,msg,pmsg
local pnd,eod,eob,pne
33 construct the file control block
fid&typ equ md ;7Set mode for later ref’s
fillfcb fid,dn,fn,ft,bs,ba
if md=3 ;7setup fcb only, so exit
exitm
endif
ft file control block and related parameters
7 are created inline, now create io function
jmp psub :7past inline subroutine
if md=1 ;;input file
getéfid:
else
put&fid:
push psw ;7Save output character
endif
ihid fid&len ;;:load current buffer length
xchg 3;de is length
lhid fidsptr ;;load next to get/put to hl
mov a,l ;3;compute cur-len
sub e
mov a,h
sbb dq precarry if next<length
je pne s3carry if len gtr current
Tl end of buffer, fill/empty buffers
lxi h,@
shld fidsptr ;;clear next to get/put
pnd:
ft process next disk sector:
xchg 37fidsptr to de
lhid fid&len ;;do not exceed length
et de is next to fill/empty, hl is max len
mov a,e s;;compute next-len
sub 1 g7to get carry if more
mov a,d
sob h s3to Fill
jne eob
a carry gen’ed, hence more to fill/empty
lhld fidsadr ;;base of buffers
dad d trhl is next buffer addr
xchg
mvi c,@dma ;;set dma address
call @bdos ;7dma address is set
lxi d,fcbhafid 3;3fcb address to de
if md=1 :;read buffer function
mvi c,@frd ;;file read function
else
mvi c,@fwr ;;file write function
endif
call @bdos sicd/wre to/from dma address
ora a ;7check return code
jynz eod s;3end of file/disk?
i not end of file/disk, increment length
Llxi d,@sect ;;sector size
lhld fid&ptr ;;next to fill
dad d
shld fid&ptr ;;back to memory
jmp pend prprocess another sector
eod:
rt end of file/disk encountered
if md=1 ssinput file
lhid fid&ptr ;;length of buffer
shld fidslen ;;reset length
else
ot fatal error, end of disk
local emsq
mv i c,@msq ;s;write the error
ixi d,emsg
call @bdos y7error to console
pop psw sy;remove stacked character
jmp filerr s;;usually reboots
emsq: db er,lf
db “disk full: sFID’
db “$°
endif

Sequential File I/O Library (Con't).

Figure 53c.

---
_Page 151_

=
se Owe
oO

se
se Ds
Q

=
=e

ee

psub:

msg:

pmsg:

end of buffer, reset dma and pointer
lxi @,@tbuf

mvi c,@dma

eall @bdos

lxi h,@

shld Fidaptr :;next to get

process the next character

xchg zrindex to get/put in de
lhld fidsadr ;;base of buffer

dad d ;r;address of char in hi
xchg ;;address of char in de
if md=1 ;;input processing differs
lhld fid&len ;;for eof check

mov a,l 730080?

ora h

mvi a,eof grend of file?

rz ;;3;zero flag if so

ldax a z3mext char in accum
else

store next character from accumulator
pop psw 37crecall saved char

stax d ;;character in buffer
endif

lhld fidsptr ;;index to get/put

inx h

shld fid&sptr ;;pointer updated

return with non zero flag if get
ret

s?past inline subroutine

xra a 33zero to acc
sta fcb& fid+12 s;clear extent
sta fcb& fid+32 3;3clear cur rec
Lexi h,fidssiz ;rbuffer size
shld fidélen s3set buff len
if md=1 srinput file

shld fidsptr ;;cause immediate read
mvi c,@opn ;;open file function
else s;output file

lLxi h,@ s7set next to fill

shld fid&sptr ;;pointer initialized
mvi c,@del

lxi d,fcbé&fid ;s;delete file
call abdos ;7to clear existing file
mvi c,@mak ;;create a new file
endif

now open (if input), or make (if output)
lxi d,fcbafid

call @bdos ;;open/make ok?

inr a 33255 becomes 60

jnz pmsg

mvi c,@msg ;;print message function
Lxi d,msg ;7e@rror message

call @bdos trprinted at console
jmp filerr ;;to restart

db er,lf

if md=1 rrinput message

db “no &FID file’

else

db “no dir space: &FID\*

endif

db “$°

endm

-

brary (Con't).

i

Sequential File I/O L

Figure 53d.

---
_Page 152_

’

finis macro fid :

3 close the file(s) given by fid rename macro new,old

irp ?PE,<fid> a rename file given by “old" to "new"
skip all but output files local psub,reng

if ?f&typ=2 include the rename subroutine once
local eob? ,peof ,msq,pmsg jmp psub

write all partially filled buffers @rens: ;;rename subroutine, hl is address of
yiare we at the end of a buffer? 3:0ld fcb, de is address of new fcb
lhla ?fsptr ssnext to Fill push h ssave for rename
mov a,l tron buffer boundary? lxi b,16 :b=90,c=16

ani (@sect-1)} and @ffh dad b shl = old fcb+l6
jnz peof srput eof if not 08 ren@: ildax a pnew fcb name

if @sect>255 mov m sto old fceb+lé

ae
oe

Oo
rare)
on @

mov a,h inx snext feb char
ani (@sect-1) shr 8 der scount down from 16
jnz peof trput eof if not 068 jnz Teng
endif old name in first half, new in second half
arrive here if end of buffer, set length pop d srecall base of old name
and write one more byte to clear buffs mi c,@ren yrename function

shld ?f&len ;;set to shorter length call @bdos
peof: mvi a,eof srwrite another eof ret

push psw ;3save zero flag psub:
call puts?f rename macro n,o z;;rtedefine rename
pop psw yirecall zero flag lxi h,fcbso ;;old fcb address

er ts
bie}

srename complete

jnz eob? s3non zero if more xi d,fcb&n ;;new fcb address
buffers have been written, close file call @rens trename subroutine
mvi c,@cls endm
ixi d,fcb&?7£ s3ready for call rename new,old
call @bdos endm
inr a 77255 if err becomes 66 H
jnz pmsg get macro dev
file cannot be closed of read character from device
mvi c,@msq if @sdev <= @lst
lxi d,msg simple input
call @bdos myi c,@&dev
jmp Pmsg y7@rror message printed call @bdos
msg: db er,lf else
db “cannot close &?F” call get&dev
ab “$° endm

pmsg:
endif
endm 3;0f the irp t macro dev
endm write character from accum to device
; if @&dev <= @lst
erase macro fid simple output
3 delete the file(s) given by fid push psw s3Save character
irp ?£,<fid> mvi c,@&dev s;write char function

3 oe
es Die

os

Ixi d,fcbs?f call @bdos twrite character
call @bdos pop psw ;eestore for testing
endm :;0f the irp else
endm call puté&dev
endm
irect macro fid
perform directory search for file
sets zero flag if not present
Lxi d,fcbé fid
mvi c,@dir
call @bdos
ine a 708 if not present
endm

Cotas

Figure 53e. Sequential File I/O Library (Con't).

---
_Page 153_

may be changed in the user's program to "trap" error conditions rather than rebooting.
The use of FILERR is apparent throughout the macro library.

The equates which follow define the usual BDOS entry points and functions,
along with the diskette sector size (@SECT), and special non-graphic characters (EOF,
CR, LF, and TAB). The equates for @KEY through G@LST are used in the GET and
PUT macros to determine the source or destination device. The INFILE, OUTFILE,
and SETFILE equates are used in the FILE macro as mnemonics for the file mode
attribute.

Referring again to Figure 53a, FILLNAM is a utility macro which is used in the
construction of a file control block. In particular, it accepts a file name or file type
along with a field size and builds a sequence of DB's which fill the name or type field
with padded blanks. FILLDEF is again a utility macro similar to FILLNAM, but fills
the file control block name or type field from the default file control block at @TFCB
or @TFCBt16. FILLDEF is invoked to extract either the default file name (first 8
characters) or default file type (following 3 character field). Note that the FILLDEF
macro constructs an inline subroutine to perform the data move operation the first
time it is invoked and calls the inline subroutine (Q@DEF) upon subsequent invocations.

The last macro definition shown in Figure 53a is FILLNXT which is used to
initialize two assembly time variables: G@NXTB and @NXTD. @GNXTB is used to count
the accumulated size of buffers as they are automatically allocated in the FILE
statement, while QNXTD is used to count files in the FILE macro for later reference
in GET and PUT statements. They are included within a macro so that they will be
properly initialized in the two successive passes of the macro assembler. FILLNXT
is invoked by the FILE macro where the expansion initializes @NXTB and G@NXTD.
Note that FILLNXT then redefines itself as an empty macro so that subsequent FILE
invocations do not reset the two counters.

A major utility macro, called FILLFCB, is shown in Figure 53b. The primary
purpose of this macro is to construct a file control block in the CP/M standard format,
where FID is the file identifier, DN is the disk name, FN is the file name, FT is the
file type, BS is the buffer size, and BA is the buffer address, as described in the FILE
statement above. Recall that some of these parameters may be empty, causing default
conditions to be selected. The FILLFCB macro begins by searching for a "1" or a "2"
as the FN parameter, indicating that either default name 1 or 2 is to be selected for
the file. Note that the IRPC loop involving ?C will result in a value of 1 for @C if
either FN=1 or FN=2, and a value of 0 for @C if FN is not 1 or 2. The FILLFCB
macro then selects either the default name, or the user specified name along with the
default or user specified drive number. The equate for FCB&FID then produces the
address of the file control block for the file identifier followed by "DB 0" for the
extent field and "DS 20" for the remainder of the file control block. The reader may
wish to cross-reference the file control block format shown in the CP/M Interface
Guide for exact formats.

The remainder of the FILLFCB macro, shown in the lower half of Figure 53b,
is devoted to storage allocation for buffer areas. The @BS variable is set to the
buffer size after rounding and size checks. FID&BUF then becomes the address of
the file's buffer area, and FID&ADR labels a "DW" containing this literal value.
FID&SIZ becomes the literal size of the buffer, and FID&LEN labels a "DW" containing
the literal size. FID&PTR is also allocated as a double byte which will subsequently

---
_Page 154_

hold the buffer index to the next character to get or put in the file. All of these
values will be used in the file operations which occur later.

The principal file access macro, called FILE, is shown in Figure 53c, and is
used to set up the file control block, buffers, and access subroutines for a particular
file. Similar to the FILLFCB macro, the parameters FID, DN, FN, FT, BS, and BA
describe the particular characteristics of a file. The MD parameter, however, is
present to indicate the file mode and must have the value 1, 2, or 3. The FILE macro
begins by assigning the mode value to FID&KTYP so that subsequent macros can determine
the type of access for this file. The FILLFCB macro is then invoked to construct
the file control block for this macro, and sets generally useful parameters for the file,
as discussed above. The FILE macro then generates either the label GET&FID or
PUT&FID for input and output files, respectively, followed by a subroutine which GET's
a single character or PUT's a single character for this file.

In general, the GET&FID reads a single character from the input buffer and,
when the input buffer is exhausted, fills the buffer area again in preparation for
following GET operations. Upon detecting a real end of file, the EOF character is
returned with the zero flag set. Similarly, the PUT&FID subroutine generated for
output files stores the accumulator character into the output buffer at the next
character position and, when the buffer is full, writes the sequence of sectors and
returns to accept more output characters. In the case of an output error, the appropriate
message is printed, and control transfers to FILERR which usually remains at 0000H,
causing a system reboot.

The generated code which follows the label PSUB in Figure 53d is used to
initialize the file pointers to the proper positions for file access. The file extent and
next reeord fields of the file control blocks are zeroed for both input and output files.
In the case of an input file, the buffer index variable FID&PTR is set to the end of
the buffer, causing an immediate read operation when the first character is read. In
the case of an output file, the FID&PTR is set to zero, indicating that the next
position to fill is the first character of the output buffer. If the file is an output
file, any duplicate files are erased, and a new file is created. In both eases, the file
is opened upon completion of the FILE operation, and the buffer pointers are set for
the next GET or PUT invocation. Note that the FILE statement is "executable" in
the sense that it must occur ahead of the GET or PUT statements for the file, and
performs its function each time contro] passes through the FILE machine code.

The FINIS, ERASE, DIRECT, RENAME, GET, and PUT macros are shown in
Figure 53e. The FINIS macro, shown on the left, serves to empty the output buffers
and close the file for output. Input files are skipped since no actions need take place
to close an input file. The main purpose of the FINIS macro is to fill the remaining
buffer segment (one sector size) with EOF's, then write the partially filled buffers.

The ERASE macro accepts a file identifier or list of file identifiers and
successively calls the BDOS to erase each file, while the DIRECT macro searches only
for a single file given by the file identifier FID. In the ease of the DIRECT macro,
the non-zero flag is set if the file exists. No prechecks are made to see if the file
exists before the ERASE operation takes place, although erasing a non-existant file is
of no consequence. The DIRECT macro can, of course, be used to check if a file
exists before the ERASE is executed if deemed necessary by the programmer.

---
_Page 155_

The RENAME macro shown in Figure 53e (right) allows a file to be renamed
by accepting two file identifiers, denoted by NEW and OLD. These file identifiers
must correspond to the FCB names created by FILLFCB in an earlier FILE invocation,
and has the effect of renaming the OLD file to the NEW file name. This is accomplished
within the RENAME macro through an inline subroutine, called @RENS, which is
included the first time the RENAME macro is invoked. The inline subroutine moves
the new file control block information (first 16 bytes) into the second half of the old
file name in the form required for a rename operation under CP/M (see the CP/M
Interface Guide), The BDOS is then called to perform the rename function. Note
again that there is no check to ensure the old file exists before the rename takes
place.

The GET and PUT macros shown in Figure 53e are similar in structure: both
accept a device or file identifier as the formal parameter DEV, and perform the
corresponding input or output function on that device. If the device is a simple
peripheral, the BDOS is called directly to perform the input or output function. If
instead, the device name was created by a FILE macro, the corresponding GET&FID
or PUT&FID subroutine is called to accomplish the input or output operation. Note
that the accumulator is preserved (PUSH PSW) upon output to a simple peripheral
within the PUT macro, while the save/restore sequence is performed within the PUT&FID
subroutine if the destination is a diskette file.

Figures 54a, 54b, and 54ec show the full expansion of a segment of the case
conversion program of Figure 52 (using the "+M" assembly parameter). Figure 54a
shows the invocation of FILE, followed by FILLFCB, again followed by FILLDEF. The
@DEF subroutine is included inline, and the FILLDEF macro is redefined to exclude
the subroutine. Upon completion of the FCB construction, the file parameters are
generated, as shown in Figure 54b, along with the beginning of the GETSOURCE
subroutine. Note that the conditional assembly ignores the portions of this FILE macro
expansion which are related to output files while including the machine code for the
input SOURCE file. In each case, the "&FID" labels result in names with the prefix
or suffix "SOURCE" in order to associate the generated labels with this particular
internal name. Figure 54¢c contains the end of the PUTSOURCE subroutine, followed
by the machine code which initializes the file control block fields and buffer pointer.
Upon completion of the FILE macro, the SOURCE file is ready for access. In particular,
each call to GETSOURCE reads one more character into the accumulator. Due to
the length of the expanded macro form, the remainder of the case translation program
is not shown.

In order to illustrate the facilities of the SEQIO macro library, two additional
programs are given. The first, called PRINT, formats the output from the macro
assembler for printing on the system line printer. The second, called MERGE, performs
a simple merge operation on two diskette files.

The PRINT program, shown in Figure 55, is executed under the console command
processor by typing

PRINT filename
where "filename" is the name of a previously assembled program. PRINT assumes that

there is a "PRN" file on the diskette, and possibly a "SYM" file on the same diskette
drive. The PRN file is first printed, with a form feed at the top of each 56 line

---
_Page 156_

+
+
O6G1+=
+
+
O001+4
+

@BOC+#

Sete teeeegeeeet

8183+C3OFG1
+
@1B6+7E
8187412
B198+23
\#149413
G18A+OD
@10B+C20681
B1GE+C9

tet ee ete

+
@1BF+215C89
6112+111D81
8115+8EOC
6117+CD6681

+

+
B11A+C34481
611D+

+
OOB0+#

FEE HEHEHE HEHEHE HEHEHE HEH HH Het

@11D+=
8129+08
@12A+

FILE INFILE,SOURCE,,1,,29088
LOCAL  PSUB,MSG,PMSG
LOCAL PND,EOD,EOB,PNC
SOURCETYP EQU INFILE
FILLFCB SOURCE,,],,2008,
LOCAL  PFCB
ec SET 1
IRPC 2?C,1
IF NOT (°&?C° = “1° OR “&?C° = °2°)
ec SET )
ENDM
IF NOT (‘1’ = “1° OR “1° = °2°)
ec SET 8
ENDM
IF ec
IF NUL
@c SET 12
ELSE
ec SET 9
ENDIF
FILLDEF FCBSOURCE, (1-1) \*16,@C
LOCAL  PSUB
JMP ??6689
@DEF: .
MOV A,M =
STAX D o
INX B E
INX D 80
DCR c Nn
JINZ @DEF ce
RET °
??0089: Z
FILLDEF MACRO ?FCB,?F,?L I
LXI H,@TFCB+?F OQ
LXI D,?FCB ha
MVI C,?L
CALL @DEF fx
ENDM rat
FILLDEF FCBSOURCE, (1-1) \*16,@C fy
LXI H,@TFCB+(1-1) \*16 ©
LXI D, FCBSOURCE a,
MVI C,@c E
CALL @DEF w
ENDM wv
ENDM
IMP 278008 3
DS ec Se
us
@CNT SET 12-@C ©
IRPC ?FC, 5
IF @CNT=6 OR NUL ?FC ,O0
EXITM fy
ENDIF
DB “S?PC\*
@CNT SET @CNT-1
ENDM
IF @CNT=6 OR NUL
EXITM
REPT @CNT
DB “\*
ENDM
ENDM
ELSE
JMP 2?B008
IF NUL
DB 8
ELSE
DB “oR 4]
ENDIF
FILLNAM 1,8
FILLNAM ,3
ENDIF
FCBSOURCE EQU $-12
DB 6
DS 29

---
_Page 157_

+

+
OOBG+#
BGO6+#

tee tet

A788+#
+
+
8376+4#
878B+%
+
+
+
+
913E+7863
8786+=
+
6146+8897
+
8142+
OB06+4
GOB7+4%
+
+
+
+

+
9144+C3B491

terete

6147+2A4901
614A+EB
@14B+2A4201
@14E+7D
G14F+93
@158+7C
6151+9A
8152+DA9D@1
9155+2128008
9158+224201
+

B1SB+EB
O@15C+2A4981
Q15F+7B
G166+95
6161+7A
8162+9C
6163+D28Fd61
0166+2A3E81
8169+19
816A+EB
B16B+HE1LA
816D+CD8588
017@+111D@1

+
G173+0E14

+

+

+
8175+CD8500
@178+B7
9179+C28961
917C+118000
617F+2A4261
8182+19
9183+224201
0186+C35B01

IF

FILLNXT

@NXTB SET
@NXTD SET
PILLNXT
ENDM
ENDM
IF
@BS SET
ELSE
@BS SET
ENDIF
IF
SOURCEBUF
@NXTB SET
ELSE
SOURCEBUF
ENDIF
SOURCEADR:
DW
SOURCESI2
SOURCELEN:
DW
SOURCEPTR:
DS
@SOURCE
@NXTD SET
ENDIF
??6808:
If
EXITM
ENDIF
JMP
IPF
GETSOURCE:
ELSE
PUTSOURCE:
PUSH
ENDIF
LHLD
XCHG
LHLD
MOV
SUB
MOV
SBB
Jc
LXI
SHLD
??0604:
XCHG
LHLD
MOV
SUB
MOV
SBB
JNC
LHLD
DAD
XCHG
MVI
CALL
LXI
IP
MVI
ELSE
MVI
ENDIF
CALL
ORA
JNZ
LXI
LHLD
DAD
SHLD
JMP

SOURCETYP<=2

]
@LST+1
MACRO

2000+0<@SECT
@SECT

(28008/@SECT) \*@SECT

NUL
SET BUF FERS+@NXTB
@NXTB+@BS

SET

SOURCEBUF
EQU @BS

@BS

SET @NXTD
@NXTD+1

ENDM
INPILE=3

2206081
INFILE=1

PSW
SOURCELEN

SOURCEPTR
A,L

E

A,H

D

228087
H,@
SOURCEPTR

SOURCELEN
A,E

L

A,D

? 20006
SOURCEADR
D

C,@DMA
@BDOS
D,FCBSOURCE
INFILE=1
C,@FRD

C,@FWR

@BDOS

A

??6885
D,@SECT
SOURCEPTR
D
SOURCEPTR
??8004

Sample FILE Expansion Segment (Con't).

Figure 54b.

---
_Page 158_

+
+

8189+2A4261

818C+224601

teeter etree eeete

818F+118860
8192+6E1A
6194+CD6508
0197+218886
619A4+224281

+
819D+EB
O19E+2A3E61
81A1+19
@1A2+EB

+
61A3+2A4061
61A6+7D
G1A7+B4
O1A8+3E1A
G1AA+C8
B1AB+1A

+

+

+

+
B1AC+2A4261
B1lAF+23
@1BG+224281
81B3+C9

+
81B4+AF
91B5+322961
61B8+323D61
61BB+218867
@1BE+224961

+
@1C14+224281
@1C4+6EGF

+e tte t tet

01C6+111D61
81C9+CDG508
B1CC+3C
@1CD+C2ECG)
G1D8+GE89
@1D2+11DB81
81D5+CDO508
G1D8+C38089
81DB+8D8A
+

@1DD+6E6F26534F

+

+

+
@1EB+24

+

+

??9005:

EMSG:

??6066:

2? ?B867:

??8081:

220802:

XHLD

SHLD
MVI
ELSE
LXI
SHLD
MVI
LXI
CALL
MVI
ENDIF
LXI
CALL
INR
JNZ
MVI
LXI
CALL
JMP

IF

DB
ELSE
DB
ENDIP

??8063:

ENDM

INFILE=1
SOURCEPTR
SOURCELEN

EMSG

C,@MSG

D,EMSG

@BpOs

PSW

FILERR

CR,LF

“disk full: SOURCE’
“$

D,@TBUF
C,@DMA
@BDOS

H,@
SOURCEPTR

SOURCEADR
D

INFILE=1
SOURCELEN
A,L

H

A,EOP

D

PSW
Dd

SOURCEPTR
H
SOURCEPTR

A
FCBSOURCE+12
FCBSOURCE+32
H,SOURCESIZ
SOURCELEN
INFILE=1
SOURCEPTR
C,@OPN

H,®
SOURCEPTR
C,@DEL
D,PCBSOURCE
@BDOS
C,@MAK

D,FCBSOURCE
@BDOS

A

2??8083

C,@MSG
D,??0002
@BDOS

FILERR

DB CR,LF
INFILE=1

“no SOURCE file’

“no dir space: SOURCE”

5°

Sample FILE Expansion Segment (Con't).

Figure 54c.

---
_Page 159_

page. If the SYM file exists, it is also printed using the same formatting. If the
files are sucessfully printed, they are both erased from the diskette.

Referring to Figure 55, the PRINT program begins by saving the console
processor's stack, with the intention of returning directly to the CCP, without a system
reboot. The input printer file is then defined with a FILE statement which specifies
the internal name PRINT, and obtains the file name from the console command line.
The file type, however, is set to PRN in this case. After performing an initial page
eject, the program loops between the PRCYC (print cycle) and ENDPR (end print)
labels by successively reading characters from the PRINT source, and writing to the
printer through the LISTING subroutine. On detecting an end of file character, control
transfers to the ENDPR label where the PRN file is erased from the diskette.

As shown on the left of Figure 55, the program then checks for the presence
of the SYM file by invoking the FILE macro with a SETFILE mode. This creates the
proper file control block for the input file with type SYM, but does not create buffers
nor does it open the file for access. Following the FILE macro, the DIRECT statement
performs a directory search and, if the file is not present, control transfers to the
ENDLST (end listing) label where execution terminates.

If the SYM file exists, the program proceeds to perform another page eject,
and then opens the SYM file for access. It should be noted that the third FILE macro
(Figure 55, left) accesses the SYM file using the internal name SYMBOL, but shares
the buffer areas of the PRINT file. This is possible since the PRINT file has been
erased at this point in the program and thus the buffers are available for use.

If the SYM file is present, the program loops between the SYCYLE (symbol
eyele) and ENDSY (end symbol) labels where characters are read from the SYMBOL
file and again sent to the printer through the LISTING subroutine. Upon detecting
the end of file, control] passes to the ENDSY label where the SYM file is removed
from the diskette. If no errors occur, control eventually reaches the ENDLST label
where the printer page is ejected. The entry stack pointer is then retrieved from
OLDSP, and control returns to the console command processor, thus completing execution
of the PRINT program.

The next program, called MERGE, is somewhat more complicated. The purpose
of the MERGE program is to accept two file names as input, taking the general
command form

MERGE filename

where "filename" is the name of a master file, with assumed file type of MAS, as
well as an update name with assumed file type UPD. The files consist of text files
with varying length records, starting with a six character numeric "sequence number"
followed by textual material, and terminated with a carriage-return line-feed sequence.
The lines of information in the master and update files are assumed to be in ascending
numeric order according to their sequence numbers. The purpose of the MERGE
program is to read these two files and "shuffle" the records together to form a new
file consisting of numerically ascending sequence numbered lines.

Upon completion of the merge operation, the newly merged file becomes the
new master file: update records are properly interspersed within the new master file

---
_Page 160_

@806Cc

G16A

@1F2
@1FS
81F8
G1FA
81FD

926B
623A

@32B
O32E
\#331

833C
O33F

219808

22CF83
31CFB3

CDBAG3

FEA

CAB362
CD5183
C3F561

CA3C83

CD8A63

FEIA

CA3463
CD5163
C32603

CDBAG3
2ACFO63
F9
c9

F
A

ow oe es fay SE ek oe

PRCYC:

ENDPR:

en on

XLINE EQU 56

ORG 106H

MACLIB SEQIO ;SEQUENTIAL I/O LIB
PRINT THE X,PRN AND X.SYM FILES ON THE
LINE PRINTER WITH PAGE FORMATTING,

EQU BCH ;FORM FEED

;MAX LINES PER PAGE

SAVE THE ENTRY STACK POINTER

LXI B,@

DAD SP rENTRY SP TO HL
SHLD OLDSP +SAVE ENTRY SP

LXKI SP,STACK;SET TO LOCAL STACK

FILE INFILE,PRINT,,1,PRN,1000

READ THE PRINT FILE UNTIL END OF FILE
CALL EJECT sTOP OF PAGE

GET PRINT

CPI EOF

Jz ENDPR ?SKIP IF END FILE
CALL LISTING ;WRITE TO LISTING DEV
oMP PRCYC

7END OF PRINT FILE, DELETE IT

ERASE PRINT

CHECK FOR THE OPTIONAL .SYM FILE
FILE SETFILE,SYMCHK,,1,SYM
DIRECT SYMCHK ;IS IT THERE?

JZ ENDLST ;SKIP SYMBOL IF SO

SYMBOL PILE IS PRESENT, PAGE EJECT
CALL EJECT 7TO TOP OF PAGE
FILE INFILE,SYMBOL,,1,SYM,1006,PRINTBUF

;
SYCYCLE:

ENDSY:

ENDLST:

GET SYMBOL
CPI EOF

JZ ENDSY ;SKIP TO END ON EOF
CALL LISTING :SEND TO PRINTER
IMP SYCYCLE ;FOR ANOTHER CHAR
ERASE SYMBOL ;ERASE .SYM FILE

tEND OF LISTING ~ EJECT AND RETURN
EJECT
OLDSP

CALL
LHLD ;ENTRY STACK POINTER
SPHL sRESTORE STACK POINTER
RET 7TO CCP

Figure 55.

@34C
634F

@35A
@35D
635F
636B
B36C
836D
O36F

637B
O37E
838i
638A
838C
638F
O3CF

43D)
@3D2

63D3

210283
cg

FE@C
C25F83
AF
32D183
32D243
3E9C
PEGA
C274863
AF
32D243
21D103

TE
PE38
D8
3E9C
FE69
C28783

3E20
CD4463
3AD283
E607
C27983

co

C34463

3E9C
34483

:
?

UTILITY SUBROUTINES

LISTOUT:

;

3;SEND A SINGLE CHARACTER TO THE PRINTER
PUT LST

LXT H,CHARC ;CHARACTER COUNTER

INR M s INCREMENT POSITION

RET

LISTING:

LIST@:

LIST1s

TABOUT:

LIST2:

EJECT:

oe en

STACK:
OLDSP:
LINEC:
CHARC:

;WRITE CHARACTER FROM REG-A TO LIST DEVICE
CPI FF ;FORM FEED?
JNZ LIST@
XRA A
STA LINEC
STA CHARC
MVI A, FF
CPI LF
INZ LIST1
XRA A
STA CHARC
LXI H,LINEC ;LINE COUNTER
INR M : INCREMENTED
MOV A,M ;CHECK FOR END OF PAGE
CPI MAXLINE ;LINE OVERFLOW?
RC ;RETURN IF NOT
MVI 4,8 :;CLEAR LINEC
MVI A,FF SEND PAGE EJECT
CPI TAB °TAB CHARACTER?
INZ LIST2
FEED BLANKS TO NEXT TAB POSITION

MVI A,” \*
CALL LISTOUT
LDA CHARC
ANI 7H ;MOD 8
JNZ TABOUT ;FOR ANOTHER BLANK
ON CHARACTER BOUNDARY
RET
;SIMPLE CHARACTER
JMP LISTOUT ;PRINT AND RETURN

?CLEAR LINE COUNT

;CLEAR TAB POSITION
;RESTORE FORM FEED
7END OF LINE?

;CLEAR TAB POSITION

;CHARACTER POSITION

;PERFORM PAGE EJECT
MVI A,FF ;FORM FEED
JMP LISTOUT

DATA AREAS
DS 64 332 LEVEL STACK
DS 2
DS 1
bs 1

yENTRY STACK POINTER
;LINE COUNTER
;CHARACTER COUNTER

BUFFERS:

Program for Line Printer Page Formatting.

END

---
_Page 161_

according to numeric order, and any update record which matches a master record
results in replacement of the master record by the update record. Upon successful
completion of the merge operation, the original master file is renamed to have the
extension MBK (master back-up), the original update file is renamed to the type UBK
(update back-up), and the newly created file becomes the new MAS file. In this way,
the operator can return to the backup files in case of error so that the source data
is not destroyed.

The MERGE program is shown in Figures 56a, 56b, and 56c. Utility subroutines
are listed first in Figure 56a, including the DIGIT subroutine which tests for valid
decimal digits in sequence numbers. The IRPC which follows the DIGIT subroutine
generates two distinct subroutines, called READU and READM for reading the update
and master files, respectively. The generation of these two subroutines has been
suppressed in the listing (see the $+PRINT and $-PRINT inline parameters) to keep the
listing short. In general, these two READ subroutines fill their respective sequence
number buffers from the input source so that the merge operation can take place
based upon the current sequence number values. Upon detecting an end of file, the
sequence number is set to OFFH as a signal that the input source has been exhausted.

The utility subroutines shown in Figure 56b include SEQERR, WRITESEQ, and
COMPARE. The SEQERR subroutine reports an error condition when a non numeric
character is detected in the sequence number field. Although the error reporting is
somewhat spartan, sequence errors are easily found using the TYPE command on the
master or update file. The WRITESEQ subroutine sends the buffered sequence number
addressed by HL to the new file. WRITESEQ is called whenever the source for the
next record has been determined. The COMPARE subroutine is used to determine the
next source record (master or update) by comparing the buffered sequence numbers
from left to right while they are equal. If a mismatch occurs in the sequence number
scan, COMPARE returns with the carry flag and zero flag set to indicate which file
holds the next source record.

Execution of the MERGE program begins following the START label in Figure
96b where the update, master, and new files are defined. The UFILE and MFILE
sources are defined with the same buffer sizes (as determined by the earlier USIZE
and MSIZE equates). Both take their primary name from the default value specified
at the CCP level by the operator. The new file is created as a temporary, with name
TEMP and type $$$, but will be altered upon completion of the program to become
the master file.

The merge operation proceeds in Figure 56b as follows. First the READU and
READM subroutines are called to fill the sequence number buffers. The loop between
MERGE and ENDMERGE in Figure 56c is then repetitively executed until the merge
is complete. On each iteration of this loop, the COMPARE subroutine is called to
eompare the buffered sequence numbers. If the update sequence number is smaller
than the master sequence number, it is moved to the new file and data is copied from
the update file to the new file until the end of the current record is encountered.
Upon completion of the copy operation, the READU subroutine is called again to refill
the update sequence number buffer.

If the COMPARE subroutine instead detects equal sequence numbers, control

transfers to the SAME label in Figure 56e where master record is deleted. Alternatively,
the COMPARE subroutine will cause control to transfer to the MASLOW label when

---
_Page 162_

83E8
83E8
67D@

610B
816C

81@D

@11lE

ORG 160H
FILE MERGE PROGRAM

MACLIB SEQIO ;SEQUENTIAL FILE I/O
= BOOT EQU O@600H SYSTEM REBOOT
= SEQSIZ EQU 6 sSIZE OF THE SEQUENCE #°S
= USIZE EQU 1869 ;UPDATE BUFFER SIZE
= MSIZE EQU USIZE sMASTER BUFFER SIZE
= NSIZE EQU USIZE+MSIZE ;NEW BUFF SIZE
’
31EC@5S LXI SP,STACK
C3C8é1 JMP START sTO PERFORM THE MERGE
,
; UTILITY SUBROUTINES
DIGIT: ;TEST ACCUMULATOR FOR VALID DIGIT
; RETURN WITH CARRY SET IF INVALID
FE3@ CPI “e°
D8 RC ;CARRY IF BELOW @
FE3A CPI “Q°4+] ;CARRY IF BELOW 146
3F CMC ;NO CARRY IF BELOW 14
cy RET
: ERROR MESSAGES FOR READU AND READM
SEQERRU:
7570646174 DB “update seq error’,@
SEQERRM:
6061737465 DB “master seq error’ ,@
; GENERATE READU AND READM SUBROUTINES
IRPC ?F,UM
: INLINE SEQUENCE NUMBER BUFFER
?P&SEQ: DB 6 ;TO START PROCESSING
DS SEQS12-1;REMAINING SPACE FOR SEQ#
READ&?F:
LXI H,?F&SEQ ;SEQUENCE BUFFER
MOV A,M 31S IT FF (END FILE)?
INR A :FF BECOMES 940
RZ ;SKIP THE READ
; READ THE SEQUENCE NUMBER PORTION
MVI C,SEQSIZ ;SIZE OF SEQUENCE #4
RD&? FEB:
PUSH H sSAVE NEXT TO FILL
PUSH B \*SAVE NUMBER COUNT
GET ?F&PFILE ;READ THE FILE
POP B ;RECALL COUNT
POP 8 s;RECALL NEXT FILL
CPI EOF sEND FILE?
JZ EOFS&?F
CALL DIGIT sASCII DIGIT?
LXI D,SEQERR&?F sERROR MESSAGE
JC SEQERR ;SEQUENCE ERROR
; NO SEQUENCE ERROR, FILL NEXT DIGIT POSITION
MOV M,A
INX H NEXT TO FILL
DCR Cc 3 COUNT=COUNT-1
JINZ RD&? FEO 3;FOR ANOTHER DIGIT
RET ;END OF PILL
EOF&?P: ;END OF FILE, SET SEQ# TO @FFH
MVI A,@FFH
STA ?F&SEQ 3;SEQ# SET TO FF
RET
ENDM

Figure 56a. File Merge Program.

---
_Page 163_

O18F
81998
g191l

@194
@195
819D
G19E
B19F

B1A2
O1A4
@1A5
G1A6
@1A7
61A8
O1AB
B1AC
G1AD
@1AE
g1Bl

61B2
61B5
81B8
@1BA
61BB
g18Cc
61BD

B1BE
81CO
81C1
61C2
81C3
@1C4
81C7

81C8

82BO

838C

847D

848C
@48F

1A
B7
CAGBGG

DS

Dl
C38FOL

BEB6
7E
E5
cs

cl
El
6D
C2A481
c9

112F61
215F61
GEO6
1A

BE

D8

CO

FEFF
C8

8D
C2BA61
cg

CD3581
CD6591

CDB261
CAAD@4
D2C884

212F81
CDA2@1

SEQERR:

’

WRITE ERROR MESSAGE FROM (DE) TIL 66
LDAX D

ORA A

JZ BOOT
; OTHERWISE, MORE TO PRINT

PUSH D

PUT CON ;WRITE TO CONSOLE

POP D

INX D

JMP SEQERR ;FOR MORE CHARS
WRITESEQ:

WRITO:

ro)

;WRITE THE SEQUENCE NUMBER GIVEN BY HL
;TO THE NEW FILE

MVI C,SEQSIZ ;SIZE OF SEQ#
MOV A,M

INX H \*NEXT TO GET
PUSH H ;SAVE NEXT ADDR
PUSH B ;SAVE COUNT

PUT NEW ;WRITE TO NEW
POP B 7RECALL COUNT
POP H ;RECALL ADDRESS
DCR Cc ; COUNT=COUNT-1
JNZ WRITS ;FOR ANOTHER CHAR
RET

COMPARE THE UPDATE SEQUENCE NUMBER WITH
THE MASTER SEQUENCE NUMBER, SET:

CARRY IF UPDATE < MASTER

ZERO IF UPDATE = MASTER

-ZERO IF UPDATE > MASTER

OMPARE:
LXI D,USEQ ;UPDATE SEQ#
LXI H,MSEQ ;MASTER SEQ#
MVI C,SEQSIZ ;SEQUENCE SIZE
CLOOP: LDAX D ;UPDATE DIGIT
CMP M ;UPDATE-MASTER
RC ;CARRY IF LESS
RNZ sNZERO IF GTR
; ITEMS ARE THE SAME, CHECK FOR @FFH
CPI OFFH 7END OF FILE
RZ 7;BOTH ARE @FFH
INX D ;NEXT UPDATE
INX H ;NEXT MASTER
DCR c 7COUNT DOWN
JNZ CLOOP 7FOR ANOTHER DIGIT
RET 7;ZERO FLAG IF EQUAL
’
? MAIN PROGRAM STARTS HERE
START:
;UPDATE FILE, WIT.’ ASSUMED .UPD TYPE
FILE INPILE,UFILE,,1,UPD,USIZE
?
;MASTER FILE, WITH ASSUMED TYPE .MAX
FILE INFILE,MFILE,,1,MAS,MSIZE
’
;NEW PILE, TEMP,.$$$ (RENAMED UPON EOs’S,
FILE OUTFILE,NEW, ,TEMP,SS$,NSIZE
CALL READU ;INITIALIZE UPDATE RECORD
CALL READM 7INITIALIZE MASTER RECORD
MERGE: ;MAIN MERGING LOOP
CALL COMPARE ;CARRY SET IF UPDATE<MASTER
JZ SAME ;ZERO IF IDENTICAL SEQ#
INC MASLOW ;MASTER LOW?

Figure 56b.

UPDATE SEQUENCE NUMBER IS LOW
LXI H,USEQ ;COPY SEQUENCE NUMBER
CALL WRITESEQ;WRITE THE SEQUENCE #

File Merge Program (Con't).

---
_Page 164_

B49A
@49C
O49F
O4Al
@4A4

Q4A7
B4AA

84AD
84B6
84B2

84B5
84B8
64BA
84BD
64BF
84C2
g4C5

84C8
84CB
B4CE
g4D1l
84D2
84D5
@4D6
84D8
04DB
84DD
O4E6

84E3
64E6

@4E9

O5AF
65B7
65C8
85C9

@5CC

146C
O@5EC

FS

Pl
FEGA
CAA7@4
PE1A
CAA764
C39204

CD3581
C38304

3A5FO1
FEPFP
CAE9B4

FELA
CAC204
FE@A
C2B504
CD6561
C38304

215F61
CDA2@1

FS

Fl
FE@A
CAE394
PEA
CAE3@4
C3CE84

CD6561
C38304

C308000

ULCOP: ;UPDATE RECORD TO NEW FILE

GET UFILE ;CHARACTER TO A

PUSH PSW ;SAVE IT

PUT NEW OUTPUT TO NEW FILE

POP PSW ;RECALL CHARACTER

CPI LF ;LINE FEED?

Jz ENDUP

CPI EOF

J2 ENDUP

IMP ULOOP ;CYCLE IF NOT END REC
,
ENDUP: CALL READU  ;READ ANOTHER SEQ#

JMP MERGE  ;FOR ANOTHER RECORD
SAME: ;SEQUENCE NUMBERS ARE IDENTICAL

LDA MSEQ ;CHECK FOR @FFH

CPI @FFH

J2 ENDMERGE
: NOT THE SAME, DELETE MASTER RECORD
DELMAS: GET MFILE

CPI EOF ;END OF FILE?

J2 GETMAS ;GET SEQ# FF

CPI LF

INZ DELMAS ;POR ANOTHER CHAR
GETMAS: CALL READM ;TO NEXT RECORD

JMP MERGE ;FOR ANOTHER
MASLOW: :MASTER SEQUENCE NUMBER IS LOW

LXI H,MSEQ

CALL WRITESEQ;SEQUENCE NUMBER
MLOOP: GET MPILE

PUSH PSW :SAVE MASTER CHARACTER

PUT NEW

POP PSW ;LF OR EOF?

CPI LF

J2 ENDMS

CPI EOF

Jz ENDMS

JMP MLOOP  ;MORE TO COPY
ENDMS: CALL READM  ;READ NEW SEQ NUMBER

JMP MERGE ;TO MERGE ANOTHER
ENDMERGE:

;CLOSE ALL FILES FOR RENAMING

FINIS <UFILE,MFILE,NEW>

;OLD MASTER FILE FOR ERASE/RENAME

FILE SETFILE,OLDMAS,,1,MBK

ERASE OLDMAS

;RENAME MASTER TO .MBK

RENAME OLDMAS,MFILE

;OLD UPDATE FILE FOR ERASE/RENAME

PILE SETFILE,OLDUPD,,1,UBK

ERASE  OLDUPD

;RENAME UPDATE TO .UBK

RENAME OLDUPD,UFILE

;RENAME NEW TO MASTER FILE

RENAME MPILE,NEW

IMP BOOT

DS 32 :16 LEVEL STACK
STACK:
: BUFFER AREA
BUFFERS:
MEMSIZE EQU BUFFERS+@NXTB  ;END OF MEMORY

END

Figure 56c. File Merge Program (Con't).

---
_Page 165_

the master sequence number is low. In this case, the master sequence number and
data record are copied to the new file in exactly the same manner as an update
record.

Upon completion of the merge operation (end of file detected in both the update
and master files), control transfers to the ENDMERGE label where the files are closed
and renamed. Following the FINIS statement, the previous MBK file (possibly from
an earlier execution) is erased so that the current master (MAS) can be renamed to
the master backup (MBK). Similarly, any previous UBK file is erased, and the current
update file is renamed to become the new UBK file. Finally, the new file (TEMP.$$$)
is renamed to become the new master file (MAS) before execution is stopped.

Figure 57 shows an example of the files which are involved in a typical merge
operation. In this application, the sequence numbers control the ordering of a list of
names which is updated periodically. The NAMES.MAS file is the original master,
which will be updated by merging the NAMES.UPD file, also shown in the figure. The
merge operation is initiated by typing

MERGE NAMES
and, upon completion, produces the new NAMES.MAS shown to the right in Figure 57.
The SEQIO library is typical of the interface one can construct to provide a
higher-level interface between assembly language programs and their operating environ-
ment. Although the library shown here performs only simple sequential file input/output,

one can construct more comprehesive libraries for random access based upon this
library.

---
_Page 166_

098108
890200
600300
600460
808500
698690
600708
808880
600900
8601000
601898
601180
801208

068110
600200
6068216
660330
6069410
868540
890620
609710
600820
090930
608968
661016
\#01119
601218

NAMES.MAS

ABERCROMBIE, SIDNEY
CARLSBAD, YOLANDA
EGGBERT, EBENIZER
GRAVELPAUGH, HORTENSE
ISENEARS, IGNATZ
KRABNATZ, TILLY
MILLYWATZ, RICARDO
OPFATZ, ADOLPHO
QUAGMIRE, DONALD
TWITSWEET, LADNER
VERANDA, VERONICA
WILLOWANDER, PRATNEY
YUPPGANDER, MANNY

NAMES.UPD

BERNSWEIGER, ALFRED
CRUENCE, CLARENCE
DENNINGSKI, HUBERT
FINKLESTEIN, FRANK
HILLSENFIELDS, RANDOLPH
JOLLYFELLOW, JUNE
LAMBAA, WILLY
NEEBEND, ASTRID
PRATTWITZ, HEADY
RUBBLEMEYER, RUNYON
SWIGSTITTS, ULYSSIS
UMPLANDER, XAVIER
XYLOPH, ERHARDT
ZEPLIPPS, EGGERWORTZ

Figure 57. Sample MERGE Disk Files.

0808108
600110
880208
600210
600308
608330
600400
800418
680508
600548
BY0608
600620
680700
6067198
000880
600828
880986
460936
880966
601080
861018
661098
6801108
601118
891280
0812190

new NAMES.MAS

ABERCROMBIE, SIDNEY
BERNSWEIGER, ALFRED
CRUENCE, CLARENCE
DENNINGSKI, HUBERT
EGGBERT, EBENIZER
FINKLESTEIN, FRANK
GRAVELPAUGH, HORTENSE
HILLSENFIELDS, RANDOLPH
ISENEARS, IGNATZ
JOLLYFELLOW, JUNE
KRABNATZ, TILLY
LAMBAA, WILLY
MILLYWATZ, RICARDO
NEEBEND, ASTRID
OPFATZ, ADOLPHO
PRATTWITZ, HEADY
QUAGMIRE, DONALD
RUBBLEMEYER, RUNYON
SWIGSTITTS, ULYSSIS
TWITSWEET, LADNER
UMPLANDER, XAVIER
VERANDA, VERONICA
WILLOWANDER, PRATNEY
XYLOPH, ERHARDT
YUPPGANDER, MANNY
ZEPLIPPS, EGGERWORTZ

---
_Page 167_

10. ASSEMBLY PARAMETERS

Assembly parameters can be included when the assembly begins to control various
assembler functions. In general, the macro assembler is initiated with the name of
the source file, followed by the assembly parameters, indicated by a preceding dollar
symbol ($). The parameters are indicated by single controls which denote particular
functions. The letter or digit shown to the left below corresponds to the function
shown to the right.

controls the source disk for the .ASM file

controls the destination of the .HEX machine code file
controls the source disk for the .LIB files (see MACLIB)
controls MACRO listings in the .PRN file

controls the destination of the .PRN file containing the listing
controls the listing of LOCAL symbols

controls the generation and destination of the .SYM file
controls pass 1 listing

MNO US OM >

Any or all of the above parameters can be included. In the case of the A, H,
L, and S parameters, they are followed by the drive name to obtain or receive the
data, where the drives are labelled A, B, ..., 42. By convention, the X disk
corresponds to the user's console, the P disk corresponds to the system line printer
(logical LIST device), and the Z disk corresponds to a null file which is not recorded.
The following is a valid assembly parameter list following the MAC command and
source file name

$PB AA HB SX
whieh directs the .PRN file to disk B, reads the .ASM file from disk A, directs the
-HEX file to the B disk, and sends the .SYM file to the user's console. Blanks are

optional between parameter specifications.

The parameters L, 8, M, Q, and 1 can be preceded by either + or - symbols
which enable or disable their respective functions. These functions are listed below

+L list the input lines read from the macro library (see MACLIB)

-L suppress listing of the macro library (default value)
+S append the .SYM to the end of the .PRN output
S$ suppress the generation of the sorted symbol table

+M list all macro lines as they are processed during assembly
-M suppress all macro lines as they are read during assembly
\*M list only "hex" generated by macro expansions

+Q list all LOCAL symbols in the symbol list

-Q suppress all LOCAL symbols in the symbol list

+1 produce a listing file on the first pass (for macro debugging)
-1 suppress listing on pass 1 (default)

The following is an example of a valid assembly parameter list which uses a
number of the parameter specifications given above:

$PB+S-M HB

---
_Page 168_

In this ease, the .PRN file is sent to disk B with the symbol list appended (no .SYM
file is created), all macro generations are suppressed, and the .HEX file is sent to
disk B with the .PRN file.

Note that the M parameter can be optionally preceded by the "\*" symbol which
causes the assembler to list only macro generations which produce machine code, and
is used to suppress the listing of the instructions which are produced (i.e., all positions
beyond the hex fields are not listed). Under normal operation, the macro assembler
lists only generations which produce machine code, along with the generated line.

Given that disk d is the currently logged drive, the macro assembler defaults
these parameters as follows: the .ASM and .LIB files are assumed to originate on
drive d, the .HEX, .PRN, and .SYM files are sent to drive d, a symbol table is generated
with LOCAL symbols suppressed (i.e., all symbols beginning with "??" are not listed),
and macro lines which generate machine code are listed. Note, however, that the
filename following the MAC command can be preceded by a drive name, in which case
the P parameter overrides the drive name, if supplied. Whenever a parameter is
repeated in the assembly parameter specification, the last value is always assumed.
Valid assembly statements are shown below, assuming the file to be assembled is called
"sample."

MAC sample $PX+S-M

assembles the file sample.ASM with listing to the console, symbols at the console, and
no listing of generated macros.

MAC A:sample $+S -mtq

assembles sample.ASM from disk A, creating sample.PRN (with appended symbols) on
the currently logged drive, suppressing generated macros, and listing symbols which
begin with the characters "??" in addition to the normally listed symbols.

MAC sample

assembles sample.ASM from the currently Jogged drive, creating sample.PRN along
with sample.SYM (eontaining the symbol table) and sample.HEX which holds the Intel
format "hex" file in ASCII form.

MAC sample $AB HA PB +Q +S +L \*M

assembles the sample.ASM file from drive B, produces the file sample.HEX on drive
A, with the sample.PRN file on drive B. The symbol table includes ?? symbols, the
symbol] table is placed at the end of the .PRN file on drive B, the .LIB files are listed
with the .PRN file as the .LIB files are read, and the instructions which correspond
to generated macro lines are not included (although generated machine code is listed).

In addition to the parameters shown above, the programmer can intersperse
controls throughout the assembly language source or library files. Interspersed controls
are denoted by a "$" in the first column of the input line, where the form shown to
the left below corresponds to the action given to the right.

---
_Page 169_

$-PRINT stops the output listing by discarding formatted lines
$+PRINT enables the output printing when previously disabled
$-MACRO_ disables generated macro lines, as in "-M" above
$+MACRO_ enables full macro trace, as in "tM" above
$\*MACRO _ enables partial macro trace, as in "\*M" above

Since MAC allows each line to be optionally prefixed by a line number, the "$" control
ean be included directly following this line number, if desired.

---
_Page 170_

---
_Page 171_

11. DEBUGGING MACROS

In completing the discussion of the macro assembler, it is worthwhile considering
common debugging practices used in developing macros and macro libraries. One
technique, called "iterative improvement," is often used in the design of programs, and
is most useful in building macros. The basic idea of iterative improvement is that a
small portion of the overall macro set is first implemented and tested before continuing
to more complicated macros. In this way, errors can be isolated at each step as the
macro evolve. Further, if errors occur in the macro generations after a small portion
of the macro set has been improved, it is most likely the ease that the error is being
caused by the macros which were changed.

In the case of the Hornblower Highway System macro libraries, for example,
iterative improvement was used to evolved the final macro library. In particular, only
the simplest macros were first implemented, including the SETLITE, TIMER, and RETRY
macros (see Section 10.1). Debugging facilities were then added to these macros so
that the programs could be traced at the console. Upon successful testing of the
basie macro facilities, the PUSH?, CLOCK?, and TREAD? macros where individually
written, added, and tested, resulting in the final macro library. -

At each step, the programmer can use the various assembly parameters to
control the debugging information. If the macro generations are not producing the
proper machine code, it may be necessary to obtain a full trace, using the "+M" option
when MAC is started. If the program produces too much output with the full trace
enabled, the programmer can use the "$+MACRO" and "$-MACRO" commands inter-
spersed throughout the assembly language source program, resulting in full macro
generation traces only in the regions selected for debugging consideration.

If macro generation errors are caused by macro libraries, the programmer can
use the "+L" parameter when MAC is started to cause the libraries to be included in
the listing as they are read.

As a final consideration, it may be necessary to enable the first pass listing of
the assembly language using the "+1" parameter. In this case, MAC will list the
program as it is being read on the first pass as well as the second pass. Note,
however, that the listing will contain spurious error messages on this pass which may
disappear on the second pass. The principal purpose of the first pass listing parameter
is to allow the programmer to view the macro generations on the two successive
expansion passes to ensure that the assembler is processing the program in the same
way in both eases.

If a particular macro expands improperly, and the source of the error is not
evident after examining various traces, it may be necessary to remove the offending
macro from the program and create an isolated smaller test case where the error is
reproduced, Full traces can then be examined to determine the source of the error
and, after fixing the macro, it can be replaced in the larger program and retested.

---
_Page 172_

---
_Page 173_

12. SYMBOL STORAGE REQUIREMENTS

The maximum program size which can be assembled by MAC is determined only
by the symbol table storage requirements for the program. The symbol table itself
occupies the region above the macro assembler in memory, up to the base of the
CP/M operating system. Thus, the size of the symbol table depends upon the size of
the current MAC version (approximately 12K program and data, plus 2.5K for I/O
buffers) and the size of the user's CP/M configuration. In any case, the symbol table
size is dynamically determined by MAC upon startup, and fills as symbols are en-
countered. In order to provide some insight regarding storage requirements, the basic
item size for identifiers and macros is given below.

A name used as a program label, data label, or variable in a SET or EQUATE
requires

N=L+5
bytes, where L is the length of the identifier name. Thus, the statement
PORTVAL EQU 37FH
makes an entry into the symbol table which occupies
N=7+5 = 12 bytes

of symbol table space. Recall that LOCAL symbols take the form ??nnnn which
generates a name of length L = 6.

Macro storage is somewhat more complicated to compute. The general form
is given by

M=L+7+H+T

where L is the macro name length, H is the parameter header storage requirement,
and T is the macro text storage requirement, computed as

H =P + Po +e... + PL tn

where P. is the length of the jth parameter name. The text length T is the number
of characters in the macro body, including tab and end of line characters. Reserved
symbols, however, are reduced to a single byte, instead of their multi-character
representations. The jump, call, and return on condition operators, however, require
their full character representations. Comments starting with double semicolon are not
included in the character count. In fact, the comment line is "backsecanned" to remove
preceding tab or blank characters in this case. For example, the macro

LOADR MACRO REG,ALPHA ;FILL REGISTER ecrlf
MVI REG,'&ALPHA' 3DATA erlf
ENDM crlf

contains a macro header, followed by two macro lines, where each line is written with
tab characters (rather than spaces) and terminated by carriage-return line-feeds (erlf's).

---
_Page 174_

In this case, the macro name length (LOADR) is five characters (L = 5), and
the parameter name lengths are three characters (REG) and five characters (ALPHA),
resulting in the parameter header storage requirement of

H =P, +P, +2=3+5 + 2 = 10 bytes

The first macro line contains a leading tab (one byte), the MVI instruction (reduced
to one byte), another tab character (one byte), the operands REG,'&ALPHA’ (twelve
characters), and the end of line (two characters) for a total of seventeen bytes. Note
that the comment, with the preceding tab, is removed from the line. The second line
contains a tab (one byte), ENDM (one byte), and end of line (two characters) for a
total of four bytes. Summing the textual characters, the total is T = 21 bytes. As
a result, the total macro storage for LOADP is

M=L+7+H+T 25 + 7+ 10 + 21 = 43 bytes

No permanent storage is required for REPT's, IRPC's, or IRP's, although temporary
storage in the symbol table is used while the groups are actively iterating. In particular,
the characters contained within the group bounds (from the header to the corresponding
ENDM) are stored in the symbol table in their literal form, with no reduction of
reserved symbols to single bytes. Upon completion of the iteration, the storage is
returned for other purposes. Similarly, active parameters for macro expansions require
temporary storage in the symbol] table which is returned upon completion of the macro
expansion.

In any case, a symbol! table overflow message will result if the total amount
of free symbo] table space is used up. As mentioned previously, the user can regenerate
the CP/M system, up to the maximum memory space of the 8080 processor, to increase
the symbol table area. Note that the "percentage" of symbol table utilization is always
printed at the console at the end of the assembly. The form of the printout is

OhhH USE FACTOR

where hh is a hexadecimal value in the range 00 to FF, where 00 results from a near
empty table, and FF is produced for a nearly full table. The value 080H, for example,
is printed when the symbol] table is half full. The programmer should keep note of
the use factor as a particular program is Ceveloped in order to guage the relative
amount of free space as the program is enhanced.

In many of the examples shown in this manual, macros include inline subroutines
which are generated at the first invocation and called upon subsequent invocations (see
the TYPEOUT macro in Figure 10, for example). These subroutines can be included
in the mainline program to reduce symbol table storage requirements, if necessary.
In this case, the subroutines are assumed to exist when the macro is invoked the first
time, and thus are not generated by the macro.

---
_Page 175_

13. ERROR MESSAGES

When errors occur within the assembly language program, they are listed as
single character flags in the leftmost position of the source listing. The line in error
is also echoed at the console so that the source listing need not be examined to
determine if errors are present. The single character error codes are:

B Balance error: macro doesn't terminate properly, or conditional assembly
operation is ill~-formed.

C Comma error: expression was encountered, but not delimited properly
from the next item by a comma.

D Data error: element in a data statement (DB or DW) cannot be placed
in the specified data area.

E Expression error: expression is ill-formed and cannot be computed at
assembly time.

I Invalid character error: a non graphic character has been found in the
line (not a carriage return, line feed, tab, or end of file); re-edit the file, delete the
line with the I error, and retype the line.

L Label error: label cannot appear in this context (may be a duplicate
label).

M Macro overflow error: internal macro expansion table overflow; may be
due to too many nested invocations or infinite recursion.

N Not implemented error: features which will appear in future MAC versions
(e.g., relocation) are recognized, but flagged in this version.

O Overflow error: expression is too complicated (i.e., too many pending
operators), string is too long, or too many successive substitutions of a formal parameter
by its actual value in a macro expansion. This error will also occur if the number
of LOCAL labels exceeds 9999.

P Phase error: label does not have the same value on two subsequent passes
through the program, or the order of macro definition differs between two successive
passes; may be due to MACLIB which follows a mainline macro (if so, move the
MACLIB to the top of the program).

R Register error: the value specified as a register is not compatible with
the operation code.

8 Statement error: the fields of this statement are ill-formed and cannot
be processed properly; may be due to invalid characters or delimiters which are out
of place.

V Value error: operand encountered in an expression is improperly formed;
may be due to delimiter out of place or non-numeric operand.

---
_Page 176_

Several error messages are printed at the console indicating terminal error
conditions which abort the MAC execution. Whenever possible, the disk drive name,
followed by the relevant file name is printed with the message.

NO SOURCE FILE PRESENT: The source program file (.ASM) following the
MAC command cannot be found on the specified diskette. Use the DIR command in
the CCP to locate the source file.

NO DIRECTORY SPACE: The diskette directory is full. Use the ERA command
of the CCP to remove files which you do not need. There are often superfluous .HEX,
-PRN, and .SYM files which can be removed.

SOURCE FILE NAME ERROR: The form of the source file name is invalid, or
not specified. The command form must be:

MAC filename $assembly parameters

where the "filename" is the (up to eight character) primary name of the source file,
with an assumed file type of ".ASM" (which is not specified).

SOURCE FILE READ ERROR: The source file cannot be read properly by the
macro assembler. Use the CCP TYPE command to display the file contents a. the
console.

OUTPUT FILE WRITE ERROR: An output file cannot be written properly,
probably due to a full diskette. As in the directory full error above, use the CCP
commands to erase unnecessary files from the diskette.

CANNOT CLOSE FILE: An output file cannot be closed. The diskette may be
write protected.

UNBALANCED MACRO LIBRARY: A MACRO definition was started within a
macro library, but the end of file was found in the library before the balancing ENDM
was encountered. Examine the macro library using the TYPE command of the CCP,
or use the "+L" assembly parameter, to ensure that the library is properly balanced.

INVALID PARAMETER: An invalid assembly parameter was found in the input
line. The assembly parameters are printed at the console up to the point of the error.

---
_Page 177_

OP
CODE

NOP
2c
2D
2E
2F

3A
3B
3C
3D
3E
3F

4A
4B
4c
4D
4E
4F

LHLD = Adr

D8 = constant, or logical/arithmetic expression that evaluates
to an 8 bit data quantity.

Adr = 16-bit address.

ie)
MNEMONIC

P
DE
2B

Appendix

8080 CPU INSTRUCTIONS IN OPERATION CODE SEQUENCE

MNEMONIC

OP

CODE| MNEMONIC
56 D.M

83 | ADD
ADD
ADD
ADD
ADD
ADC
ADC
ADC
ADC
ADC
ADC
ADC
ADC
SUB
SUB
SUB
SUB
SUB
SUB
SUB
SUB
SBB
SBB
SBB
SBB
SBB
SBB
SBB
SBB
ANA
ANA
ANA
ANA
ANA
ANA
ANA
ANA
XRA
XRA
XRA
XRA

OP
CODE] MNEM

MOOODPYPZTIMIOOv,ver-rImMm9onwDrPe-rIMIGVN0wTWrPZr-rmgV0wWreririmgs

oP oP
ONIC |CODE} MNEMONIC | CODE
Cc AC H D7

MNEMONIC

XRA
XRA

PSeTrmMmI0o0oOrZzgrFrIMV0cVrPser

iss)

POP PSW
JP Adr
DI

CP Adr
PUSH PSW
ORI} D8
RST 6

RM

SPHL

Adr

D16 = constant, or logicalV/arithmetic expression that evaluates
to a 16 bit data quantity.

Reproduced with Permission from Intel Corporation, Santa Clara, CA.

---
_Page 178_

595-2548

