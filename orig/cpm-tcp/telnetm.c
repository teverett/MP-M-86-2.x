/* User TELNET, with some semi-automatic FTP support added */

#include <stdio.h>
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "icmp.h"
#include "interface.h"
#include "tcp.h"
#include "telnet.h"
#include "820.h"

#define	FTPDATA	20
FILE *ftpfp;
static struct tcb *ftpdata;
static int ftptype;
#define	ASCII_TYPE	0
#define	IMAGE_TYPE	1
#define CTLZ	26

int trace;
static struct telnet *tn;
extern struct interface if_lo,if_sl[];
int32 aton();
static char escape = 0x1d;	/* default escape character is ^] */
static int16 port = 23;	/* default to TELNET port */
static int16 lport = 1001;
static int speed = 9600;
static char prompt[] = "telnet> ";
static char noconn[] = "No connection\r\n";
static char pending[] = "Transfer already pending\r\n";
static char cantalloc[] = "Can't allocate control msg\r\n";
static int mode;
#define	CMD_MODE	0
#define	COM_MODE	1

/* Command lookup and branch table */
int go(),domyaddr(),doport(),doopen(),doexit(),doclose(),
	doreset(),dostate(),dotrace(),doescape(),dospeed(),dohelp(),
	doget(),doput(),dotype();

static
struct cmds {
	char *name;
	int (*func)();
} cmds[] = {
	"",		go,		/* must be first */
	"close",	doclose,
	"escape",	doescape,
#ifdef EXIT
	"exit",		doexit,
#endif
	"get",		doget,
	"help",		dohelp,
	"myaddr",	domyaddr,
	"open",		doopen,
	"port",	 	doport,
	"put",		doput,
	"reset",	doreset,
	"speed",	dospeed,
	"state",	dostate,
	"trace",	dotrace,
	"type",		dotype,
	"?",		dohelp,
	NULL,		NULL
};

main(argc,argv)
int argc;
char *argv[];
{
	char *ttybuf,c,inbuf[BUFSIZ];
	int cnt,i;
	int ttydriv();
	void tncmd();

	ioinit();
	set_slip(0,speed);

	my_addr = aton("128.96.32.60");
	rt_add(my_addr,0L,0,&if_lo);		/* Route our packets to ourself */
	rt_add(aton("128.96.0.0"),0L,0,&if_lo);		/* Also take broadcasts */
	rt_add(0L,0L,0,&if_sl[0]);	/* Default entry to slip line #0 */

	cmdmode();
	printf(prompt);

	for(;;){
		while(srx(&cfifo,&c,1) == 1){
			if((cnt = ttydriv(c,&ttybuf)) != 0){
				switch(mode){
				case CMD_MODE:
					tncmd(ttybuf);
					break;
				case COM_MODE:
					if(ttybuf[0] == escape && escape != 0){
						printf("\r\n");
						cmdmode();

					} else {
						send_tel(tn,ttybuf,cnt);
					}
					break;
				}
				if(mode == CMD_MODE){
					printf(prompt);
				}
			}

		}
		/* Process SLIP line I/O */
		for(i=0;i<NSLIP;i++){
			if((cnt = srx(&sfifo[i],inbuf,BUFSIZ)) != 0)
				recv_slip(i,inbuf,cnt);
			if(stxrdy(i)){
				slip_start(i);
			}
		}
		if(_clktick != 0){
			tick();
			(void)iss();
			_clktick = 0;
		}
		eihalt();		/* wait until interrupt */
	}
}
/* Convert Intenet address in dotted-decimal to binary */
int32
aton(s)
char *s;
{
	int32 n;
	int atoi();
	char *index();

	n = 0;
	for(;;){
		n = atoi(s) | (n << 8);
		if((s = index(s,'.')) == NULL)
			break;
		s++;
	}
	return n;
}

/* Receiver upcall routine */
static void
rcv_char(tcb,cnt)
register struct tcb *tcb;
int16 cnt;
{
	struct mbuf *bp;

	/* Hold output if we're in command mode */
	if(mode != COM_MODE)
		return;

	if(recv_tcp(tcb,&bp,cnt) > 0)
		tel_input(tn,bp);

	if(tcb->state == CLOSE_WAIT){
		close_tcp(tcb);
		cmdmode();
		printf(prompt);
	}
}

/* State change upcall routine */
static void
t_state(tcb,old,new)
register struct tcb *tcb;
char old,new;
{
	extern char *tcpstates[];
	extern char *reasons[];
	extern char *unreach[];
	extern char *exceed[];

	printf("%s",tcpstates[new]);
	switch(new){
	case CLOSED:	/* heh heh */
		printf(" (%s",reasons[tcb->reason]);
		if(tcb->reason == NETWORK){
			switch(tcb->type){
			case DEST_UNREACH:
				printf(": %s unreachable",unreach[tcb->code]);
				break;
			case TIME_EXCEED:
				printf(": %s time exceeded",exceed[tcb->code]);
				break;
			}
		}
		printf(")\r\n");
		del_tcp(tcb);
		if(tn != NULL && tcb == tn->tcb){
			free_telnet(tn);
			tn = NULL;
		} else if(tcb == ftpdata){
			ftpdata = NULL;
		}
		break;
	default:
		printf("\r\n");
	}
}

/* Parse commands, calling out to appropriate functions */
static void
tncmd(cmd)
register char *cmd;
{
	register char *arg;	/* Points to first char of argument token */
	char *eol;
	register struct cmds *cmdp;
	int cmdlen;
	
	if((eol = index(cmd,'\r')) != NULL)
		*eol = '\0';

	/* Strip leading blanks */
	while(*cmd == ' ')
		cmd++;

	/* Find end of command token */
	for(arg = cmd;*arg != ' ' && *arg != '\0';arg++)
		;
	cmdlen = arg - cmd;
	if(*arg == ' '){
		/* An argument was given; delimit the command */
		*arg++ = '\0';
		/* Now find the beginning of the argument */
		while(*arg == ' ')
			arg++;
	}
	/* Look up command in table; prefix matches are OK */
	for(cmdp = cmds;cmdp->name != NULL;cmdp++){
		if(strncmp(cmd,cmdp->name,cmdlen) == 0)
			break;
	}
	if(cmdp->name == NULL)
		printf("eh?\r\n");	/* Canadian accent */
	else
		(*cmdp->func)(arg);
}
/* Enter command mode */
static
cmdmode()
{
	mode = CMD_MODE;
	cooked();
}
/* Enter communicate mode */
static
go()
{
	if(tn == NULL)
		return;
	if(tn->remote[TN_ECHO])
		raw();		/* Re-establish raw mode if it was set */
	mode = COM_MODE;
	rcv_char(tn->tcb,0);	/* Get any pending output */
}
static
domyaddr(arg)
char *arg;
{
	rt_drop(my_addr);		/* Remove old */
	my_addr = aton(arg);
	rt_add(my_addr,0L,0,&if_lo);	/* Route our packets to ourself */
}
static
doport(arg)
char *arg;
{
	port = atoi(arg);
}
static
doopen(arg)
char *arg;
{
	struct tcb *tcb;
	struct socket lsocket,fsocket;

	if(tn != NULL){
		printf("Connection active to %s:%d\r\n",inet_ntoa(fsocket.address),
			fsocket.port);
		return;
	}
	lsocket.address = my_addr;
	lsocket.port = lport++;
	fsocket.address = aton(arg);
	fsocket.port = port;
	tcb = open_tcp(&lsocket,&fsocket,TCP_ACTIVE,200,rcv_char,NULL,t_state,0);
	if(tcb != NULL && tcb->state != CLOSED){
		tn = open_telnet(tcb);
		go();
	}
}
#ifdef EXIT
static
doexit()
{
	if(tn == NULL)
		exit(0);
	if(tn->tcb->state == ESTABLISHED || tn->tcb->state == CLOSE_WAIT)
		close_tcp(tn->tcb);
	exit(0);
}
#endif
static
doclose()
{
	if(tn == NULL){
		printf(noconn);
	} else {
		close_tcp(tn->tcb);
	}
}
static
doreset()
{
	if(tn == NULL){
		printf(noconn);
	} else {
		close_self(tn->tcb,RESET);
	}
	if(ftpfp != NULL){
		fclose(ftpfp);
		ftpfp = NULL;
	}
}
static
dostate()
{
	if(tn == NULL){
		printf(noconn);
	} else {
		state_tcp(tn->tcb);				
	}
	if(ftpdata != NULL){
		printf("FTP data connection:\r\n");
		state_tcp(ftpdata);
	}
}
static
dotrace()
{
	trace = !trace;
	if(trace)
		printf("trace on\r\n");
	else
		printf("trace off\r\n");
}
static
doescape(arg)
char *arg;
{
	escape = *arg;
}
static
dospeed(arg)
char *arg;
{
	speed = atoi(arg);
	set_slip(0,speed);
}
static
dohelp()
{
	register struct cmds *cmdp;

	printf("Telnet commands:\r\n");
	for(cmdp = cmds;cmdp->name != NULL;cmdp++)
		printf("%s\r\n",cmdp->name);
}
static
dotype(arg)
char *arg;
{
	switch(*arg){
	case 'i':
	case 'b':
		ftptype = IMAGE_TYPE;
		break;
	case 'a':
		ftptype = ASCII_TYPE;
		break;
	default:
		printf("Invalid type\r\n");
		break;
	}
}
/* Start receive transfer. Syntax: get <remote name> [<local name>] */
static
doget(remotename)
char *remotename;
{
	void ftp_recv(),ftp_state();
	char *localname,*index();
	struct mbuf *bp;

	if(tn == NULL){
		printf(noconn);
		return;
	}
	if(*remotename == '\0'){
		printf("File?\r\n");
		return;
	}
	if((localname = index(remotename,' ')) == NULL){
		localname = remotename;
	} else {
		*localname++ = '\0';
		while(*localname == ' ')
			localname++;
	}
	if((bp = alloc_mbuf(10 + strlen(remotename))) == NULL){
		printf(cantalloc);
		return;
	}
	if(ftpfp != NULL)
		fclose(ftpfp);

	if((ftpfp = fopen(localname,"w")) == NULL){
		printf("Cannot write %s\r\n",localname);
		free_p(bp);
		return;
	}
	ftpsetup(ftp_recv,NULL,ftp_state);
	/* Generate the command to start the transfer */
	sprintf(bp->data,"RETR %s\r\n",remotename);
	bp->cnt = strlen(bp->data);
	send_tcp(tn->tcb,bp);

	/* Enter com mode so we'll see what's going on */
	go();
}
/* Start transmit. Syntax: put <local name> [<remote name>] */
static
doput(localname)
char *localname;
{
	void ftp_send(),ftp_state();
	char *remotename,*index();
	struct mbuf *bp;

	if(tn == NULL){
		printf(noconn);
		return;
	}
	if(*localname == '\0'){
		printf("File?\r\n");
		return;
	}
	if((remotename = index(localname,' ')) == NULL){
		remotename = localname;
	} else {
		*remotename++ = '\0';
		while(*remotename == ' ')
			remotename++;
	}
	if((bp = alloc_mbuf(10 + strlen(remotename))) == NULL){
		printf(cantalloc);
		return;
	}
	if(ftpfp != NULL)
		fclose(ftpfp);

	if((ftpfp = fopen(localname,"r")) == NULL){
		printf("Cannot read %s\r\n",localname);
		return;
	}
	ftpsetup(NULL,ftp_send,ftp_state);

	/* Generate the command to start the transfer */
	sprintf(bp->data,"STOR %s\r\n",remotename);
	bp->cnt = strlen(bp->data);
	send_tcp(tn->tcb,bp);

	/* Enter com mode so we'll see what's going on */
	go();
}
/* Handle incoming FTP data */
void
ftp_recv(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mbuf *bp;
	char c;


	if(tcb != ftpdata)
		return;

	if(recv_tcp(tcb,&bp,cnt) > 0){
		while(pullup(&bp,&c,1) == 1){
#ifdef	CPM
			putc(c,ftpfp);
#else
			if(ftptype == IMAGE_TYPE || c != '\r')
				putc(c,ftpfp);
#endif
		}
	}
	if(tcb->state == CLOSE_WAIT){
#ifdef	CPM
		if(ftptype == ASCII_TYPE)
			putc(CTLZ,ftpfp);
#endif
		fclose(ftpfp);
		ftpfp = NULL;
		close_tcp(tcb);
		return;
	}
}
/* Send outgoing FTP data */
void
ftp_send(tcb,cnt)
struct tcb *tcb;
register int16 cnt;
{
	register struct mbuf *bp;
	register char *cp;
	int c;

	if(tcb != ftpdata)
		return;

	if((bp = alloc_mbuf(cnt)) == NULL){
		printf("Can't allocate send buffer!\r\n");
		return;
	}
	cp = bp->data;
	while(cnt > 1 && (c = getc(ftpfp)) != EOF){
#ifdef	CPM
		if(ftptype == ASCII_TYPE && c == CTLZ)
			break;	/* CTLZ is CP/M's text EOF marker */
#else
		if(ftptype == ASCII_TYPE && c == '\n'){
			*cp++ = '\r';
			bp->cnt++;
			cnt--;
		}
#endif
		*cp++ = c;
		bp->cnt++;
		cnt--;
	}
	if(bp->cnt != 0)
		send_tcp(tcb,bp);
	else
		free_p(bp);

	if(cnt > 1){	/* EOF seen */
		fclose(ftpfp);
		ftpfp = NULL;
		close_tcp(tcb);
	}
}
/* Handle FTP data connection state changes */
void
ftp_state(tcb,old,new)
struct tcb *tcb;
char old,new;
{
	printf("FTP data %u: ",tcb->conn.local.port);
	t_state(tcb,old,new);
}
/* create data port, and send PORT and TYPE messages */
ftpsetup(recv,send,state)
void (*send)();
void (*recv)();
void (*state)();
{
	struct socket lsocket,fsocket;
	struct mbuf *bp;

	lsocket.address = my_addr;
	lsocket.port = lport++;
	fsocket.address = tn->tcb->conn.remote.address;
	fsocket.port = FTPDATA;

	/* Compose and send PORT a,a,a,a,p,p message */

	if((bp = alloc_mbuf(35)) == NULL){	/* 5 more than worst case */
		printf(cantalloc);
		return;
	}
	/* I know, this looks gross, but it works! */
	sprintf(bp->data,"PORT %u,%u,%u,%u,%u,%u\r\n",
		hibyte(hiword(lsocket.address)),
		lobyte(hiword(lsocket.address)),
		hibyte(loword(lsocket.address)),
		lobyte(loword(lsocket.address)),
		hibyte(lsocket.port),
		lobyte(lsocket.port));
	bp->cnt = strlen(bp->data);
	send_tcp(tn->tcb,bp);

	/* Post a listen on the data connection */
	ftpdata = open_tcp(&lsocket,&fsocket,TCP_PASSIVE,200,recv,send,state,0);
	/* Send a TYPE message */
	if((bp = alloc_mbuf(10)) == NULL){	/* 2 more than worst case */
		printf(cantalloc);
		return;
	}
	switch(ftptype){
	case ASCII_TYPE:
		sprintf(bp->data,"TYPE a\r\n");
		break;
	case IMAGE_TYPE:
		sprintf(bp->data,"TYPE i\r\n");
		break;
	}
	bp->cnt = strlen(bp->data);
	send_tcp(tn->tcb,bp);
}
