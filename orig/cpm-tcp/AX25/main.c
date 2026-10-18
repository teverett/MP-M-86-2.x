/* The main loop of the program runs through an infinite loop,
 * polling queues and executing higher level functions whenever
 * things happen at the interrupt level
 */
#include "ax25.h"
main(argc,argv)
int argc;
char *argv[];
{
	char *sbot();
	extern char _Uend;

	di();
	meminit();
	terminit();
	timerinit();
	load(argc,argv);
	di();
	ioinit();
	im2();
	ei();
	tprintf("Available memory: %u bytes from %x to %x\n",
		memlimit,(unsigned)&_Uend,(unsigned)sbot());
	for(;;){
		level2();
		terminal();
		timer_srv();
		eihalt();
	}
}
