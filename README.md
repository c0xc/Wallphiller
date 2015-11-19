WALLPHILLER
===========

This tool can change your wallpaper.
Several desktop environments are supported.



Build
-----

Default build:
$ make

Windows build:
$ make PLATFORM=win32-g++



Resources
---------

A few pictures are embedded into the application.
The resource files are in res/.
The resource definition list is in res/res.qrc.

Generate resource file:
$ cd res/ && $QTDIR/bin/rcc res.qrc -o res.src



Author
------

Philip Seeger (philip@philip-seeger.de)



License
-------

Please see the file called LICENSE.



