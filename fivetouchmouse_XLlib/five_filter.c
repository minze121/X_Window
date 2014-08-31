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

/* Once xcb-icccm's API is stable, these should be replaced by calls to it */
# define GET_TEXT_PROPERTY(Dpy, Win, Atom)  xcb_get_property (Dpy, False, Win, Atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 200)
# define xcb_icccm_get_wm_name(Dpy, Win)  GET_TEXT_PROPERTY(Dpy, Win, XCB_ATOM_WM_NAME)

int xi_opcode;
int isMoveCursor = 0; //if accept multitouch event , don't move
int clickWindowLiveFlag = 0;
int moveWindowLiveFlag = 0;

Display * click_display = NULL;
Display * move_display = NULL;

Window activeWin = 0;
Window focus_window;
Window pre_window;

static xcb_connection_t *xcb_dpy;
static xcb_screen_t * xcb_screen;
static xcb_generic_error_t *err;

int ret_revert;

//#define DEBUG

static char * blacklist_window[] =
{
		"devel@", //gnome-terminal
		"Eclipse",
		"Mozilla Firefox",
		"gedit",
		"Qt Creator",
		"Desktop",
		"System Monitor",
		"Make Startup Disk",
		"Disks"
		"NULL"
};

static char * whitelist_window[] =
{
		"E5App",
		"TouchScreenWnd",
		"Simulate front panel",
		"Patient Information",
		"Maintenance",
		"Dialog",
		"Preset",
		"Form",
		"WorkSheet",//Report
		" ", //small Dialog
		"NULL"
};

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

/* ################## CLICK ################## */
void * create_click_thread(void * arg)
{
	clickWindowLiveFlag = 1;

	click_display = XOpenDisplay(NULL);
	char * end;
	Window win = strtoul((char *)arg,&end,0);

	//judge whether the window exists.
	if (!isExistWindow(win))
		goto OVER;

	int ret = XGrabPointer(click_display,win,False,
			 ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask|EnterWindowMask|LeaveWindowMask,
			 GrabModeAsync,GrabModeAsync,
			 win,
			 None,CurrentTime);

	if (ret != GrabSuccess)
	{
		printf("create_click_thread\n");
		printf("grab failure\n\n\n");
		goto OVER;
	}

	XWindowAttributes windowattribute;
	XEvent event;

#ifdef DEBUG
	printf("grab begin\n");
#endif

	static int count = 0;
	printf("grab begin : %d\n",count++);

	unsigned long mask = 0l;

	while(clickWindowLiveFlag)
	{
		XNextEvent(click_display,&event);

		XSendEvent(click_display,win,False,mask,&event);

		switch(event.type)
		{
		case ButtonRelease:
		{
			clickWindowLiveFlag = 0;
		}
		default:
			break;
		}
	}


OVER:
	XUngrabPointer(click_display,CurrentTime);
    XCloseDisplay(click_display);
#ifdef DEBUG
	printf("click grab over\n\n\n");
#endif
}

void handle_click_signal()
{
	if (activeWin != 0)
	{
#ifdef DEBUG
		printf("click active win :%d\n",activeWin);
#endif
		int pthread_id = 0;
		char pthread_arg[20];
		sprintf(pthread_arg,"%d",activeWin);

		int ret = pthread_create(&pthread_id,NULL,create_click_thread,(void *)pthread_arg);
	}
}

/* ################## MOVE ################## */

void * create_move_thread(void * arg)
{
#if 1
	move_display = XOpenDisplay(NULL);
	char * end;
	Window win = strtoul((char *)arg,&end,0);

	//judge whether the window exists.
	if (!isExistWindow(win))
		goto OVER;

	int ret = XGrabPointer(move_display,win,False,
			ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask|EnterWindowMask|LeaveWindowMask,
			 GrabModeAsync,GrabModeAsync,
			 win,
			 None,CurrentTime);
#if 0
	//request against
	if(ret == BadRequest)
	{
		printf("request again\n");
		XUngrabPointer(move_display,CurrentTime);
		ret = XGrabPointer(move_display,win,False,
					ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask|EnterWindowMask|LeaveWindowMask,
					 GrabModeAsync,GrabModeAsync,
					 win,
					 None,CurrentTime);
	}
#endif
	if (ret != GrabSuccess)
	{
			printf("move grab failure\n\n\n");
			goto OVER;
	}

	XEvent event;

#ifdef DEBUG
	printf("move grab begin\n");
#endif

	unsigned long mask = 0l;

	static int count = 0;
	moveWindowLiveFlag = 1;
	printf("%d move grab\n",count++);
	while(moveWindowLiveFlag)
	{
		usleep(1000);
		if (moveWindowLiveFlag)
			XNextEvent(move_display,&event);
		else
			goto OVER;

		if (moveWindowLiveFlag)
		{
			usleep(1000);
			if (moveWindowLiveFlag)
				XSendEvent(move_display,win,False,mask,&event);
			else
				goto OVER;
		}
		switch(event.type)
		{
		case ButtonPress:
		{
			//Don't handle
		}
		break;
		case ButtonRelease:
		{
			moveWindowLiveFlag = 0;
		}
		default:
			break;
		}
	}

OVER:
	XUngrabPointer(move_display,CurrentTime);
	printf("move grab over\n\n\n");
    XCloseDisplay(move_display);

#ifdef DEBUG
	printf("move grab over\n\n\n");
#endif
#endif

}

void handle_move_over_signal ()
{
	moveWindowLiveFlag = 0;
}

void handle_move_signal()
{
	if (activeWin != 0)
	{
#ifdef DEBUG
		printf("move active win :%d\n",activeWin);
#endif
		int pthread_id = 0;
		char pthread_arg[20];
		sprintf(pthread_arg,"%d",activeWin);

		int ret = pthread_create(&pthread_id,NULL,create_move_thread,(void *)pthread_arg);
	}
}

Cursor createnullcursor(Display *display,Window root)
{
    Pixmap cursormask;
    XGCValues xgc;
    GC gc;
    XColor dummycolour;
    Cursor cursor;

//    cursormask = XCreatePixmap(display, root, 1, 1, 1/*depth*/);
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
	XDefineCursor(display,root,cursor);
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

Bool get_parent_win(Display * dpy,Window w)
{
	Window root_return,parent_return,*child_return;
	unsigned int count = 0;

	int ret;
	ret = XQueryTree(dpy,w,&root_return,&parent_return,&child_return,&count);

	if (count == 0)
		return False;

	while (count != 0)
	{
		printf("E5App count %d  child_return : %d\n",count,*child_return);

		Cursor null_cursor = createnullcursor(dpy,*child_return);
		XDefineCursor(dpy,*child_return,null_cursor);
		XSync(dpy,False);

		count--;
		get_parent_win(dpy,*child_return);
		child_return++;
	}
#ifdef DEBUG
	printf("ret = %d parent_return = %dmake",ret,parent_return);
#endif

	return True;
//	return parent_return;
}

int main(int argc, char * argv[])
{

	pid_t pid,sid;
	int ret;

#if 0
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
	}else if (pid > 0)PointerWindow
#endif
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


		signal(SIGALRM,handle_click_signal);
		signal(SIGUSR1,handle_move_signal);
		signal(SIGUSR2,handle_move_over_signal);

		struct sigaction act,oact;

		focus_window = 0x0;
		pre_window = 0x0;

		Display * display = XOpenDisplay(NULL);

		xcb_dpy = xcb_connect(NULL,NULL);

		  int screen_num = DefaultScreen(display);
//		  Window rootWindow = RootWindow(display,screen_num);

		  Window rootWindow = DefaultRootWindow(display);
		XWindowAttributes windowattribute;

			static int setFocusCount = 0;
		    xcb_get_property_reply_t *prop = NULL;
		    xcb_translate_coordinates_reply_t *trans_coords = NULL;

			XGetWindowAttributes(display,rootWindow,&windowattribute);

			int E5 = 0;
			if ((screen_num == 0) && (windowattribute.width == 3200) && (windowattribute.height == 1280))
			{
				E5 = 1;
			}
#ifdef DEBUG
		printf("trans_coords : %d  %d\n",windowattribute.width,windowattribute.height);
		printf("screen number : %d\n",screen_num);
#endif
	    if (display == NULL) {
	    	fprintf(stderr, "Unable to connect to X server\n");
	    	goto out;
	    }

	    if (!XQueryExtension(display, "XInputExtension", &xi_opcode, &event, &error)) {
	        printf("X Input extension not available.\n");
	        goto out;
	    }




		while(1)
		{
			usleep(1000*50);
			if (display != NULL)
			{
//				int ret_revert;

				focus_window = 0x0;

				XGetInputFocus(display,&focus_window,&ret_revert);

				//focus_window maybe is equal to 0x1
				if (focus_window >= 0x100)
				{
					if (isExistWindow(focus_window))
					{
						XGetWindowAttributes(display,focus_window,&windowattribute);

					{
						xcb_translate_coordinates_cookie_t trans_coords_cookie = xcb_translate_coordinates (
													xcb_dpy,
													focus_window,
													windowattribute.root,
													-(windowattribute.border_width),
												    -(windowattribute.border_width));

						trans_coords=xcb_translate_coordinates_reply (xcb_dpy, trans_coords_cookie, NULL);

					}



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

					free(trans_coords);
					}

#if 1
					if ((E5) && (activeWin != 0x0) && (isExistWindow(activeWin)))
						{

							  Atom actual_type;
							  int actual_format;
							  unsigned long _nitems;
							  /*unsigned long nbytes;*/
							  unsigned long bytes_after; /* unused */
							  unsigned char *prop2 = NULL;
							  int status;

							  Window nullWindow = activeWin;
	//						  focus_window = focus_window-1;
							  if (ret_revert == RevertToPointerRoot)
							  {
								  status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "_NET_WM_NAME", False),
										  	  	  	  	      0, (~0L),False,
										  	  	  	  	      AnyPropertyType, &actual_type,&actual_format,
										  	  	  	  	      &_nitems, &bytes_after,&prop2);
							  }

							  else if (ret_revert == RevertToParent)
							  {
	//							  focus_window = get_parent_win(display,focus_window);
								  status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "WM_NAME", False),
															  0, (~0L),False,
															  AnyPropertyType, &actual_type,&actual_format,
															  &_nitems, &bytes_after,&prop2);

							  }

							  if (status == BadWindow) {
							    fprintf(stderr, "window id # 0x%lx does not exists!", nullWindow);
							    return NULL;
							  } if (status != Success) {
							    fprintf(stderr, "XGetWindowProperty failed!");
							    return NULL;
							  }

					    if (prop2 != NULL)
					    {
								int i = 0;

								while (strcmp(whitelist_window[i],"NULL") != 0)
								{
									if (strcmp(whitelist_window[i],prop2) != 0)
									{
										i++;
										continue;
									}
									else
									{
//									    printf("WM_NAME : %s\n",prop2);
									    get_parent_win(display,nullWindow);
										Cursor null_cursor = createnullcursor(display,nullWindow);
	//									change_cursor_shape(display,focus_window,null_cursor);
										XDefineCursor(display,nullWindow,null_cursor);
										XSync(display,False);
										break;
									}
								}
					    }
						}
#endif

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

								{
									xcb_translate_coordinates_cookie_t  trans_coords_cookie = xcb_translate_coordinates (
																xcb_dpy,
																activeWin,
																windowattribute.root,
																-(windowattribute.border_width),
																-(windowattribute.border_width));

									trans_coords=xcb_translate_coordinates_reply (xcb_dpy, trans_coords_cookie, NULL);

								}
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
								free(trans_coords);
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
