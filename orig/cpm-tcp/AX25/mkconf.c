#include <stdio.h>
#include "ax25.h"
#include "command.h"

struct addr mycall;
struct link unproto;
char btext[128];
struct term term;
struct line *lines[NLINES];
int nlines;
struct link linkdefault;

main(argc,argv)
int argc;
char *argv[];
{
	FILE *fp;
	char *fn;
	int i;
	char *calloc(),*strcpy();

	strcpy(btext,"AX.25 TNC V1.0 By KA9Q");
	term.speed = 9600;
	term.abit = 1;
	term.awlen = 8;
	term.canline = 0x18;
	term.canpac = 0x19;
	term.command = 0x3;
	term.cr = ON;
	term.delete = '\b';
	term.echo = ON;
	term.flow = OFF;
	term.hiwat = FIFOSIZE/2;
	term.lowat = FIFOSIZE/4;
	term.parity = 3;
	term.pass = 0x16;
	term.sendpac = '\r';
	term.start = 0x11;
	term.stop = 0x13;
	term.xoff = 0x13;
	term.xon = 0x11;
	
	nlines = 2;
	for(i=0;i<nlines;i++){
		lines[i] = (struct line *)calloc(1,sizeof(struct line));
		lines[i]->digipeat = ON;
		lines[i]->mode = CSMA;
		lines[i]->psize = 256;
		lines[i]->speed = 1200;
		lines[i]->tin = TDATA;
		lines[i]->tout = TDATA;
		lines[i]->tused = OFF;
		lines[i]->ktimer.start = 4;
	}
	linkdefault.t2.start = 0;
	linkdefault.t1.start = 40;
	linkdefault.t3.start = 600;
	linkdefault.maxframe = 4;
	linkdefault.nproto = ON;
	linkdefault.n2 = 10;
	linkdefault.tfrmr = TDATA;
	linkdefault.tstate = TDATA;
	linkdefault.ttimer = TDATA;

	if(argc == 1)
		fn = CONFIG;
	if((fp = fopen(fn,"w")) == NULL){
		printf("Cannot open %s\n",fn);
		return;
	}
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
