/*--------------------------------------------------------------------------
 Definitions for users of the uwho object.
--------------------------------------------------------------------------*/
#ifndef uwho_h
#define uwho_h

#ifndef lint
static char uwho_h_sccsid[]="@(#)uwho.h	2.8 11/14/92";
#endif

/* Array sizes */
#define uwho_MAXQUERIES	200
#define MAXHOSTLEN	128
#define MAXSTR		1024

/* End of Configurable Parameters */

/* Values for uwho_query_t->state */
#define uwho_STATE_CONNECTING 0
#define uwho_STATE_READING 1
#define uwho_STATE_CLOSED 2
#define uwho_STATE_FREED 3

/* Values for uwho_query_t->protocol; also indices for uwho_update(,,**h) */
#define uwho_PROTO_UNKNOWN -2
#define uwho_PROTO_ALL -1
#define uwho_PROTO_WHOIS 0
#define uwho_PROTO_PH 1
#define uwho_PROTO_KIS 2

/*--------------------------------------------------------------------------
 A structure to describe a query and its result, from birth to death.
--------------------------------------------------------------------------*/
typedef struct {
    char name[MAXHOSTLEN], descrip[MAXHOSTLEN];
    char *infobuffer, *query;
    int infofree;
    int infoalloc;		/* Number of bytes allocated for infobuffer */
    int infostate;
    int protocol, fds, state;
} uwho_query_t;

/*--------------------------------------------------------------------------
 A structure to describe a set of queries.
--------------------------------------------------------------------------*/
typedef struct uwho_s {
    /* Global options. */
    char def_host[MAXHOSTLEN];
    int def_host_protocol;
    char uwho_dir[MAXSTR];
    int Literal;			/* query is literal server command */

    /* Internet port number constants. */
    int whois_port;
    int ph_port;
    int kis_port;

    FILE *fp;			/* Opened to the data file of all servers. */
    char fname[MAXSTR];		/* Name of the data file of all servers. */

    /* Status variables. */
    int Max_FD;			/* used by select() */
    char errmsg[MAXSTR];	/* Holds all error messages. */
    int nqueries;
    int n_open;			/* number of queries still open */
    uwho_query_t queries[uwho_MAXQUERIES];

#ifdef ANSI
    void (*errFn)(struct uwho_s  *this);
#else
    void (*errFn)();
#endif
    /* Called when an error is placed in errmsg
     * uwho_errFn_t gets typedef'ed to this later...
     */
} uwho_t;

/* Here is that typedef for errFn - 
 * A callback function for use when an error occurs. 
 * Should print out this->errmsg.
 */
#ifdef ANSI
typedef void (*uwho_errFn_t)(uwho_t *this);
#else
typedef void (*uwho_errFn_t)();
#endif

/* A callback function for use when a query completes. */
#ifdef ANSI
typedef void (*uwho_doneFn_t)(uwho_query_t *);
#else
typedef void (*uwho_doneFn_t)();
#endif

/* Hardcoded servernames.  Nominally only for server types that don't
 * have online lists of servers.
 *
 * You can include your local server here if it is private and therefore 
 * doesn't appear in the list of servers obtained online.
 * However, unless you're running on a Mac, it's just as easy to add 
 * in your server by appending it to uwho.dat; e.g. under Unix or MS-DOS,
 *    echo  whois  whois.podunk.edu  Podunk University >> uwho.dat
 * after invoking uwho -update.
 */
#define uwho_OTHER_SERVERS "\
kis	regulus.cs.bucknell.edu		MCImail (mcimail.com)\n\
"

/* uwho.c */
#ifdef ANSI
int uwho_AddExactQuery(uwho_t *this, int protocol, char *query, char *name, char *descrip);
int uwho_AddApproxQuery(uwho_t *this, char *org, char *query);
void uwho_closeQueries(uwho_t *this, uwho_doneFn_t doneFn);
void uwho_deleteQueries(uwho_t *this);
void uwho_destroy(uwho_t *this);
uwho_t *uwho_create(char *def_host, int def_proto, char *def_dir, uwho_errFn_t errFn);
void uwho_poll_setup(uwho_t *this);
int uwho_poll(uwho_t *this, struct timeval *timeout, uwho_doneFn_t doneFn);
int uwho_update(uwho_t *this, int Timeout, char **servernames);
int uwho_getfilename(uwho_t *this, char *filename, int protocol, int perm);
#else
int uwho_AddExactQuery();
int uwho_AddApproxQuery();
void uwho_closeQueries();
void uwho_deleteQueries();
void uwho_destroy();
uwho_t *uwho_create();
void uwho_poll_setup();
int uwho_poll();
int uwho_update();
int uwho_getfilename();
#endif

#endif
