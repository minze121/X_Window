X11_LIBS =-lX11 -lXext -lXi -lframe -lXcursor -lxcb
DBUS_INCLUDE = -I/usr/include/gio-unix-2.0  -I/usr/include/dbus-1.0 -I/usr/lib/x86_64-linux-gnu/dbus-1.0/include -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include
DBUS_LIB=-ldbus-1 -lgio-2.0 -L/usr/lib/x86_64-linux-gnu/gstreamer-1.0 -lgstgio
PTHREAD_LIB =-lpthread

all:
	gcc -g  five_filter.c $(I)$(DBUS_INCLUDE) $(DBUS_LIB) $(PTHREAD_LIB) $(X11_LIBS) -o five_filter -g
	
clean:
	rm -rf *.o five_filter read_mouse_pos
	
