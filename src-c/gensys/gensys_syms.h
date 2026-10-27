/*
 * gensys_syms.h -- the /src-assem layout src-c/gensys builds the low-memory
 * image by: the offset of every SYSDAT.A86 label (SYSTEM.DAT and
 * DATA.BDO: SYS_*) and every XIOS86Q.A86 data label (its private tail
 * past SYSDAT, XQD_*); each module's code length and init/entry offsets
 * (the module table holds them; the 8086 code itself isn't needed); and
 * the code lengths of the RSPs with C versions. Each value is what
 * tools/asm86 gives; tests/src-c/test_syms.py checks them all against
 * the assembled source, so the build itself doesn't need /src-assem.
 */
#ifndef GENSYS_SYMS_H
#define GENSYS_SYMS_H

/* SYSDAT.A86 */
#define SYS_MPMSEG 0x0040 /* SYSDAT.A86: mpmseg */
#define SYS_RSPSEG 0x0042 /* SYSDAT.A86: rspseg */
#define SYS_ENDSEG 0x0044 /* SYSDAT.A86: endseg */
#define SYS_MODULE_MAP 0x0046 /* SYSDAT.A86: module_map */
#define SYS_NCNS 0x0047 /* SYSDAT.A86: ncns */
#define SYS_NLST 0x0048 /* SYSDAT.A86: nlst */
#define SYS_NCCB 0x0049 /* SYSDAT.A86: nccb */
#define SYS_NFLAGS 0x004A /* SYSDAT.A86: nflags */
#define SYS_SRCHDISK 0x004B /* SYSDAT.A86: srchdisk */
#define SYS_MMP 0x004C /* SYSDAT.A86: mmp */
#define SYS_NSLAVES 0x004E /* SYSDAT.A86: nslaves */
#define SYS_DAYFILE 0x004F /* SYSDAT.A86: dayfile */
#define SYS_TEMPDISK 0x0050 /* SYSDAT.A86: tempdisk */
#define SYS_TICKSPERSEC 0x0051 /* SYSDAT.A86: tickspersec */
#define SYS_FREE_ROOT 0x0052 /* SYSDAT.A86: free_root */
#define SYS_CCB 0x0054 /* SYSDAT.A86: ccb */
#define SYS_FLAGS 0x0056 /* SYSDAT.A86: flags */
#define SYS_MDUL 0x0058 /* SYSDAT.A86: mdul */
#define SYS_MFL 0x005A /* SYSDAT.A86: mfl */
#define SYS_PUL 0x005C /* SYSDAT.A86: pul */
#define SYS_QUL 0x005E /* SYSDAT.A86: qul */
#define SYS_QMAU 0x0060 /* SYSDAT.A86: qmau */
#define SYS_RLR 0x0068 /* SYSDAT.A86: rlr */
#define SYS_DLR 0x006A /* SYSDAT.A86: dlr */
#define SYS_DRL 0x006C /* SYSDAT.A86: drl */
#define SYS_PLR 0x006E /* SYSDAT.A86: plr */
#define SYS_SLR 0x0070 /* SYSDAT.A86: slr */
#define SYS_THRDRT 0x0072 /* SYSDAT.A86: thrdrt */
#define SYS_QLR 0x0074 /* SYSDAT.A86: qlr */
#define SYS_MAL 0x0076 /* SYSDAT.A86: mal */
#define SYS_VERSION 0x0078 /* SYSDAT.A86: version */
#define SYS_VERNUM 0x007A /* SYSDAT.A86: vernum */
#define SYS_MPMVERNUM 0x007C /* SYSDAT.A86: mpmvernum */
#define SYS_TOD 0x007E /* SYSDAT.A86: tod */
#define SYS_TOD_DAY 0x007E /* SYSDAT.A86: tod_day */
#define SYS_TOD_HR 0x0080 /* SYSDAT.A86: tod_hr */
#define SYS_TOD_MIN 0x0081 /* SYSDAT.A86: tod_min */
#define SYS_TOD_SEC 0x0082 /* SYSDAT.A86: tod_sec */
#define SYS_NCONDEV 0x0083 /* SYSDAT.A86: ncondev */
#define SYS_NLSTDEV 0x0084 /* SYSDAT.A86: nlstdev */
#define SYS_NCIODEV 0x0085 /* SYSDAT.A86: nciodev */
#define SYS_SYSLREC 0x0086 /* SYSDAT.A86: syslrec */
#define SYS_SYSLFIL 0x0088 /* SYSDAT.A86: syslfil */
#define SYS_LOCK_MAX 0x008A /* SYSDAT.A86: lock_max */
#define SYS_OPEN_MAX 0x008C /* SYSDAT.A86: open_max */
#define SYS_SYSLTOT 0x008E /* SYSDAT.A86: sysltot */
#define SYS_SYSENT 0x00A0 /* SYSDAT.A86: sysent */
#define SYS_MXLOADQD 0x03A0 /* SYSDAT.A86: mxloadqd */
#define SYS_MXLOADQPB 0x03BC /* SYSDAT.A86: mxloadqpb */
#define SYS_MXCLIQD 0x03CC /* SYSDAT.A86: mxcliqd */
#define SYS_MXCLIQPB 0x03E8 /* SYSDAT.A86: mxcliqpb */
#define SYS_MXMEMQD 0x03F8 /* SYSDAT.A86: mxmemqd */
#define SYS_MXMEMQPB 0x0414 /* SYSDAT.A86: mxmemqpb */
#define SYS_LOAD_DMA 0x0424 /* SYSDAT.A86: load_dma */
#define SYS_LOAD_UDA 0x04A4 /* SYSDAT.A86: load_uda */
#define SYS_LOAD_LSTK 0x04A6 /* SYSDAT.A86: load_lstk */
#define SYS_LOAD_BASEP 0x04A8 /* SYSDAT.A86: load_basep */
#define SYS_LOAD_8080 0x04AA /* SYSDAT.A86: load_8080 */
#define SYS_LOAD_NRELSEGS 0x04AB /* SYSDAT.A86: load_nrelsegs */
#define SYS_LOAD_NLDTABENTS 0x04AC /* SYSDAT.A86: load_nldtabents */
#define SYS_LOAD_PD 0x04AE /* SYSDAT.A86: load_pd */
#define SYS_LOAD_FCB 0x04B0 /* SYSDAT.A86: load_fcb */
#define SYS_LOAD_MAXWANTED 0x04B2 /* SYSDAT.A86: load_maxwanted */
#define SYS_LOAD_MINWANTED 0x04B4 /* SYSDAT.A86: load_minwanted */
#define SYS_LOAD_INDMA 0x04B6 /* SYSDAT.A86: load_indma */
#define SYS_LOAD_CHAIN 0x04B8 /* SYSDAT.A86: load_chain */
#define SYS_LDTAB 0x04B9 /* SYSDAT.A86: ldtab */
#define SYS_CLI_DMA 0x0563 /* SYSDAT.A86: cli_dma */
#define SYS_CLI_DMA_OFST 0x05E3 /* SYSDAT.A86: cli_dma_ofst */
#define SYS_CLI_DMA_SEG 0x05E5 /* SYSDAT.A86: cli_dma_seg */
#define SYS_CLI_CHAIN 0x05E7 /* SYSDAT.A86: cli_chain */
#define SYS_CLI_TERM 0x05E8 /* SYSDAT.A86: cli_term */
#define SYS_CLI_PFLAG 0x05E9 /* SYSDAT.A86: cli_pflag */
#define SYS_CLI_NET 0x05EB /* SYSDAT.A86: cli_net */
#define SYS_CLI_PPD 0x05EC /* SYSDAT.A86: cli_ppd */
#define SYS_CLI_CMDTAIL 0x05EE /* SYSDAT.A86: cli_cmdtail */
#define SYS_CLI_FCB 0x0670 /* SYSDAT.A86: cli_fcb */
#define SYS_CLI_CUSPQPB 0x0691 /* SYSDAT.A86: cli_cuspqpb */
#define SYS_CLI_ACB 0x06A1 /* SYSDAT.A86: cli_acb */
#define SYS_CLI_PCB 0x06AD /* SYSDAT.A86: cli_pcb */
#define SYS_CLI_PD 0x06B1 /* SYSDAT.A86: cli_pd */
#define SYS_CLI_ERR 0x06B3 /* SYSDAT.A86: cli_err */
#define SYS_CLI_BPAGE 0x06B5 /* SYSDAT.A86: cli_bpage */
#define SYS_CLI_LDDSK 0x06B7 /* SYSDAT.A86: cli_lddsk */
#define SYS_CLI_CNS 0x06B8 /* SYSDAT.A86: cli_cns */
#define SYS_CLI_USER 0x06B9 /* SYSDAT.A86: cli_user */
#define SYS_CLI_DSK 0x06BA /* SYSDAT.A86: cli_dsk */
#define SYS_CLI_ERR_MODE 0x06BB /* SYSDAT.A86: cli_err_mode */
#define SYS_CLI_DFIL 0x06BC /* SYSDAT.A86: cli_dfil */
#define SYS_INITUDA 0x06C0 /* SYSDAT.A86: inituda */
#define SYS_IDLEUDA 0x07C0 /* SYSDAT.A86: idleuda */
#define SYS_TICKUDA 0x08C0 /* SYSDAT.A86: tickuda */
#define SYS_INITPD 0x09C0 /* SYSDAT.A86: initpd */
#define SYS_IDLEPD 0x09F0 /* SYSDAT.A86: idlepd */
#define SYS_TICKPD 0x0A20 /* SYSDAT.A86: tickpd */
#define SYS_IDLE_TOS 0x0A7A /* SYSDAT.A86: idle_tos */
#define SYS_TICK_TOS 0x0AAA /* SYSDAT.A86: tick_tos */
#define SYS_INIT_TOS 0x0AFE /* SYSDAT.A86: init_tos */
#define SYS_DSPTCHTOS 0x0B5E /* SYSDAT.A86: dsptchtos */
#define SYS_INDISP 0x0B5E /* SYSDAT.A86: indisp */
#define SYS_INTFLAG 0x0B5F /* SYSDAT.A86: intflag */
#define SYS_ES_SAV 0x0B60 /* SYSDAT.A86: es_sav */
#define SYS_BX_SAV 0x0B62 /* SYSDAT.A86: bx_sav */
#define SYS_MXMEMOWNER 0x0B64 /* SYSDAT.A86: mxmemowner */
#define SYS_MXMEMCOUNT 0x0B66 /* SYSDAT.A86: mxmemcount */
#define SYS_EFCB 0x0C00 /* SYSDAT.A86: efcb */
#define SYS_RODSK 0x0C01 /* SYSDAT.A86: rodsk */
#define SYS_DLOG 0x0C03 /* SYSDAT.A86: dlog */
#define SYS_RLOG 0x0C05 /* SYSDAT.A86: RLOG */
#define SYS_TLOG 0x0C07 /* SYSDAT.A86: TLOG */
#define SYS_NTLOG 0x0C09 /* SYSDAT.A86: NTLOG */
#define SYS_REM_DRV 0x0C0B /* SYSDAT.A86: REM_DRV */
#define SYS_FCBDSK 0x0C0C /* SYSDAT.A86: fcbdsk */
#define SYS_PARCOPFL 0x0C0D /* SYSDAT.A86: parcopfl */
#define SYS_RESEL 0x0C0E /* SYSDAT.A86: resel */
#define SYS_ARET 0x0C0F /* SYSDAT.A86: aret */
#define SYS_COMP_FCB_CKS 0x0C11 /* SYSDAT.A86: COMP_FCB_CKS */
#define SYS_SEARCH_USER0 0x0C12 /* SYSDAT.A86: SEARCH_USER0 */
#define SYS_MAKE_XFCB 0x0C13 /* SYSDAT.A86: MAKE_XFCB */
#define SYS_FIND_XFCB 0x0C14 /* SYSDAT.A86: FIND_XFCB */
#define SYS_USRCODE 0x0C15 /* SYSDAT.A86: usrcode */
#define SYS_SELDSK 0x0C16 /* SYSDAT.A86: SELDSK */
#define SYS_INFO 0x0C17 /* SYSDAT.A86: info */
#define SYS_SRCHA 0x0C19 /* SYSDAT.A86: srcha */
#define SYS_DMAAD 0x0C1B /* SYSDAT.A86: dmaad */
#define SYS_DMABASE 0x0C1D /* SYSDAT.A86: dmabase */
#define SYS_FX 0x0C1F /* SYSDAT.A86: FX */
#define SYS_SRCHL 0x0C20 /* SYSDAT.A86: srchl */
#define SYS_SRCHAOFST 0x0C21 /* SYSDAT.A86: srchaofst */
#define SYS_SRCHABASE 0x0C23 /* SYSDAT.A86: srchabase */
#define SYS_DCNT 0x0C25 /* SYSDAT.A86: dcnt */
#define SYS_DBLK 0x0C27 /* SYSDAT.A86: DBLK */
#define SYS_ERROR_MODE 0x0C29 /* SYSDAT.A86: ERROR_MODE */
#define SYS_MULT_CNT 0x0C2A /* SYSDAT.A86: MULT_CNT */
#define SYS_DF_PASSWORD 0x0C2B /* SYSDAT.A86: DF_PASSWORD */
#define SYS_PD_CNT 0x0C33 /* SYSDAT.A86: PD_CNT */
#define SYS_HIGH_EXT 0x0C34 /* SYSDAT.A86: HIGH_EXT */
#define SYS_XFCB_READ_ONLY 0x0C35 /* SYSDAT.A86: XFCB_READ_ONLY */
#define SYS_CURDSK 0x0C36 /* SYSDAT.A86: CURDSK */
#define SYS_PACKED_DCNT 0x0C37 /* SYSDAT.A86: PACKED_DCNT */
#define SYS_PDADDR 0x0C3A /* SYSDAT.A86: PDADDR */
#define SYS_CDRMAXA 0x0C3C /* SYSDAT.A86: cdrmaxa */
#define SYS_DRVLBLA 0x0C3E /* SYSDAT.A86: DRVLBLA */
#define SYS_BUFFA 0x0C40 /* SYSDAT.A86: buffa */
#define SYS_DPBADDR 0x0C42 /* SYSDAT.A86: dpbaddr */
#define SYS_CHECKA 0x0C44 /* SYSDAT.A86: checka */
#define SYS_ALLOCA 0x0C46 /* SYSDAT.A86: alloca */
#define SYS_SECTPT 0x0C48 /* SYSDAT.A86: sectpt */
#define SYS_BLKSHF 0x0C4A /* SYSDAT.A86: blkshf */
#define SYS_BLKMSK 0x0C4B /* SYSDAT.A86: blkmsk */
#define SYS_EXTMSK 0x0C4C /* SYSDAT.A86: extmsk */
#define SYS_MAXALL 0x0C4D /* SYSDAT.A86: maxall */
#define SYS_DIRMAX 0x0C4F /* SYSDAT.A86: dirmax */
#define SYS_DIRBLK 0x0C51 /* SYSDAT.A86: dirblk */
#define SYS_CHKSIZ 0x0C53 /* SYSDAT.A86: chksiz */
#define SYS_OFFSETV 0x0C55 /* SYSDAT.A86: offsetv */
#define SYS_COMMON_DMA 0x0C57 /* SYSDAT.A86: COMMON_DMA */
#define SYS_ENDLIST 0x0C57 /* SYSDAT.A86: endlist */
#define SYS_XDMAAD 0x0C67 /* SYSDAT.A86: XDMAAD */
#define SYS_RETURN_FFFF 0x0C69 /* SYSDAT.A86: RETURN_FFFF */
#define SYS_MAKE_FLAG 0x0C6A /* SYSDAT.A86: MAKE_FLAG */
#define SYS_FCB_EXISTS 0x0C6B /* SYSDAT.A86: FCB_EXISTS */
#define SYS_ACTUAL_RC 0x0C6C /* SYSDAT.A86: ACTUAL_RC */
#define SYS_SAVE_XFCB 0x0C6D /* SYSDAT.A86: SAVE_XFCB */
#define SYS_SAVE_MOD 0x0C6E /* SYSDAT.A86: SAVE_MOD */
#define SYS_SAVE_EXT 0x0C6F /* SYSDAT.A86: SAVE_EXT */
#define SYS_ATTRIBUTES 0x0C70 /* SYSDAT.A86: ATTRIBUTES */
#define SYS_CHK_OLIST_FLAG 0x0C71 /* SYSDAT.A86: CHK_OLIST_FLAG */
#define SYS_LOCK_SP 0x0C72 /* SYSDAT.A86: LOCK_SP */
#define SYS_LOCK_SHELL 0x0C74 /* SYSDAT.A86: LOCK_SHELL */
#define SYS_CHECK_FCB_RET 0x0C75 /* SYSDAT.A86: CHECK_FCB_RET */
#define SYS_LOCK_UNLOCK 0x0C76 /* SYSDAT.A86: LOCK_UNLOCK */
#define SYS_INCR_PDCNT 0x0C77 /* SYSDAT.A86: INCR_PDCNT */
#define SYS_FREE_MODE 0x0C78 /* SYSDAT.A86: FREE_MODE */
#define SYS_CUR_POS 0x0C79 /* SYSDAT.A86: CUR_POS */
#define SYS_PRV_POS 0x0C7B /* SYSDAT.A86: PRV_POS */
#define SYS_SDCNT 0x0C7D /* SYSDAT.A86: SDCNT */
#define SYS_SDBLK 0x0C7F /* SYSDAT.A86: SDBLK */
#define SYS_SDCNT0 0x0C81 /* SYSDAT.A86: SDCNT0 */
#define SYS_SDBLK0 0x0C83 /* SYSDAT.A86: SDBLK0 */
#define SYS_DONT_CLOSE 0x0C85 /* SYSDAT.A86: DONT_CLOSE */
#define SYS_OPEN_CNT 0x0C86 /* SYSDAT.A86: OPEN_CNT */
#define SYS_LOCK_CNT 0x0C87 /* SYSDAT.A86: LOCK_CNT */
#define SYS_FILE_ID 0x0C88 /* SYSDAT.A86: FILE_ID */
#define SYS_DELETED_FILES 0x0C8A /* SYSDAT.A86: DELETED_FILES */
#define SYS_SET_RO_FLAG 0x0C8B /* SYSDAT.A86: SET_RO_FLAG */
#define SYS_CHECK_DISK 0x0C8C /* SYSDAT.A86: CHECK_DISK */
#define SYS_FLUSHED 0x0C8D /* SYSDAT.A86: FLUSHED */
#define SYS_OPEN_ROOT 0x0C90 /* SYSDAT.A86: OPEN_ROOT */
#define SYS_LOCK_ROOT 0x0C92 /* SYSDAT.A86: LOCK_ROOT */
#define SYS_TRANV 0x0C94 /* SYSDAT.A86: tranv */
#define SYS_FCBCOPIED 0x0C96 /* SYSDAT.A86: fcbcopied */
#define SYS_RMF 0x0C97 /* SYSDAT.A86: rmf */
#define SYS_WFLAG 0x0C98 /* SYSDAT.A86: wflag */
#define SYS_DIRLOC 0x0C99 /* SYSDAT.A86: dirloc */
#define SYS_SEQIO 0x0C9A /* SYSDAT.A86: seqio */
#define SYS_LINFO 0x0C9B /* SYSDAT.A86: linfo */
#define SYS_DMINX 0x0C9C /* SYSDAT.A86: dminx */
#define SYS_TINFO 0x0C9D /* SYSDAT.A86: tinfo */
#define SYS_SINGLE 0x0C9F /* SYSDAT.A86: single */
#define SYS_OLDDSK 0x0CA0 /* SYSDAT.A86: olddsk */
#define SYS_RCOUNT 0x0CA1 /* SYSDAT.A86: rcount */
#define SYS_EXTVAL 0x0CA2 /* SYSDAT.A86: extval */
#define SYS_VRECORD 0x0CA3 /* SYSDAT.A86: VRECORD */
#define SYS_ARECORD 0x0CA4 /* SYSDAT.A86: arecord */
#define SYS_ABLOCK 0x0CA7 /* SYSDAT.A86: ablock */
#define SYS_DPTR 0x0CA9 /* SYSDAT.A86: dptr */
#define SYS_XDCNT 0x0CAA /* SYSDAT.A86: XDCNT */
#define SYS_XDBLK 0x0CAC /* SYSDAT.A86: XDBLK */
#define SYS_USER_ZERO_PASS 0x0CAE /* SYSDAT.A86: USER_ZERO_PASS */
#define SYS_SHELL_SI 0x0CAF /* SYSDAT.A86: SHELL_SI */
#define SYS_SHELL_DMA 0x0CB1 /* SYSDAT.A86: SHELL_DMA */
#define SYS_SHELL_FLAG 0x0CB3 /* SYSDAT.A86: SHELL_FLAG */
#define SYS_SHELL_RR 0x0CB4 /* SYSDAT.A86: SHELL_RR */
#define SYS_INFOSAVE 0x0CB7 /* SYSDAT.A86: infosave */
#define SYS_PARAMETERSEGMENT 0x0CB9 /* SYSDAT.A86: parametersegment */
#define SYS_RETURNSEG 0x0CBB /* SYSDAT.A86: returnseg */
#define SYS_PARLG 0x0CBD /* SYSDAT.A86: parlg */
#define SYS_DSKMSG 0x0CBE /* SYSDAT.A86: dskmsg */
#define SYS_DSKERR 0x0CCA /* SYSDAT.A86: dskerr */
#define SYS_PERMSG 0x0CCE /* SYSDAT.A86: permsg */
#define SYS_SELMSG 0x0CD9 /* SYSDAT.A86: selmsg */
#define SYS_ROFMSG 0x0CE0 /* SYSDAT.A86: rofmsg */
#define SYS_RODMSG 0x0CE5 /* SYSDAT.A86: rodmsg */
#define SYS_XERR_LIST 0x0CE9 /* SYSDAT.A86: XERR_LIST */
#define SYS_XE3 0x0CFB /* SYSDAT.A86: XE3 */
#define SYS_XE4 0x0D18 /* SYSDAT.A86: XE4 */
#define SYS_XE5 0x0D19 /* SYSDAT.A86: XE5 */
#define SYS_XE6 0x0D2D /* SYSDAT.A86: XE6 */
#define SYS_XE7 0x0D42 /* SYSDAT.A86: XE7 */
#define SYS_XE8 0x0D51 /* SYSDAT.A86: XE8 */
#define SYS_XE9 0x0D65 /* SYSDAT.A86: XE9 */
#define SYS_XE10 0x0D76 /* SYSDAT.A86: XE10 */
#define SYS_XE11 0x0D8F /* SYSDAT.A86: XE11 */
#define SYS_CRLFSTR 0x0DAB /* SYSDAT.A86: crlfstr */
#define SYS_PR_FX 0x0DAE /* SYSDAT.A86: PR_FX */
#define SYS_PR_FX1 0x0DBD /* SYSDAT.A86: PR_FX1 */
#define SYS_PR_FCB 0x0DC0 /* SYSDAT.A86: PR_FCB */
#define SYS_PR_FCB1 0x0DC8 /* SYSDAT.A86: PR_FCB1 */
#define SYS_DENIEDMSG 0x0DD5 /* SYSDAT.A86: DENIEDMSG */
#define SYS_DENIEDDRV 0x0DF0 /* SYSDAT.A86: DENIEDDRV */
#define SYS_DENIEDCNS 0x0DFB /* SYSDAT.A86: DENIEDCNS */
#define SYS_DENIEDPRC 0x0E05 /* SYSDAT.A86: DENIEDPRC */
#define SYS_LOC_PAR_AREA 0x0E0E /* SYSDAT.A86: loc_par_area */
#define SYS_SAVE_SP 0x0F10 /* SYSDAT.A86: SAVE_SP */
#define SYS_SSSAVE 0x0F12 /* SYSDAT.A86: sssave */
#define SYS_SPSAVE 0x0F14 /* SYSDAT.A86: spsave */
#define SYS_BDOSSTACK 0x0F8E /* SYSDAT.A86: bdosstack */
#define SYS_SETDF 0x0F8E /* SYSDAT.A86: setdf */
#define SYS_SETBF 0x0F8F /* SYSDAT.A86: setbf */
#define SYS_PF_KEEPSAV 0x0F90 /* SYSDAT.A86: pf_keepsav */
#define SYS_MXDISKQD 0x0F92 /* SYSDAT.A86: mxdiskqd */
#define SYS_MXDISKQPB 0x0FAE /* SYSDAT.A86: mxdiskqpb */
#define SYS_DSEG_LEN 0x1000 /* SYSDAT.A86: dseg length */

/* XIOS86Q.A86: its data past SYSDAT */
#define XQD_TICK_COUNT 0x1000 /* XIOS86Q.A86: tick_count */
#define XQD_CLOCKON 0x1001 /* XIOS86Q.A86: clockon */
#define XQD_STOPPOLL 0x1002 /* XIOS86Q.A86: stoppoll */
#define XQD_DISK 0x1004 /* XIOS86Q.A86: disk */
#define XQD_TRK 0x1005 /* XIOS86Q.A86: trk */
#define XQD_SECT 0x1006 /* XIOS86Q.A86: sect */
#define XQD_DMA_ADR 0x1007 /* XIOS86Q.A86: dma_adr */
#define XQD_DMA_SEG 0x1009 /* XIOS86Q.A86: dma_seg */
#define XQD_CUR_CYL 0x100B /* XIOS86Q.A86: cur_cyl */
#define XQD_CUR_HEAD 0x100C /* XIOS86Q.A86: cur_head */
#define XQD_CUR_PSEC 0x100D /* XIOS86Q.A86: cur_psec */
#define XQD_CUR_BUFOFF 0x100E /* XIOS86Q.A86: cur_bufoff */
#define XQD_OUTBYTE_TMP 0x1010 /* XIOS86Q.A86: outbyte_tmp */
#define XQD_DTRNTBL 0x1011 /* XIOS86Q.A86: dtrntbl */
#define XQD_CCBTAB0 0x1015 /* XIOS86Q.A86: ccbtab0 */
#define XQD_SPECIFY_PARMS 0x102D /* XIOS86Q.A86: specify_parms */
#define XQD_RECAL_PARMS 0x1031 /* XIOS86Q.A86: recal_parms */
#define XQD_SEEK_PARMS 0x1034 /* XIOS86Q.A86: seek_parms */
#define XQD_READ_PARMS 0x1038 /* XIOS86Q.A86: read_parms */
#define XQD_WRITE_PARMS 0x1042 /* XIOS86Q.A86: write_parms */
#define XQD_SECBUF 0x104C /* XIOS86Q.A86: secbuf */
#define XQD_DPH0 0x1250 /* XIOS86Q.A86: dph0 */
#define XQD_DPB0 0x1260 /* XIOS86Q.A86: dpb0 */
#define XQD_ALLOC0 0x126F /* XIOS86Q.A86: alloc0 */
#define XQD_CSV0 0x1286 /* XIOS86Q.A86: csv0 */
#define XQD_DIRBUF0 0x1296 /* XIOS86Q.A86: dirbuf0 */
#define XQD_INIT_SS_SAVE 0x1356 /* XIOS86Q.A86: init_ss_save */
#define XQD_TICKINT_TOS 0x1356 /* XIOS86Q.A86: tickint_tos */
#define XQD_INIT_SP_SAVE 0x1358 /* XIOS86Q.A86: init_sp_save */
#define XQD_SIGNON 0x135A /* XIOS86Q.A86: signon */
#define XQD_DSEG_LEN 0x139B /* XIOS86Q.A86: dseg length */

/* the modules, in the order the image holds them: code length, init, entry */
#define GS_SUP_CODE_LEN 0x137B /* SUP.A86: cseg length */
#define GS_SUP_INIT 0x007A /* SUP.A86: init */
#define GS_SUP_ENTRY 0x02E5 /* SUP.A86: entry */
#define GS_RTM_CODE_LEN 0x0F98 /* RTM.A86: cseg length */
#define GS_RTM_INIT 0x0037 /* RTM.A86: init */
#define GS_RTM_ENTRY 0x00EC /* RTM.A86: entry */
#define GS_MEM_CODE_LEN 0x0AB6 /* MEM.A86: cseg length */
#define GS_MEM_INIT 0x0037 /* MEM.A86: init */
#define GS_MEM_ENTRY 0x006D /* MEM.A86: entry */
#define GS_CIO_CODE_LEN 0x08F9 /* CIO.A86: cseg length */
#define GS_CIO_INIT 0x0037 /* CIO.A86: init */
#define GS_CIO_ENTRY 0x0081 /* CIO.A86: entry */
#define GS_BDOS_CODE_LEN 0x268E /* BDOS.A86: cseg length */
#define GS_BDOS_INIT 0x0037 /* BDOS.A86: init */
#define GS_BDOS_ENTRY 0x0101 /* BDOS.A86: entry */
#define GS_XIOS_CODE_LEN 0x04FB /* XIOS86Q.A86: cseg length */
#define GS_XIOS_INIT 0x0044 /* XIOS86Q.A86: init */
#define GS_XIOS_ENTRY 0x00FD /* XIOS86Q.A86: entry */

/* the RSPs: code length (CACHE.RSP's is in src-c/rsp/cache.c) */
#define GS_TMP_CODE_LEN 0x0422 /* TMP.A86: cseg length */
#define GS_CLOCK_CODE_LEN 0x0074 /* CLOCK.A86: cseg length */
#define GS_ECHO_CODE_LEN 0x0085 /* ECHO.A86: cseg length */

/* their start addresses (the IP in their initial stack frame) */
#define GS_TMP_START 0x0003 /* TMP.A86: tmp */
#define GS_CLOCK_START 0x0003 /* CLOCK.A86: clock */
#define GS_ECHO_START 0x0003 /* ECHO.A86: main */

#endif
