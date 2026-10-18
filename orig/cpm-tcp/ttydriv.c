/* TTY input driver */
#define	NULL	(char *)0

int ttymode;
#define TTY_COOKED	0
#define	TTY_RAW	1

#define	LINESIZE	256

#define	CTLU	21

raw()
{
	ttymode = TTY_RAW;
}

cooked()
{
	ttymode = TTY_COOKED;
}

/* Accept characters from the incoming tty buffer and process them
 * (if in cooked mode) or just pass them directly (if in raw mode).
 * Returns the number of characters available for use; if non-zero,
 * also stashes a pointer to the character(s) in the "buf" argument.
 */
int
ttydriv(c,buf)
char c;
char **buf;
{
	static char linebuf[LINESIZE];
	static char *cp = linebuf;
	int cnt;

	if(buf == (char **)NULL)
		return 0;	/* paranoia check */

	cnt = 0;
	switch(ttymode){
	case TTY_RAW:
		*cp++ = c;
		cnt = cp - linebuf;
		cp = linebuf;
		break;
	case TTY_COOKED:
		/* Perform cooked-mode line editing */
		switch(c & 0x7f){
		case '\r':	/* CR and LF are equivalent */
		case '\n':
			*cp++ = '\r';
			*cp++ = '\n';
			printf("\r\n");
			cnt = cp - linebuf;
			cp = linebuf;
			break;
		case '\b':		/* Backspace */
			if(cp != linebuf){
				cp--;
				printf("\b \b");
			}
			break;
		case CTLU:	/* Line kill */
			while(cp != linebuf){
				cp--;
				printf("\b \b");
			}
			break;
		default:	/* Ordinary character */
			*cp++ = c;
			putchar(c);
			if(cp >= &linebuf[LINESIZE]){
				cnt = cp - linebuf;
				cp = linebuf;
			}
			break;
		}
	}
	if(cnt != 0)
		*buf = linebuf;
	else
		*buf = NULL;
	return cnt;
}

