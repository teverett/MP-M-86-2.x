/* cmdrout.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Command routines */

#include <stdio.h>
#include "ax25.h"
#include "command.h"

/* Set or display terminal parameters */
termparam(argc,argv)
int argc;
char *argv[];
{
	register struct vartab *tp;
	char *param,*arg;

	if(argc == 1){
		for(tp = termtab;tp->name != NULL; tp++){
			tprintf("%-10s%s\n",tp->name,pval(tp));
		}
		return;
	}
	param = argv[1];
	for(tp = termtab;tp->name != NULL; tp++){
		if(cmdchk(tp->name,param))
			break;
	}
	if(tp->name == NULL){
		tprintf("unknown parameter %s\n",param);
		return;
	}
	if(argc == 2){
		tprintf("%-10s%s\n",tp->name,pval(tp));
		return;
	}
	arg = argv[2];
	if(setval(tp,arg) != -1)
		asyncparam();
}

/* Display and change line parameters */
lineparam(argc,argv)
int argc;
char *argv[];
{
	int lineno;
	char *param,*arg;
	register struct vartab *lp;
	struct line *line;

	lineno = 0;
	if(argc == 1){
		d_line();
		return;
	}
	lineno = atoi(argv[1]);
	if(lineno < 0 || lineno >= nlines){
		tprintf("Line number out of range\n");
		return;
	}
	line = lines[lineno];
	movmem((char *)line,(char *)&lt,sizeof(struct line));
	if(argc == 2){
		for(lp=linetab;lp->name!=NULL;lp++)
			tprintf("%-10s%s\n",lp->name,pval(lp));
		return;
	}
	param = argv[2];
	for(lp=linetab;lp->name!=NULL;lp++){
		if(cmdchk(lp->name,param))
			break;
	}
	if(lp->name == NULL){
		tprintf("Line parameter %s unknown\n",param);
		return;
	}
	if(argc == 3){
		tprintf("%-10s%s\n",lp->name,pval(lp));
		return;
	}
	arg = argv[3];
	if(setval(lp,arg) != -1){
		movmem((char *)&lt,(char *)line,sizeof(struct line));
		hdlcparam(lineno);
	}
}

/* Display and change link parameters */
linkparam(argc,argv)
int argc;
char *argv[];
{
	struct link *link;
	char *param,*arg;
	struct addr addr;
	struct vartab *lp;

	if(argc == 1){
		d_link();
		return;
	}
	if(cmdchk("default",argv[1])){
		link = &linkdefault;
	} else {
		if(setcall(&addr,argv[1]) == -1){
			tprintf("Invalid address\n");
			return;
		}
		if((link = find_link(&addr)) == NULL){
			tprintf("Not connected to %s\n",pcall(&addr));
			return;
		}
	}
	movmem((char *)link,(char *)&ln,sizeof(struct link));
	if(argc == 2){
		for(lp=linktab;lp->name!=NULL;lp++){
			tprintf("%-10s%s\n",lp->name,pval(lp));
		}
		return;
	}
	param = argv[2];

	for(lp=linktab;lp->name!=NULL;lp++){
		if(cmdchk(lp->name,param))
			break;
	}
	if(lp->name == NULL){
		tprintf("Link parameter %s unknown\n",param);
		return;
	}
	if(argc == 3){
		tprintf("%-10s%s\n",lp->name,pval(lp));
		return;
	}
	arg = argv[3];
	if(setval(lp,arg) != -1)
		movmem((char *)&ln,(char *)link,sizeof(struct link));
}

/* load all variables from disk file */
load(argc,argv)
int argc;
char *argv[];
{
	FILE *fp;
	char *fn;
	int i;

	if(argc == 1)
		fn = CONFIG;
	else
		fn = argv[1];
	if((fp = fopen(fn,"r")) == NULL){
		tprintf("Cannot open %s\n",fn);
	} else {
		fread((char *)&mycall,sizeof(mycall),1,fp);
		fread((char *)&unproto,sizeof(unproto),1,fp);
		fread(btext,sizeof(btext),1,fp);
		fread((char *)&term,sizeof(term),1,fp);
		fread((char *)&linkdefault,sizeof(linkdefault),1,fp);
		fread((char *)&nlines,sizeof(nlines),1,fp);
		for(i=0;i<nlines;i++){
			if(lines[i] == NULL)
				lines[i] = (struct line *)calloc(1,
				 sizeof(struct line));
			fread((char *)lines[i],sizeof(struct line),1,fp);
		}
		fclose(fp);
	}
	/* Initialize pointers in the control blocks we just read */
	terminit();
	lineinit();
}
/* (Re)initialize everything */
reset()
{
	char i_state;

	i_state = disable();
	terminit();
	timerinit();
	ioinit();
	lineinit();
	linkinit();
	meminit();
	restore(i_state);
}
/* Save variables to disk */
save(argc,argv)
int argc;
char *argv[];
{
	FILE *fp;
	char *fn;
	int i;

	if(argc == 1)
		fn = CONFIG;
	else
		fn = argv[1];
	if((fp = fopen(fn,"w")) == NULL){
		tprintf("Cannot open %s\n",fn);
	} else {
		fwrite((char *)&mycall,sizeof(mycall),1,fp);
		fwrite((char *)&unproto,sizeof(unproto),1,fp);
		fwrite(btext,sizeof(btext),1,fp);
		fwrite((char *)&term,sizeof(term),1,fp);
		fwrite((char *)&linkdefault,sizeof(linkdefault),1,fp);
		fwrite((char *)&nlines,sizeof(nlines),1,fp);
		for(i=0;i<nlines;i++)
			fwrite((char *)lines[i],sizeof(struct line),1,fp);
		fclose(fp);
	}
}

/* Display the link table
 * Format:
 * Station   State    L# Tq Rq Un Vr Vs Rt/Lim T1   T2   T3   Loc  Rem  P/F
 * WA2LQQ-0  DiscPend 0  20 20 3  7  7  10 20  400R 600S 200S RDY  BSY  YES
*/
d_link()
{
	int i;
	struct link *link;

	tputs("Station   State    L# Tq Rq Un Vr Vs Rt/Lim   T1   T2   T3  Loc  Rem  P/F\n");
	for(i=0;i<NHASH;i++){
		for(link=hashtab[i];link != NULL; link = link->next){
			p_link(link);
		}
	}
}

static
p_link(link)
struct link *link;
{
	tprintf("%-10s%-9s%2u%3u%3u%3u%3u%3u%3u%4u%4u%c%4u%c%4u%c%5s%5s%5s\n",
	pcall(&link->address[0]),pstate[link->state],
	link->line->lineno,
	link->txq.count,link->rxq.count,
	link->unack,link->vr,link->vs,link->retries,link->n2,
	link->t1.count,timstate[link->t1.state][0],
	link->t2.count,timstate[link->t2.state][0],
	link->t3.count,timstate[link->t3.state][0],
	link->localbusy ? "Busy" : "Rdy",
	link->remotebusy ? "Busy" : "Rdy",
	link->nproto ? "Yes" : "No");
}
/* Display line statistics in the form:
 *    Transmitter                       Receiver
 * L#  State Queue Frame Bytes    State Queue Frame Bytes   Errs
 *  0  IDLE      0     0     0    IDLE      0     0     0      0
 *
 * if lineno == -1, display all lines; otherwise display specified line
 */
d_line()
{
	int i;

	tputs("    Transmitter                      Receiver\n");
	tputs("L#  State Queue Frame Bytes    State Queue Frame Bytes  Errs\n");
	for(i =0;i<nlines;i++)
		p_line(i);
}
p_line(lineno)
int lineno;
{
	struct line *lp;

	lp = lines[lineno];
	tprintf("%2u  %5s%6u%6u%6u    %5s%6u%6u%6u%6u\n",
		lp->lineno,lstate[lp->tstate],
		lp->txq.count,lp->tframes,lp->tcnt,
		lstate[lp->rstate],
		lp->rxq.count,lp->rframes,lp->rcnt,lp->rerrors);
}
/* Set AX.25 address of originating station */
setmyc(argc,argv)
int argc;
char *argv[];
{
	register char *cp;

	tputs("mycall    ");
	cp = pcall(&mycall);
	if(cp != NULL)
		tputs(cp);
	if(argc > 1){
		tputs(" -> ");
		if(setcall(&mycall,argv[1]) != 0){
			tputs(" <invalid>");
			return;
		} else {
			tputs(pcall(&mycall));
			/* Also change the template for UI frames */
			movmem((char *)&mycall,(char *)&unproto.address[1],sizeof(struct addr));
		}
	}
	tputs("\n");
}

/* Display and set UNPROTO, the address to be used for
 * UI frames while disconnected
 */
setunp(argc,argv)
int argc;
char *argv[];
{
	struct addr *addr;
	int len,i;
	register struct addr *ap;
	register char *cp;

	tputs("unproto  ");
	addr = unproto.address;
	len = unproto.addrlen;
	for(ap=addr;ap<&addr[len];ap++){
		if(ap == &addr[1])	/* Don't display our address */
			continue;
		if(ap == &addr[2])
			tputs(" via");	/* Display digipeater string */
		tputc(' ');
		tputs(pcall(ap));
	}
	if(argc < 2){
		tputs("\n");
		return;
	}
	tputs(" -> ");
	/* Set destination field */
	cp = argv[1];
	ap = addr;
	if(setcall(ap,cp) != 0){
		unproto.addrlen = 0;
		tputs(" <invalid address>\n");
		return;
	}
	tputs(pcall(ap));
	ap++;
	/* Insert our call */
	movmem((char *)&mycall,(char *)ap++,sizeof(struct addr));
	/* Copy digipeaters, if any */
	for(i=2;i < argc && ap<&addr[MAXRPT+2];ap++,i++){
		if(setcall(ap,argv[i]) != 0)
			break;
		tputc(' ');
		tputs(pcall(ap));
	}		
	ap[-1].ssid |= E;
	tputs("\n");
	if(i != argc){
		tputs(" extra digipeaters ignored\n");
	}
	unproto.addrlen = ap - &unproto.address[0];
}
/* Set beacon text */
setbt(argc,argv)
int argc;
char *argv[];
{
	tprintf("btext     %s",btext);
	if(argc > 1){
		strncpy(btext,argv[1],sizeof(btext));
		tprintf(" -> %s",btext);
	}
	tputs("\n");
}
/* Enter conversational mode */
convers(argc,argv)
int argc;
char *argv[];
{
	if(argc > 1)
		do_talk(argv[1],CONVERS_MODE);
	else
		do_talk((char *)NULL,CONVERS_MODE);
}
/* Enter transparent mode */
trans(argc,argv)
int argc;
char *argv[];
{
	if(argc > 1)
		do_talk(argv[1],TRANS_MODE);
	else
		do_talk((char *)NULL,TRANS_MODE);
}

static
do_talk(line,m)
char *line;
int m;
{
	struct addr addr;
	register struct link *link;

	if(line != NULL){
		setcall(&addr,line);
		link = find_link(&addr);
		if(link == NULL)
			tprintf("%s not connected\n",pcall(&addr));
	} else {
		link = term.tlink;
		if(link == NULL)
			tprintf("No current connection\n");
	}
	term.tlink = link;
	if(term.tlink != NULL)
		term.mode = m;
}

/* Set a value to the argument, according to its type */
int
setval(tp,arg)
struct vartab *tp;
char *arg;
{
	unsigned argval;

	switch(tp->type){
	case BYTE:
		tprintf("Was %u\n",*tp->val);
		argval = atoi(arg);
		if(argval < tp->minval || argval > tp->maxval){
			tprintf("Arg out of range\n");
			return -1;
		}
		*tp->val = (char)(argval & 0xff);
		break;
	case CHAR:
		tprintf("Was %s\n",visible(*tp->val));
		*tp->val = (*arg & 0xff);
		break;
	case NUM:
		tprintf("Was %u\n",*(unsigned *)tp->val);
		argval = atoi(arg);
		if(argval < tp->minval || argval > tp->maxval){
			tprintf("Arg out of range\n");
			return -1;
		}
		*(unsigned *)tp->val = argval;
		break;
	case BOOL:
		tprintf("Was %s\n",*tp->val ? "On" : "Off");
		if(strcmp("on",arg) == 0)
			*tp->val = ON;
		else if(strcmp("off",arg) == 0)
			*tp->val = OFF;
		else {
			tprintf("Arg must be 'on' or 'off'\n");
			return -1;
		}
		break;
	}
	return 0;
}
/* Return string with decoded value depending on type */
char *
pval(p)
register struct vartab *p;
{
	static char ret[80];

	switch(p->type){
	case BYTE:
		sprintf(ret,"%u",(unsigned)*p->val);
		break;
	case BOOL:
		sprintf(ret,"%s",*p->val?"On":"Off");
		break;
	case NUM:
		sprintf(ret,"%u",*(unsigned *)p->val);
		break;
	case CHAR:
		sprintf(ret,"%s",visible(*p->val));
		break;
	case STRING:
		sprintf(ret,"%s",p->val);
		break;
	}
	return ret;
}
/* Clean out specified or all disconnected entries in link table */
purge(argc,argv)
int argc;
char *argv[];
{
	struct addr addr;
	struct link *link,*tmp;
	int i;

	if(argc == 1){
		for(i=0;i<NHASH;i++){
			for(link = hashtab[i];link != NULL; link = tmp){
				tmp = link->next;
				if(link->state == DISCONNECTED){
					del_link(link);
				}
			}
		}
	} else {
		for(i=1;i<argc;i++){
			setcall(&addr,argv[i]);
			link = find_link(&addr);
			if(link == NULL){
				tprintf("%s: not found\n",pcall(&addr));
				continue;
			}
			if(link->state != DISCONNECTED){
				tprintf("%s: not disconnected\n",pcall(&addr));
				continue;
			}
			del_link(link);
		}
	}
}

/* Display and modify memory allocation controls */
memory(argc,argv)
int argc;
char *argv[];
{
	tprintf("Buffers in use: %u blocks containing %u bytes\n",
		aframes,memused);
	tprintf("Flow control point: %u bytes\n",memlimit);
	if(argc > 1){
		memlimit = atoi(argv[1]);
		tprintf("New flow control point: %u bytes\n",memlimit);
	}
}
