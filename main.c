/*--------------------------------------------------------------------------
 uwho - a program which knows how to look up e-mail addresses.
--------------------------------------------------------------------------*/

static char sccsid[] = "%W% %G%";

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

#include <assert.h>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef WIN32
#define popen _popen
#define pclose _pclose
#endif

#ifdef VMS
/* cc under vms has hard time allowing multiple defines, so stick them
 * here; user should edit vmsdef.h.
 */
#include "vmsdef.h"
#endif

#include "mystring.h"
#include "tcpip.h"
#include "uwho.h"

#define VERSION sccsid

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

/* VMS/Unix/MS-DOS commandline version of uwho */

/* Configurable Parameters
 * These can also be set via the commandline.
 */
#ifndef UWHO_DIR
#define UWHO_DIR "/etc"
#endif

#ifndef DEF_HOST
#define DEF_HOST "horton.jpl.nasa.gov"
#endif
#ifndef DEF_HOST_PROTOCOL
#define DEF_HOST_PROTOCOL uwho_PROTO_PH
#endif

#define MAXSTR 1024

/* Global variables */
uwho_t *callback_uwho;
char callback_eol[3] = "\n";
static int Literal = 0;
static int Verbose = 0;
static int Time_Out = 15;
static int be_whois_server = 0;
static char uwho_dir[MAXSTR] = UWHO_DIR;
static char def_host[MAXSTR] = DEF_HOST;
static int def_host_protocol = DEF_HOST_PROTOCOL;
static char *update_hostnames[2];
static char Program[MAXSTR] = "";
static char _tmp[MAXSTR];

#ifdef POPEN_PAGER
static FILE *Pager = stdout;
static char Pagername[MAXSTR] = "more";
#define PAGER_START                                             \
if (UsePager) {                                                 \
    if ((Pager=popen(Pagername,"w")) == NULL) {                  \
	fprintf(stderr,"uwho: PAGER_START: popen() failed\n");  \
	Pager = stdout;                                         \
    }                                                           \
}
#define PAGER_END if (Pager!=stdout) {pclose(Pager); Pager=stdout;}
#define PAGER_PUTS(s) { fputs(s, Pager); fwrite(callback_eol, 1, strlen(callback_eol), Pager); }
#define PAGER_FLUSH fflush(Pager);
#define PAGER_POLL(done) done
#endif

#ifdef DUMB_PAGER
#include "pg.h"
pg_t *pg;
/* for now, don't know size of screen, and assume user wants
 * behavior like the "more" pager rather than the "less" pager.
 */
#define PAGER_START if (UsePager) pg = pg_create(24, 80, 1);
#define PAGER_PUTS(s) { if (pg) { pg_puts(pg, s); } else \
 { fputs(s, stdout); fwrite(callback_eol, 1, strlen(callback_eol), stdout); } }
#define PAGER_FLUSH 
#define PAGER_POLL(done) (pg ? pg_poll(pg, done) : done)
#define PAGER_END if (pg) { while (!PAGER_POLL(1)) ; pg_destroy(pg); }
#endif

#ifdef NO_PAGER
#define PAGER_START
#define PAGER_END
#define PAGER_PUTS(s) { fputs(s, stdout); fwrite(callback_eol, 1, strlen(callback_eol), stdout); }
#define PAGER_FLUSH fflush(stdout);
#define PAGER_POLL(done) done
#else
static int UsePager = 0;
#endif

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
#ifdef NO_SIGPIPE
#define ignore_writeEOF(x)
#else
#define ignore_writeEOF(x) signal(SIGPIPE,x?SIG_IGN:SIG_DFL);
#endif
#else
#define ignore_kills(x)
#define ignore_writeEOF(x)
#endif

/*--------------------------------------------------------------------------
 Callback routine to print out a uwho-generated message.
--------------------------------------------------------------------------*/
void printErr(this)
    uwho_t *this;
{
    fprintf(stderr, "uwho: %s\n", this->errmsg);
}

/*--------------------------------------------------------------------------
 Copy the given file to the terminal.
--------------------------------------------------------------------------*/
static int do_cat(filename)
    char *filename;
{
    FILE *ifile;
    char linebuffer[MAXSTR];
    int lines;

    if ((ifile=fopen(filename, "r"))) {
	lines = 0;
	while (fgets(linebuffer, sizeof(linebuffer),ifile)) {
	    /* delete end of line char */
	    linebuffer[strlen(linebuffer)-1] = 0;
	    PAGER_PUTS(linebuffer);
	    if (lines++ == 8) {
		/* Only poll every 8th line- polling is slow sometimes */
		lines = 0;
		if (PAGER_POLL(0)) break;
	    }
	}
	PAGER_PUTS("");
	fclose(ifile);
	return 0;
    } 

    sprintf(_tmp,"Could not open %s", filename);
    PAGER_PUTS(_tmp);
    return 1;
}

/*--------------------------------------------------------------------------
 Print a list of available organizations and servers.
--------------------------------------------------------------------------*/
static void do_list(uwho)
    uwho_t *uwho;
{
    char  filename[MAXSTR];

    uwho_getfilename(uwho, filename, uwho_PROTO_PH, 1);
    PAGER_PUTS("---- Ph Servers ----");
    (void) do_cat(filename);

    uwho_getfilename(uwho, filename, uwho_PROTO_WHOIS, 1);
    PAGER_PUTS("---- Whois Servers ----");
    (void) do_cat(filename);

    PAGER_PUTS("---- Other Servers ----");
    /* Kludge: should parse into lines? */
    PAGER_PUTS(uwho_OTHER_SERVERS);
}

void usage()
{
    fprintf(stderr, "\
Usage: %s [-options] lastname[, firstname][@organization] [ , lastname...]\n\
Prints out information about the given person(s).\n\
If no name is given, prompt user for one.\n\
", Program);

    fprintf(stderr, "\
User options:\n\
  -help         Print this information.\n\
  -p            Prompt user before each page of output.\n\
  -t t_out      Set the time out (for no time out use -t 0).\n\
  -verbose      Print extra info about progress.\n\
  -list         List all known information servers.\n\
Wizard options:\n\
  -literal      Query is a command rather than a name, don't reformat it.\n\
  -h host prot  Set the default host and protocol.\n\
  -dir def_dir  Set the data directory where uwho's server lists are stored.\n\
  -update whois-host ph-host   Update the server lists from the given servers.\n\
  -whoisd       In interactive mode, act like a whois server.\n\
");
}

void help()
{
    fprintf(stderr, "%s\n", VERSION);
    usage();
    fprintf(stderr, "\n\
Given a query of the form\n\
  lastname, firstname @ organization\n\
%s queries the given organization's server(s) about the given person.\n\
The organization name may be ambiguous, e.g.\n\
   houde @ tech\n\
will search for all people named Houde at any organization with 'tech'\n\
   houde @ c=de\n\
would search for all people named Houde in Germany.\n\
\n\
If several words are given in the organization name, all must match, i.e.\n\
   horn@tech c=us\n\
would look for people named Horn at organizations named ...tech... in the US.\n\
", Program);
    fprintf(stderr, "\n\
Caveat: the exact way in which your query is handled depends on the server.\n\
Some servers may not support searching by first name, and output format.\n\
will vary from server to server.\n\
");
}


/*--------------------------------------------------------------------------
 Given a command/query, fill hostlist[] with all matching hosts, and
 return number of matching hosts.
 Embedded " , " strings separate queries to be executed in parallel.
 Print error messages to pager.
--------------------------------------------------------------------------*/
int gethosts(this, command)
    uwho_t *this;
    char *command;
{
    char user[MAXSTR], org[MAXSTR];
    char *begin, *end, *at, *p;
    int miss=0;
    int n;

    /* Queries are separated by " , ". Find each query. */
    user[0]=org[0]='\0';
    for (begin=command; begin; begin=end) {
	if (!*begin) break; /* nothing left (not even a \n) */

	/* Skip initial spaces and commas */
	while (*begin && (isspace(*begin) || (*begin == ',')))
	    *begin++ = 0;
	end=strstr(begin, " , ");
	if (end) *end++='\0';		/* mark end of command */

	/* Put part before @ into user */
	at=strchr(begin,'@');
	if (at) *at++='\0';		/* mark end of user */

	/* nuke trailing spaces */
	p = at ? at-1 : (end ? end-1 : (begin + strlen(begin)));
	while (p != begin && isspace(p[-1]))
	    *--p = 0;

	if (!*begin) {
	    if (!*user) { 
		PAGER_PUTS("No user specified!");
		/* usage(); exit(0); */
		continue;
	    }
	} else {
	    /* look out! this statement is paired with !*begin, NOT !user */
	    my_strncpy(user,begin,MAXSTR);
	}

	/* Put part after @ into org */
	if (!at) at="";  /* KLUDGE!!!! (valiantly avoid seg fault) */

	/* Skip initial spaces and commas */
	for (p=at; *p && (isspace(*p) || *p == ','); *p++ = '\0')
	    ;
	strnlower(org, p, MAXSTR-1);
	if (Verbose) {
	    sprintf(_tmp,"Starting query for %s@%s\n",user, org);
	    PAGER_PUTS(_tmp);
	    PAGER_FLUSH;
	}
	n = uwho_AddApproxQuery(this, org, user);
	if (n == 0 && *org) {
	    sprintf(_tmp,"Your search for \"%s\" at \"%s\" matched no organizations.",user,org?org:"default server");
	    PAGER_PUTS(_tmp);
	    miss=1;
	}
	if (this->nqueries >= uwho_MAXQUERIES) break;
    }

    return (this->nqueries);
}

/*--------------------------------------------------------------------------
 Parse the given commandline.
 Return an int holding one of the CMD_ constants;
 place a pointer to a malloc'd string holding query text in *queryp.

 If a bad option is recognized, cli_parse() prints error message and aborts.
--------------------------------------------------------------------------*/
#define CMD_NONE 1
#define CMD_UPDATE 2
#define CMD_HELP 3
#define CMD_LIST 4
#define CMD_QUERY 5

int cli_parse(nargc, nargv, queryp)
    int nargc;
    char **nargv;
    char **queryp;
{
    char *cp, *query, *newbuf;
    int len=0, nowsize;
    int cmd=CMD_NONE;

    if (!(query=(char *)malloc(MAXSTR*sizeof(char)))) {
	fprintf(stderr,"uwho: parse: could not malloc query\n");
	exit(-1);
    }

    nowsize=MAXSTR;
    *query='\0';
    while (--nargc>0) {
	cp = *++nargv;
	assert(cp);
	if ((*cp == '-' || *cp == '/') && cp[1]) {
	    cp++;
	    if (strncmp(cp,"list",4)==0) {
		cmd = CMD_LIST;
	    }
	    else if (strcmp(cp,"update")==0) {
		cmd = CMD_UPDATE;
		if ((nargc -= 2) <= 0) {
		    fprintf(stderr,"uwho: arg missing; syntax: -update whois-list-server ph-list-server\n");
		    usage(); exit(-1);
		}
		/* blech!  Globals!  Wish I had time to rewrite. */
		update_hostnames[0] = *++nargv;
		update_hostnames[1] = *++nargv;
	    }
	    else if (strcmp(cp,"help")==0) {
		cmd = CMD_HELP;
	    }
	    else if (strcmp(cp,"h")==0) {
		if (--nargc < 2) {
		    fprintf(stderr,"uwho: arg missing; syntax: -h hostname kis|ph|whois\n");
		    usage(); exit(-1);
		}
		my_strncpy(def_host, cp = *++nargv, MAXHOSTLEN-1);
		nargc--;
		if (strcmp(cp = *++nargv, "kis")==0) def_host_protocol=uwho_PROTO_KIS;
		else if (strcmp(cp, "ph")==0) def_host_protocol=uwho_PROTO_PH;
		else if (strcmp(cp, "whois")==0) def_host_protocol=uwho_PROTO_WHOIS;
		else {
		    fprintf(stderr,"uwho: arg \"%s\" invalid; syntax: -h hostname ph|whois|kis\n",cp);
		    usage(); exit(-1);
		}
	    }
	    else if (strcmp(cp,"dir")==0) {
		if (--nargc < 1) {
		    fprintf(stderr,"uwho: arg missing; syntax: -dir default_dir\n");
		    usage(); exit(-1);
		}
		my_strncpy(uwho_dir, cp = *++nargv, MAXSTR);
	    }
	    else if (strcmp(cp,"whoisd")==0) {
		be_whois_server = 1;
	    }
	    else if (strcmp(cp,"verbose")==0) {
		Verbose = 1;
	    }
	    else if (strcmp(cp,"literal")==0) {
		Literal = 1;
	    }
	    else if (strcmp(cp,"p")==0) {
#ifdef POPEN_PAGER
		char *p;
		p = getenv("PAGER");
		if (p != NULL)
		    strcpy(Pagername, p);
#endif
#ifdef NO_PAGER
		fprintf(stderr, "uwho: sorry, pager not supported.\n");
#else
		if (isatty(fileno(stdout)) && isatty(fileno(stdin)))
		    UsePager = 1;
#endif
	    }
	    else if (strcmp(cp,"t")==0) {
		if (--nargc < 1){
		    fprintf(stderr,"uwho: arg missing; syntax: -t timeout\n");
		    usage(); exit(-1);
		}
		Time_Out = atoi(cp = *++nargv);
	    }
	    else {
		fprintf(stderr,"uwho: Invalid flag: -%s\n",cp);
		usage(); exit(-1);
	    }
	}
	else { 
	    /* Found a non-flag argument; query will be built with all words
	     * from here to 1st trailing flag argument (this is a kludge).
	     * Concatenate all these words into string 'query'.
	     */
	    cmd = CMD_QUERY;
	    for (; nargc; nargc--, nargv++) {
		if (**nargv=='-') {
		    nargc++; nargv--;
		    break; /* skip nargv - it's a flag */
		    /* quick fix to allow flags after command */
		}
		if ((len + strlen(*nargv)) >= nowsize) {
		    /* allow unlimited command line length for big jobs */
		    if (!(newbuf=(char *)realloc(query,(nowsize*=2)*sizeof(char)))){
			fprintf(stderr, "uwho: cli_parse: could not realloc query\n");
			continue;
		    }
		    query=newbuf;
		}
		if (len) strcat(query, " ");
		strcat(query, *nargv);
		len=strlen(query);
	    }
	}
    }

    /* printf("cli_parse: using uwho_dir: %s%s\n",uwho_dir, DIR_SEPARATOR); */

    *queryp = query;
    return cmd;
}

/*--------------------------------------------------------------------------
 Callback function to print a query result.
--------------------------------------------------------------------------*/
void printQuery(qp)
    uwho_query_t *qp;
{
    sprintf(_tmp,"------ %s %s ------%s",qp->name,qp->descrip, callback_eol);
    PAGER_PUTS(_tmp);
    if (Verbose)
	sprintf(_tmp,"---query---%s%s-----------%s",qp->query, 
	    callback_eol,callback_eol);

    if (!qp->infobuffer) return;

    {
	char *begin, *end, *p, *q;
	int entry, old_entry=1;
	int lines=0;

	if (!(p=(char *)malloc((strlen(qp->infobuffer)+1) * sizeof(char))) ) {
	    sprintf(_tmp,"uwho: can't malloc space for printQuery%s", 
		callback_eol);
	    return;
	}
	strcpy(p,qp->infobuffer);

	/* split up infobuffer line by line */
	for (begin=p; begin; begin=end) {
	    if (!*begin) break;    /* nothing there */
	    end=strchr(begin,'\n');
	    if (end) *end++='\0';		/* mark end of line */

	    if (qp->protocol==uwho_PROTO_PH) {

		if (*begin!='-') continue;	/* not a data line */

		 /* advance to first colon separator */
		q=strchr(begin,':');
		if (q) {
		    q++;
		    /* compare number after first colon with old_entry */
		    if ((entry=atoi(q)) != old_entry) {
			/* add a line feed if we are on a new entry */
			PAGER_PUTS("");
			old_entry=entry;
		    }
		    PAGER_PUTS(q);
		}
	    } else 
		PAGER_PUTS(begin);

	    if (lines++ == 8) {
		/* Only poll every 8th line- polling is slow sometimes */
		lines = 0;
		PAGER_POLL(0);
	    }
	}
	free(p);
	PAGER_PUTS("");
    } 

    if (Verbose) {
	if (callback_uwho->n_open > 0) {
	    sprintf(_tmp,"%d quer%s pending...%s",
		 callback_uwho->n_open,
		 ((callback_uwho->n_open > 1) ? "ies" : "y"),
		 callback_eol);
	    PAGER_PUTS(_tmp);
	}
    }
    PAGER_POLL(0);

    PAGER_FLUSH;
}

/*--------------------------------------------------------------------------
 Process a query.
 Print result to console.
 Return 0 upon success, 1 upon failure.
--------------------------------------------------------------------------*/
int do_query(uwho, command)
    uwho_t *uwho;
    char *command;
{
    int nhosts;
    struct timeval timeout;
    long time0, now;
    char filename[MAXSTR];
    int quit = 0;

    if (my_strncasecmp(command, "ns-servers", 10) ==0) {
	uwho_getfilename(uwho, filename, uwho_PROTO_PH, 1);
	return do_cat(filename);
    } else if (my_strncasecmp(command, "whois-servers", 10) ==0) {
	uwho_getfilename(uwho, filename, uwho_PROTO_WHOIS, 1);
	return do_cat(filename);
    }

    nhosts=gethosts(uwho, command);
    if (nhosts <= 0) {
	PAGER_PUTS("Sorry, no hosts selected!");
	PAGER_PUTS("The organization(s) you are looking for might not have a whois or ph server.");
	PAGER_PUTS("Type 'list' or 'uwho -list' for a list of organizations with whois or ph servers.");
	uwho_deleteQueries(uwho);
	return 1;
    } else if (nhosts >= uwho_MAXQUERIES)
	PAGER_PUTS("Too many hosts matched your query- truncating.");

    sprintf(_tmp,"%d quer%s pending...", nhosts, ((nhosts>1) ? "ies":"y"));
    PAGER_PUTS(_tmp);
    PAGER_FLUSH;

#if 0
/* gotta generate these somehow */
	if (Fatal_Socket_err)
	    fprintf(errfile,"uwho: can't open any more sockets.\n");
	fprintf(errfile,
	    "uwho: only %d of %d servers were opened\n",result,nhosts);
#endif

    uwho_poll_setup(uwho);

    time(&time0);
    while (uwho->n_open > 0 && ((time(&now)<time0+Time_Out) || !Time_Out)) {
	timeout.tv_sec = 1;
	/* timeout.tv_sec = Time_Out + time0 - now;  appopriate but for pg */
	timeout.tv_usec = 0;
	uwho_poll(uwho, &timeout, printQuery);
	quit = PAGER_POLL(0);
	if (quit) break;
    }
    uwho_closeQueries(uwho, printQuery);
    uwho_deleteQueries(uwho);
    return 0;
}

/*--------------------------------------------------------------------------
 Set/Display various global options
 Print result to stdout.
 called by main()
--------------------------------------------------------------------------*/
void uwho_set(uwho, args)
    uwho_t *uwho;
    char *args;
{
    char *arg2;

    if (!args) args="";  /* avoid seg fault */
    if (!*args) {
#ifndef NO_PAGER
	fprintf(stdout,"\tPager: %s\n",UsePager?"ON":"OFF");
#endif
	fprintf(stdout,"\tVerbose: %s\n",Verbose?"ON":"OFF");
	fprintf(stdout,"\tLiteral: %s\n",Literal?"ON":"OFF");
	fprintf(stdout,"\tTime out: %d\n",Time_Out);
	fprintf(stdout,"\tData dir: %s\n",uwho_dir);
	fprintf(stdout,"\tDefault host: %s %s\n",def_host,
	    def_host_protocol==uwho_PROTO_KIS?"KIS":
	    def_host_protocol==uwho_PROTO_PH?"ph":
	    def_host_protocol==uwho_PROTO_WHOIS?"whois":
	    "bug: unknown");
	return;
    }

    /* have arg2 point to second argument in args */    
    arg2=strpbrk(args, " \f\n\r\t\v");
    if (arg2)
	for ( ; isspace(*arg2) && *arg2; arg2++);

    if (my_strncasecmp(args, "literal", 7)==0) {
	if (arg2) {
	    if (my_strncasecmp(arg2, "on", 2)==0) Literal=1;
	    else if (my_strncasecmp(arg2, "off", 2)==0) Literal=0;
	    else if (*arg2) fprintf(stdout, "Syntax error: \"%s\"\n",arg2);
	}
	fprintf(stdout,"Literal: %s\n",Literal?"ON":"OFF");
	uwho->Literal = Literal;
    } else if (my_strncasecmp(args, "verbose", 7)==0) {
	if (arg2) {
	    if (my_strncasecmp(arg2, "on", 2)==0) Verbose=1;
	    else if (my_strncasecmp(arg2, "off", 2)==0) Verbose=0;
	    else if (*arg2) fprintf(stdout, "Syntax error: \"%s\"\n",arg2);
	}
	fprintf(stdout,"Verbose: %s\n",Verbose?"ON":"OFF");
    }
#ifndef NO_PAGER
    else if (my_strncasecmp(args, "pager", 5)==0) {
	if (arg2) {
	    if (my_strncasecmp(arg2, "on", 2)==0) {
		UsePager=1;
	    }
	    else if (my_strncasecmp(arg2, "off", 2)==0) UsePager=0;
	    else if (*arg2) fprintf(stdout, "Syntax error: \"%s\"\n",arg2);
	}
	fprintf(stdout,"Pager: %s\n",UsePager?"ON":"OFF");
    }
#endif
    else if (my_strncasecmp(args, "time", 4)==0) {
	if (arg2) if (*arg2) Time_Out=atoi(arg2);
	fprintf(stdout,"Time out: %d\n",Time_Out);
    }
    else if (my_strncasecmp(args, "host", 4)==0) {
	fprintf(stdout,"Not implemented yet\n");
    }
    else {
	fprintf(stdout,"Syntax error: \"%s\"\n",args);
	fprintf(stdout,"Use one of:\n");
#ifndef NO_PAGER
	fprintf(stdout,"\tset pager [on|off]\n");
#endif
	fprintf(stdout,"\
\tset literal [on|off]\n\
\tset verbose [on|off]\n\
\tset time [duration]\n\
\tset host [host prot]\n");
    }
}

/*--------------------------------------------------------------------------
 whois server mode.   Read a single line from stdin and treat it as
 a whois query.  All output lines should be terminated with CR LF.
--------------------------------------------------------------------------*/
static void
whois_server(uwho)
    uwho_t *uwho;
{
    char query[MAXSTR];
    char *s;

    if (!fgets(query, MAXSTR, stdin)) return;
    /* Truncate EOL. */
    for (s=query; *s && *s != '\n' && *s != '\r'; s++)
	;
    *s = 0;

    if (my_strncasecmp(query, "help", 4) ==0) {
	fprintf(stdout,"\
Enter the name (usually lastname, possibly followed by comma and firstname)\r\n\
of the person you are looking for, possibly followed by @ and an organization\r\n\
name.\r\n\
org can be any fragment of the school or business name or\r\n\
hostname; e.g. \"houde, john @mit\" or \"kegel@caltech\"\r\n\
Other commands:\r\n\
help               - print this message\r\n\
ns-servers         - list ns (aka ph) servers\r\n\
whois-servers      - list whois servers\r\n\
");
    } else if (my_strncasecmp(query, "ns-servers", 10) ==0) {
	uwho_getfilename(uwho, query, uwho_PROTO_PH, 1);
	(void) do_cat(query);
    } else if (my_strncasecmp(query, "whois-servers", 10) ==0) {
	uwho_getfilename(uwho, query, uwho_PROTO_WHOIS, 1);
	(void) do_cat(query);
    } else
	(void) do_query(uwho, query);
}

/*--------------------------------------------------------------------------
 Interactive mode.  Read multiple lines from stdin; treat each one as
 a query or command.
--------------------------------------------------------------------------*/
static void
interactive_server(uwho)
    uwho_t *uwho;
{
    char querybuf[MAXSTR];
    char *query = querybuf;

    fprintf(stdout,"Welcome to uwho (version %s)\n", VERSION);
    while (!feof(stdin)) {
	char *s, *arg;
	fprintf(stdout,"uwho> "); fflush(stdout);
	if (!fgets(query, MAXSTR, stdin)) continue;
	/* Truncate EOL. */
	for (s=query; *s && *s != '\n' && *s != '\r'; s++)
	    ;
	*s = 0;
	/* arg points to next argument */
	arg=strpbrk(query, " \f\n\r\t\v");
	if (arg)
	    for ( ; isspace(*arg) && *arg; arg++);

	for ( ; isspace(*query) && *query; query++);

	if (my_strncasecmp(query, "quit", 4)==0) {
	    break;
	} else if (my_strncasecmp(query, "list", 4)==0) {
	    PAGER_START
	    do_list(uwho);
	    PAGER_END
	} else if (my_strncasecmp(query, "set", 3)==0) {
	    uwho_set(uwho, arg);
	} else if (my_strncasecmp(query, "find", 4)==0) {
	    PAGER_START
	    (void) do_query(uwho, arg);
	    PAGER_END
	} else if (*query) {
	    if (my_strncasecmp(query, "help", 4) !=0) 
		fprintf(stdout, "Syntax error: \"%s\"\n",query);
	    fprintf(stdout,"Commands:\n\
\tquit - terminates uwho\n\
\tlist - lists servers uwho knows about\n\
\tset  - sets/displays options\n\
\tfind name - searches for given person at default org.\n\
\t\tname should usually be lastname, firstname.\n\
\tfind name @ org - searches for given person at a given organization.\n\
\t\torg can be any fragment of the school or business name or\n\
\t\thostname; e.g. \"houde, john @mit\" or \"kegel@caltech\"\n\
");
	}
    }
}

#define MAXEARGC 20
void main(argc, argv)
    int argc;
    char **argv;
{
    int eargc, nargc, i;
    char *eargv[MAXEARGC], **nargv;
    char *uwho_env, *uwho_opts, *end;
    int cmd;
    char *query;
    int status;
    uwho_t *uwho;

    strcpy(Program,argv[0]);

    /* Merge words of environment variable UWHO_OPTS into nargv. */
    nargv = argv; nargc = argc;
    uwho_opts=getenv("UWHO_OPTS");
    if (uwho_opts) {
	if (!(uwho_env=(char *)malloc((strlen(uwho_opts)+1)*sizeof(char)))) {
	    fprintf(stderr,"uwho: could not malloc space for uwho_env\n");
	    exit(-1);
	}
	strcpy(uwho_env,uwho_opts);

	/* Break uwho_env into words, put them in eargv[]. */
	for (eargc=0, eargv[eargc]=uwho_env; eargv[eargc] && eargc < MAXEARGC; eargv[++eargc] = end){
	    if (!*eargv[eargc]) break;
	    for (end=eargv[eargc];*end && !isspace(*end);end++);
	    if (*end) *end++='\0';
	}

	/* Merge eargv[] into nargv[].  Do not modify environment. */
	nargc=eargc+argc;
	nargv=(char **)malloc(nargc*sizeof(char *));
	assert(nargv != NULL);
	nargv[0]=argv[0];
	for (i=0;i<eargc;i++)
	    nargv[i+1]=eargv[i];
	for (i++;i<nargc;i++)
	    nargv[i]=argv[i-eargc];
    }

    /* Handle commandline options.
     * cli_parse() returns an int describing the type of query, and
     * places the query in the given string.
     */
    cmd = cli_parse(nargc, nargv, &query);

    /* if a pipe breaks, let write() return -1.
     * This keeps uwho from aborting if a host refuses connection,
     * but it means we have to check feof(stdout)
     * to exit if user runs uwho | more and hits q.
     */
    ignore_writeEOF(1);
#ifdef WIN32
    {
	WSADATA WSAData;
	int ret;

	ret = WSAStartup(1, &WSAData);

	if (ret != 0) {
	    fprintf(stderr, "uwho:  main:  WSAStartup() failed\n");
	    exit(-1);
	}
    }
#endif

    uwho = uwho_create(def_host, def_host_protocol, uwho_dir, printErr);
    if (!uwho) {
	fprintf(stderr,"uwho: main: uwho_create() failed\n");
	exit(-1);
    }
    callback_uwho = uwho;
    uwho->Literal = Literal;

    switch (cmd) {
    case CMD_LIST:
	PAGER_START
	do_list(uwho);
	PAGER_END
	break;
    case CMD_HELP:
	help();
	break;
    case CMD_UPDATE:
	if (uwho_update(uwho, Time_Out, update_hostnames)) {
	    fprintf(stderr, "Update failed.\n");
	    exit(-1);
	}
	break;
    case CMD_QUERY:
	assert (query[0] != 0);
	/* Interpret the uwho query, execute it, print results to stdout. */
	PAGER_START
	status = do_query(uwho, query);
	PAGER_END
	free(query);
	break;
    case CMD_NONE:
	/* Either present a terse interface, or a chatty one,
	 * depending on whether we're trying to be a whois server.
	 */
	if (be_whois_server) {
	    strcpy(callback_eol, "\r\n");
	    whois_server(uwho);
	} else {
	    interactive_server(uwho);
	}
	break;
    default:
	assert(FALSE);
    }
    uwho_destroy(uwho); 

#ifdef WIN32
    WSACleanup();
#endif
    exit(status);
}
