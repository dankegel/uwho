$ define sys twg$tcp:[netdist.include.sys]
$ define vms twg$tcp:[netdist.include.vms]
$ cc/list/define=VMS raw.c
$ cc/list/define=VMS pg.c
$ cc/list/define=VMS mystring.c
$ cc/list/define=VMS main.c
$ cc/list/define=VMS uwho.c
$ link uwho,main,mystring,raw,pg,tcp$lib:twglib/lib,sys$input/opt
sys$share:vaxcrtl/share
