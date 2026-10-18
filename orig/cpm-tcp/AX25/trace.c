/* trace.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Code to do frame tracing */

#include "ax25.h"
#include <ctype.h>

dump(lineno,fp,reason,tdata)
int lineno;
struct frame *fp;
int reason,tdata;
{
	char *ctl,*data;
	char control,*cp,c,type,cmdrsp;
	struct addr *ap;
	char outbuf[80];
	int i,cnt;

	if(fp->iop == NULL)
		return;
	tprintf("%d bytes ",fp->iocnt);
	switch(reason){
	case DUMPORIG:
		tputs("sent to");
		break;
	case DUMPIN:
		tputs("received from");
		break;
	case DUMPUSED:
		tputs("used from");
		break;
	}
	tprintf(" line %d\n",lineno);
	if((ctl = cbyte(fp)) != NULL){
		/* Don't decode unless there is a valid ctl field */
		ap = (struct addr *)fp->iop;
		control = *ctl;
		data = ctl + 1;
		if((ap[0].ssid & C) != (ap[1].ssid & C)){
			if(ap[0].ssid & C)
				cmdrsp = COMMAND;
			else
				cmdrsp = RESPONSE;
		}
		type = ftype(control);
		switch(type){
		case I:
			tputs("I");
			break;
		case SABM:
			tputs("SABM");
			break;
		case DISC:
			tputs("DISC");
			break;
		case DM:
			tputs("DM");
			break;
		case UA:
			tputs("UA");
			break;
		case RR:
			tputs("RR");
			break;
		case RNR:
			tputs("RNR");
			break;
		case REJ:
			tputs("REJ");
			break;
		case FRMR:
			tputs("FRMR");
			break;
		case UI:
			tputs("UI");
			break;
		default:
			tputs("[invalid]");
		}
		/* Dump poll/final bit */
		if(control & PF){
			switch(cmdrsp){
			case COMMAND:
				tputs("(P)");
				break;
			case RESPONSE:
				tputs("(F)");
				break;
			default:
				tputs("(P/F)");
				break;
			}
		}
		/* Dump sequence numbers */
		switch(type & 0x3){
		case I:
		case I+2:
			tprintf(" NS=%d ",(control>>1)&7);
		case S:	/* Fall through */
			tprintf(" NR=%d",(control>>5)&7);
			break;
		}
		/* Display address list */
		tprintf(" %s -> ",pcall(&ap[1]));
		tprintf("%s\n",pcall(&ap[0]));
		/* Check for and display digipeaters on separate line */
		if((ap[1].ssid & E) == 0){
			struct addr *ap1;
	
			tputs("via");
			ap1 = &ap[1];
			do {
				ap1++;
				tprintf(" %s(H=%d)",pcall(ap1),ap1->ssid&REPEATED?1:0);
			} while((ap1->ssid & E) == 0);
			tputs("\n");
		}
		if(type == FRMR){
			tputs("Decoded frame reject information:\n");
			tprintf("Control field: 0x%2x, remote V(S): %d, V(R): %d\n",
				data[0],(data[1] >> 1) & 7,(data[1] >> 5) & 7);
			tprintf("Frame type: %s\n",data[1] & 16?"Response":"Command");
			if(data[2] & W)
				tputs("Invalid control field\n");
			if(data[2] & X)
				tputs("Impermissable information field\n");
			if(data[2] & Y)
				tputs("Excessive information field length\n");
			if(data[2] & Z)
				tputs("Invalid sequence number\n");
		}
	}
	if(tdata < 2)
		return;
	/* Dump data fields in hex, shifted ascii and straight ascii */
	tputs("      0 1 2 3 4 5 6 7 8 9 a b c d e f");
	tputs("  0123456789abcdef  0123456789abcdef\n");
	cp = fp->iop;
	cnt = fp->iocnt;
	while(cnt != 0){
		char *hp,*sp,*rp,sc;

		/* Set starting columns in the output line for each field */
		hp = &outbuf[5];	/* hex */
		sp = &outbuf[39];	/* shifted ascii */
		rp = &outbuf[57];	/* regular ascii */

		setmem(outbuf,sizeof(outbuf),' ');
		strcpy(&outbuf[73],"\n");
		/* Starting address of line */
		sprintf(outbuf,"%03x: ",cp - fp->iop);

		for(i = 0;i<16 && cnt != 0;i++){
			c = *cp++;	/* get char from frame */
			cnt--;
			ctoh(hp,c);	/* hex display */
			hp += 2;
			sc = (c >> 1) & 0x7f;	/* shifted ascii */
			*sp++ = isprint(sc) ? sc : '.';
			c &= 0x7f;	/* regular ascii */
			*rp++ = isprint(c) ? c : '.';
		}
		/* Send the completed line */
		tputs(outbuf);
	}
}
/* Convert byte into two-byte hex-ascii */
ctoh(s,c)
char *s;
char c;
{
	static char convert[] = "0123456789abcdef";

	*s++ = convert[(c >> 4)&0xf];
	*s = convert[c&0xf];
}
