#include "ax25.h"
#include "command.h"
#define	NARG	20

/* Parse command line and set up command arguments Unix-style.
 * Note: argument is modified (delimiters are overwritten with nulls)
 */
cmd(line)
register char *line;
{
	struct cmdtab *cm;
	char *argv[NARG],*cp,*cmdchk();
	int argc,qflag;

	for(argc = 0;argc < NARG;argc++)
		argv[argc] = NULL;

	for(argc = 0;argc < NARG;){
		qflag = 0;
		/* Skip leading white space */
		while(*line == ' ' || *line == '\t')
			line++;
		if(*line == '\0')
			break;
		/* Check for quoted token */
		if(*line == '"'){
			line++;	/* Suppress quote */
			qflag = 1;
		}
		argv[argc++] = line;	/* Beginning of token */
		/* Find terminating delimiter */
		if(qflag){
			/* Find quote, it must be present */
			if((line = index(line,'"')) == NULL){
				tprintf("Unterminated quote\n");
				return;
			}
		} else {
			/* Find space or tab. If not present,
			 * then we've already found the last
			 * token
			 */
			if((cp = index(line,' ')) == NULL
			 && (cp = index(line,'\t')) == NULL){
				break;
			}
			line = cp;
		}
		*line++ = '\0';
	}
	if(argc == 0)
		return;
	/* Find command in table and execute */
	for(cm = cmdtab;cm->name != NULL;cm++){
		if(cmdchk(cm->name,argv[0])){
			(*cm->funct)(argc,argv);
			break;
		}
	}
	if(cm->name == NULL)
		tprintf("Command not found\n");
}
/* Process a completed line typed on the user's terminal */
do_line(lp,len)
char *lp;
unsigned len;
{
	switch(term.mode){
	case COMMAND_MODE:
		if(len == 0)
			break;
		lp[--len] = '\0';	/* Discard newline */
		cmd(lp);
		break;
	case CONVERS_MODE:
		if(lp[len-1] == term.command){
			term.mode = COMMAND_MODE;
			break;
		}
		if(!term.cr)			/* Discard newline */
			lp[--len] = '\0';
	case TRANS_MODE:		/* note fall-thru */
		if(trans_brk(lp,len))
			term.mode = COMMAND_MODE;
		else {
			send(term.tlink,(char)NOLEVEL3,lp,len);
		}
		break;
	}
}
/* Check for a transparent-mode escape (a buffer with exactly three
 * characters, all of which are the command-recall character)
 */
int
trans_brk(line,len)
char *line;
unsigned len;
{
	if(len == 3 && term.command != 0
	 && *line++ == term.command && *line++ == term.command && *line == term.command)
		return 1;
	return 0;
}

/* See if 'b' is at least a prefix to 'a'.
 * Return pointer to first char in 'b' past the matching part
 * if so, NULL otherwise.
 */
char *
cmdchk(a,b)
register char *a,*b;
{
	register int blen;

	blen = strlen(b);
	if(strlen(a) >= blen && strncmp(a,b,blen) == 0)
		return b + blen;
	return NULL;
}
