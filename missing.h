/* @(#)missing.h	1.5 10/15/92 */
/* these are some of the prototypes that are missing in Sun OS include files */

void fprintf(FILE *stream, const char *format, ...);
void fclose(FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nobj, FILE *stream);
int fputc(int c, FILE *stream);
int fputs(char *s, FILE *stream);
int tolower(int c);
void perror(const char *);
void bcopy(char *b1, char *b2, int length);
void bzero(char *b, int length);
time_t time(time_t *tp);
int socket(int domain, int type, int protocol);
int connect(int s, struct sockaddr *name, int namelen);
int select(int width, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);
int rename(const char *oldname, const char *newname);
int remove(const char *path);
char *mktemp(char *template);
int fflush(FILE *stream);
void rewind(FILE *stream);
int strcasecmp(char *s1, char *s2);
int strncasecmp(char *s1, char *s2, int n);
#ifdef POPEN_PAGER
int pclose(FILE *stream);
#endif
