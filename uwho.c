/*--------------------------------------------------------------------------
 uwho - an object which knows how to look up e-mail addresses.
 Results currently available only in human-readable form, but
 (when the whois protocol update is available from IETF)
 will also act as an e-mail address resolver.

 Public entry points are listed in uwho.h.
--------------------------------------------------------------------------*/

#ifndef lint
static char SCCSid[]="%W% %G%";
#endif

/*
 * (b) BeerWare 1992 
 *
 * Nye Liu nyet@cco.caltech.edu
 * Dan Kegel dank@alumni.caltech.edu
 *
 * If you like and use this code, please donate a case of your
 * favorite beer (or a recommendation and money) to:
 *
 * House 'o Da Month c/o Nye Liu
 * 255 S. Hill Ave.
 * Pasadena, CA  91106
 *
 */   

#include <errno.h>
#include <stdio.h>
#include <assert.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <time.h>
#include <io.h>
#define access _access
#endif

#include "mystring.h"
#include "tcpip.h"
#include "uwho.h"

/* Kludge: our VMS doesn't define these args to access() */
#ifndef R_OK
#define F_OK            0       /* does file exist */
#define X_OK            1       /* is it executable by caller */
#define W_OK            2       /* is it writable by caller */
#define R_OK            4       /* is it readable by caller */
#endif

#define VERSION		sccsid
#define MAXORGLEN	128

/* make sure INFOBLOCKSIZE >= BLOCKSIZE or there will be trouble */

#define MAXLINELEN	512
#define INFOBLOCKSIZE	512

#define BLOCKSIZE	101

#ifdef MSDOS
#define DIR_SEPARATOR   "\\"
#define OLD_FILE_SUFFIX "o"
/* make room for suffix */
/* MS-DOS restricted to 8.3 = 12 chars */
#define uwho_DAT_FILE "uwho.dt" 
#endif
#ifdef WIN32	/* Least common demoninator is 8.3 file system */
#define DIR_SEPARATOR   "\\"
#define OLD_FILE_SUFFIX "o"
/* make room for suffix */
/* MS-DOS restricted to 8.3 = 12 chars */
#define uwho_DAT_FILE "uwho.dt" 
#endif
#ifdef VMS
#define DIR_SEPARATOR	""
#define OLD_FILE_SUFFIX	"_old"
#define uwho_DAT_FILE	"uwho.dat"
#endif
#ifdef UNIX
#define DIR_SEPARATOR   "/"
#define OLD_FILE_SUFFIX	".old"
#define uwho_DAT_FILE	"uwho.dat"
#endif

/* Restrict length to 8 chars for benefit of MS-DOS and NT */
#define dat_TEMPLATE	"uDXXXXXX"
#define kis_TEMPLATE	"uKXXXXXX"
#define ph_TEMPLATE	"uPXXXXXX"
#define whois_TEMPLATE	"uWXXXXXX"

#define uwho_PROTO_PH_FILE	"uwho.ph"
#define uwho_PROTO_WHOIS_FILE	"uwho.ws"

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifdef ANSI
#ifdef SUNOS
/* this is so I can use gcc -Wall on Suns, since the include files are missing 
   many prototypes */
#include "missing.h"
#endif
#endif

/*--------------------------------------------------------------------------
 Print out this->errmsg by calling errFn if not null.
--------------------------------------------------------------------------*/
static void uwho_printErr(this) 
    uwho_t *this;
{
    if (this->errFn) (*this->errFn)(this);
}

/*--------------------------------------------------------------------------
 ignore_kills():
 Ignore control-C or any other abort signal if x is nonzero; used when 
 entering or leaving a critical section.

 ignore_writeEOF():
 if a pipe breaks, let write() return -1 rather than abort program.
--------------------------------------------------------------------------*/
#ifndef NO_SIGNALS
#include <signal.h> 
#define ignore_kills(x) signal(SIGINT,x?SIG_IGN:SIG_DFL);
#ifdef MSDOS
#define ignore_writeEOF(x)
#else
#define ignore_writeEOF(x) signal(SIGPIPE,x?SIG_IGN:SIG_DFL);
#endif
#else
#define ignore_kills(x)
#define ignore_writeEOF(x)
#endif

/*--------------------------------------------------------------------------
 Generate a filename of a temporary, permanent, or backup datafile
 for the given protocol
 (perm = 0 is temp, 1 is perm, 2 is backup).
 Returns zero upon success, nonzero on failure; error message in 
 this->errmsg.
--------------------------------------------------------------------------*/
int uwho_getfilename(this, filename, protocol, perm)
    uwho_t *this;
    char *filename;
    int protocol, perm;
{
    char *file;

    char dat_template[sizeof(dat_TEMPLATE)+1];
    char ph_template[sizeof(ph_TEMPLATE)+1];
    char whois_template[sizeof(whois_TEMPLATE)+1];

    my_strncpy(dat_template,dat_TEMPLATE,sizeof(dat_template));
    my_strncpy(ph_template,ph_TEMPLATE,sizeof(ph_template));
    my_strncpy(whois_template,whois_TEMPLATE,sizeof(whois_template));

    switch(protocol) {
	case uwho_PROTO_KIS:
	    strcpy(this->errmsg,
		"bug: uwho_getfilename: uwho_PROTO_KIS has no data file");
	    uwho_printErr(this);
	    return (-1);
	    break;
	case uwho_PROTO_PH:
	    if (!perm) file = (char *)mktemp(ph_template);
	    else file=uwho_PROTO_PH_FILE;
	    break;
	case uwho_PROTO_WHOIS:
	    if (!perm) file = (char *)mktemp(whois_template);
	    else file=uwho_PROTO_WHOIS_FILE;
	    break;
	case uwho_PROTO_ALL:
	    if (!perm) file = (char *)mktemp(dat_template);
	    else file=uwho_DAT_FILE;
	    break;
	default:
	    strcpy(this->errmsg,"bug: uwho_getfilename: unknown protocol");
	    uwho_printErr(this);
    }
    if (file==NULL) return (-1); /* mktemp returns null on error */

    sprintf(filename, "%s%s%s", this->uwho_dir, DIR_SEPARATOR, file);
    if (perm==2) strcat(filename, OLD_FILE_SUFFIX);
    return (0);
}

/*--------------------------------------------------------------------------
 Rearrange a name in form "last, first" to "first last" for benefit of
 services (kis, ph) that don't support "last, first".
 CONCATENATES result onto string 'to'.
--------------------------------------------------------------------------*/
static void swapLastFirstcat(to, from)
    char *to, *from;
{
    char *comma, *p;

    comma = strchr(from, ',');
    if (comma == NULL)
	(void) strcat(to, from);
    else {
	for (p=comma+1; isspace(*p);p++)
	;
	(void) strcat(to, p);
	*comma = 0;
	(void) strcat(to, " ");
	(void) strcat(to, from);
	/* added to restore "from" string */
	*comma = ',';
    }
}


/*--------------------------------------------------------------------------
 look up the hostent of a tcpip host
--------------------------------------------------------------------------*/
struct hostent *my_gethostbyname(name) 
    char *name;
{
    static struct hostent def;
    static struct in_addr defaddr;
    struct hostent *result;

#ifdef WOLLENGONG
    char ahostname[MAXHOSTLEN];
    char **ahostnameptr;
    
    strcpy(ahostname, name);
    ahostnameptr = (char **) malloc(sizeof ahostname);	/* MEMORY LEAK */
    *ahostnameptr = ahostname;
    
    if ((defaddr.s_addr = rhost (&name)) == -1)
	result = (struct hostent *) NULL;
    else {
	extern char *alist[1];
	def.h_addr_list = alist;
	def.h_name = name;
	def.h_addr = (char *)&defaddr;
	def.h_length = sizeof(struct in_addr);
	def.h_addrtype = AF_INET;
	def.h_aliases = 0;
	result = &def;
    }
#else
    result = gethostbyname(name);
#endif

    /* If gethostbyname fails, try inet_addr. */
    if (result == NULL) {
	defaddr.s_addr = inet_addr(name);
	if (defaddr.s_addr == -1) {
	    result = (struct hostent *) NULL;
	} else {
#ifdef h_addr_list
	    /* Added by SOB.  Looks cheezy to me.  Who defines h_addr_list? */
	    static char *alist[1];
	    def.h_addr_list = alist;
#endif
	    def.h_name = name;
	    def.h_addr = (char *)&defaddr;
	    def.h_length = sizeof(struct in_addr);
	    def.h_addrtype = AF_INET;
	    def.h_aliases = 0;
	    result = &def;
	}
    }
    return result;
}

/*--------------------------------------------------------------------------
 Look up the port number of a tcp service.  Returns zero upon failure.
--------------------------------------------------------------------------*/
int my_getservbyname(name) 
    char *name;
{
    struct servent *sp;
    if (!(sp = getservbyname(name, "tcp"))) {
	/*(void)fprintf(stderr, "uwho: tcp/%s: unknown service\n",name);*/
	if (strcmp(name, "whois")==0) {
	    /*(void)fprintf(stderr, "uwho: assuming port #43\n");*/
	    return htons(43);
	} else if (strcmp(name, "ph")==0 || strcmp(name, "csnet-ns")==0) {
	    /*(void)fprintf(stderr, "uwho: assuming port #105\n");*/
	    return htons(105);
	} else if (strcmp(name, "remote-kis")==0) {
	    /*(void)fprintf(stderr, "uwho: assuming port #185\n");*/
	    return htons(185);
	} 
	return 0;
    }
    return sp->s_port;
}

/*--------------------------------------------------------------------------
 Append a query with the given hostname, protocol, and query information
 to the given uwho_t.
 Open a socket to the given host and allocate space for the response.
 Query information is loaded differently depending on protocol.

 Returns zero upon success, nonzero + error in this->errmsg upon error.
 -1 = random error
 -2 = ran out of sockets
--------------------------------------------------------------------------*/
int uwho_AddExactQuery(this,protocol,query,name,descrip)
    uwho_t *this;
    int protocol;
    char *query, *name, *descrip;
{
    uwho_query_t *qp;
    char xquery[MAXSTR];

    struct hostent *hp;
    struct sockaddr_in sin;
    unsigned long inet_addr();
    int err;

    qp = this->queries + this->nqueries++;
    if (this->nqueries >= uwho_MAXQUERIES) {
	strcpy(this->errmsg, "Too many queries!");
	uwho_printErr(this);
	return -1;
    }

    /* Fill the qp's fields that don't depend on protocol */
    qp->infobuffer = NULL;
    qp->infofree = 0;
    qp->infoalloc = 0;
    qp->infostate = 0;
    qp->protocol=protocol;
    my_strncpy(qp->name, name, MAXHOSTLEN-1);
    my_strncpy(qp->descrip, descrip, MAXHOSTLEN-1);

    /* Fill the qp's fields that *do* depend on protocol */
    xquery[0] = '\0';
    switch(protocol) {
	case uwho_PROTO_KIS:
	    if (this->Literal || strncmp(query, "help", 4)==0)
		strcat(xquery, query);
	    else {
		strcpy(xquery, "service mcimail\r\n");
		swapLastFirstcat(xquery, query);
	    }
	    strcat(xquery, "\r\nquit\r\n");
	    break;
	case uwho_PROTO_PH:
	    if (this->Literal || strncmp(query, "help", 4)==0)
		strcat(xquery, query);
	    else {
		strcpy(xquery, "ph ");
		swapLastFirstcat(xquery, query);
	    }
	    strcat(xquery, "\r\nquit\r\n");
	    break;
	case uwho_PROTO_WHOIS:
	    strcpy(xquery, query);
	    strcat(xquery, "\r\n");
	    break;
	default:
	    strcpy(this->errmsg, "bug: uwho_AddExactQuery: unknown protocol");
	    uwho_printErr(this);
	    return (-1);
    }
    qp->query = malloc(strlen(xquery)+1);
    if (qp->query == NULL) {
	strcpy(this->errmsg, "Out of memory");
	uwho_printErr(this);
	return -1;
    }
    strcpy(qp->query, xquery);

    /* Following code opens the socket & connects to the host */

    qp->fds = -1;
    qp->state = uwho_STATE_CLOSED;

    /* Get the host's address, create a socket for use with the host */
    if (!(hp = my_gethostbyname(qp->name))) {
	sprintf(this->errmsg, "uwho_AddExactQuery: unknown host: %s", 
	    qp->name);
	uwho_printErr(this);
	return -1;
    }
    sin.sin_family = hp->h_addrtype;

#ifdef USE_BCOPY
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
#else
    memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
#endif

    switch (qp->protocol) {
	case uwho_PROTO_KIS:	sin.sin_port = this->kis_port; break;
	case uwho_PROTO_PH:	sin.sin_port = this->ph_port; break;
	case uwho_PROTO_WHOIS:	sin.sin_port = this->whois_port; break;
	default: sprintf(this->errmsg, 
	    "\nuwho_AddExactQuery: server %s has unknown protocol\n",qp->name);
	    uwho_printErr(this);
	    return -1;
    }
    if (( qp->fds = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0 ) {
#ifdef PTK40
	int neterrno = tk_geterrno(qp->fds);
#endif
	sprintf(this->errmsg, "uwho_AddExactQuery: %s: socket", qp->name);
	/*netperror(errmsg);*/
	if (neterrno==EMFILE||neterrno==ENFILE||neterrno==ENOBUFS) {
	    sprintf(this->errmsg,
		"uwho_AddExactQuery: Out of Sockets opening %s",qp->name);
	    uwho_printErr(this);
	    return -2;
	}
	uwho_printErr(this);
	return -1;
    }

#ifndef PTK40
    /* Use non-blocking I/O on this socket */
#ifdef USE_FIONBIO
    { 
	int state=1;
	netioctl(qp->fds, FIONBIO, (char *)&state);
    }
#else
    {
    int flags;
    flags = fcntl(qp->fds, F_GETFL, 0);
#ifdef USE_O_NDELAY
    flags |= O_NDELAY;
#else
    flags |= FNDELAY;
#endif
    fcntl(qp->fds, F_SETFL, flags);
    }
#endif
#endif
    /* Start connecting to the host.  May get EINPROGRESS. */
    err = connect(qp->fds, (struct sockaddr *)&sin, sizeof(sin));
#ifdef PTK40
    if (err != 0 ) {
#else
    if (err != 0 && neterrno != EINPROGRESS) {
#endif
	sprintf(this->errmsg, "\n%s: connect", qp->name);
	uwho_printErr(this);
	/*netperror("connect"); */
	netclose(qp->fds);
	qp->fds = -1;
	return -1;
    }
#ifdef PTK40
#ifdef USE_FIONBIO
    { 
	int state=1;
	netioctl(qp->fds, FIONBIO, (int *)&state);
    }
#endif
#endif
    /* malloc space for info returned from each host */
    qp->infoalloc=INFOBLOCKSIZE;
    qp->infobuffer=malloc(qp->infoalloc);
    if (!qp->infobuffer) {
	sprintf(this->errmsg,"uwho_AddExactQuery: malloc failed");
	uwho_printErr(this);
	netclose(qp->fds);
	qp->fds = -1;
	return -1;
    }
    *(qp->infobuffer)='\0';
    qp->infofree=INFOBLOCKSIZE-1;

    /* One more fd opened successfully */
    this->n_open++;
    qp->state = uwho_STATE_CONNECTING;
    if (qp->fds > this->Max_FD) this->Max_FD = qp->fds;

    return 0;
}

/*--------------------------------------------------------------------------
 Given an orginization's approximate name or hostname and a query for them,
 find all the servers whose orgname or hostname match,
 add ready-to-go queries to those servers to this.

 If the given org contains spaces, it is considered to be a set of patterns
 and all of the patterns must match, in no particular order.

 Returns the number of queries set, or -1 for fatal error.
 Exits early if too many orgs match the approximate org name.
--------------------------------------------------------------------------*/
#define MAXORGPATS	10
int uwho_AddApproxQuery(this, org, query)
    uwho_t *this;
    char *org, *query;
{
    char lowerbuf[MAXLINELEN],linebuf[MAXLINELEN];
    char *fieldp2, *fieldp3, *trunc;
    int linenum, nhosts=0, protocol=0;
    int i;
    char *p;
    char orgbuf[MAXLINELEN];	/* Lowercase copy of org */
    int nOrgPat;		/* Number of patterns in org string */
    char *orgPat[MAXORGPATS];	/* patterns from org string */

/* strtok is evil :-) so use isspace to parse out fields */
/* or could use fscanf but that's squirrely sometimes */

    if(!this->fp) {
	sprintf(this->errmsg,
	     "database file %s unreadable or non-existent; was this software installed properly?",this->fname);
	uwho_printErr(this);
	return -1;  /* no data file to search through */
    }

    /* Extract whitespace-separated patterns from org string */
    nOrgPat = 0;
    strnlower(orgbuf,org,MAXLINELEN-1);
    for (p=strtok(orgbuf," \t"); p; p=strtok(NULL," \t")) {
	if (nOrgPat == MAXORGPATS) {
	    sprintf(this->errmsg, "warning: too many patterns in org string\n");
	    uwho_printErr(this);
	    break;
	}
	orgPat[nOrgPat++] = p;
    }

    if (nOrgPat == 0) {
	/* no at field, assume def_host */
	return uwho_AddExactQuery(this, this->def_host_protocol, query, 
	    this->def_host, "Default Host");
    }

    rewind(this->fp);
    for (linenum=0; fgets(linebuf,MAXLINELEN-1,this->fp) != 0; linenum++) {
	/* advance fieldp2 to end of first field */
	for (fieldp2=linebuf; *fieldp2 && !isspace(*fieldp2); fieldp2++);
	/* advance fieldp2 to beginning of second field */
	for ( ; *fieldp2 && isspace(*fieldp2); fieldp2++);
	if (!*fieldp2) {
	    sprintf(this->errmsg, "error reading uwho data file: \
line %d: host field is missing\n", linenum);
	    uwho_printErr(this);
	    continue;
	}
	
	strnlower(lowerbuf,fieldp2,MAXLINELEN-1);

	for (i=0; i<nOrgPat; i++)
	    if (strstr(lowerbuf, orgPat[i]) == NULL) break;

	if (i == nOrgPat) {
	    if (nhosts >= uwho_MAXQUERIES) {
		sprintf(this->errmsg, "uwho_addApproxQuery: too many matches");
		uwho_printErr(this);
		return (nhosts);
	    }

	    /* advance fieldp3 to end of second field */
	    for (fieldp3=fieldp2; *fieldp3 && !isspace(*fieldp3); fieldp3++);
	    /* advance fieldp3 to beginning of third field */
	    for ( ; *fieldp3 && isspace(*fieldp3); fieldp3++);

	    if (!*fieldp3) {
		sprintf(this->errmsg,"uwho_addApproxQuery: warning: \
line %d: description field is missing in uwho data file", linenum);
		uwho_printErr(this);
	    }

	    /* terminate host name and host description strings */ 
	    /* could use isspace but this is more concise */
	    trunc=strpbrk(fieldp2, " \f\n\r\t\v");
	    if (trunc) *trunc='\0';
	    trunc=strpbrk(fieldp3, "\f\n\r\t\v");
	    if (trunc) *trunc='\0';
	     /* note: descriptions may contain spaces */
     
	    if (strncmp(linebuf, "kis", 3) == 0) 
		protocol=uwho_PROTO_KIS;
	    else if (strncmp(linebuf, "ph", 2) == 0) 
		protocol=uwho_PROTO_PH;
	    else if (strncmp(linebuf, "whois", 5) == 0) 
		protocol=uwho_PROTO_WHOIS;
	    else {
		sprintf(this->errmsg, "uwho_addApproxQuery: error reading \
data file: line %d: unknown protocol", linenum);
		uwho_printErr(this);
		continue;
	    }
	    uwho_AddExactQuery(this,protocol,query,fieldp2,fieldp3);
	    nhosts++;
	}
    }
    return (nhosts);
}

/*--------------------------------------------------------------------------
 Append the null-terminated data to the given query's buffer.
 Called by uwho_poll.

 Returns 0 normally, -1 if qp->infobuffer is full
--------------------------------------------------------------------------*/
static int info_update(this, qp, data)
    uwho_t *this;
    uwho_query_t *qp;
    char *data;
{
    int datalen;
    char *p, *q, *newbuf;

    /* filter out \r's and replace all escape chars with $ */
    for (p=q=data; *p; p++) {
	if (*p == '\033')
	    *q++ = '$';		
	else if (*p != '\r')
	    *q++ = *p;
    }
    *q='\0'; /* lop off rest of result */

    if ((datalen=strlen(data)) > qp->infofree){
	/* not enough space in infobuffer for data, so double it */
	qp->infofree += qp->infoalloc;
	qp->infoalloc += qp->infoalloc;
	newbuf = realloc(qp->infobuffer, qp->infoalloc);
	if (!newbuf) {
	    sprintf(this->errmsg,
		"info_update: could not realloc infobuffer for %s\n",qp->name);
	    uwho_printErr(this);
	    return -1;
	} 
	qp->infobuffer = newbuf;
    }
    assert(qp->infofree >= datalen);
    strncat(qp->infobuffer, data, MIN(datalen,qp->infofree));
    qp->infofree-=datalen;
    return 0;
}

/*--------------------------------------------------------------------------
 Terminate any still-live queries.
 As each response is completed, call doneFn with a pointer to the completed
 query.
--------------------------------------------------------------------------*/
void uwho_closeQueries(this, doneFn)
    uwho_t *this;
    uwho_doneFn_t doneFn;
{
    int i;
    uwho_query_t *qp;

    for (i=this->nqueries, qp=this->queries; i>0; i--, qp++) {
	if (qp->state!=uwho_STATE_CLOSED) {
	    char msg[100];
	    strcpy(msg,"\n*** timed out ***\n");
	    (void) info_update(this, qp, msg);
	    netclose(qp->fds); this->n_open--;
	    qp->state=uwho_STATE_CLOSED; 
	    if (NULL != doneFn) (*doneFn)(qp);
	}
    }
}

/*--------------------------------------------------------------------------
 Delete all the queries in a uwho_t.
 Prepares for reuse of uwho_t.
--------------------------------------------------------------------------*/
void uwho_deleteQueries(this)
    uwho_t *this;
{
    int i;
    uwho_query_t *qp;

    uwho_closeQueries(this, NULL);
    for (i=this->nqueries, qp=this->queries; i>0; i--, qp++) {
	free(qp->query);
	if (qp->infobuffer != NULL)
	    free(qp->infobuffer);
    }

    /* Clear state variables. */
    this->nqueries = 0;
    this->n_open = 0;
    this->Max_FD = -1;
    this->errmsg[0] = 0;
}

/*--------------------------------------------------------------------------
 Destroy a uwho_t.
--------------------------------------------------------------------------*/
void uwho_destroy(this)
    uwho_t *this;
{
    uwho_deleteQueries(this);
    if (this->fp) fclose(this->fp);
    free(this);
}

/*--------------------------------------------------------------------------
 Init a uwho_t.
 Returns NULL if unable to- either because data file does not exist,
 or system is out of RAM.
--------------------------------------------------------------------------*/
uwho_t *uwho_create(def_host, def_proto, def_dir, errFn)
    char *def_host;
    int  def_proto;
    char *def_dir;
    uwho_errFn_t errFn;		/* error callback */
{
    uwho_t *this;

    this = (uwho_t *)malloc(sizeof(uwho_t));
    if (!this) return NULL;

    /* Set up default values for all global options. */
    this->def_host[0] = '\0';
    this->uwho_dir[0] = '\0';
    if(def_host) strcpy(this->def_host, def_host);
    if(def_dir) strcpy(this->uwho_dir, def_dir);
    this->def_host_protocol = def_proto;
    this->errFn = errFn;
    this->Literal = 0;

    /* Clear state variables. */
    this->nqueries = 0;
    this->n_open = 0;
    this->Max_FD = -1;
    this->errmsg[0] = 0;

    this->whois_port= my_getservbyname("whois");
    this->ph_port   = my_getservbyname("csnet-ns");
    this->kis_port  = my_getservbyname("remote-kis");

    uwho_getfilename(this, this->fname, uwho_PROTO_ALL, 1);

    /* this->fp is solely for uwho_AddApproxQuery().  Delay
     * error message until fp referenced.
     */
    this->fp=fopen(this->fname, "r");

    return this;
}

/*--------------------------------------------------------------------------
 Perform any hoohah that has to happen before the first call to uwho_poll.
--------------------------------------------------------------------------*/
void uwho_poll_setup(this)
    uwho_t *this;
{
#ifdef WOLLENGONG
    /* delay 1 second to allow connect to work on sockets */
    sleep(1);
#endif
}

/*--------------------------------------------------------------------------
 Poll the queries in the uwho_t; perform any needed I/O, and update
 the queries' state variable.  Block for at most the time specified in
 *timeval.

 Upon entry, this->queries[] contains this->nqueries queries; this->n_open 
 of them have an open socket to the proper server to handle the query (the 
 rest are already defunct).
 Send the queries and collect the responses.
 As each response is completed, call doneFn with a pointer to the completed
 query.

 Return the number of queries still open, or -1 upon error.
--------------------------------------------------------------------------*/
int uwho_poll(this, timeout, doneFn)
    uwho_t *this;
    struct timeval *timeout;
    uwho_doneFn_t doneFn;
{
    int i;
    int nready;
    fd_set readfds, writefds;
    uwho_query_t *qp;

    /* Wait for some fd's to be ready for writing, some for reading */
    FD_ZERO (&readfds);
    FD_ZERO (&writefds);
    for (i=this->nqueries, qp=this->queries; i>0; i--, qp++) {
	switch (qp->state) {
	    case uwho_STATE_CONNECTING:
		FD_SET (qp->fds, &writefds); break;
	    case uwho_STATE_READING:
		FD_SET (qp->fds, &readfds); break;
	}
    }
    nready = select(this->Max_FD+1, &readfds, &writefds, NULL, timeout);

#ifdef WOLLENGONG
    /* ignore the following errors */
    if ((-1==nready)&&((ENOTCONN == neterrno) || (EWOULDBLOCK == neterrno )))
    ;
    else 
#endif
    if (nready == -1) {
	strcpy(this->errmsg, "select");
	uwho_printErr(this);
	return -1;
    } else if (nready == 0)
	return 0;

    /* Handle I/O for sockets which are ready */
    for (i=this->nqueries, qp=this->queries; i>0; i--, qp++) {
	int n, len;

	if (qp->state == uwho_STATE_CONNECTING) {
	    if (FD_ISSET (qp->fds, &writefds)) {
		len = strlen(qp->query);
		n = netwrite(qp->fds, qp->query, len);
		if (n== strlen(qp->query)) {
		    qp->state = uwho_STATE_READING;
		} else {
#ifdef PTK40
		    if (n == -1 && tk_geterrno(qp->fds) == EPIPE) {
#else
		    if (n == -1 && EPIPE == neterrno) {
#endif
			char msg[100];
			strcpy(msg,"\n*** connection refused ***\n");
			/* early EOF- background connect was refused */
			(void) info_update(this, qp, msg);
		    } else {
			char msg[100];
			strcpy(msg, "\n*** unknown error during write ***\n");
			(void) info_update(this, qp, msg);
		    }
		    netclose(qp->fds); this->n_open--;
		    qp->state = uwho_STATE_CLOSED;
		    if (NULL != doneFn) (*doneFn)(qp);
		}
	    }
	} else if (qp->state == uwho_STATE_READING) {
	    if (FD_ISSET (qp->fds, &readfds)) {
		char result[BLOCKSIZE];
		int n, fail=0;

		result[0] = '\0';
		n = netread(qp->fds, result, BLOCKSIZE-1);
		if (n > 0) {
		    result[n] = '\0';
		    /* do special checking to see if info_update failed */
		    fail = info_update(this, qp, result);
		}
		if (n == 0 || fail) { 
		    netclose(qp->fds); this->n_open--;
		    qp->state = uwho_STATE_CLOSED;
		    if (NULL != doneFn) (*doneFn)(qp);
		    if(!fail) qp->infostate=1;
		    /* qp->infostate == 1 means info buffer is good */
		}
	    }
	}
    }
    return this->n_open;
}

/*--------------------------------------------------------------------------
 moves file to old and then temp to file.
 Called by uwho_update.
 Returns zero upon success, nonzero & error in this->errmsg upon error.
--------------------------------------------------------------------------*/
static int rename3(this,temp,file,old)
    uwho_t *this;
    char *temp, *file, *old;
{
    if (access(temp,W_OK|R_OK)) {
	sprintf(this->errmsg,"%s access error",temp);
	uwho_printErr(this);
	/* perror("could not be accessed"); */
	return(-1);
    }

    if (access(file,R_OK|W_OK)) {
	if (errno!=ENOENT) {
	    sprintf(this->errmsg,"%s access error",file);
	    uwho_printErr(this);
	    return -1;
	} else {
	    sprintf(this->errmsg,"warning: %s does not exist;\
 creating one...",file);
	    uwho_printErr(this);
	}
    }
    else {
	if (access(old,R_OK|W_OK)) {
	    if (errno!=ENOENT) {
		sprintf(this->errmsg,"%s access error",old);
		uwho_printErr(this);
		return -1;
	    }
	}
	else if (remove(old)) {
	    sprintf(this->errmsg,"%s could not be removed",old);
	    uwho_printErr(this);
	    return -1;
	}
	if (rename(file, old)) {
	    sprintf(this->errmsg,"could not rename %s to %s",file,old);
	    uwho_printErr(this);
	}
    }

    if (access(file,W_OK|R_OK)) {
	if (errno!=ENOENT) {
	    sprintf(this->errmsg,"bug: %s: fatal access error!",file);
	    /* perror("fatal access error! report to authors"); */
	    uwho_printErr(this);
	    return -1;
	}
    }
    if (rename(temp,file)) {
	sprintf(this->errmsg,"could not rename %s to %s",temp,file);
	uwho_printErr(this);
	return -1;
    }
    return 0;
}

#define ADDCOUNTRY
/*--------------------------------------------------------------------------
 Given a site description and hostname,
 check to see if the description has a country code.
 If it does not, deduce one from the hostname, and append it to the 
 description.
--------------------------------------------------------------------------*/
#ifdef ADDCOUNTRY
static void addcountry(descrip, hostname)
    char *descrip;
    char *hostname;
{
    char *p;
    char domain[16];
    char country[3];

    p = strrchr(descrip, '=');
    if (p != NULL && p != descrip && (p[-1]=='c' || p[-1]=='C'))
	return;		/* country code already exists */

    /* Grab last component of hostname, turn it into country code
     * if possible.
     */
    p = strrchr(hostname, '.');
    if (p == NULL || !*++p)
	return;
    strnlower(domain, p, sizeof(domain)-1);

    if (strcmp(domain, "edu")==0
	|| strcmp(domain, "com")==0
	|| strcmp(domain, "mil")==0
	|| strcmp(domain, "gov")==0)
	    strcpy(country, "us");		/* some canadians, too? sigh */
    else if (strcmp(domain, "uk")==0)
	    strcpy(country, "gb");		/* historical accident? */
    else if (strcmp(domain, "net")==0
	|| strcmp(domain, "org")==0
	|| strlen(domain) > 2)			/* bad country? */
	    strcpy(country, "??");		/* no country? */
    else
	    strcpy(country, domain);

    strcat(descrip, " C=");
    strcat(descrip, country);
}
#endif


/*--------------------------------------------------------------------------
 Get a new copy of the whois / ph server lists from the list servers;
 servernames[uwho_PROTO_WHOIS] and servernames[uwho_PROTO_PH] are taken
 as the names of the servers to use. 
 Updates uwho.dat file.
 Returns zero upon success, nonzero on error;

 Not great.  Shouldn't take control for so long.  Might require rewrite for
 GUI versions.  Leaves a mess in the data directory on rename3 failures.
--------------------------------------------------------------------------*/
#define TEMPCLEAN	{ \
if (dat_outfile) fclose(dat_outfile);	\
if (outfile[0])  fclose(outfile[0]);	\
if (outfile[1])  fclose(outfile[1]);	\
if (dat_temp[0]) remove(dat_temp);	\
if (temp[0][0])  remove(temp[0]);	\
if (temp[1][0])  remove(temp[1]); }

int uwho_update(this, Timeout, servernames)
    uwho_t *this;
    int Timeout;		/* # of seconds to wait */
    char **servernames;
{
    FILE *outfile[2], *dat_outfile;
    char dat_temp[MAXSTR], temp[2][MAXSTR];
    char dat_file[MAXSTR], file[2][MAXSTR];
    char dat_old[MAXSTR], old[2][MAXSTR];
    char  *p, *begin, *end;
    uwho_query_t *qp;
    struct timeval timeout;
    long time0, now;
    int i;

    outfile[0]=outfile[1]=dat_outfile = NULL;
    temp[0][0]=temp[1][0]=dat_temp[0] = (char)0;
    assert(servernames[uwho_PROTO_PH] != NULL);
    assert(servernames[uwho_PROTO_WHOIS] != NULL);

    /* Enqueue the queries to the list servers. */
    uwho_AddExactQuery(this, uwho_PROTO_PH, "ns-servers", 
	servernames[uwho_PROTO_PH], "ph List Server");
    uwho_AddExactQuery(this, uwho_PROTO_WHOIS, "whois-servers",
	 servernames[uwho_PROTO_WHOIS], "whois List Server");

    uwho_poll_setup(this);

    time(&time0);
    while (this->n_open > 0 && ((time(&now)<time0+Timeout) || !Timeout)) {
	timeout.tv_sec = Timeout + time0 - now;
	timeout.tv_usec = 0;
	uwho_poll(this, &timeout, NULL);
    }
    /* uwho_closeQueries(this, NULL);  Don't close them!  Let them close
     * by themselves!  Otherwise we can't tell if query completed.
     */

    uwho_getfilename(this,dat_file,uwho_PROTO_ALL,1);
    uwho_getfilename(this,dat_old,uwho_PROTO_ALL,2);
    if (uwho_getfilename(this,dat_temp,uwho_PROTO_ALL,0)) {
	sprintf(this->errmsg,"could not get temp name");
	uwho_printErr(this);
	return -1;
    }
    if (!(dat_outfile=fopen(dat_temp,"w"))) {
	sprintf(this->errmsg, "could not create %s",dat_temp);
	uwho_printErr(this);
	return -1;
    }
    fwrite(uwho_OTHER_SERVERS, sizeof(char), strlen(uwho_OTHER_SERVERS),dat_outfile);

    for (i=0, qp=this->queries; i<this->nqueries; i++, qp++) {
	char name[MAXHOSTLEN], descrip[MAXHOSTLEN];
	long ti;
	char *cti;

	/* Make sure uwho_poll() detected no errors on this query (qp->infostate == 1) */
	if (uwho_STATE_CLOSED != qp->state || !qp->infostate || !qp->infobuffer) {
	    sprintf(this->errmsg,
		 "Error in update: query did not complete successfully!\n\
qp->state=%d qp->infostate=%d qp->infobuffer=%ld ifree=%d ialloc=%d\n",
		 qp->state, qp->infostate, (long)qp->infobuffer, qp->infofree,
		 qp->infoalloc);
	    uwho_printErr(this);
	    if(qp->infobuffer) {
		my_strncpy(this->errmsg,qp->infobuffer,MAXSTR);
		uwho_printErr(this);
	    }
	    TEMPCLEAN
	    return -1;
	}

	uwho_getfilename(this,file[i],qp->protocol,1);
	uwho_getfilename(this,old[i],qp->protocol,2);
	if (uwho_getfilename(this,temp[i],qp->protocol,0)) {
	    sprintf(this->errmsg,"could not get temp name");
	    uwho_printErr(this);
	    TEMPCLEAN
	    return -1;
	}
	if (!(outfile[i]=fopen(temp[i],"w"))) {
	    sprintf(this->errmsg, "could not create %s",temp[i]);
	    uwho_printErr(this);
	    TEMPCLEAN
	    return -1;
	}
	time(&ti);
	cti = ctime(&ti);
	fprintf(outfile[i], "; Copied from %s on %s",qp->name,cti);
	name[0]=descrip[0]='\0';

	p=malloc((strlen(qp->infobuffer)+1) * sizeof(char));
	if (!p) {
	    strcpy(this->errmsg, "can't malloc space for uwho_update");
	    uwho_printErr(this);
	    TEMPCLEAN
	    return -1;
	}
	if (!*(qp->infobuffer) || !qp->infobuffer) continue;
	strcpy(p,qp->infobuffer);

	/* split up infobuffer line by line */
	for (begin=p; begin; begin=end) {
	    char *temp;
	    if (!*begin) break;		/* nothing left (not even a \n) */
	    end=strchr(begin,'\n');
	    if (end) *end++='\0';	/* mark end of line */
	    switch(qp->protocol) {
		case uwho_PROTO_PH:
		    if (*begin=='-') {
			if ((temp=strstr(begin,"site:"))) {
			    my_strncpy(descrip, temp+5, MAXHOSTLEN);
			    /* descrip comes in before name */
			} else if ((temp=strstr(begin,"server:"))) {
			    my_strncpy(name, temp+7, MAXHOSTLEN);
			    fwrite("ph\t", sizeof(char), 3, dat_outfile);
			    fwrite(name,sizeof(char),strlen(name), outfile[i]);
			    fwrite(name,sizeof(char),strlen(name), dat_outfile);
			    fputc('\t',outfile[i]);
			    fputc('\t',dat_outfile);
#ifdef ADDCOUNTRY
			    addcountry(descrip, name);
#endif
			    fwrite(descrip, sizeof(char), strlen(descrip),
				outfile[i]);
			    fwrite(descrip, sizeof(char), strlen(descrip),
				dat_outfile);
			    fputc('\n', outfile[i]);
			    fputc('\n', dat_outfile);
			    descrip[0]='\0';
			}
		    }
		    break;
		case uwho_PROTO_WHOIS:
		    if (*begin) {
			fwrite(begin, sizeof(char), strlen(begin), outfile[i]);
			fputc('\n', outfile[i]);
		    }
		    if (*begin && *begin!=';') {
			fwrite("whois\t", sizeof(char), 6, dat_outfile);
			fwrite(begin, sizeof(char), strlen(begin), dat_outfile);
			fputc('\n', dat_outfile);
		    }
		    break;
		default: break;
	    }
	}
	free(p);
	fclose(outfile[i]);
    }
    fclose(dat_outfile);

    /* Delete old files; rename temporaries to permanant files. */
    ignore_kills(1);		/* ignore ctrl-c's */

    /* move dat_file to dat_old and dat_temp to dat_file */
    if(rename3(this,dat_temp,dat_file,dat_old)) remove(dat_temp);

    for (i=0; i<this->nqueries; i++) {
	if(rename3(this, temp[i], file[i], old[i])) remove(temp[i]);
    }

    ignore_kills(0);		/* restore default */

    return 0;
}
