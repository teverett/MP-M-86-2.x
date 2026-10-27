/*
 * rsp_syms.h -- the offsets in src-assem/rsp/'s programs that the C versions
 * in src-c/rsp/ lay their data images out by and print their
 * code-segment messages at: <PROGRAM>_<LABEL> = the label's offset in
 * its .A86 (in the data segment, or for TMP's messages the code
 * segment). Each is the value tools/asm86 gives the label;
 * tests/src-c/test_syms.py checks them all against the assembled source,
 * so the build itself doesn't need /src-assem.
 */
#ifndef RSP_SYMS_H
#define RSP_SYMS_H

#define CLOCK_SYSDAT 0x0000 /* CLOCK.A86: sysdat */
#define CLOCK_UDA 0x0040 /* CLOCK.A86: uda */
#define CLOCK_DMA 0x0140 /* CLOCK.A86: dma */
#define CLOCK_STACK 0x01C0 /* CLOCK.A86: stack */
#define CLOCK_STACK_TOS 0x01DE /* CLOCK.A86: stack_tos */

#define ECHO_PD 0x0010 /* ECHO.A86: pd */
#define ECHO_UDA 0x0040 /* ECHO.A86: uda */
#define ECHO_QBUF 0x0140 /* ECHO.A86: qbuf */
#define ECHO_QD 0x01C3 /* ECHO.A86: qd */
#define ECHO_DMA 0x01DF /* ECHO.A86: dma */
#define ECHO_STACK 0x025F /* ECHO.A86: stack */
#define ECHO_STACK_TOS 0x027D /* ECHO.A86: stack_tos */
#define ECHO_PDADR 0x0283 /* ECHO.A86: pdadr */
#define ECHO_CMDTAIL 0x0285 /* ECHO.A86: cmdtail */
#define ECHO_QPB 0x0309 /* ECHO.A86: qpb */
#define ECHO_CONSOLE 0x0319 /* ECHO.A86: console */

#define TMP_SDATVAR 0x0002 /* TMP.A86: sdatvar */
#define TMP_DEFCONSOLE 0x0004 /* TMP.A86: defconsole */
#define TMP_PD 0x0010 /* TMP.A86: pd */
#define TMP_UDA 0x0040 /* TMP.A86: uda */
#define TMP_DMA 0x0140 /* TMP.A86: dma */
#define TMP_STACK 0x01C0 /* TMP.A86: stack */
#define TMP_STACK_TOP 0x01D8 /* TMP.A86: stack_top */
#define TMP_READ_BUF 0x01DE /* TMP.A86: read_buf */
#define TMP_CLICB 0x01DF /* TMP.A86: clicb */
#define TMP_CLICB_NET 0x01DF /* TMP.A86: clicb_net */
#define TMP_CLICB_CMD 0x01E0 /* TMP.A86: clicb_cmd */
#define TMP_CMDSENT 0x0261 /* TMP.A86: cmdsent */
#define TMP_PARSERET 0x0262 /* TMP.A86: parseret */
#define TMP_PCB 0x0264 /* TMP.A86: pcb */
#define TMP_FCB 0x0268 /* TMP.A86: fcb */
#define TMP_SAVEBUF 0x0288 /* TMP.A86: savebuf */
#define TMP_PROMPT 0x033A /* TMP.A86: prompt */
#define TMP_CRLFSTR 0x033C /* TMP.A86: crlfstr */
#define TMP_MEMERR 0x033F /* TMP.A86: memerr */
#define TMP_PDERR 0x0352 /* TMP.A86: pderr */
#define TMP_FNAMEERR 0x0361 /* TMP.A86: fnameerr */
#define TMP_LOADERR 0x0370 /* TMP.A86: loaderr */
#define TMP_OPENERR 0x037C /* TMP.A86: openerr */
#define TMP_CATCHERR 0x0390 /* TMP.A86: catcherr */
#define TMP_QFULLERR 0x0392 /* TMP.A86: qfullerr */
#define TMP_USERERRMSG 0x03A8 /* TMP.A86: usererrmsg */
#define TMP_USERMSG 0x03C9 /* TMP.A86: usermsg */
#define TMP_PRINTEMSG 0x03DA /* TMP.A86: printemsg */
#define TMP_PRINTERMSG 0x03FE /* TMP.A86: printermsg */
#define TMP_USERCMD 0x0412 /* TMP.A86: usercmd */
#define TMP_PRINTERCMD 0x041A /* TMP.A86: printercmd */

#define SUBMIT_FCB 0x005C /* SUBMIT.A86: fcb */
#define SUBMIT_CMDTAIL 0x0080 /* SUBMIT.A86: cmdtail */
#define SUBMIT_STACK 0x0100 /* SUBMIT.A86: stack */
#define SUBMIT_DMA 0x013C /* SUBMIT.A86: dma */
#define SUBMIT_NUMARGS 0x01BC /* SUBMIT.A86: numargs */
#define SUBMIT_ARGTABLE 0x01BE /* SUBMIT.A86: argtable */
#define SUBMIT_DEBLANKBUF 0x01FE /* SUBMIT.A86: deblankbuf */
#define SUBMIT_CMDBUF 0x027F /* SUBMIT.A86: cmdbuf */
#define SUBMIT_BLEN 0x0280 /* SUBMIT.A86: blen */
#define SUBMIT_CMD 0x0281 /* SUBMIT.A86: cmd */
#define SUBMIT_CLICB 0x0302 /* SUBMIT.A86: clicb */
#define SUBMIT_CLICMD 0x0303 /* SUBMIT.A86: clicmd */
#define SUBMIT_COMMAND_USER 0x0384 /* SUBMIT.A86: command_user */
#define SUBMIT_COMMAND_CON 0x0386 /* SUBMIT.A86: command_con */
#define SUBMIT_BUF_PTR 0x0389 /* SUBMIT.A86: buf_ptr */
#define SUBMIT_PROMPT 0x038A /* SUBMIT.A86: prompt */
#define SUBMIT_PROMPTUTENS 0x038B /* SUBMIT.A86: promptutens */
#define SUBMIT_PROMPTEND 0x038E /* SUBMIT.A86: promptend */
#define SUBMIT_PCB 0x0390 /* SUBMIT.A86: pcb */
#define SUBMIT_PARSERET 0x0394 /* SUBMIT.A86: parseret */
#define SUBMIT_PARSERESULT 0x0396 /* SUBMIT.A86: parseresult */
#define SUBMIT_CMDSENT 0x03BA /* SUBMIT.A86: cmdsent */
#define SUBMIT_PDADR 0x03BB /* SUBMIT.A86: pdadr */
#define SUBMIT_SYSDATSEG 0x03BD /* SUBMIT.A86: sysdatseg */
#define SUBMIT_WRONGVERS 0x03BF /* SUBMIT.A86: wrongvers */
#define SUBMIT_NEWLINE 0x03CF /* SUBMIT.A86: newline */
#define SUBMIT_ASKABORTMSG 0x03D2 /* SUBMIT.A86: askabortmsg */
#define SUBMIT_SUBFILENAME 0x03DF /* SUBMIT.A86: subfilename */
#define SUBMIT_USERCMD 0x03F4 /* SUBMIT.A86: usercmd */
#define SUBMIT_PRINTERCMD 0x03FC /* SUBMIT.A86: printercmd */
#define SUBMIT_INCLUDECMD 0x0404 /* SUBMIT.A86: includecmd */
#define SUBMIT_USERERRMSG 0x040C /* SUBMIT.A86: usererrmsg */
#define SUBMIT_USERMSG 0x042D /* SUBMIT.A86: usermsg */
#define SUBMIT_PRINTERERRMSG 0x043E /* SUBMIT.A86: printererrmsg */
#define SUBMIT_PRINTERMSG 0x0462 /* SUBMIT.A86: printermsg */
#define SUBMIT_INCLUDERRMSG 0x0476 /* SUBMIT.A86: includerrmsg */
#define SUBMIT_INCLUDEMSG 0x0486 /* SUBMIT.A86: includemsg */
#define SUBMIT_MEMERR 0x0491 /* SUBMIT.A86: memerr */
#define SUBMIT_PDERR 0x04A4 /* SUBMIT.A86: pderr */
#define SUBMIT_FNAMEERR 0x04B3 /* SUBMIT.A86: fnameerr */
#define SUBMIT_LOADERR 0x04C4 /* SUBMIT.A86: loaderr */
#define SUBMIT_OPENERR 0x04D0 /* SUBMIT.A86: openerr */
#define SUBMIT_CATCHERR 0x04E4 /* SUBMIT.A86: catcherr */
#define SUBMIT_QFULLERR 0x04E6 /* SUBMIT.A86: qfullerr */
#define SUBMIT_SOPENERR 0x04FC /* SUBMIT.A86: sopenerr */
#define SUBMIT_ARGERR 0x0512 /* SUBMIT.A86: argerr */

#define RANDOM_FCB 0x005C /* RANDOM.A86: fcb */
#define RANDOM_RANREC 0x007D /* RANDOM.A86: ranrec */
#define RANDOM_RANOVF 0x007F /* RANDOM.A86: ranovf */
#define RANDOM_BUFF 0x0080 /* RANDOM.A86: buff */
#define RANDOM_BADVER 0x0100 /* RANDOM.A86: badver */
#define RANDOM_NOSPACE 0x011F /* RANDOM.A86: nospace */
#define RANDOM_DATMSG 0x0132 /* RANDOM.A86: datmsg */
#define RANDOM_ERRMSG 0x013E /* RANDOM.A86: errmsg */
#define RANDOM_PROMPT 0x0150 /* RANDOM.A86: prompt */
#define RANDOM_CONBUF 0x015F /* RANDOM.A86: conbuf */
#define RANDOM_CONSIZ 0x0160 /* RANDOM.A86: consiz */
#define RANDOM_CONLIN 0x0161 /* RANDOM.A86: conlin */
#define RANDOM_STACK 0x01A0 /* RANDOM.A86: stack */


/* the transients' code lengths: their CMD files' code groups (src-c/tools/mkdisk.py) */
#define SUBMIT_CODE_LEN 0x0652 /* SUBMIT.A86: cseg length */
#define RANDOM_CODE_LEN 0x01A4 /* RANDOM.A86: cseg length */
#endif
