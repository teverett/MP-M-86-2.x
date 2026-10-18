/* term.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */
/* Serial Terminal I/O */
#include "ax25.h"
#include "fad.h"
#include <ctype.h>

/* Top-level terminal processing */
terminal()
{
	register struct frame *fp;
	int len;

	/* Input processing */
	if((len = ttyproc(term.lbuf,sizeof(term.lbuf))) != -1)
		do_line(term.lbuf,(unsigned)len);
	/*
	 * Output processing
	 * Don't process any buffers on the raw output queue unless there is
	 * enough room for them on the cooked transmit fifo in the absolute
	 * worst case (where every character is a newline).
	 */
	if(term.mode == TRANS_MODE){
		while(term.txq.count != 0
		 && term.txq.first->iocnt < FIFOSIZE - term.txfifo.count){
			if((fp = getframe(&term.txq)) == NULL)
				break;	/* Can't happen, but just in case */
			twrite(fp->iop,fp->iocnt);
			freeframe(fp);
		}
	} else {
		while(term.txq.count != 0
		 && 2*term.txq.first->iocnt < FIFOSIZE - term.txfifo.count){
			if((fp = getframe(&term.txq)) == NULL)
				break;	/* Can't happen, but just in case */
			while(fp->iocnt-- != 0)
				tputc(*fp->iop++);
			freeframe(fp);
		}
	}
}

#ifdef	FASTCRT	/* for quick output on Xerox 820. But breaks ^S/^Q */

stxint()
{
}
tputc(c)
char c;
{
	if(c == '\n')
		putchar(c);
	putchar(c);
}

#else

/* "putchar" function to serial line. Expands newline. */
int
tputc(c)
char c;
{
	if(c == '\n'){
		putf(&term.txfifo,'\r');
		putf(&term.txfifo,c);
		stxint();
	} else {
		putf(&term.txfifo,c);
	}
}

/* Serial transmitter interrupt service routine */
stxint()
{
	int c;

	/* Keep throwing stuff at the transmitter as long as it
	 * stays ready (minimize interrupts on a very fast device
	 * such as a memory-mapped CRT)
	 */
	while(stxrdy() && !term.outflow){
		if((c = getf(&term.txfifo)) == -1)
			break;
		stx(c);
	}
}
#endif FASTCRT

/* Serial receive interrupt */
srxint()
{
	/* Keep getting characters as long as the device has 'em
	 * (minimize interrupts for a VERY fast typist!)
	 */
	while(srxrdy())
		putf(&term.rxfifo,srx());
	if(term.rxfifo.count > term.hiwat && !term.outflow){
		term.outflow = YES;
		srxflow(1);
	} else if(term.rxfifo.count < term.lowat && term.outflow){
		term.outflow = NO;
		srxflow(0);
	}
}
/* Flow control the serial receiver */
/*ARGSUSED*/
srxflow(state)
char state;
{
	/* TO BE WRITTEN */
}

/* Terminal input "process" periodically called from main loop.
 * Read characters (max n) from serial input queue and assemble them
 * in the caller's buffer.
 * If we're in command or conversational mode, cook the line a la gets();
 * otherwise return exactly n characters. (needs transparent timeouts!)
 * Keep track of where we were on the last call unless the last call
 * returned a completed buffer.
 * Keep returning NULL until a line is ready.
 */
int
ttyproc(buf,n)
char *buf;
int n;
{
	static char *lp;
	static int count;
	register int c;

	if(lp == NULL){
		/* Fresh user buffer */
		lp = buf;
		count = 0;
	}
	while((c = getf(&term.rxfifo)) != -1){
		if(term.mode == COMMAND_MODE || term.mode == CONVERS_MODE){
			/* Cooked mode */
			c &= 0x7f;	/* Strip parity */
			if(term.stop != 0 && c == term.stop){
				term.outflow = YES;
				continue;
			} else if(term.start != 0 && c == term.start) {
				term.outflow = NO;
				stxint();
				continue;
			} else if(term.delete != 0 && c == term.delete){
				if(lp > buf){
					unechoit(*--lp);
					count--;
				}
				continue;
			} else if(c != 0 && c == term.canline){
				while(lp > buf){
					unechoit(*--lp);
				}
				count = 0;
				continue;
			}
		}
		*lp++ = c;
		count++;
		if(term.mode == TRANS_MODE)
			start_timer(&term.timer_cin);
		else if(term.echo)
			echoit(c);
		/* Check if buffer should be returned to user */
		if(c == term.sendpac || c == term.command || count >= n){
			lp = NULL;
			return count;
		}
	}
	/* Input silo is emptied, now check for a transparent mode timeout */
	if((term.mode == TRANS_MODE && count != 0
	 && term.timer_cin.state == TIMER_EXPIRE) ){
		lp = NULL;
		return count;
	}
	return -1;
}
/* Echo character. If control, display in two-character form, e.g,
 * ^C
 */
echoit(c)
register char c;
{
	if(c == '\r' || c == '\n')
		tputs("\n");
	else
		tputs(visible(c));
	stxint();
}
/* Emit enough space-backspace-space sequences to cover up the given
 * character which had been echoed with echoit()
 */
unechoit(c)
register char c;
{
	if(c == '\r' || c == '\n')
		return;	/* Can't un-echo these */
	if(!isprint(c))
		tputs("\b \b");
	tputs("\b \b");
	stxint();
}

/* "printf" function to serial line. */
/*VARARGS1*/
int
tprintf(fmt,a)
char *fmt;
int a;
{
	int tputc();

	return format(tputc,fmt,&a);
}
/* String put to terminal. */
char *
tputs(str)
char *str;
{
	register char *cp;

	if(str == NULL)
		return NULL;
	cp = str;
	while(*cp != '\0')
		tputc(*cp++);
	return str;
}

/* Raw buffer write to terminal output fifo.
 * (To be replaced with something more efficient.)
 */
twrite(buf,len)
register char *buf;
register unsigned len;
{
	while(len-- != 0)
		putf(&term.txfifo,*buf++);
}
