Motivation
==========

Initially ```cosmos``` was created to boot specifically ```ClickOS``` domains, in essence
just to instantiate a middlebox. Given we were using MiniOS we also started exploring short 
boot up times. We started exploring these tools and gradually building our own while we 
studied the boot process. The end result is this toolstack, focused on MiniOS-based domains.

Building
========

You will need Xen sources only when building cosmos with domain libraries. Mostly the headers
are needed which means you don't need to compile xen or its tools.
Make sure the xen version installed in your host and sources you got match,
otherwise you won't be able to build cosmos with domain libraries.

    $ cd /path/to/xen-sources
    $ ./configure
    $ cd tools
    $ make -C include

You will need to export your Xen sources location into ```XEN_ROOT```.

    $ export XEN_ROOT=/path/to/xen-sources

To build cosmos just type ```make```.

    $ cd /path/to/cosmos
    $ make

By default, it only builds a minimal core to start up middleboxes. If you
want to enable domain management you will to explicitly enable a domain library:

To use an ```libxl``` based domains creation:

	$ make DOMLIB=xl

To use our ```libxcl``` (Xen control light) based domains creation:

	$ make DOMLIB=xcl

The latter enables you faster boot times, although it is still experimental.

Our build process can generate any binding. Examples are shown below (also the only ones tested):

    # For python (default)
    $ make python-binding

    # For ocaml
    $ make ocaml-binding

    # For nodejs (use this swig: github.com/olivier----/swig-v8
    $ make javascript-binding V8=y

Note that not all bindings were tested. For example, ruby binding is currently not compiling.
