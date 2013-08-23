mesa_srcdir = /home/artem/data/dev/gstack/mesa/mesa.dev0

CFLAGS = \
	-I. \
	-I/usr/include/libdrm                   \
	-I$(mesa_srcdir)/include                \
	-I$(mesa_srcdir)/src/gallium/auxiliary  \
	-I$(mesa_srcdir)/src/gallium/include    \
	-I$(mesa_srcdir)/src/gallium/drivers    \
	-I$(mesa_srcdir)/src/gallium/winsys     \
                                            \
    -DPIPE_SEARCH_DIR=\"$(mesa_srcdir)/src/gallium/targets/pipe-loader/.libs\"

LDFLAGS = \
     -lxcb-dri2 -lxcb  -ldrm   -ludev  -ldl -lpthread -lm                         \
     -lX11 -lX11-xcb                                                              \
     $(mesa_srcdir)/src/gallium/auxiliary/pipe-loader/.libs/libpipe_loader.a      \
     $(mesa_srcdir)/src/gallium/auxiliary/.libs/libgallium.a                      \
     $(mesa_srcdir)/src/gallium/winsys/sw/null/.libs/libws_null.a
    
all:
#	gcc $(CFLAGS) -o demo0  demo0.c  $(LDFLAGS) 
#	gcc $(CFLAGS) -c -o utl.o  utl.c  $(LDFLAGS) 
#	gcc $(CFLAGS) -o gallium3  utl.c gallium3.c  $(LDFLAGS) 

	gcc $(CFLAGS) -c -o utl.o  utl.c  $(LDFLAGS) 
	gcc $(CFLAGS) -o gallium4  utl-galt.c gallium4.c  $(LDFLAGS) 
