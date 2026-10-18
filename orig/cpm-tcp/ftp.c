/* FTP Server state machine - see RFC 959 */

#define	LINELEN		128

#include <stdio.h>
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "tcp.h"

/* Per-session control block */
struct ftp {
	struct ftp *prev;		/* Linked list pointers */
	struct ftp *next;
	struct tcb *control;		/* TCP control connection */
	char state;
#define	COMMAND_STATE	0	/* Awaiting user command */
#define	SENDING_STATE	1	/* Sending data to user */
#define	RECEIVING_STATE	2	/* Storing data from user */

	char type;				/* Transfer type */
#define	IMAGE_TYPE	0
#define	ASCII_TYPE	1

	char *username;			/* User's name */
	char buf[LINELEN];		/* Input command buffer */
	char cnt;				/* Length of input buffer */
	FILE *fp;				/* File descriptor being transferred */
	long size;				/* Bytes remaining to be read or written */
	struct socket port;		/* Remote port for data connection */
	struct tcb *data;		/* Data connection */
};
/* Head of chain */
static struct ftp *ftp;

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
	"cwd",
#define	CWD_CMD		5
	"dele",
#define	DELE_CMD	6
	"name",
#define	NAME_CMD	7
	"quit",
#define	QUIT_CMD	8
	"retr",
#define	RETR_CMD	9
	"stor",
#define	STOR_CMD	10
	"port",
#define	PORT_CMD	11
	NULL
};

/* Reply messages */
static char banner[] = "220 FTP Ready\r\n";
static char badcmd[] = "500 Unknown command\r\n";
static char logged[] = "230 Logged in\r\n";
static char typeok[] = "200 Type OK\r\n";
static char badtype[] = "501 Unknown type\r\n";
static char badport[] = "501 Bad port syntax\r\n";
static char unimp[] = "502 Command not yet implemented\r\n";
static char bye[] = "221 Goodbye!\r\n";
static char cantopen[] = "550 Can't open file\r\n";
static char sending[] = "150 Opening data connection\r\n";

/* SFTP connection state change upcall handler */
s_ftp(tcb,old,new)
struct tcb *tcb;
char old,new;
{
	struct ftp *mp,*ftp_create(),*ftp_lookup();

	switch(new){
	case ESTABLISHED:
		mp = ftp_create(tcb);
		printf("%s:%d - open SFTP\r\n",
			inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port);
		respond(mp,banner,sizeof(banner));
		break;		
	case CLOSE_WAIT:
		mp = ftp_lookup(tcb);
		ftp_delete(mp);				
		close_tcp(tcb);
		break;
	case CLOSED:
		del_tcp(tcb);
		break;
	}
}

/* SMTP receiver upcall handler */
r_ftp(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct ftp *mp,*ftp_lookup();
	int cnt1;
	char *cp,*index();
	struct mbuf *bp,*bp1;
	void docommand(),respond();

	mp = ftp_lookup(tcb);
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
				r_ftp(tcb,cnt-cnt1);
		}
	}
}

/* SFTP transmit upcall handler */
t_ftp(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct ftp *mp,*ftp_lookup();
	struct mbuf *bp;
	int cnt1;

	mp = ftp_lookup(tcb);
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
static struct ftp *
ftp_create(tcb)
register struct tcb *tcb;
{
	register struct ftp *mp;
	char *calloc();

	mp = (struct ftp *)calloc(1,sizeof (struct ftp));
	mp->tcb = tcb;
	mp->next = ftp;
	mp->prev = NULL;
	if(mp->next != NULL)
		mp->next->prev = mp;
	ftp = mp;
	return mp;
}

/* Free resources, delete control block */
static ftp_delete(mp)
register struct ftp *mp;
{
	if(mp->prev != NULL)
		mp->prev->next = mp->next;
	else
		ftp = mp->next;
	if(mp->next != NULL)
		mp->next->prev = mp->prev;
	free((char *)mp);
}

/* Find a control block given TCB */
static struct ftp *
ftp_lookup(tcb)
register struct tcb *tcb;
{
	register struct ftp *mp;

	for(mp = ftp;mp != NULL;mp = mp->next){
		if(mp->tcb == tcb)
			break;
	}
	return mp;
}

/* Parse and execute ftp commands */
static
void
docommand(mp)
register struct ftp *mp;
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
	case TYPE_CMD:
		switch(*arg){
		case 'a':	/* Ascii */
			mp->type = ASCII_TYPE;
			respond(mp,typeok,sizeof(typeok));
			break;
		case 'b':	/* Binary */
		case 'i':	/* Image */
			mp->type = IMAGE_TYPE;
			respond(mp,typeok,sizeof(typeok));
			break;
		default:	/* Invalid */
			respond(mp,badtype,sizeof(badtype));
			break;
		}
		return;
	case QUIT_CMD:
		respond(mp,bye,sizeof(bye));
		close_tcp(mp->tcb);
		ftp_delete(mp);
		return;
	case RETR_CMD:
		if((mp->fp = fopen(arg,"r")) == NULL){
			respond(mp,cantopen,sizeof(cantopen));
		} else {
			mp->state = SENDING;
			respond(mp,sending,strlen(sending));
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
	case PORT_CMD:
		if(pport(&mp->port,arg) == -1){
			respond(mp,badport,sizeof(badport));
		else
			respond(mp,portok,sizeof(portok));
		return;
	case LIST_CMD:
	case ACCT_CMD:
	case PASS_CMD:
	case CWD_CMD:
	case DELE_CMD:
		respond(mp,unimp,sizeof(unimp));
		return;
	}
}
int
pport(sock,arg)
struct socket *sock;
char *arg;
{
	int32 n;
	int atoi();

	n = 0;
	for(i=0;i<4;i++){
		n = atoi(arg) + (n << 8);
		if((arg = index(arg,',')) == NULL)
			return -1;
	}
	sock->address = n;
	n = atoi(arg);
	if((arg = index(arg,',')) == NULL)
		return -1;
	n = atoi(arg) + (n << 8);
	sock->port = n;
	return 0;
}
/* Send message back to client */
static void
respond(mp,message,length)
struct ftp *mp;
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
