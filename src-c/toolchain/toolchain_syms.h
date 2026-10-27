/*
 * toolchain_syms.h -- the offsets in DDT86.CMD (DRI's debugger, built
 * from src-assem/toolchain/ by tools/build_ddt86.py) that the C versions
 * (src-c/toolchain/) lay their data out by. DDT86 is an 8080-model
 * program, code and data in one segment: DDT86.A86's own data (from
 * endcs), then the disassembler overlay (INS86 + DIS86, code then data)
 * and the assembler overlay (ASMTAB + ASS86). Each constant is
 * "<piece>:<label>" in that build (ddt86, dis, ass) or a layout value;
 * tests/src-c/test_syms.py checks them all against it.
 */
#ifndef TOOLCHAIN_SYMS_H
#define TOOLCHAIN_SYMS_H

/* the layout */
#define TC_DIS_ORG 0x1420 /* DDT86.CMD: layout:lasmorg */
#define TC_DIS_CODE_END 0x25CD /* DDT86.CMD: layout:dis_code_end */
#define TC_DIS_END 0x2A6E /* DDT86.CMD: layout:dis_end */
#define TC_ASS_ORG 0x2A70 /* DDT86.CMD: layout:asmorg */
#define TC_ASS_CODE_END 0x5322 /* DDT86.CMD: layout:ass_code_end */
#define TC_ASS_END 0x559B /* DDT86.CMD: layout:ass_end */
#define TC_DDT86_ENDCS 0x12C7 /* DDT86.CMD: layout:endcs */
#define TC_DDT86_DATA_END 0x141A /* DDT86.CMD: layout:ddt_data_end */
#define TC_DDT86_IMAGE_LEN 0x55A0 /* DDT86.CMD: layout:image_len */

/* DDT86.A86's user register save area, in its code segment (userax up to endcs) */
#define DDT86_USERAX 0x12A6 /* DDT86.CMD: ddt86:userax */
#define DDT86_USERBX 0x12A8 /* DDT86.CMD: ddt86:userbx */
#define DDT86_USERCX 0x12AA /* DDT86.CMD: ddt86:usercx */
#define DDT86_USERDX 0x12AC /* DDT86.CMD: ddt86:userdx */
#define DDT86_USERSP 0x12AE /* DDT86.CMD: ddt86:usersp */
#define DDT86_USERBP 0x12B0 /* DDT86.CMD: ddt86:userbp */
#define DDT86_USERSI 0x12B2 /* DDT86.CMD: ddt86:usersi */
#define DDT86_USERDI 0x12B4 /* DDT86.CMD: ddt86:userdi */
#define DDT86_USERCS 0x12B6 /* DDT86.CMD: ddt86:usercs */
#define DDT86_USERDS 0x12B8 /* DDT86.CMD: ddt86:userds */
#define DDT86_USERSS 0x12BA /* DDT86.CMD: ddt86:userss */
#define DDT86_USERES 0x12BC /* DDT86.CMD: ddt86:useres */
#define DDT86_USERIP 0x12BE /* DDT86.CMD: ddt86:userip */
#define DDT86_USERFL 0x12C0 /* DDT86.CMD: ddt86:userfl */
#define DDT86_SAVESS 0x12C2 /* DDT86.CMD: ddt86:savess */
#define DDT86_SAVESP 0x12C4 /* DDT86.CMD: ddt86:savesp */
#define DDT86_BREAKFL 0x12C6 /* DDT86.CMD: ddt86:breakfl */

/* DDT86.A86's data (dseg, from endcs) */
#define DDT86_CCPSS 0x12C7 /* DDT86.CMD: ddt86:ccpss */
#define DDT86_CCPSP 0x12C9 /* DDT86.CMD: ddt86:ccpsp */
#define DDT86_CONBUFFHDR 0x12CB /* DDT86.CMD: ddt86:conbuffhdr */
#define DDT86_CONBUFFCNT 0x12CC /* DDT86.CMD: ddt86:conbuffcnt */
#define DDT86_CONBUFF 0x12CD /* DDT86.CMD: ddt86:conbuff */
#define DDT86_CONPTR 0x130E /* DDT86.CMD: ddt86:conptr */
#define DDT86_ASSEMPRESENT 0x1310 /* DDT86.CMD: ddt86:assempresent */
#define DDT86_ASMSPSAV 0x1311 /* DDT86.CMD: ddt86:asmspsav */
#define DDT86_DISLOC 0x1313 /* DDT86.CMD: ddt86:disloc */
#define DDT86_DISSEG 0x1315 /* DDT86.CMD: ddt86:disseg */
#define DDT86_DISMAX 0x1317 /* DDT86.CMD: ddt86:dismax */
#define DDT86_TDISP 0x1319 /* DDT86.CMD: ddt86:tdisp */
#define DDT86_LINEMAX 0x131B /* DDT86.CMD: ddt86:linemax */
#define DDT86_ND80 0x131C /* DDT86.CMD: ddt86:nd80 */
#define DDT86_ND40 0x131D /* DDT86.CMD: ddt86:nd40 */
#define DDT86_NDW40 0x131E /* DDT86.CMD: ddt86:ndw40 */
#define DDT86_NLINES 0x131F /* DDT86.CMD: ddt86:nlines */
#define DDT86_GOLOC 0x1320 /* DDT86.CMD: ddt86:goloc */
#define DDT86_GOSEG 0x1322 /* DDT86.CMD: ddt86:goseg */
#define DDT86_VECTORSAVE 0x1324 /* DDT86.CMD: ddt86:vectorsave */
#define DDT86_BPCNT 0x1330 /* DDT86.CMD: ddt86:bpcnt */
#define DDT86_BRK1LOC 0x1331 /* DDT86.CMD: ddt86:brk1loc */
#define DDT86_BRK1SEG 0x1333 /* DDT86.CMD: ddt86:brk1seg */
#define DDT86_BRK1BYT 0x1335 /* DDT86.CMD: ddt86:brk1byt */
#define DDT86_BRK2LOC 0x1336 /* DDT86.CMD: ddt86:brk2loc */
#define DDT86_BRK2SEG 0x1338 /* DDT86.CMD: ddt86:brk2seg */
#define DDT86_BRK2BYT 0x133A /* DDT86.CMD: ddt86:brk2byt */
#define DDT86_LASLOC 0x133B /* DDT86.CMD: ddt86:lasloc */
#define DDT86_LASSEG 0x133D /* DDT86.CMD: ddt86:lasseg */
#define DDT86_LASMAX 0x133F /* DDT86.CMD: ddt86:lasmax */
#define DDT86_LASCNTSW 0x1341 /* DDT86.CMD: ddt86:lascntsw */
#define DDT86_LASCNT 0x1342 /* DDT86.CMD: ddt86:lascnt */
#define DDT86_DISEMPRESENT 0x1343 /* DDT86.CMD: ddt86:disempresent */
#define DDT86_STARTREADOFF 0x1344 /* DDT86.CMD: ddt86:startreadoff */
#define DDT86_STARTREADSEG 0x1346 /* DDT86.CMD: ddt86:startreadseg */
#define DDT86_ENDREADOFF 0x1348 /* DDT86.CMD: ddt86:endreadoff */
#define DDT86_ENDREADSEG 0x134A /* DDT86.CMD: ddt86:endreadseg */
#define DDT86_DMAOFF 0x134C /* DDT86.CMD: ddt86:dmaoff */
#define DDT86_DMASEG 0x134E /* DDT86.CMD: ddt86:dmaseg */
#define DDT86_TRACECOUNT 0x1350 /* DDT86.CMD: ddt86:tracecount */
#define DDT86_TRACEPRINT 0x1352 /* DDT86.CMD: ddt86:traceprint */
#define DDT86_SKIPBDOS 0x1353 /* DDT86.CMD: ddt86:skipbdos */
#define DDT86_USERIFOFF 0x1354 /* DDT86.CMD: ddt86:userifoff */
#define DDT86_STARTWRITEOFF 0x1355 /* DDT86.CMD: ddt86:startwriteoff */
#define DDT86_STARTWRITESEG 0x1357 /* DDT86.CMD: ddt86:startwriteseg */
#define DDT86_ENDWRITEOFF 0x1359 /* DDT86.CMD: ddt86:endwriteoff */
#define DDT86_ENDWRITESEG 0x135B /* DDT86.CMD: ddt86:endwriteseg */
#define DDT86_NREG 0x135D /* DDT86.CMD: ddt86:nreg */
#define DDT86_SEGFLAG 0x135E /* DDT86.CMD: ddt86:segflag */
#define DDT86_REGNUM 0x135F /* DDT86.CMD: ddt86:regnum */
#define DDT86_XTEMP 0x1361 /* DDT86.CMD: ddt86:xtemp */
#define DDT86_TYPE1LOC 0x1362 /* DDT86.CMD: ddt86:type1loc */
#define DDT86_TYPE1SEG 0x1364 /* DDT86.CMD: ddt86:type1seg */
#define DDT86_TYPE2LOC 0x1366 /* DDT86.CMD: ddt86:type2loc */
#define DDT86_TYPE2SEG 0x1368 /* DDT86.CMD: ddt86:type2seg */
#define DDT86_USERMAX 0x136A /* DDT86.CMD: ddt86:usermax */
#define DDT86_USERLOC2 0x136C /* DDT86.CMD: ddt86:userloc2 */
#define DDT86_USERSEG2 0x136E /* DDT86.CMD: ddt86:userseg2 */
#define DDT86_MCB 0x1370 /* DDT86.CMD: ddt86:mcb */
#define DDT86_MCBBASE 0x1370 /* DDT86.CMD: ddt86:mcbbase */
#define DDT86_MCBLEN 0x1372 /* DDT86.CMD: ddt86:mcblen */
#define DDT86_MCBEXT 0x1374 /* DDT86.CMD: ddt86:mcbext */
#define DDT86_SYSIF 0x1375 /* DDT86.CMD: ddt86:sysif */
#define DDT86_NUMREQ 0x1376 /* DDT86.CMD: ddt86:numreq */
#define DDT86_WMODE 0x1377 /* DDT86.CMD: ddt86:wmode */
#define DDT86_MODE 0x1378 /* DDT86.CMD: ddt86:mode */
#define DDT86_SAVEVECFLAG 0x1379 /* DDT86.CMD: ddt86:savevecflag */
#define DDT86_COL40 0x137A /* DDT86.CMD: ddt86:col40 */
#define DDT86_ERRMODE 0x137B /* DDT86.CMD: ddt86:errmode */
#define DDT86_BASEPAGESAVE 0x137C /* DDT86.CMD: ddt86:basepagesave */
#define DDT86_DELIMS 0x13AC /* DDT86.CMD: ddt86:delims */
#define DDT86_LASTCHAR 0x13B7 /* DDT86.CMD: ddt86:lastchar */
#define DDT86_FCBADR 0x13B8 /* DDT86.CMD: ddt86:fcbadr */
#define DDT86_STACKP 0x141A /* DDT86.CMD: ddt86:stackp */

/* the disassembler overlay's data (INS86, DIS86) */
#define INS86_ALTTABLEPTRS 0x25CD /* DDT86.CMD: dis:pl_alttableptrs */
#define INS86_ALT1TAB 0x25DD /* DDT86.CMD: dis:ins86_pl_alt1tab */
#define INS86_ALT2TAB 0x25ED /* DDT86.CMD: dis:ins86_pl_alt2tab */
#define INS86_ALT3TAB 0x25FD /* DDT86.CMD: dis:ins86_pl_alt3tab */
#define INS86_ALT4TAB 0x260D /* DDT86.CMD: dis:ins86_pl_alt4tab */
#define INS86_ALT5TAB 0x261D /* DDT86.CMD: dis:ins86_pl_alt5tab */
#define INS86_ALT6TAB 0x262D /* DDT86.CMD: dis:ins86_pl_alt6tab */
#define INS86_ALT7TAB 0x263D /* DDT86.CMD: dis:ins86_pl_alt7tab */
#define INS86_ALT8TAB 0x264D /* DDT86.CMD: dis:ins86_pl_alt8tab */
#define INS86_INSTRTABLE 0x265D /* DDT86.CMD: dis:pl_instrtable */
#define DIS86_OPS2 0x285D /* DDT86.CMD: dis:dis86_pl_ops2 */
#define DIS86_OPS3 0x2875 /* DDT86.CMD: dis:dis86_pl_ops3 */
#define DIS86_OPS4 0x2944 /* DDT86.CMD: dis:dis86_pl_ops4 */
#define DIS86_OPS5 0x29A8 /* DDT86.CMD: dis:dis86_pl_ops5 */
#define DIS86_OPS6 0x29F8 /* DDT86.CMD: dis:dis86_pl_ops6 */
#define DIS86_NOPS 0x2A04 /* DDT86.CMD: dis:pl_nops */
#define DIS86_OPNIN 0x2A09 /* DDT86.CMD: dis:pl_opnin */
#define DIS86_TABPTRS 0x2A0F /* DDT86.CMD: dis:pl_tabptrs */
#define DIS86_LEFTBRACKET 0x2A19 /* DDT86.CMD: dis:dis86_pl_leftbracket */
#define DIS86_RIGHTBRACKET 0x2A1A /* DDT86.CMD: dis:dis86_pl_rightbracket */
#define DIS86_ALTTABLEBASE 0x2A1B /* DDT86.CMD: dis:dis86_pl_alttablebase */
#define DIS86_MODBITS 0x2A1D /* DDT86.CMD: dis:dis86_pl_modbits */
#define DIS86_REGBITS 0x2A1E /* DDT86.CMD: dis:dis86_pl_regbits */
#define DIS86_RMBITS 0x2A1F /* DDT86.CMD: dis:dis86_pl_rmbits */
#define DIS86_BYTE1REGBITS 0x2A20 /* DDT86.CMD: dis:dis86_pl_byte1regbits */
#define DIS86_DBIT 0x2A21 /* DDT86.CMD: dis:dis86_pl_dbit */
#define DIS86_SBIT 0x2A22 /* DDT86.CMD: dis:dis86_pl_sbit */
#define DIS86_VBIT 0x2A23 /* DDT86.CMD: dis:dis86_pl_vbit */
#define DIS86_WBIT 0x2A24 /* DDT86.CMD: dis:dis86_pl_wbit */
#define DIS86_ZBIT 0x2A25 /* DDT86.CMD: dis:dis86_pl_zbit */
#define DIS86_MNEMONICINDEX 0x2A26 /* DDT86.CMD: dis:dis86_pl_mnemonicindex */
#define DIS86_INSTRTYPE 0x2A27 /* DDT86.CMD: dis:dis86_pl_instrtype */
#define DIS86_TABLEPTR 0x2A28 /* DDT86.CMD: dis:dis86_pl_tableptr */
#define DIS86_DISEMPTR 0x2A2A /* DDT86.CMD: dis:dis86_pl_disemptr */
#define DIS86_DISEMEND 0x2A2E /* DDT86.CMD: dis:dis86_pl_disemend */
#define DIS86_BORWFLAG 0x2A30 /* DDT86.CMD: dis:dis86_pl_borwflag */
#define DIS86_ERRORFLAG 0x2A31 /* DDT86.CMD: dis:dis86_pl_errorflag */
#define DIS86_REG16 0x2A32 /* DDT86.CMD: dis:pl_reg16 */
#define DIS86_REG8 0x2A42 /* DDT86.CMD: dis:pl_reg8 */
#define DIS86_SEGREG 0x2A52 /* DDT86.CMD: dis:pl_segreg */
#define DIS86_ARR7 0x2A5A /* DDT86.CMD: dis:dis86_arr7 */
#define DIS86_ARR11 0x2A62 /* DDT86.CMD: dis:dis86_arr11 */
#define DIS86_ARR12 0x2A68 /* DDT86.CMD: dis:dis86_arr12 */

/* the assembler overlay's data (ASMTAB, ASS86) */
#define ASMTAB_OPTAB 0x5322 /* DDT86.CMD: ass:pl_optab */
#define ASS86_TOKEN 0x5502 /* DDT86.CMD: ass:ass86_pl_token */
#define ASS86_TOKENLEN 0x5522 /* DDT86.CMD: ass:ass86_pl_tokenlen */
#define ASS86_TOKENTYPE 0x5523 /* DDT86.CMD: ass:ass86_pl_tokentype */
#define ASS86_TOKENVALUE 0x5524 /* DDT86.CMD: ass:ass86_pl_tokenvalue */
#define ASS86_NEXTCH 0x5526 /* DDT86.CMD: ass:ass86_pl_nextch */
#define ASS86_ASSEMPTR 0x5527 /* DDT86.CMD: ass:ass86_pl_assemptr */
#define ASS86_BASEREG 0x552B /* DDT86.CMD: ass:ass86_pl_basereg */
#define ASS86_INDEXREG 0x552F /* DDT86.CMD: ass:ass86_pl_indexreg */
#define ASS86_OPBASE 0x5533 /* DDT86.CMD: ass:ass86_pl_opbase */
#define ASS86_OP1 0x5535 /* DDT86.CMD: ass:ass86_pl_op1 */
#define ASS86_OP2 0x5548 /* DDT86.CMD: ass:ass86_pl_op2 */
#define ASS86_PREFIX 0x555B /* DDT86.CMD: ass:ass86_pl_prefix */
#define ASS86_NPREFIX 0x555E /* DDT86.CMD: ass:ass86_pl_nprefix */
#define ASS86_SEGPREFIXFLAG 0x555F /* DDT86.CMD: ass:ass86_pl_segprefixflag */
#define ASS86_LOCKFLAG 0x5560 /* DDT86.CMD: ass:ass86_pl_lockflag */
#define ASS86_REPFLAG 0x5561 /* DDT86.CMD: ass:ass86_pl_repflag */
#define ASS86_MODBITS 0x5562 /* DDT86.CMD: ass:ass86_pl_modbits */
#define ASS86_REGBITS 0x5563 /* DDT86.CMD: ass:ass86_pl_regbits */
#define ASS86_RMBITS 0x5564 /* DDT86.CMD: ass:ass86_pl_rmbits */
#define ASS86_OPTYPE 0x5565 /* DDT86.CMD: ass:ass86_pl_optype */
#define ASS86_MNEMONICINDEX 0x5566 /* DDT86.CMD: ass:ass86_pl_mnemonicindex */
#define ASS86_INSTR1 0x5567 /* DDT86.CMD: ass:ass86_pl_instr1 */
#define ASS86_DISPLEN 0x5568 /* DDT86.CMD: ass:ass86_pl_displen */
#define ASS86_DISPWORD 0x5569 /* DDT86.CMD: ass:ass86_pl_dispword */
#define ASS86_DELIMITERS 0x556B /* DDT86.CMD: ass:ass86_pl_delimiters */
#define ASS86_LEFTBRACKET 0x5571 /* DDT86.CMD: ass:ass86_pl_leftbracket */
#define ASS86_RIGHTBRACKET 0x5572 /* DDT86.CMD: ass:ass86_pl_rightbracket */
#define ASS86_TYPE7STRUC 0x5573 /* DDT86.CMD: ass:ass86_pl_type7struc */
#define ASS86_TYPE8STRUC 0x5577 /* DDT86.CMD: ass:ass86_pl_type8struc */
#define ASS86_TYPE11STRUC 0x557F /* DDT86.CMD: ass:ass86_pl_type11struc */
#define ASS86_CJSTRUC 0x5583 /* DDT86.CMD: ass:ass86_pl_cjstruc */
#define ASS86_ARR1 0x5587 /* DDT86.CMD: ass:ass86_arr1 */

#endif
