#include "ax25.h"

/* Address hash function. Exclusive-ORs each byte, ignoring
 * such insignificant, annoying things as E and H bits
 */
static int
hash(s)
register struct addr *s;
{
	register int x;
	register char *cp;

	x = 0;
	for(cp = &s->call[0];cp < &s->call[ALEN];cp++)
		x ^= (*cp & 0xff);
	x ^= (s->ssid & SSID);
	return x & 0xff;
}
/* Look up entry in hash table */
struct link *
find_link(addr)
register struct addr *addr;
{
	int hashval;
	register struct link *link;
	char i_state;

	/* Find appropriate hash chain */
	hashval = hash(addr) % NHASH;

	/* Search hash chain */
	i_state = disable();
	for(link = hashtab[hashval] ; link != NULL ; link = link->next){
		if(addreq(&link->address[0],addr)){
			restore(i_state);
			return link;
		}
	}
	restore(i_state);
	return NULL;
}

/* Remove address entry from hash table */
del_link(link)
register struct link *link;
{
	int hashval;
	char i_state;

	if(link == NULL)
		return;
	/* Remove from hash header list if first on chain */
	hashval = hash(link->address) % NHASH;
	i_state = disable();
	if(hashtab[hashval] == link)
		hashtab[hashval] = NULL;

	/* Remove from chain list */
	if(link->prev != NULL)
		link->prev->next = link->next;
	if(link->next != NULL)
		link->next->prev = link->prev;
	free((char *)link);
	restore(i_state);
}

/* Create a link. Allocate a new structure, if necessary,
 * and fill it with all the defaults. The caller
 * is still responsible for filling in the reply address
 */
struct link *
cr_link(addr)
struct addr *addr;
{
	struct link *link,*link1;
	int hashval;
	char i_state;

	link = find_link(addr);

	if(link == NULL){
		/* Not already in table; create an entry
		 * and insert it at the head of the chain
		 */
		/* Find appropriate hash chain */
		hashval = hash(addr) % NHASH;
		i_state = disable();
		link = (struct link *)calloc(1,sizeof(struct link));
		link1 = hashtab[hashval];
		link->next = link1;	/* point to entry previously on top, if any */
		link->prev = NULL;
		if(link1 != NULL)
			link1->prev = link;
		hashtab[hashval] = link;
		restore(i_state);
	}
	movmem((char *)&linkdefault,(char *)link,sizeof(struct link));
	movmem((char *)addr,(char *)&link->address[0],sizeof(struct addr));
	link->address[0].ssid &= ~(REPEATED | E);
	movmem((char *)&mycall,(char *)&link->address[1],sizeof(struct addr));
	link->address[1].ssid &= ~(REPEATED | E);
	return link;
}
