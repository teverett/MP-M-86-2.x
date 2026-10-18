/* SMTP Server state machine - see RFC 821
 * Very simple implementation; no forwarding allowed
 * (who wants to re-create "sendmail" ??)
 */

/* Local system name. Mail to "user" or "user@hostname" is accepted */
char *hostname = "ka9q";
/* Turn off the next line if your system doesn't have a clock */
#define	DATE

/* File name templates - edit to taste */
#define	mailtmp		"mailtmp.%3d"	/* Temporary files */
#define	mailspool	"%s.txt"		/* Mailbox names */

#define	LINELEN		128

#include <stdio.h>
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "tcp.h"

/* Recipient address entry */
struct addr {
	struct addr *next;
	char *val;
};

/* Per-session control block */
struct mail {
	struct mail *prev;		/* Linked list pointers */
	struct mail *next;
	struct tcb *tcb;		/* TCP control block pointer */
	char state;
#define	START_STATE	0
#define	COMMAND_STATE	1
#define	DATA_STATE	2

	char *from;				/* Address given in MAIL FROM: command */
	struct addr *to;		/* Linked list of recipients */
	char buf[LINELEN];		/* Input buffer */
	char cnt;				/* Length of input buffer */
	char tmpfile[14];		/* Name of temporary input file */
	FILE *data;				/* Temporary input file pointer */
};
/* Head of chain */
static struct mail *mail;

/* Command table */
static char *commands[] = {
	"helo",
#define	HELO_CMD	0
	"noop",
#define	NOOP_CMD	1
	"mail from:",
#define	MAIL_CMD	2
	"quit",
#define	QUIT_CMD	3
	"rcpt to:",
#define	RCPT_CMD	4
	"help",
#define	HELP_CMD	5
	"data",
#define	DATA_CMD	6
	"rset",
#define	RSET_CMD	7
	NULL
};

/* Reply messages */
static char help[] = "214-Commands:\r\n\
214-HELO NOOP MAIL QUIT RCPT HELP DATA RSET\r\n\
214 End\r\n";
static char banner[] = "220 SMTP Ready\r\n";
static char closing[] = "221 Closing\r\n";
static char ok[] = "250 Ok\r\n";
static char reset[] = "250 Reset state\r\n";
static char sent[] = "250 Sent\r\n";
static char ourname[] = "250 %s\r\n";
static char enter[] = "354 Enter mail, end with .\r\n";
static char ioerr[] = "452 temp file write error\r\n";
static char mboxerr[] = "452 Mailbox %s write error\r\n";
static char badcmd[] = "500 Command unrecognized\r\n";
static char syntax[] = "501 Syntax error\r\n";
static char needmail[] = "503 Need MAIL command\r\n";
static char needrcpt[] = "503 Need RCPT (recipient)\r\n";
static char noforward[] = "551 Forwarding not implemented\r\n";
static char nestmail[] = "554 Nested MAIL command\r\n";

/* SMTP connection state change upcall handler */
s_mail(tcb,old,new)
struct tcb *tcb;
char old,new;
{
	struct mail *mp,*mail_create(),*mail_lookup();

	switch(new){
	case ESTABLISHED:
		mail_create(tcb);
		printf("%s:%d - open SMTP\r\n",
			inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port);
		break;		
	case CLOSE_WAIT:
		mp = mail_lookup(tcb);
		mail_delete(mp);				
		close_tcp(tcb);
		break;
	case CLOSED:
		del_tcp(tcb);
		break;
	}
}

/* SMTP receiver upcall handler */
r_mail(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mail *mp,*mail_lookup();
	int cnt1;
	char *cp,*index();
	struct mbuf *bp;
	void docommand(),respond();

	mp = mail_lookup(tcb);
	if(mp == NULL){
		/* The only way this "should" happen is if the other guy
		 * keeps on sending after we've initiated a close. So throw
		 * away his data and close again, just in case.
		 */
		close_tcp(tcb);
		recv_tcp(tcb,&bp,0);
		free_p(bp);
		return;
	}
	if(cnt == 0){
		printf("%s:%d - close SMTP\r\n",
			inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port);
		return;
	}
	/* Assemble an input line in the session buffer. Return if incomplete */
	cnt1 = min(cnt,LINELEN - mp->cnt);
	if(mp->cnt == LINELEN){
		/* Line too long; discard and start over */
		mp->cnt = 0;
		cnt1 = cnt;
	}
	bp = NULL;
	recv_tcp(tcb,&bp,cnt1);
	dqdata(bp,&mp->buf[mp->cnt],cnt1);
	mp->cnt += cnt1;

	if((cp = index(mp->buf,'\r')) == NULL)
		return;
	*cp = '\0';
	switch(mp->state){
	case COMMAND_STATE:
		docommand(mp);
		break;
	case DATA_STATE:
		if(mp->buf[0] == '.' && strlen(mp->buf) == 1){
			fprintf(mp->data,"\n");	/* Leave a blank line between msgs */
			deliver(mp);	/* Also sends appropriate response */
			mp->data = NULL;
			mp->state = COMMAND_STATE;
			break;
		}
		/* Append to data file */
		if(fprintf(mp->data,"%s\n",mp->buf) < 0){
			respond(mp,ioerr,sizeof(ioerr));
			mp->state = COMMAND_STATE;
		}
		break;
	}
	mp->cnt = 0;	/* Clear input buffer */
}

/* SMTP transmit upcall handler */
t_mail(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mail *mp,*mail_lookup();
	void respond();

	mp = mail_lookup(tcb);
	if(mp == NULL)
		return;
	switch(mp->state){
	case START_STATE:
		respond(mp,banner,sizeof(banner));
		mp->state = COMMAND_STATE;
		break;		
	case COMMAND_STATE:
	case DATA_STATE:
		break;
	}
}

/* Create control block, initialize */
static struct mail *
mail_create(tcb)
register struct tcb *tcb;
{
	register struct mail *mp;
	char *calloc();

	mp = (struct mail *)calloc(1,sizeof (struct mail));
	mp->tcb = tcb;
	mp->next = mail;
	mp->prev = NULL;
	if(mp->next != NULL)
		mp->next->prev = mp;
	mail = mp;
	return mp;
}

/* Free resources, delete control block */
static mail_delete(mp)
register struct mail *mp;
{
	struct addr *ap,*ap1;

	if(mp->from != NULL)
		free(mp->from);
	for(ap = mp->to;ap != NULL;ap = ap1){
		free(ap->val);
		ap1 = ap->next;
		free((char *)ap);
	}
	if(mp->data != NULL)
		fclose(mp->data);
	if(mp->prev != NULL)
		mp->prev->next = mp->next;
	else
		mail = mp->next;
	if(mp->next != NULL)
		mp->next->prev = mp->prev;
	free((char *)mp);
}

/* Find a control block given TCB */
static struct mail *
mail_lookup(tcb)
register struct tcb *tcb;
{
	register struct mail *mp;

	for(mp = mail;mp != NULL;mp = mp->next){
		if(mp->tcb == tcb)
			break;
	}
	return mp;
}

/* Parse and execute mail commands */
static
void
docommand(mp)
register struct mail *mp;
{
	char *cmd,*arg,*cp,*cp1,**cmdp;
	char *index(),*malloc(),*getname();
	struct tcb *tcb;
	struct addr *ap;
	void respond();
	static int tmpseq;	/* Sequence number for unique temporary file */
#ifdef	DATE
	long t;
	char *ctime();
#endif

	cmd = mp->buf;
	if(mp->cnt < 4){
		/* Can't be a legal SMTP command */
		respond(mp,badcmd,sizeof(badcmd));
		return;
	}	
	cmd = mp->buf;

	/* Translate entire buffer to lower case */
	for(cp = cmd;*cp != '\0';cp++)
		*cp = tolower(*cp);

	/* Find command in table; if not present, return syntax error */
	for(cmdp = commands;*cmdp != NULL;cmdp++)
		if(strncmp(*cmdp,cmd,strlen(*cmdp)) == 0)
			break;
	if(cmdp == NULL){
		respond(mp,badcmd,sizeof(badcmd));
		return;
	}
	arg = &cmd[strlen(*cmdp)];
	/* Execute specific command */
	switch(cmdp-commands){
	case HELO_CMD:	/* Useless */
		sprintf(cmd,ourname,hostname);
		respond(mp,cmd,strlen(cmd)+1);
		break;
	case NOOP_CMD:	/* Also useless */
		respond(mp,ok,sizeof(ok));
		break;
	case MAIL_CMD:	/* Specify sender */
		if(mp->from != NULL){
			respond(mp,nestmail,sizeof(nestmail));
			break;
		}
		if((mp->from = getname(arg)) == NULL){
			respond(mp,syntax,sizeof(syntax));
			break;
		}
		respond(mp,ok,sizeof(ok));
		break;
	case QUIT_CMD:
		respond(mp,closing,sizeof(closing));
		close_tcp(mp->tcb);
		mail_delete(mp);
		break;
	case RCPT_CMD:
		if((cp = getname(arg)) == NULL){
			respond(mp,syntax,sizeof(syntax));
			break;
		}
		/* See if it's valid (no forwarding allowed) */
		if((cp1 = index(cp,'@')) != NULL){
			cp1++;
			if(strcmp(cp1,hostname) != 0){
				respond(mp,noforward,sizeof(noforward));
				break;
			}
		}
		/* Allocate an entry on the recipient list. This
		 * assembles the list backwards, but what the heck.
		 */
		ap = (struct addr *)malloc(sizeof(struct addr));
		ap->val = cp;
		ap->next = mp->to;
		mp->to = ap;
		respond(mp,ok,sizeof(ok));
		break;
	case HELP_CMD:
		respond(mp,help,sizeof(help));
		break;
	case DATA_CMD:
		if(mp->from == NULL){
			respond(mp,needmail,sizeof(needmail));
			break;
		}
		if(mp->to == NULL){
			respond(mp,needrcpt,sizeof(needrcpt));
			break;
		}
		sprintf(mp->tmpfile,mailtmp,tmpseq++);
		if((mp->data = fopen(mp->tmpfile,"w+")) == NULL){
			respond(mp,ioerr,sizeof(ioerr));
			break;
		}
		fprintf(mp->data,"From: %s\n",mp->from);
		fprintf(mp->data,"To: ");
		for(ap = mp->to;ap != NULL;ap = ap->next)
			fprintf(mp->data,"%s\n",ap->val);
		fprintf(mp->data,"\n");
#ifdef	DATE
		time(&t);
		fprintf(mp->data,"Received: %s",ctime(&t));	/* ctime adds newline */
#endif
		if(ferror(mp->data)){
			respond(mp,ioerr,sizeof(ioerr));
		} else {
			respond(mp,enter,sizeof(enter));
			mp->state = DATA_STATE;
		}
		break;
	case RSET_CMD:
		tcb = mp->tcb;
		mail_delete(mp);
		mp = mail_create(tcb);
		mp->state = COMMAND_STATE;
		respond(mp,reset,sizeof(reset));
		break;
	}
}
/* Send message back to client */
static void
respond(mp,message,length)
struct mail *mp;
char *message;
int length;
{
	struct mbuf *bp,*qdata();

	bp = qdata(message,length - 1);	/* Adjust for the null terminator */
	send_tcp(mp->tcb,bp);
}
/* Given a string of the form <user@host>, extract the part inside the
 * brackets. A pointer to a malloc'ed copy is passed back.
 */
static char *
getname(cp)
register char *cp;
{
	char *cp1,*rval;
	char *index(),*malloc(),*strncpy();

	if((cp = index(cp,'<')) == NULL){
		return NULL;
	}
	cp++;	/* cp -> first char of name */
	if((cp1 = index(cp,'>')) == NULL){
		return NULL;
	}
	rval = malloc(cp1 - cp + 1);
	strncpy(rval,cp,cp1-cp);
	rval[cp1-cp] = '\0';
	return rval;
}
/* Deliver mail to the appropriate mail boxes */
void
deliver(mp)
register struct mail *mp;
{
	char mailbox[14],*cp,*index();
	int c;
	register struct addr *ap;
	register FILE *fp;
	char msg[40];

	for(ap = mp->to;ap != NULL;ap = ap->next){
		rewind(mp->data);
		if((cp = index(ap->val,'@')) != NULL)
			*cp = '\0';		/* Get rid of @host part */
		sprintf(mailbox,mailspool,ap->val);
		fp = fopen(mailbox,"a+");
		while((c = getc(mp->data)) != EOF){
			if(putc(c,fp) == EOF)
				break;
		}
		if(ferror(fp)){
			sprintf(msg,mboxerr,mailbox);
			respond(mp,msg,strlen(msg)+1);
			fclose(fp);
			unlink(mp->tmpfile);
			return;
		}
		fclose(fp);
	}
	respond(mp,sent,sizeof(sent));
	unlink(mp->tmpfile);
}
