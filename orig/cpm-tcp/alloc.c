/* memory allocation routines
 *
 * Adapted from Aztec user-supplied code by William C. Colley III; MUST
 * be used in place of original, *BUGGY* Aztec code!
 *
 * Routines realloc() and calloc() were added; only minor format changes
 * made elsewhere.
 */

#define	NULL	0

union header {
	struct {
		union header *ptr;
		unsigned size;
	} s;
	long l;
};

typedef union header HEADER;

static HEADER base, *allocp = NULL;

/* Allocate block of 'nb' bytes */
char *malloc(nb)
unsigned nb;
{
	HEADER *morecore();
	register HEADER *p, *q;
	register unsigned nu;

	nu = ((nb + 3) >> 2) + 1;
	if ((q = allocp) == NULL){
		base.s.ptr = allocp = q = &base;
		base.s.size = 1;
	}
	for (p = q->s.ptr; ; q = p, p = p->s.ptr){
		if (p->s.size >= nu){
			if (p->s.size == nu){
				q->s.ptr = p->s.ptr;
			} else {
				p->s.size -= nu;
				p += p->s.size;
				p->s.size = nu;
			}
			allocp = q;
			p->s.ptr = p;	/* for auditing */
			return (char *)(p + 1);
		}
		if (p == allocp && (p = morecore(nu)) == NULL)
			return NULL;
	}
}

/* Get more memory from the system and put it on the heap */
HEADER *
morecore(nu)
unsigned nu;
{
	char *sbrk();
	register char *cp;
	register HEADER *up;

	if ((int)(cp = sbrk(nu << 2)) == -1)
		return NULL;
	up = (HEADER *)cp;
	up->s.size = nu;
	up->s.ptr = up;	/* satisfy audit */
	free((char *)(up + 1));
	return allocp;
}

/* Put memory block back on heap */
free(blk)
char *blk;
{
	register HEADER *p, *q;

	p = (HEADER *)blk - 1;
	/* Audit check */
	if(p->s.ptr != p){
		printf("PANIC: freeing garbage (0x%x)\r\n",blk);
		printf("SP = 0x%x\r\n",getsp());
		for(;;) ;
	}
	for(q = allocp; !(p > q && p < q->s.ptr); q = q->s.ptr){
		if (q >= q ->s.ptr && (p > q || p < q->s.ptr))
			break;
	}
	if(p + p->s.size == q->s.ptr){
		p->s.size += q->s.ptr->s.size;
		p->s.ptr = q->s.ptr->s.ptr;
	} else
		p->s.ptr = q->s.ptr;
	if(q + q->s.size == p){
		q->s.size += p->s.size;
		q->s.ptr = p->s.ptr;
	}
	else q->s.ptr = p;
	allocp = q;
}

/* Move existing block to new area */
char *
realloc(area,size)
char *area;
unsigned size;
{
	unsigned osize;
	HEADER *hp;
	char *cp;

	hp = ((HEADER *)area) - 1;
	osize = (hp->s.size -1) << 2;
	free(area);
	if((cp = malloc(size)) != NULL && cp != area)
		movmem(area,cp,size>osize? osize : size);
	return cp;
}
/* Allocate block of cleared memory */
char *
calloc(nelem,size)
unsigned nelem;	/* Number of elements */
unsigned size;	/* Size of each element */
{
	register unsigned i;
	register char *cp;

	i = nelem * size;
	if((cp = malloc(i)) != NULL)
		setmem(cp,i,0);
	return cp;
}

	