/*
 * utils_syms.h -- the offsets in src-assem/utils/'s PL/M utilities that
 * their C versions (src-c/utils/) lay their data images out by, as
 * tools/plmc.py compiles them with the PL/M interface stub (as
 * src-assem's build does): each program's data labels from 100h on (the
 * PL/M variables, without plmc's pl_ prefix; plmc's constant strings,
 * arrNN; the stack base), and its data and code lengths (the CMD file's
 * groups). tests/src-c/test_syms.py checks them all against the compiled
 * source, so the build itself doesn't need src-assem.
 */
#ifndef UTILS_SYMS_H
#define UTILS_SYMS_H

#define DIR_PARSEFN 0x0100 /* DIR.PLM: pl_parsefn */
#define DIR_DIRTITLE 0x0104 /* DIR.PLM: pl_dirtitle */
#define DIR_SYS 0x011B /* DIR.PLM: pl_sys */
#define DIR_TEMP 0x011C /* DIR.PLM: pl_temp */
#define DIR_DCNT 0x011D /* DIR.PLM: pl_dcnt */
#define DIR_CNT 0x011E /* DIR.PLM: pl_cnt */
#define DIR_USER 0x011F /* DIR.PLM: pl_user */
#define DIR_I 0x0120 /* DIR.PLM: pl_i */
#define DIR_NEWUSER 0x0121 /* DIR.PLM: pl_newuser */
#define DIR_SYSEXISTS 0x0122 /* DIR.PLM: pl_sysexists */
#define DIR_INCLSYS 0x0123 /* DIR.PLM: pl_inclsys */
#define DIR_OPTION 0x0124 /* DIR.PLM: pl_option */
#define DIR_DIRBUF 0x0125 /* DIR.PLM: pl_dirbuf */
#define DIR_LASTDSEGBYTE 0x01A5 /* DIR.PLM: pl_lastdsegbyte */
#define DIR_ARR47 0x01A6 /* DIR.PLM: arr47 */
#define DIR_ARR51 0x01B8 /* DIR.PLM: arr51 */
#define DIR_ARR55 0x01CD /* DIR.PLM: arr55 */
#define DIR_STACK_BASE 0x05D9 /* DIR.PLM: pl_stack_base */
#define DIR_DATA_LEN 0x05DA /* DIR.PLM: dseg length */
#define DIR_CODE_LEN 0x091E /* DIR.PLM: cseg length */

#define TYPE_PARSEFN 0x0100 /* TYPE.PLM: pl_parsefn */
#define TYPE_EOD 0x0104 /* TYPE.PLM: pl_eod */
#define TYPE_I 0x0105 /* TYPE.PLM: pl_i */
#define TYPE_CHAR 0x0106 /* TYPE.PLM: pl_char */
#define TYPE_CNT 0x0107 /* TYPE.PLM: pl_cnt */
#define TYPE_TCNT 0x0108 /* TYPE.PLM: pl_tcnt */
#define TYPE_VER 0x0109 /* TYPE.PLM: pl_ver */
#define TYPE_ERRORCODE 0x010B /* TYPE.PLM: pl_errorcode */
#define TYPE_LASTDSEGBYTE 0x010D /* TYPE.PLM: pl_lastdsegbyte */
#define TYPE_ARR10 0x010E /* TYPE.PLM: arr10 */
#define TYPE_ARR39 0x011A /* TYPE.PLM: arr39 */
#define TYPE_ARR91 0x012C /* TYPE.PLM: arr91 */
#define TYPE_STACK_BASE 0x0535 /* TYPE.PLM: pl_stack_base */
#define TYPE_DATA_LEN 0x0536 /* TYPE.PLM: dseg length */
#define TYPE_CODE_LEN 0x0797 /* TYPE.PLM: cseg length */

#define ERA_PLMSTACK 0x0100 /* ERA.PLM: pl_plmstack */
#define ERA_STACKSIZE 0x0120 /* ERA.PLM: pl_stacksize */
#define ERA_PARSEFN 0x0122 /* ERA.PLM: pl_parsefn */
#define ERA_PDPOINTER 0x0126 /* ERA.PLM: pl_pdpointer */
#define ERA_UDAPOINTER 0x012A /* ERA.PLM: pl_udapointer */
#define ERA_XFCB 0x012E /* ERA.PLM: pl_xfcb */
#define ERA_I 0x012F /* ERA.PLM: pl_i */
#define ERA_RESPONSE 0x0130 /* ERA.PLM: pl_response */
#define ERA_USER 0x0131 /* ERA.PLM: pl_user */
#define ERA_CODE 0x0132 /* ERA.PLM: pl_code */
#define ERA_VER 0x0133 /* ERA.PLM: pl_ver */
#define ERA_LASTDSEGBYTE 0x0135 /* ERA.PLM: pl_lastdsegbyte */
#define ERA_ARR7 0x0136 /* ERA.PLM: arr7 */
#define ERA_ARR11 0x0148 /* ERA.PLM: arr11 */
#define ERA_ARR19 0x0151 /* ERA.PLM: arr19 */
#define ERA_ARR23 0x015B /* ERA.PLM: arr23 */
#define ERA_ARR27 0x016C /* ERA.PLM: arr27 */
#define ERA_ARR56 0x017B /* ERA.PLM: arr56 */
#define ERA_ARR78 0x0187 /* ERA.PLM: arr78 */
#define ERA_ARR95 0x0194 /* ERA.PLM: arr95 */
#define ERA_ARR102 0x01A6 /* ERA.PLM: arr102 */
#define ERA_ARR112 0x01B8 /* ERA.PLM: arr112 */
#define ERA_ARR126 0x01DD /* ERA.PLM: arr126 */
#define ERA_STACK_BASE 0x05E7 /* ERA.PLM: pl_stack_base */
#define ERA_DATA_LEN 0x05E8 /* ERA.PLM: dseg length */
#define ERA_CODE_LEN 0x0A2D /* ERA.PLM: cseg length */

#define REN_PARSEFN 0x0100 /* REN.PLM: pl_parsefn */
#define REN_PDPOINTER 0x0104 /* REN.PLM: pl_pdpointer */
#define REN_UDAPOINTER 0x0108 /* REN.PLM: pl_udapointer */
#define REN_FAILED 0x010C /* REN.PLM: pl_failed */
#define REN_READONLY 0x011C /* REN.PLM: pl_readonly */
#define REN_BADWILDCARD 0x012E /* REN.PLM: pl_badwildcard */
#define REN_PASSWD 0x013F /* REN.PLM: pl_passwd */
#define REN_NEWFCBADR 0x0147 /* REN.PLM: pl_newfcbadr */
#define REN_CURFCB 0x0149 /* REN.PLM: pl_curfcb */
#define REN_VER 0x016A /* REN.PLM: pl_ver */
#define REN_LASTDSEGBYTE 0x016C /* REN.PLM: pl_lastdsegbyte */
#define REN_ARR7 0x016D /* REN.PLM: arr7 */
#define REN_ARR11 0x0184 /* REN.PLM: arr11 */
#define REN_ARR21 0x0196 /* REN.PLM: arr21 */
#define REN_ARR25 0x01A7 /* REN.PLM: arr25 */
#define REN_ARR29 0x01B6 /* REN.PLM: arr29 */
#define REN_ARR51 0x01C5 /* REN.PLM: arr51 */
#define REN_ARR97 0x01D1 /* REN.PLM: arr97 */
#define REN_ARR107 0x01E1 /* REN.PLM: arr107 */
#define REN_ARR115 0x01EE /* REN.PLM: arr115 */
#define REN_STACK_BASE 0x0600 /* REN.PLM: pl_stack_base */
#define REN_DATA_LEN 0x0601 /* REN.PLM: dseg length */
#define REN_CODE_LEN 0x0BE3 /* REN.PLM: cseg length */
#endif
