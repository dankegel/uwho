$ define multi multinet_root:[multinet.include]
$ define sys multinet_root:[multinet.include.sys]
$ define vms multinet_root:[multinet.include.vms]
$ define arpa multinet_root:[multinet.include.arpa]
$ define netinet multinet_root:[multinet.include.netinet]
$ cc/list/define=VMS raw.c
$ cc/list/define=VMS pg.c
$ cc/list/define=VMS mystring.c
$ cc/list/define=VMS main.c
$ cc/list/define=VMS uwho.c
$ link uwho,main,mystring,raw,pg,sys$input/opt
sys$share:vaxcrtl/share
multinet:multinet_socket_library/share
