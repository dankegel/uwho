/*--------------------------------------------------------------------------
 Pager module definitions.
--------------------------------------------------------------------------*/

#ifndef pg_h
#define pg_h

typedef struct pg_s {
    int rows;		/* screen vert size */
    int cols;		/* screen horiz size */
    int currow;		/* cursor position */

    int curline;	/* memory buffer position */
    int nlines;		/* memory buffer used size */
    int alines;		/* memory buffer allocated size */
    char **lines;	/* memory buffer */

    int quit_at_eof;	/* TRUE if pager automatically quits when all read */
    int ateof;		/* TRUE if user has reached end of file */
    int quit;		/* TRUE if user has asked to quit */
    int last_percent;	/* last % reported to user */
} pg_t;

pg_t *pg_create();
void pg_destroy();
void pg_puts();
int pg_poll();

#endif
