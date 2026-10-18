#define	CONFIG	"param.pad"

char *cmdchk();
char *pval();
int setval();
/* Type fields in cmdtab */
#define	BOOL	0
#define	NUM	1
#define	BYTE	2
#define	STRING	3
#define	CHAR	4

/* Immediate command lookup table entry */
struct cmdtab {
	char *name;
	int (*funct)();
};

/* Variable parameter lookup table entry */
struct vartab {
	char *name;	/* Name of command */
	char *val;	/* A pointer to the value */
	char type;	/* string/integer/immediate */
	unsigned minval;	/* Minimum legal value */
	unsigned maxval;	/* Maximum legal value */
};
extern struct cmdtab cmdtab[];
extern struct vartab linetab[];
extern struct vartab termtab[];
extern struct vartab linktab[];
extern struct line lt;
extern struct link ln;

int connect(),setmyc(),setunp(),trans(),setbt(),lineparam(),linkparam(),
	termparam(),purge(),convers(),disconnect(),load(),memory(),
	save(),reset();
