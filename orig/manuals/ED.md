# ED

> OCR text extracted from `ED.pdf` with `pdftoppm` + `tesseract` (this manual is a scanned image with no embedded text layer, unlike `CBASIC_1982.pdf`). This is raw, uncorrected OCR output -- only page-number noise has been stripped and literal `*`/leading `#` escaped for Markdown safety. Expect OCR errors (misread characters, garbled logos/graphics, table layout loss); cross-check against the PDF for anything that matters. No heading structure has been reconstructed -- each `_Page N_` marker corresponds to that page in `ED.pdf`.

---

---
_Page 01_

lt DIGTAL RESEARCH

Post Office Box 579, Pacific Grove, California 93950, (408) 649-3896

ED: A CONTEXT EDITOR FOR THE CP/M DISK SYSTEM
USER'S MANUAL

COPYRIGHT (c) 1976, 1978
DIGITAL RESEARCH

---
_Page 02_

Copyright (ce) 1976, 1978 by Digital Research. All rights
reserved. No part of this publication may be reproduced,
transmitted, transcribed, stored in a retrieval system, or
translated into any language or computer language, in any
form or by any means, electronic, mechanical, magnetic,
optical, chemical, manual or otherwise, without the prior
written permission of Digital Research, Post Office Box 579,
Pacifie Grove, California 93950.

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

ED: A CONTEXT EDITOR FOR THE CP/M DISK SYSTEM
USER’S MANUAL

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
California 93950,

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

ED TUTORIAL - + -« «© «© © «© «© «© «=
1.1 Introduction to ED... .
1.2 ED Operation. »- « «© « « «
1.3 Text Transfer Functions

1.4 Memory Buffer Organization
1.5 Memory Buffer Operation .
1.6 Command Strings .....
1.7 Text Search and Alteration

1.8 Source Libraries. ...-.

1.9 Repetitive Command Execution

ED ERROR CONDITIONS . . - «© « «

CONTROL CHARACTERS AND COMMANDS

Li

a ee ee

yu uw Ow

ll

---
_Page 06_

---
_Page 07_

ED USER'S MANUAL

1. ED TUTORIAL
1.1. Introduction to ED.

ED is the context editor for CP/M, and is used to create
and alter CP/M source files. ED is initiated in CP/M by

typing

<filename>
ED
<filename>.<filetype>

In general, ED reads segments of the source file given by
<filename> or <filename> . <filetype> into central memory,
where the file is manipulated by the operator, and subse-
quently written back to disk after alterations. If the
source file does not exist before editing, it is created by
ED and initialized to empty. The overall operation of ED
is shown in Figure l.

1.2. ED Operation

ED operates upon the source file, denoted in Figure 1l
by x.y, and passes all text through a memory buffer where
the text can be viewed or altered (the number of lines which
can be maintained in the memory buffer varies with the line
length, but has a total capacity of about 6000 characters
in a 16K CP/M system). Text material which has been edited
is written onto a temporary work file under command of the
operator. Upon termination of the edit, the memory buffer
is written to the temporary file, followed by any remaining
(unread) text in the source file. The name of the original
file is changed from x.y to x.BAK so that the most recent
previously edited source file can be reclaimed if necessary
(see the CP/M commands ERASE and RENAME). The temporary
file is then changed from x.$$$ to x.y which becomes the
resulting edited file.

The memory buffer is logically between the source file
and working file as shown in Figure 2.

1.3. Text Transfer Functions

Given that n is an integer value in the range 0 through
65535, the following ED commands transfer lines of text
from the source file through the memory buffer to the tem-
porary (and eventually final) file:

---
_Page 08_

Figure 1. Overall ED Operation

Source
Libraries

Temporary

Source Append Write
(A) (W)

File

x. $$$

File

Memory Buffer

After

Edit (E)

New

Source
File
XeY

Note: the ED program accepts both lower and upper case ASCII
characters as input from the console. Single letter commands
can be typed in either case. The U command can be issued to

cause ED to translate lower case alphabetics to upper case as
characters are filled to the memory buffer from the console.

Characters are echoed as typed without translation, however.
The -U command causes ED to revert to "no translation" mode.

ED starts with an assumed -U in effect.

---
_Page 09_

he

Source File

\ “Appended , \

\ x

Unprocessed

Source

Figure 2. Memory Buffer Organization

Memory Buffer Temporary File

1 |) First Line \ |

‘ Buffered \

\ Processed \,

| Space

Figure 3. Logical Organization of Memory Buffer

Memory Buffer

<cr><lf>

<cer><lf>
current
line CL

last
line

<er><l£f>

---
_Page 10_

nA<cr>\* - append the next n unprocessed source
lines from the source file at SP to
the end of the memory buffer at MP.
Increment SP and MP by n.

nW<cr> - write the first n lines of the memory
buffer to the temporary file free space.
Shift the remaining lines n+l through
MP to the top of the memory buffer.
Increment TP by n.

E<xcr> - end the edit. Copy all buffered text
to temporary file, and copy all un-
processed source lines to the temporary
file. Rename files as described
previously.

H<cr> - move to head of new file by performing
automatic E command. Temporary file
becomes the new source file, the memory
buffer is emptied, and a new temporary
file is created (equivalent to issuing
an E command, followed by a reinvocation
of ED using x.y as the file to edit).

O<cr> - return to original file. The memory
buffer is emptied, the temporary file
id deleted, and the SP is returned to
position 1 of the source file. The
effects of the previous editing commands
are thus nullified.

Q<cr> - quit edit with no file alterations,
return to CP/M.

There are a number of special cases to consider. If the
integer n is omitted in any ED command where an integer is
allowed, then 1 is assumed. Thus, the commands A and W append
one line and write 1 line, respectively. In addition, if a
pound sign (#) is given in the place of n, then the integer
65535 is assumed (the largest value for n which is allowed).
Since most reasonably sized source files can be contained
entirely in the memory buffer, the command #A is often issued
at the beginning of the edit to read the entire source file
to memory. Similarly, the command #W writes the entire buffer
to the temporary file. Two special forms of the A and W

\*<cr> represents the carriage-return key

---
_Page 11_

commands are provided as a convenience. The command 0A fills
the current memory buffer to at least half-full, while Ow
writes lines until the buffer is at least half empty. It
should also be noted that an error is issued if the memory
buffer size is exceded. The operator may then enter any
command (such as W) which does not increase memory require-
ments. The remainder of any partial line read during the
overflow will be brought into memory on the next successful
append.

1.4. Memory Buffer Organization

The memory buffer can be considered a sequence of source
lines brought in with the A command from a source file. The
memory buffer has an associated (imaginary) character pointer
CP which moves throughout the memory buffer under command of
the operator. The memory buffer appears logically as shown
in Figure 3 where the dashes represent characters of the
source line of indefinite length, terminated by carrigage-
return (<cr>) and line-feed (<l1f>) characters, and
represents the imaginary character pointer. Note that the
CP is always located ahead of the first character of the
first line, behind the last character of the last line, or
between two characters. The current line CL is the source
line which contains the CP.

1.5. Memory Buffer Operation

Upon initiation of ED, the memory buffer is empty (ie,
CP is both ahead and behind the first and last character).
The operator may either append lines (A command) from the
source file, or enter the lines directly from the console
with the insert command

I<cr>

ED then accepts any number of input lines, where each line
terminates with a <cr> (the <lf> is supplied automatically),
until a control-z (denoted by +z is typed by the operator.
The CP is positioned after the last character entered. The
sequence

I<cr>

NOW IS THE<cr>
TIME FOR<cr>

ALL GOOD MEN<cr>
tz

leaves the memory buffer as shown below

---
_Page 12_

NOW IS THE<cr><lf>
TIME FOR<cr><lf>

ALL GOOD MEN<cr><lf> 2

Various commands can then be issued which manipulate the CP

or display source text in the vicinity of the CP. The
commands shown below with a preceding n indicate that an
optional unsigned value can be specified. When preceded by

+, the command can be unsigned, or have an optional preceding
plus or minus sign. As before, the pound sign (#) is replaced
by 65535. If an integer n is optional, but not supplied,

then n=l is assumed. Finally, if a plus sign is optional,

but none is specified, then + is assumed.

+B<cr> - move CP to beginning of memory buffer
if +, and to bottom if -.

+nC<cr> - move CP by tn characters (toward front
of buffer if +), counting the <cr><lf>
as two distinct characters

+nD<cr> - delete n characters ahead of CP if plus
and behind CP if minus.

tnK<cr> - kill (ie remove) tn lines of source text
using CP as the current reference. If
CP is not at the beginning of the current
line when K is issued, then the charac-
ters before CP remain if + is specified,
while the characters after CP remain if -
is given in the command.

tnL<cr> - if n=0 then move CP to the beginning of
the current line (if it is not already
there) if n#40 then first move the CP to
the beginning of the current line, and
then move it to the beginning of the
line which is n lines down (if +) or up
(if -). The CP will stop at the top or
bottom of the memory buffer if too large
a value of nis specified.

---
_Page 13_

tnT<cr> - If n=0 then type the contents of the
current line up to CP. If n=l then
type the contents of the current line
from CP to the end of the line. If
n>l then type the current line along
with n-l lines which follow, if +
is specified. Similarly, if n>l and
- is given, type the previous n lines,
up to the CP. The break key can be
depressed to abort long type-outs.

tn<cr> - equivalent to tnLT, which moves up or
down and types a single line

1.6. Command Strings

Any number of commands can be typed contiguously (up to
the capacity of the CP/M console buffer), and are executed
only after the <cr> is typed. Thus, the operator may use
the CP/M console command functions to manipulate the input
command:

Rubout remove the last character
Contro1-U delete the entire line
Control-C re-initialize the CP/M System
Control-E return carriage for long lines

without transmitting buffer
(max 128 chars)

Suppose the memory buffer contains the characters shown
in the previous section, with the CP following the last
character of the buffer. The command strings shown below
produce the results shown to the right

Command String Effect Resulting Memory Buffer
1. B2T<cr> move to beginning Ep) NOW IS THE<cr><lf>
of buffer and type
2 lines: TIME FOR<cr><l1f>
"NOW IS THE ALL GOOD MEN<cr><lf>
TIME FOR"
2. 5cC0T<cr> move CP 5 charac- NOW Tepy8 THE<cr><lf>
ters and type the
beginning of the
line
"Now I"

---
_Page 14_

3. 2L-T<cr> move two lines down NOW IS THE<cr><lf>
and type previous TIME FOR<cr><1f>

line
"TIME FOR" ; ALL GOOD MEN<cr><l1f>
4. -L#K<cr> move up one line, Now IS THE <er><1£> 2s
delte 65535 lines
which follow
5. I<cer> insert two lines NOW IS THE<cr><lf>
TIME TO<cr> of text
< >< >
INSERT<cr> TIME TO<cr><lf

tz INSERT <cr><1£> o>)

6. —-2L#T<cr> move up two lines, Now IS THE<cr><1f> >

and type 65535
lines ahead of CP TIME TO<cr><1f>

"NOW IS THE" INSERT<cr><lf>
7. <cr> move down one line NOW IS THE<cr><lf>
and type one line
“TNSERT" TIME TO<er><1F> ep)
INSERT<cr><lLf>

1.7. Text Search and Alteration

ED also has a command which locates strings within the
memory buffer. The command takes the form

FP <cr>
n CySgre hz

where c, through cy represent the characters to match followed
by either a <cr> or control -z. ED starts at the current
position of CP and attempts to match all k characters. The
match is attempted n times, and if successful, the CP is

moved directly after the character c,. If the n matches are
not successful, the CP is not moved from its initial position.
Search strings can include tl (control-1), which is replaced
by the pair of symbols <cr><lf>.

\*The control-z is used if additional commands will be typed
following the +z.

---
_Page 15_

The following commands illustrate the use of the F
command :

Command String Effect Resulting Memory Buffer
l. B#T<cr> move to beginning (ep) NOW IS THE<cr><lf>
and type entire TIME FOR<cr><1£>
buffer
ALL GOOD MEN<cr><lf>
2. FS T<cr> find the end of NOW IS T epi HBsex? <1
the string "S T"
3. FI+z0TT find the next "I" NOW IS THE<cr><l1f>
and type to the
CP then type the TI ey" FOR<cr><1f>
remainder of the ALL GOOD MEN<cr><lf>
current line:
"“TTME FOR"

An abbreviated form of the insert command is also allowed,
which is often used in conjunction with the F command to make
simple textual changes. The form is:

Ioc,c c tz or
n

l 2°°°

I c,c

aoe C <OY>
172 n

where c; through c, are characters to insert. If the inser-
tion string is terminated by a tz, the characters c, through
Cy are inserted directly following the CP, and the CP is
moved directly after character cy). The action is the same.
if the command is followed by a <cr> except that a <cr><l1f>
is automatically inserted into the text following character
Cy- Consider the following command sequences as examples

of the F and I commands:

Command String Effect Resulting Memory Buffer
BITHIS IS tz<cr> Insert "THIS IS " THIS IS NOW THE <cr><lf>
at the beginning
of the text

TIME FOR<cr><1Lf>
ALL GOOD MEN<cr><1f>

---
_Page 16_

FTIME+ z-4DIPLACEtz<cr> THIS IS NOW THE<cr><1lf>

find "TIME" and delete PLACE FOR<cr><1f>
it; then insert "PLACE" ALL GOOD MEN<cr><1f>
3FO0+z-3D5DICHANGES ¢t<cr> THIS IS NOW THE <cr><l1f>

find third occurrence PLACE FOR<cr><1f>

of "O" (ie the second ALL CHANGES /Cxy cr? <1 £>
"O" in GOOD), delete

previous 3 characters;

then insert "CHANGES"

-8CISOURCE<cr> move back 8 characters THIS IS NOW THE<cr><lf>
and insert the line
"“SOURCE<cE><1£>" PLACE FOR<cr><1f>

ALL SOURCE<cr><l1f>

(Spy CHANGES <cr? <1£>

ED also provides a single command which combines the F and
I commands to perform simple string substitutions. The command
takes the form

<er>
ns CpCaee Cy £2 djdo---d. { <cx

and has exactly the same effect as applying the command string

<or>
F C1 Core eC, tz kDIdjd,---d., { kz

a total of n times. That is, ED searches the memory buffer
starting at the current position of CP and successively sub-
stitutes the second string for the first string until the
end of buffer, or until the substitution has been performed
n times.

As a convenience, a command similar to F is provided by
ED which automatically appends and writes lines as the search
proceeds. The form is

cr
nwN Cy Caer Cy {3}

which searches the entire source file for the nth occurrence
of the string cjC32...c, (recall that F fails if the string
cannot be found in the current buffer). The operation of the

---
_Page 17_

Ww) command is precisely the same as F except in the case that
the string cannot be found within the current memory buffer.
In this case, the entire memory contents is written (ie, an
automatic #W is issued). Input lines are then read until
the buffer is at least half full, or the entire source file
is exhausted. The search continues in this manner until the
string has been found n times, or until the source file has
been completely transferred to the temporary file.

A final line editing function, called the juxtaposition
command takes the form

<cr>
nJ Cp Cane Oy tz d,da+--d tz P,Q Fy (<2)

with the following action applied n times to the memory buffer:
search from the current CP for the next occurrence of the
string ¢c)Co...c,. If found, insert the string dyd9..-,dy,

and move cb to follow d,- Then delete all characters following
CP up to (but not including) the string e},€9,---@q, leaving

CP directly after d,- If e),e2,-..@,g cannot be foiind, then

no deletion is made. If the current ‘line is

ep) NO" IS THE TIME<cr><1f>

Then the command
JW t2WHATt+zt1l<cr>

Results in

NOW WHAT (cp <cr><1f>

(Recall that tl represents the pair <cr><lf> in search and
substitute strings).

It should be noted that the number of characters allowed
by ED in the F,S,N, and J commands is limited to 100 symbols.

1.8. Source Libraries
ED also allows the inclusion of source libraries during

the editing process with the R command. The form of this
command is

il

---
_Page 18_

R £,f,--f,tz or

R f,f£,--f,<er?

where f ,f2..f, is the name of a source file on the disk with
as assumed filetype of 'LIB'. ED reads the specified file,
and places the characters into the memory buffer after CP,
in a manner similar to the I command. Thus, if the command

RMACRO<cr>

is issued by the operator, ED reads from the file MACRO.LIB
until the end-of-file, and automatically inserts the charac-
ters into the memory buffer.

1.9. Repetitive Command Execution

The macro command M allows the ED user to group ED com-
mands together for repeated evaluation. The M command takes

the form:
<cr>
nM Cy Core Cy {(<<2"\

where Cc )C92...C, represent a string of ED commands, not inclu-
ding another M command. ED executes the command string n
times if n>l. If n=0 or 1, the command string is executed
repetitively until an error condition is encountered (e.g.,
the end of the memory buffer is reached with an F command).
As an example, the following macro changes all occur-
rences of GAMMA to DELTA within the current buffer, and
types each line which is changed:

MF GAMMAt Z-S5DIDELTAt z0TT<cr>

or equivalently

MSGAMMA?t ZDELTAtzOTT<cr>

---
_Page 19_

2. ED ERROR CONDITIONS

On error conditions, ED prints the last character read
before the error, along with an error indicator:

? unrecognized command

> Memory buffer full (use one of
the commands D,K,N,S, or W to
remove characters), F,N, or S
strings too long.

\# cannot apply command the number
of times specified (e.g., in
F command)

fe) cannot open LIB file in R
command

Cyclic redundancy check (CRC) information is written with
each output record under CP/M in order to detect errors on
subsequent read operations. If a CRC error is detected, CP/M
will type

PERM ERR DISK da

where d is the currently selected drive (A,B,...). The oper-
ator can choose to ignore the error by typing any character
at the console (in this case, the memory buffer data should
be examined to see if it was incorrectly read), or the user
can reset the system and reclaim the backup file, if it
exists. The file can be reclaimed by first typing the con-
tents of the BAK file to ensure that it contains the proper
information:

TYPE x. BAK<cr>

where x is the file being edited. Then remove the primary
file:

ERA x.y<cr>
and rename the BAK file:
REN x.y=x. BAK<cr>

The file can then be re-edited, starting with the previous
version.

---
_Page 20_

3. CONTROL CHARACTERS AND COMMANDS

The following table summarizes the control characters

and commands available in ED:

Control Character

4c

te

Fi

tu

AZ

rubout

break

Function
system reboot
physical <cr><1f> (not
actually entered in

command)

logical tab (cols 1,8,
15,...)

logical <cr><lf> in
search and substitute
strings

line delete

string terminator

character delete

discontinue command
(e.g., stop typing)

---
_Page 21_

Command Function

nA append lines

+B begin bottom of buffer
+nc move character positions
+nD delete characters

E end edit and close files

(normal end)

nF find string
H end edit, clcse and reopen
files
I insert characters
nJ place strings in juxtaposition
+nK kill lines
+nL move down/up lines
nM macro definition
nN find next occurrence with
autoscan
co) return to original file
tnP move and print pages
Q quit with no file changes
R read library file
ns substitute strings
tnT type lines

translate lower to upper case if U,

U
no translation if -U
nw write lines
nZ sleep
+n<cr> move and type (+nLT)

---
_Page 22_

---
_Page 23_

Appendix A: ED 1.4 Enhancements

The ED context editor contains a number of commands which enhance its
usefulness in text editing. The improvements are found in the addition of line numbers,
free space interrogation, and improved error reporting.

The context editor issued with CP/M 1.4 produces absolute line number prefixes
when the "V" (Verify Line Numbers) command is issued. Following the V command,
the line number is displayed ahead of each line in the format:

Innnn:

where nnnnn is an absolute line number in the range 1 to 65535. If the memory buffer
is empty, or if the current line is at the end of the memory buffer, then nnnnn appears
as 5 blanks.

The user may reference an absolute line number by preceding any command by
a number followed by a colon, in the same format as the line number display. In this
ease, the ED program moves the current line reference to the absolute line number,
if the line exists in the current memory buffer. Thus, the command

345:T

is interpreted as "move to absolute line 345, and type the line." Note that absolute
line numbers are produced only during the editing process, and are not recorded with
the file. In particular, the line numbers will change following a deleted or expanded
section of text.

The user may also reference an absolute line number as a backward or forward
distance from the current line by preceding the absolute line number by a colon. Thus,
the command

:400T

is interpreted as "type from the current line number through the line whose absolute
number is 489." Combining the two line reference forms, the command

345 ::400T
for example, is interpreted as "move to absolute line 345, then type through absolute
line 496." Note that absolute line references of this sort can precede any of the
standard ED commands.

A special case of the V command, "$V", prints the memory buffer statistics in
the form:

free/total

where "free" is the number of free bytes in the memory buffer (in decimal), and "total"
is the size of the memory buffer.

---
_Page 24_

ED 1.4 also includes a "block move" facility implemented through the '"X" (Xfer)
command. The form

nxX
transfers the next n lines from the current line to a temporary file called
X$$$$$$$.LIB

which is active only during the editing process. In general, the user can reposition
the current line reference to any portion of the source file and transfer lines to the
temporary file. The transferred line accumulate one after another in this file, and
ean be retrieved by simply typing:

R

which is the trivial case of the library read command. In this case, the entire
transferred set of lines is read into the memory buffer. Note that the X command
does not remove the transferred lines from the memory buffer, although a K command
ean be used directly after the X, and the R command does net empty the transferred
line file. That is, given that a set of lines has been transferred with the X command,
they can be re-read any number of times back into the source file. The command

OX
is provided, however, to empty the transferred line file.

Note that upon normal completion of the ED program through Q or E, the
temporary LIB file is removed. If ED is aborted through ctl-C, the LIB file will exist
if lines have been transferred, but will generally be empty (a subsequent ED invocation
will erase the temporary file).

Due to common typographical errors, ED 1.4 requires several potentially disas-
terous commands to be typed as single letters, rather than in composite commands.
The commands

E (end), H (head), O (original), Q (quit)
must be typed as single letter commands.

ED 1.4 also prints error messages in the form

BREAK "x" AT ¢

where x is the error character, and ¢ is the command where the error occurred.

---
_Page 25_

---
_Page 26_


