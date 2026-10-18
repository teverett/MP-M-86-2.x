/* SFTP Server state machine - see RFC 913a */

#define	LINELEN		128

#include <stdio.h>
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "tcp.h"

/* Per-session control block */
struct sftp {
	struct sftp *prev;		/* Linked list pointers */
	struct sftp *next;
	struct tcb *tcb;		/* TCP control block pointer */
	char state;
#define	COMMAND_STATE	0	/* Awaiting user command */
#define	SEND_WAIT	1		/* User sent RETR, awaiting SEND */
#define	SENDING_STATE	2	/* Sending data to user */
#define	SIZE_WAIT		3	/* User sent STOR, awaiting SIZE */
#define	RECEIVING_STATE	4	/* Storing data from user */

	char type;				/* Transfer type */
#define	BINARY_TYPE	0
#define	ASCII_TYPE	1

	char *username;			/* User's name */
	char buf[LINELEN];		/* Input command buffer */
	char cnt;				/* Length of input buffer */
	FILE *fp;				/* File descriptor being written */
	long size;				/* Bytes remaining to be read or written */
};
/* Head of chain */
static struct sftp *sftp;

/* Command table */
static char *commands[] = {
	"user",
#define	USER_CMD	0
	"acct",
#define	ACCT_CMD	1
	"pass",
#define	PASS_CMD	2
	"type",
#define	TYPE_CMD	3
	"list",
#define	LIST_CMD	4
	"cdir",
#define	CDIR_CMD	5
	"kill",
#define	KILL_CMD	6
	"name",
#define	NAME_CMD	7
	"done",
#define	DONE_CMD	8
	"retr",
#define	RETR_CMD	9
	"stor",
#define	STOR_CMD	10
	"send",
#define SEND_CMD	11
	"stop",
#define	STOP_CMD	12
	"size",
#define	SIZE_CMD	13
	NULL
};

/* Reply messages */
static char banner[] = "+SFTP Ready\377\373\1";/* Hack! */
static char badcmd[] = "-Command unknown";
static char logged[] = "!Logged in";
static char acctok[] = "!Account not needed";
static char passok[] = "!Password not needed";
static char asciimode[] = "+Using ASCII mode";
static char binarymode[] = "+Using binary mode";
static char badmode[] = "-Type not valid";
static char unimp[] = "-Command not yet implemented";
static char bye[] = "+Goodbye!";
static char noretr[] = "-Need RETR";
static char cantopen[] = "-Can't read file";
static char rabort[] = "+OK, retrieve aborted";
static char plus[] = "+";	/* Note lack of cr/lf */
static char alreadyexists[] = "-File already exists";
static char willmake[] = "+Will create file";
static char willoverwrite[] = "+Will overwrite old file";
static char willappend[] = "+Will append to file";
static char cantmake[] = "-Can't write file";

/* SFTP connection state change upcall handler */
s_sftp(tcb,old,new)
struct tcb *tcb;
char old,new;
{
	struct sftp *mp,*sftp_create(),*sftp_lookup();

	switch(new){
	case ESTABLISHED:
		mp = sftp_create(tcb);
		printf("%s:%d - open SFTP\r\n",
			inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port);
		respond(mp,banner,sizeof(banner));
		break;		
	case CLOSE_WAIT:
		mp = sftp_lookup(tcb);
		sftp_delete(mp);				
		close_tcp(tcb);
		break;
	case CLOSED:
		del_tcp(tcb);
		break;
	}
}

/* SMTP receiver upcall handler */
r_sftp(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct sftp *mp,*sftp_lookup();
	int cnt1;
	char *cp,*index();
	struct mbuf *bp,*bp1;
	void docommand(),respond();

	mp = sftp_lookup(tcb);
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
		printf("%s:%d - close SFTP\r\n",
			inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port);
		return;
	}
	switch(mp->state){
	case COMMAND_STATE:
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

		/* Look for the null that terminates each command */
		for(cp = mp->buf;cp < &mp->buf[mp->cnt];cp++)
			if(*cp == ' ')
				break;
		if(cp != &mp->buf[mp->cnt]){
			docommand(mp);
			mp->cnt = 0;
		}
		/* else no null present yet to terminate command */
		break;
	case SENDING_STATE:
		/* Ignore? */
		recv_tcp(tcb,&bp,cnt);
		free_p(bp);
		break;
	case RECEIVING_STATE:
		cnt1 = min(cnt,mp->size);
		recv_tcp(tcb,&bp,cnt1);
		while(bp != NULL){
			/* Write data into the current file, one mbuf at a time */
			fwrite(bp->data,1,(int)bp->cnt,mp->fp);
			bp1 = bp->next;
			free_mbuf(bp);
			bp = bp1;
		}
		mp->size -= cnt1;
		if(mp->size == 0){
			/* Done, return to command state. */
			fclose(mp->fp);
			mp->fp = NULL;
			mp->state = COMMAND_STATE;
			/* If there's still something on the queue, reinvoke ourselves
			 * in command mode (unlikely, but possible)
			 */
			if(cnt1 != cnt)
				r_sftp(tcb,cnt-cnt1);
		}
	}
}

/* SFTP transmit upcall handler */
t_sftp(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct sftp *mp,*sftp_lookup();
	struct mbuf *bp;
	int cnt1;

	mp = sftp_lookup(tcb);
	if(mp == NULL)
		return;
	switch(mp->state){
	case SENDING_STATE:
		cnt1 = min(mp->size,cnt);
		mp->size -= cnt1;		
		bp = alloc_mbuf(cnt1);
		fread(bp->data,1,cnt1,mp->fp);
		send_tcp(tcb,bp);
		if(mp->size == 0){
			/* All done, send null and return to command state */
			fclose(mp->fp);
			bp = alloc_mbuf(1);
			bp->data[0] = '\0';
			bp->cnt = 1;
			send_tcp(tcb,bp);
			mp->state = COMMAND_STATE;
		}	
		break;
	}
}

/* Create control block, initialize */
static struct sftp *
sftp_create(tcb)
register struct tcb *tcb;
{
	register struct sftp *mp;
	char *calloc();

	mp = (struct sftp *)calloc(1,sizeof (struct sftp));
	mp->tcb = tcb;
	mp->next = sftp;
	mp->prev = NULL;
	if(mp->next != NULL)
		mp->next->prev = mp;
	sftp = mp;
	return mp;
}

/* Free resources, delete control block */
static sftp_delete(mp)
register struct sftp *mp;
{
	if(mp->prev != NULL)
		mp->prev->next = mp->next;
	else
		sftp = mp->next;
	if(mp->next != NULL)
		mp->next->prev = mp->prev;
	free((char *)mp);
}

/* Find a control block given TCB */
static struct sftp *
sftp_lookup(tcb)
register struct tcb *tcb;
{
	register struct sftp *mp;

	for(mp = sftp;mp != NULL;mp = mp->next){
		if(mp->tcb == tcb)
			break;
	}
	return mp;
}

/* Parse and execute sftp commands */
static
void
docommand(mp)
register struct sftp *mp;
{
	char *cmd,*arg,*cp,**cmdp;
	char *index(),*malloc(),*strcpy();
	void respond();
	int cnt;
	struct mbuf *bp;
	char exists,c;

	cmd = mp->buf;
	if(mp->cnt < 4){
		/* Can't be a legal SFTP command */
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
	while(*arg == ' ')
		arg++;
	/* Execute specific command */
	switch(cmdp-commands){
	case USER_CMD:
		mp->username = malloc(strlen(arg)+1);
		strcpy(mp->username,arg);
		respond(mp,logged,sizeof(logged));
		return;
	case ACCT_CMD:
		respond(mp,acctok,sizeof(acctok));
		return;
	case PASS_CMD:
		respond(mp,passok,sizeof(passok));
		return;
	case TYPE_CMD:
		switch(*arg){
		case 'a':	/* Ascii */
			mp->type = ASCII_TYPE;
			respond(mp,asciimode,sizeof(asciimode));
			break;
		case 'b':	/* Binary */
		case 'c':	/* Continuous */
			mp->type = BINARY_TYPE;
			respond(mp,binarymode,sizeof(binarymode));
			break;
		default:	/* Invalid */
			respond(mp,badmode,sizeof(badmode));
			break;
		}
		return;
	case LIST_CMD:
		respond(mp,unimp,sizeof(unimp));
		return;		
	case CDIR_CMD:
		respond(mp,unimp,sizeof(unimp));
		return;		
	case KILL_CMD:
		respond(mp,unimp,sizeof(unimp));
		return;		
	case NAME_CMD:
		respond(mp,unimp,sizeof(unimp));
		return;		
	case DONE_CMD:
		respond(mp,bye,sizeof(bye));
		close_tcp(mp->tcb);
		sftp_delete(mp);
		return;
	case RETR_CMD:
		if((mp->fp = fopen(arg,"r")) == NULL){
			respond(mp,cantopen,sizeof(cantopen));
		} else {
			mp->state = SEND_WAIT;
			mp->size = getsize(mp->fp,mp->type);
			sprintf(cmd,"#%ld\0",mp->size);
			respond(mp,cmd,strlen(cmd));
		}
		return;
	case STOR_CMD:
		c = *arg++;
		while(*arg == ' ')
			arg++;
		/* See if file already exists */
		exists = 0;
		if((mp->fp = fopen(arg,"r")) != NULL)
			exists = 1;
		fclose(mp->fp);
		switch(c){
		case 'n':
			if(exists){
				respond(mp,alreadyexists,sizeof(alreadyexists));
				break;
			}
		case 'o':		/* note fall-thru */
			if((mp->fp = fopen(arg,"w")) == NULL){
				respond(mp,cantmake,sizeof(cantmake));
				break;
			}
			if(exists){
				respond(mp,willoverwrite,sizeof(willoverwrite));
			} else {
		 		respond(mp,willmake,sizeof(willmake));
			}
			mp->state = SIZE_WAIT;
			break;
		case 'a':
			if((mp->fp = fopen(arg,"a")) == NULL){
				respond(mp,cantmake,sizeof(cantmake));
				break;
			}
			if(exists){
				respond(mp,willappend,sizeof(willappend));
			} else {
				respond(mp,willmake,sizeof(willmake));
			}
			mp->state = SIZE_WAIT;
			break;
		}
		return;
	case SEND_CMD:
		if(mp->state != SEND_WAIT){
			respond(mp,noretr,sizeof(noretr));
			return;
		}
		mp->state = SENDING_STATE;
		respond(mp,plus,sizeof(plus));
		/* Prime the transmitter by filling the send window */
		cnt = min(mp->tcb->window - mp->tcb->sndcnt,mp->size);
		bp = alloc_mbuf(cnt);
		/* FIX TO ADJUST MODE */
		fread(bp->data,1,cnt,mp->fp);
		bp->cnt = cnt;
		mp->size -= cnt;
		send_tcp(mp->tcb,bp);
		return;
	case STOP_CMD:
		if(mp->state != SEND_WAIT){
			respond(mp,noretr,sizeof(noretr));
			return;
		}
		mp->state = COMMAND_STATE;
		fclose(mp->fp);
		respond(mp,rabort,sizeof(rabort));
		return;
	}
}
/* Send message back to client */
static void
respond(mp,message,length)
struct sftp *mp;
char *message;
int length;
{
	struct mbuf *bp,*qdata();

	bp = qdata(message,length);
	send_tcp(mp->tcb,bp);
}
getsize(fd,type)
FILE *fd;
int type;
{
	int cnt;

	cnt = 0;
	while(getc(fd) != EOF)
		cnt++;
	rewind(fd);
	return cnt;
}
