/*--------------------------------------------------------------------------
 A 'more'-like module for use in programs running on single-tasking operating
 systems with tty interfaces, e.g. MS-DOS and VMS.
--------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

#include "pg.h"
#include "raw.h"

static char SCCSID[]="%W% %G%";

/*--------------------------------------------------------------------------
 Initialize the pager; set screen size, zero the line counter.
--------------------------------------------------------------------------*/
pg_t *pg_create(rows, cols, quit_at_eof)
    int rows, cols;
    int quit_at_eof;
{
    pg_t *this;
    this = (pg_t *) malloc(sizeof(pg_t));
    if (!this) return this;

    this->last_percent = -1;
    this->quit_at_eof = quit_at_eof;
    this->ateof = 0;
    this->quit = 0;
    this->rows = rows;
    this->cols = cols;
    this->currow = 0;
    this->nlines = 0;
    this->curline = 0;
    this->alines = 1024;
    this->lines = (char **) malloc(this->alines * sizeof(char *));
    if (!this->lines) { free(this); return NULL; }
    raw_init();
    return this;
}

void pg_destroy(this)
    pg_t *this;
{
    free(this->lines);
    free(this);
    raw_end();
}

/*--------------------------------------------------------------------------
 Output one line of text to the pager.
 Line should not end with a newline.
 Text is stored in memory until caller calls pg_poll().
--------------------------------------------------------------------------*/
void pg_puts(this, s)
    pg_t *this;
    char *s;
{
    char *p;

    assert(this);

    /* Stick line in memory */
    p = malloc(strlen(s)+1);
    if (!p) return;             /* hmm */
    strcpy(p, s);
    if (this->nlines == this->alines) {
	char **tmp;
	/* reallocate */
	this->alines *= 2;
	tmp = (char **) realloc(this->lines, this->alines * sizeof(char *));
	if (!tmp) return;               /* hmm */
	this->lines = tmp;
    }
    this->lines[this->nlines] = p;
    this->nlines ++;
}

/*--------------------------------------------------------------------------
 Check for a command from the user, and display more data if appropriate.
 If eof is TRUE, no more data will be written with pg_write();
 this lets pg_poll() prompt user before the end of the screen is reached.
 Returns -1 if user indicates he's done with data.
--------------------------------------------------------------------------*/
int pg_poll(this, eof)
    pg_t *this;
    int eof;
{
    if (this->quit) return -1;

#if 0
    printf("pg_poll; curline %d, currow %d, nlines %d\n",
	 this->curline, this->currow, this->nlines); sleep(1);
#endif

    /* Loop until no more commands from user */
    for (;;) {
	/* Can we output any text? */
	while ((this->curline< this->nlines) && (this->currow < this->rows-2)) {
	    puts(this->lines[this->curline++]);
	    this->currow++;
	}
	/* Should we prompt the user? */
	if (eof && !this->ateof && this->curline==this->nlines) {
	    if (this->quit_at_eof) {
		fflush(stdout);
		this->quit = 1;
		return -1;
	    }
	    fputs("\r--- End --- Hit q to quit ---",stdout);
	    this->ateof = 1;
	    fflush(stdout);
	} else if (this->currow >= this->rows-2) {
	    int percent = (int) ((100L*this->curline) / this->nlines);
	    if (percent != this->last_percent) {
		printf("\r--- %2d%% --- Hit space for more --- ", 
		    percent);
		this->last_percent = percent;
		fflush(stdout);
	    }
	    this->currow = this->rows-1;
	}
	if (feof(stdin)) return -1;

	/* Should we get a command from the user? */
	if ((eof || (this->currow == this->rows-1)) && raw_kbhit()) {
	    int c;
	    fflush(stdout);
	    c = raw_getc();
	    if (isupper(c)) c=tolower(c);

	    switch (c) {
	    case 'h':
	    case '?':
		printf("\n");
		printf("help:\n");
		printf("q, escape, ^C, or EOF quits\n");
		printf("space or ENTER displays the next page\n");
		printf("b displays the previous page\n");
		this->last_percent = -1;
		this->ateof = 0;
		break;
	    case 'b':
		printf("\n");
		this->currow = 0;
		this->ateof = 0;
		this->curline -= this->rows*2-3;
		this->last_percent = -1;
		if (0 > this->curline) this->curline = 0;
		break;
	    case 27:
	    case 3:
	    case 'q' :
		printf("\n");
		this->quit = 1;
		return -1;
		/* break; */
	    case ' ' : 
	    case '\n' : 
	    case '\r' : 
		if (this->ateof) break;		/* should this quit? */
		printf("\n");
		this->currow = 0;
		break;
	    default :
		;
	    }
	} else
	    break;
    }
    return 0;
}

#ifdef MAIN

int main()
{
    pg_t *pg;
    int i;
    char buf[128];

    pg = pg_create(25, 80);
    assert(pg);
    for (i=1;i<=40;i++) {
	sprintf(buf, "Line %d", i);
	pg_puts(pg, buf);
	if (pg_poll(pg, 0) == -1) {
	    pg_destroy(pg);
	    exit(0);
	}
    }
    while (pg_poll(pg, 1) != -1) 
	usleep(100000);
    pg_destroy(pg);
    exit(0);
}
#endif
