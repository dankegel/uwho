/*--------------------------------------------------------------------------
 Include file for users of raw.c.
 For details, see raw.c.
 26 Nov 1992, dan kegel: added raw_init(), raw_end(), and raw_getc().

 $Log:	raw.h $
 * Revision 1.2  90/07/14  09:00:55  dan_kegel
 * Added raw_set_stdio(), which should make it easy to use RAW mode.
--------------------------------------------------------------------------*/
#ifndef raw_h
#define raw_h

void raw_set_stdio(/* int raw */);

/* Set stdin and stdout to raw mode */
#define raw_init() raw_set_stdio(1)

/* Set stdin and stdout to cooked mode */
#define raw_end() raw_set_stdio(0)

/* Check for char on stdin.  Returns nonzero if one is waiting, 0 if not. */
int raw_kbhit(/* void */);

/* Return -1 if no char available, otherwise return char. 
 * Return 3 if user hits the interrupt key (usually ^C, hence 3)
 */
int raw_getc(/* void */);

#endif
