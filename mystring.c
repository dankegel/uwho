#include <stdio.h>
#include <ctype.h>
#include "mystring.h"

static char sccsid[] = "%W% %G%";

/*--------------------------------------------------------------------------
 Like strncpy, but always leaves result null-terminated.
--------------------------------------------------------------------------*/
void my_strncpy(out, in, n)
    char *out, *in;
    int n;
{
    for (; n>1 && *in; n--)
	*out++ = *in++;
    *out = '\0';
}
   
/*--------------------------------------------------------------------------
 Copy a string and make it lowercase.
 At most n chars are copied.
 Result is always null-terminated.
--------------------------------------------------------------------------*/
void strnlower(out, in, n)
    char out[], *in;
    int n;
{
    int i;
    for (i=0; in[i] && i < n; i++) {
	if (isupper(in[i])) out[i] = tolower(in[i]);
	else out[i]=in[i];
    }
    out[i]='\0';
}

/*--------------------------------------------------------------------------
 Compare a string, ignoring case, comparing at most n chars.
--------------------------------------------------------------------------*/
int my_strncasecmp(a, b, n)
    char *a, *b;
    int n;
{
    int i;
    for (i=0; a[i] && b[i] && i < n; i++) {
	int ca, cb;
	ca = a[i]; if (isupper(ca)) ca = tolower(ca);
	cb = b[i]; if (isupper(cb)) cb = tolower(cb);
	if (ca != cb) return ca - cb;
    }
    if (i == n) return 0;
    else return a[i] - b[i];
}
