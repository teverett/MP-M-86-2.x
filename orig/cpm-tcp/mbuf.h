#define	min(x,y)	((x)<(y)?(x):(y))

struct mbuf {
	struct mbuf *next;	/* Links mbufs belonging to single packets */
	struct mbuf *anext;	/* Links packets on queues */

	char *_buf;			/* Allocated data buffer, if any */

	char *data;		/* Active working pointers */
	int16 cnt;
};

void enqueue();
struct mbuf *alloc_mbuf(),*dequeue(),*copy_p(),*free_p(),*qdata();
int16 pullup(),append(),dup_p(),len_mbuf(),dqdata();
