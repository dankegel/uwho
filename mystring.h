/* mystring.c @(#)mystring.h	2.2 10/15/92 */
#ifdef ANSI
int tolower(int c);
void my_strncpy(char *out, char *in, int n);
int my_strncasecmp(char *a, char *b, int n);
void strnlower(char out[], char *in, int n);
#else
void my_strncpy();
int my_strncasecmp();
void strnlower();
#endif
