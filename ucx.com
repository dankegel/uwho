$ cc/list/define=VMS uwho.c
$ cc/list/define=VMS main.c
$ cc/list/define=VMS mystring.c
$ cc/list/define=VMS raw.c
$ cc/list/define=VMS pg.c
$ link uwho,main,mystring,raw,pg,sys$library:ucx$ipc/lib,sys$input/opt
sys$share:vaxcrtl/share
