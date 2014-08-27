//Common
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
//X11
#include <X11/Xlib.h>
#include <X11/X.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XInput.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/cursorfont.h>
//XCB
#include <xcb/xcb.h>
#include <xcb/xproto.h>

//Defines
#define FIVETOUCHNAME "PIXCIR HID Touch Panel"
#define TENTOUCHNAME "Nuvoton HID Transfer"
#define PID_FILE  "/home/devel/FIVE_FILTER_PID"
#define HAVE_XI22
#define INVALID_EVENT_TYPE	-1

int xi_opcode;
int isMoveCursor = 0; //if accept multitouch event , don't move
int windowLive = 0;

Display * display_thread = NULL;
Window activeWin = 0;
Window focus_window;
Window pre_window;
XEvent backup_event;

static xcb_connection_t *xcb_dpy;
static xcb_screen_t * xcb_screen;
static xcb_generic_error_t *err;

//#define DEBUG

int isExistWindow(Window win)
{
	xcb_get_geometry_cookie_t gg_cookie = xcb_get_geometry (xcb_dpy, win);

	xcb_get_geometry_reply_t * geometry = xcb_get_geometry_reply(xcb_dpy, gg_cookie, &err);

	if (!geometry) {
		printf("Can't find active window : %d\n",win);
		return 0;
	}
	//It can find normal.
	return 1;
}

void * create_grabpointer(void * arg)
{
	windowLive = 1;

	display_thread = XOpenDisplay(NULL);
	char * end;
	Window win = strtoul((char *)arg,&end,0);

	//judge whether the window exists.
	if (!isExistWindow(win))
		goto OVER;

	int ret = XGrabPointer(display_thread,win,False,
			 ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask|EnterWindowMask|LeaveWindowMask,
			 GrabModeAsync,GrabModeAsync,
			 win,
			 None,CurrentTime);

	if (ret != GrabSuccess)
	{
		printf("grab failure\n");
		goto OVER;
	}

	XWindowAttributes windowattribute;
	XEvent event;

#ifdef DEBUG
	printf("grab begin\n");
#endif

	while(windowLive)
	{
		XNextEvent(display_thread,&event);

		unsigned long mask = 0l;
		backup_event = event;

		XSendEvent(display_thread,win,False,mask,&event);

		switch(event.type)
		{
		case ButtonRelease:
		{
			windowLive = 0;
		}
		default:
			break;
		}
	}
	XUngrabPointer(display_thread,CurrentTime);

OVER:
    XCloseDisplay(display_thread);
#ifdef DEBUG
	printf("grab over\n\n\n");
#endif
}

void active()
{
	if (activeWin != 0)
	{
#ifdef DEBUG
		printf("touch active win :%d\n",activeWin);
#endif
		int pthread_id = 0;
		char pthread_arg[20];
		sprintf(pthread_arg,"%d",activeWin);

		int ret = pthread_create(&pthread_id,NULL,create_grabpointer,(void *)pthread_arg);
	}
}

Cursor createnullcursor(Display *display,Window root)
{
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
    xgc.function = GXclear;
    gc =  XCreateGC(display, cursormask, GCFunction, &xgc);
    XFillRectangle(display, cursormask, gc, 0, 0, 1, 1);
    dummycolour.pixel = 0;
    dummycolour.red = 0;
    dummycolour.flags = 04;
    cursor = XCreatePixmapCursor(display, cursormask, cursormask,
	      &dummycolour,&dummycolour, 0,0);
    XFreePixmap(display,cursormask);
    XFreeGC(display,gc);
    return cursor;
}

void change_cursor_shape(Display *display,Window root,Cursor cursor)
{
	int x=0,y=0;
	int tmp;
	unsigned int tmp2;
	Window fromroot,tmpwin;
	XQueryPointer(display,root,&fromroot,&tmpwin,&x,&y,&tmp,&tmp,&tmp2);
	XDefineCursor(display,tmpwin,cursor);
	XSync(display,False);
}

Bool device_matches(XIDeviceInfo *info, char *name)
{
    if (strcmp(info->name, name) == 0) {
        return True;
    }
    return False;
}

XIDeviceInfo* find_device_info_xi2(Display *display, char *name)
{
    XIDeviceInfo *info;
    XIDeviceInfo *found = NULL;
    int ndevices;
    int i;

    info = XIQueryDevice(display, XIAllDevices, &ndevices);
    for(i = 0; i < ndevices; i++)
    {
        if (device_matches (&info[i], name)) {
            if (found) {
                fprintf(stderr,"Warning: Don't match '%s'.\n", name);
                XIFreeDeviceInfo(info);
                return NULL;
            } else {
                found = &info[i];
            }
        }
    }

    return found;
}

int run_filter(Display *display)
{
	Window root =DefaultRootWindow(display);

	int fivetouch_deviceid = -1;
	int tentouch_deviceid = -1;
	int rc;

#if 1
	//5-inch event filter register
	{
		XIEventMask fivetouch_mask;
		XIDeviceInfo * fivetouch_info;

		fivetouch_info = find_device_info_xi2(display, FIVETOUCHNAME);

		if (fivetouch_info != NULL)
		{
			//XI2 event
			fivetouch_deviceid = fivetouch_info->deviceid;

			fivetouch_mask.deviceid = (fivetouch_deviceid == -1) ? XIAllDevices : fivetouch_deviceid;
			fivetouch_mask.mask_len = XIMaskLen(XI_LASTEVENT);
			fivetouch_mask.mask = calloc(fivetouch_mask.mask_len, sizeof(char));

			memset(fivetouch_mask.mask, 0, fivetouch_mask.mask_len);

			XISetMask(fivetouch_mask.mask, XI_RawTouchBegin);
			XISetMask(fivetouch_mask.mask, XI_RawTouchUpdate);
			XISetMask(fivetouch_mask.mask, XI_RawTouchEnd);
			XISetMask(fivetouch_mask.mask, XI_TouchBegin);
			XISetMask(fivetouch_mask.mask, XI_TouchUpdate);
			XISetMask(fivetouch_mask.mask, XI_TouchEnd);

			XISelectEvents(display, root, &fivetouch_mask, 1);

			free(fivetouch_mask.mask);
		}
	}
#endif
#if 1
	//10-inch event filter register
	{
		XIEventMask tentouch_mask;
		XIDeviceInfo *tentouch_info;

		tentouch_info = find_device_info_xi2(display, TENTOUCHNAME);
		if (tentouch_info != NULL)
		{
			//XI2 event
			tentouch_deviceid = tentouch_info->deviceid;

			tentouch_mask.deviceid = (tentouch_deviceid == -1) ? XIAllDevices : tentouch_deviceid;
			tentouch_mask.mask_len = XIMaskLen(XI_LASTEVENT);
			tentouch_mask.mask = calloc(tentouch_mask.mask_len, sizeof(char));

			memset(tentouch_mask.mask, 0, tentouch_mask.mask_len);

			XISetMask(tentouch_mask.mask, XI_TouchBegin);
			XISetMask(tentouch_mask.mask, XI_TouchUpdate);
			XISetMask(tentouch_mask.mask, XI_TouchEnd);

			XISelectEvents(display, root, &tentouch_mask, 1);
			free(tentouch_mask.mask);
		}
	}
#endif
	Cursor null_cursor = createnullcursor(display,root);

	int updateing = 1;
	int count = 0;


	while(1)
	{
		XEvent ev;
		XGenericEventCookie *cookie = (XGenericEventCookie*)&ev.xcookie;
		XNextEvent(display, (XEvent*)&ev);

        unsigned long mask = 0l;
//        XSendEvent(display,pre_window,False,mask,&ev);

        if (   XGetEventData(display, cookie)
			&& cookie->type == GenericEvent
			&& cookie->extension == xi_opcode)
		{
        	switch (cookie->evtype)
			{
			case XI_RawTouchBegin:
			case XI_RawTouchUpdate:
			case XI_RawTouchEnd:
			{

				break;
			}
			case XI_TouchBegin:
			{
				//Active
//				kill(getppid(),SIGALRM);
			}
			break;
			case XI_TouchUpdate:
			{

			}
			break;
			case XI_TouchEnd:
			{
			}
			break;
			default:
				break;
			}
		}
		XFreeEventData(display, cookie);
	}

	return EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{

	pid_t pid,sid;
	int ret;

	pid = fork();
	if (pid < 0)
	{
		printf("error in fork!\n");

		exit(1);
	}
	else if (pid == 0)
	{
	    Display     *display;
	    int event, error;

	    display = XOpenDisplay(NULL);

	    if (display == NULL) {
	        fprintf(stderr, "Unable to connect to X server\n");
	        goto out;
	    }

	    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
	        printf("X Input extension not available.\n");
	        goto out;
	    }

	        //start the event filter
	    int ret = run_filter(display);

	    printf("close children\n");
	    XSync(display, False);
	    XCloseDisplay(display);

	    return ret;
	}else if (pid > 0)
	{
		//Save the pid into file
		char pidInfo[20];
		int fd = open(PID_FILE,O_CREAT|O_RDWR|O_TRUNC,S_IREAD|S_IWRITE);
		sprintf(pidInfo,"%d",getpid());
		if (fd > 0)
		{
#ifdef DEBUG
			printf("pidInfo : %s\n",pidInfo);
#endif
			write(fd,pidInfo,strlen(pidInfo));
		}
		else
		{
			printf("It can't open %s\n",PID_FILE);
		}
		close(fd);

		int flag = 0;
		int error;
		int event;

		signal(SIGALRM,active);

		focus_window = 0x0;
		pre_window = 0x0;

		Display * display = XOpenDisplay(NULL);

		xcb_dpy = xcb_connect(NULL,NULL);


	    if (display == NULL) {
	    	fprintf(stderr, "Unable to connect to X server\n");
	    	goto out;
	    }

	    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
	        printf("X Input extension not available.\n");
	        goto out;
	    }

		XWindowAttributes windowattribute;
		xcb_translate_coordinates_cookie_t trans_coords_cookie;

		static int setFocusCount = 0;
		while(1)
		{
			usleep(1000*50);
			if (display != NULL)
			{
				int ret_revert;

				focus_window = 0x0;

				XGetInputFocus(display,&focus_window,&ret_revert);

				//focus_window maybe is equal to 0x1
				if (focus_window >= 0x100)
				{
					XGetWindowAttributes(display,focus_window,&windowattribute);

					trans_coords_cookie = xcb_translate_coordinates (
													xcb_dpy,
													focus_window,
													windowattribute.root,
													-(windowattribute.border_width),
													focus_window				-(windowattribute.border_width));
					xcb_translate_coordinates_reply_t *trans_coords=xcb_translate_coordinates_reply (xcb_dpy, trans_coords_cookie, NULL);

					flag = 0;

					//The window is in Main Screen
					if ((trans_coords->dst_x >= 1280) && (pre_window != focus_window) )
					{
						//Full screen
//						if ((windowattributrans_coords_cookiete.width == 1920))
						{
							flag = 1;
							pre_window = focus_window;
						}
					}

					if (flag == 1)
					{
#ifdef DEBUG
						printf("focus window: %d\n",pre_window);
						printf("x %d\n", (int16_t)trans_coords->dst_x);
						printf("y %d\n", (int16_t)trans_coords->dst_y);
#endif
						activeWin = pre_window;
						setFocusCount = 0;
					}
					//reset the focus
					if ((activeWin != 0x0) && (windowattribute.width != 1920))
					{
						if (setFocusCount%20 == 0)
						{
							if (isExistWindow(activeWin))
							{
								XGetWindowAttributes(display,activeWin,&windowattribute);

								trans_coords_cookie = xcb_translate_coordinates (
																xcb_dpy,
																activeWin,
																windowattribute.root,
																-(windowattribute.border_width),
																-(windowattribute.border_width));
								xcb_translate_coordinates_reply_t *trans_coords=xcb_translate_coordinates_reply (xcb_dpy, trans_coords_cookie, NULL);

								char command[128];
								sprintf(command,"xdotool windowfocus %d",activeWin);

								int result = system(command);
#if 0
								if ((trans_coords->dst_x >= 1280) && ((trans_coords->dst_y >= 0)) && (windowattribute.width == 1920))
								{
									static int count = 0;
									printf("%d setInputFocus : %d\n",count++,activeWin);
									printf("windowattribute : x %d y %d width %d  height %d\n",trans_coords->dst_x,trans_coords->dst_y,windowattribute.width,windowattribute.height);
									if (isExistWindow(activeWin))
										XSetInputFocus(display,activeWin,RevertToNone,CurrentTime);
								}
#endif
							}
						}
					}
				}
			}
		}

		XSync(display, False);
		XCloseDisplay(display);
		xcb_disconnect(xcb_dpy);

		out:
		    if (display)
		        XCloseDisplay(display);

	    return EXIT_FAILURE;
	}
}
