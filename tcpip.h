/*--------------------------------------------------------------------------
 %W% %G%

 The Berkeley Sockets programming interface to the TCP/IP protocol suite
 is widely supported, but the exact names of the entry points change
 from implementation to implementation.

 This file uses #define to provide a consistant set of entry point names:
 netread(), netwrite(), socket(), netioctl(), select(), and
 netperror(), as well as a consistantly named global, int neterrno.

 Some semantic differences persist in poorer implementations; for instance,
 gethostbyname() doesn't work properly under Wollengong,
 and select() doesn't work with all versions.
 Also, on some systems, errno is a function of a set of file descriptors,
 and can't be handled here.

 The original version of this file was donated by Mike Kienenberger 
 <FSMLK1@acad3.alaska.edu>.  Please forward any corrections to him, and 
 to the uwho group at uwho@punisher.caltech.edu.
--------------------------------------------------------------------------*/

#ifdef VMS
/* vms cc can't /define more than one thing... so options go in .h file */
#include "vmsdef.h"
#endif

/*------------ VMS: WOLLENGONG -----------*/
#ifdef WOLLENGONG
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>          /* for inet_addr */
#include <sys/ioctl.h>          /* for FIONBIO */
#include <errno.h>              /* for EINPROGRESS */
#define USE_FIONBIO             /* this system uses ioctl to set nonblocking.*/
#define netioctl ioctl
#define neterrno errno
#define netperror perror
#endif

/*------------ VMS: TGV MULTINET -----------*/
#ifdef MULTINET
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <multi/netdb.h>
#include <multi/errno.h>        /* for EINPROGRESS */
#include <sys/ioctl.h>          /* for FIONBIO */
#include <netinet/in.h>
#include <arpa/inet.h>          /* for inet_addr */
#define netclose socket_close
#define netread socket_read
#define netwrite socket_write
#define netioctl socket_ioctl
#define neterrno socket_errno
#define netperror socket_perror
#define USE_FIONBIO             /* this system uses ioctl to set nonblocking.*/
#endif

/*------------ VMS: DEC UCX aka TCP/IP services for VMS ------------*/
#ifdef UCX
#include <types.h>
#include <sys/time.h>
#include <socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define netclose close
#define netread read
#define netwrite write
#define netioctl ioctl
#define neterrno errno
#define netperror perror
#endif

/*------------ MSDOS ------------*/
#ifdef MSDOS
#include <time.h>       /* for ctime */
#define NO_SIGPIPE
#endif

/*------------ MSDOS: Novell Lan Workplace for DOS ------------*/
#ifdef LWP
#include <sys/types.h>
#include <sys/socket.h> /* for timeval among other things */
#include <netdb.h>
#include <netinet/in.h> /* for inet_addr */
#include <sys/filio.h>  /* for FIONBIO */
#define USE_FIONBIO     /* this system uses ioctl to set nonblocking.*/
#define netclose soclose
#define netread soread
#define netwrite sowrite
#define netioctl ioctl
#define neterrno errno          /* really? */
#define netperror soperror
#endif

/*------------ MSDOS: PC-NFS -------------*/
#ifdef PTK40            /* PC-NFS Programmer's Toolkit Version 4.0 or later */
#include <tklib.h>
#include <tk_errno.h>
#include <sys/ioctl.h>  /* for FIONBIO */
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <in_addr.h>     /* for inet_addr */
#include <sys/nfs_time.h>/* for timeval */
#define USE_FIONBIO     /* this system uses ioctl to set nonblocking.*/
#define netclose close
#define netread read
#define netwrite write
#define netioctl ioctl
#define netperror tk_perror
/* neterrno handled in uwho.c directly - it's a function of filedescriptors */ 
#endif

/*------------ MSDOS: PC/TCP -------------- */
#ifdef PCTCP            /* PC/TCP Developer's Toolkit 2.05 */
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>   /* for timeval */
#include <errno.h>
#include <sys/ioctl.h>  /* for FIONBIO */
#include <sys/socket.h> 
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>  /* for inet_addr */
#include <io.h>
#include <fcntl.h>
#define netclose close
#define netread read
#define netwrite write
#define netioctl ioctl
#define netperror perror
#define neterrno  errno
#endif

/*---------- MS-Windows-NT: Native Winsock ----------*/
#ifdef WIN32
#include <winsock.h>
#include <time.h>
#define NO_SIGPIPE
#define USE_FIONBIO             /* this system uses ioctl to set nonblocking.*/
#define netioctl ioctlsocket
#define netclose closesocket
#define netread(d, bu, by)  recv(d, bu, by, 0)
#define netwrite(d, bu, by) send(d, bu, by, 0) 
#define neterrno WSAGetLastError()
#define ENOBUFS WSAENOBUFS
#define EINPROGRESS WSAEINPROGRESS
u_long inet_addr();
#endif

/*---------- Unix ----------*/
#ifdef UNIX
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>  /* for inet_addr */

#ifdef NEXT
/* most of unistd.h is in libc.h   */
/* most of malloc.h is in stdlib.h */
#else
#include <unistd.h>
#include <malloc.h>
#endif

/* FNDELAY and O_NDELAY: */
#include <fcntl.h>
#ifdef HPUX
/* HP/UX defines FNDELAY in sys/file.h which is not included */
#define USE_O_NDELAY
#endif

#define netclose close
#define netread read
#define netwrite write
#define netioctl ioctl
#define neterrno errno
#define netperror perror
#endif

/*---------------------------------------------------------------*/
/* Need FD_SET for select.  Not sure which systems don't define it. */
#ifndef FD_SET
#define FD_SET(n,p)	((p)->fds_bits[0] |= (1<<(n)))
#define FD_ZERO(p)	((p)->fds_bits[0] = 0)
#define FD_ISSET(n,p)	((p)->fds_bits[0] & (1<<(n)))
#endif

