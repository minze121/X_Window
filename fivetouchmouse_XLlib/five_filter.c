/*
 *Function:
 *		1.Click
 *		2.Move
 *		3.Focus
 *		4.Null Cursor
 *
 *Implement:
 *		1.XGetInputFocus
 *		2.XGrabPointer
 *		3.XGetWindowAttributes
 *		4.XQueryTree
 *
 *Tool:
 *		1.xdotool
 */

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
int E5 = 0;
int isCanGrab = 1;

Cursor null_cursor ;

Display * click_display;
Display * move_display;

Window activeWin = 0;
Window focus_window = 0;
Window pre_window = 0;

Window tenTouchWId = 0;
Window fiveTouchWId = 0;

#define MAX 20
Window activeWinStack[MAX];
int currentActiveWindowNumber = 0;

static xcb_connection_t *xcb_dpy;
static xcb_screen_t * xcb_screen;
static xcb_generic_error_t *err;

int ret_revert;

#define DEBUG
#define AddMoveControl

pthread_mutex_t click_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t move_mutex = PTHREAD_MUTEX_INITIALIZER;
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
		"Simulate front panel",
		"Patient Information",
		"Maintenance",
		"Dialog",
		"Preset",
		"Form",
		"WorkSheet",//Report
		" ", //small Dialog
		"Import exam path",
		"Export exam path",
		"Open Directory",
		"Tools for Engineer",
		"Print Preview",
		"NULL"
};

static char * message_window[] =
{
		"Message",
		"NULL"
};
static char * touch_device_name[] =
{
		"TouchScreenWnd",
		"touchPadApp",
		"NULL"
};

static char * focus_window_name[] =
{
		"E5App",
		"Simulate front panel",
		"Patient Information",
		"Maintenance",
		"Dialog",
		"Preset",
		"Form",
		"WorkSheet",//Report
		" ", //small Dialog
		"Import exam path",
		"Export exam path",
		"Open Directory",
		"Tools for Engineer",
		"Print Preview",
		"NULL"
};

static char * move_window_name[] =
{
		"E5App",
		"Simulate front panel",
		"Patient Information",
		"Maintenance",
		"Dialog",
		"Preset",
		"Form",
		"WorkSheet",//Report
		" ", //small Dialog
		"Import exam path",
		"Export exam path",
		"Open Directory",
		"Tools for Engineer",
		"Print Preview",
		"NULL"
};

int isExistWindow(Window win)
{
	int flag = 1;
	xcb_get_geometry_cookie_t gg_cookie = xcb_get_geometry (xcb_dpy, win);

	xcb_get_geometry_reply_t * geometry = xcb_get_geometry_reply(xcb_dpy, gg_cookie, &err);

	if (!geometry) {
		if (currentActiveWindowNumber > 0)
		{

			int i = 0;
			for(i; i<currentActiveWindowNumber;i++)
			{
				//find the valid Window and then delete
				if (activeWinStack[i] == win)
				{
					currentActiveWindowNumber--;
					for (i;i<currentActiveWindowNumber;i++)
						activeWinStack[i] = activeWinStack[i+1];


					activeWin = activeWinStack[currentActiveWindowNumber-1];
					printf("get Older Active window : 0x%x\n",(int)activeWin);
					break;
				}
			}
		}


		printf("Can't find active window : 0x%x\n",(int)win);

		flag = 0;
	}
	free(geometry);

	return flag;
}

int isExpectWindow(Display * display,Window win,char * windowList[])
{
	if (isExistWindow(win))
	{
		//Get Window Attributes
		Atom actual_type;
		int actual_format;
		unsigned long _nitems;
		/*unsigned long nbytes;*/
		unsigned long bytes_after; /* unused */
		unsigned char *wm_name = NULL;

		int status = 0;

		Window nullWindow = win;

		//_NET_WM_NAME
		{
			status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "_NET_WM_NAME", False),
					  0, (~0L),False,
					  AnyPropertyType, &actual_type,&actual_format,
					  &_nitems, &bytes_after,&wm_name);

			if (status == BadWindow)
			{
				fprintf(stderr, "window id # 0x%lx does not exists!", nullWindow);
				return 0;
			}
			if (status != Success)
			{
				fprintf(stderr, "XGetWindowProperty failed!");
				return 0;
			}

			if (wm_name != NULL)
			{
				int i = 0;

				while (strcmp(windowList[i],"NULL") != 0)
				{
					if (strcmp(windowList[i],wm_name) != 0)
					{
						i++;
						continue;
					}
					else
					{
						return 1;
					}
				}
			}
		}
#if 0
		//WM_NAME
		{
			status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "WM_NAME", False),
					  0, (~0L),False,
					  AnyPropertyType, &actual_type,&actual_format,
					  &_nitems, &bytes_after,&wm_name);

			if (status == BadWindow)
			{
				fprintf(stderr, "window id # 0x%lx does not exists!", nullWindow);
				return 0;
			}
			if (status != Success)
			{
				fprintf(stderr, "XGetWindowProperty failed!");
				return 0;
			}

			if (wm_name != NULL)
			{
				int i = 0;

				while (strcmp(windowList[i],"NULL") != 0)
				{
					if (strcmp(windowList[i],wm_name) != 0)
					{
						i++;
						continue;
					}
					else
					{
						return 1;
					}
				}
			}
		}
#endif
	}
	return 0;
}

int isEUPWindow(Display * display,Window win)
{
	if (isExistWindow(win))
	{
		//Get Window Attributes
		Atom actual_type;
		int actual_format;
		unsigned long _nitems;
		/*unsigned long nbytes;*/
		unsigned long bytes_after; /* unused */

		int status = 0;

		Window nullWindow = win;

		//_NET_WM_NAME
		{
			unsigned char *wm_name = NULL;
			status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "_NET_WM_NAME", False),
					  0, (~0L),False,
					  AnyPropertyType, &actual_type,&actual_format,
					  &_nitems, &bytes_after,&wm_name);

			if (status == BadWindow)
			{
				fprintf(stderr, "window id # 0x%lx does not exists!", nullWindow);
			}
			if (status != Success)
			{
				fprintf(stderr, "XGetWindowProperty failed!");
			}

			if ((wm_name != NULL) && (strncmp("EUP",wm_name,3) == 0))
			{
				printf("	2 _NET_WM_NAME success : %s\n",wm_name);
				return 1;
			}
		}

		//WM_NAME
		{
			unsigned char *wm_name = NULL;

			status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "WM_NAME", False),
					  0, (~0L),False,
					  AnyPropertyType, &actual_type,&actual_format,
					  &_nitems, &bytes_after,&wm_name);

			if (status == BadWindow)
			{
				fprintf(stderr, "window id # 0x%lx does not exists!", nullWindow);
			}
			if (status != Success)
			{
				fprintf(stderr, "XGetWindowProperty failed!");
			}

			if ((wm_name != NULL) && (strncmp("EUP",wm_name,3) == 0))
			{
				printf("	1 WM_NAME success : %s\n",wm_name);
				return 1;
			}
		}
	}
	else
	{
		printf("EUP dont exist\n");
	}
	return 0;
}

int getWindowName(Display * display,Window win,char * name)
{
	if (isExistWindow(win))
	{
		//Get Window Attributes
		Atom actual_type;
		int actual_format;
		unsigned long _nitems;
		/*unsigned long nbytes;*/
		unsigned long bytes_after; /* unused */
		unsigned char *wm_name = NULL;

		int status = 0;

		Window nullWindow = win;

		//_NET_WM_NAME
		{
			status = XGetWindowProperty(display,nullWindow,  XInternAtom(display, "_NET_WM_NAME", False),
					  0, (~0L),False,
					  AnyPropertyType, &actual_type,&actual_format,
					  &_nitems, &bytes_after,&wm_name);

			if (status == BadWindow)
			{
				fprintf(stderr, "window id # 0x%lx does not exists!", nullWindow);
				return 0;
			}
			if (status != Success)
			{
				fprintf(stderr, "XGetWindowProperty failed!");
				return 0;
			}

			if ((wm_name != NULL) && (strlen(wm_name) > 0))
			{
				strcpy(name,wm_name);
				return 1;
			}
		}
	}
	return 0;
}
/* ################## create null cursor ################## */
Cursor createnullcursor(Display *display)
{
	Cursor cursor;
	Pixmap cursormask;
	XGCValues xgc;
	GC gc;
	XColor dummycolour;

	cursormask = XCreatePixmap(display, XRootWindow(display,0), 1, 1, 1/*depth*/);
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

void change_cursor_shape(Display *display,Window root)
{
	XDefineCursor(display,root,null_cursor);
	XSync(display,False);
}


/* ################## CLICK ################## */
void * create_click_thread(void * arg)
{
	moveWindowLiveFlag = 0;

	pthread_mutex_lock(&click_mutex);
	char name[240];
	click_display = XOpenDisplay(NULL);
	Window win = (Window)arg;

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
		printf("create_click_thread 0x%x\n",(int)win);
		printf("grab failure\n\n\n");
		goto OVER;
	}

	XEvent event;

#ifdef DEBUG
	printf("grab begin\n");
#endif

	unsigned long mask = 0l;
	clickWindowLiveFlag = 1;

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
#ifdef DEBUG

	if (getWindowName(click_display,win,name))
		printf("click grab over : %s\n\n\n",name);
	else
		printf("click grab over\n\n\n");
#endif
	XUngrabPointer(click_display,CurrentTime);
    XCloseDisplay(click_display);

	pthread_mutex_unlock(&click_mutex);
}

void handle_click_signal()
{
	if ((activeWin != 0) && (isCanGrab == 1))
	{
#ifdef DEBUG
		printf("click active win : 0x%x\n",(int)activeWin);
#endif
		pthread_t pthread_id = 0;

		int ret = pthread_create(&pthread_id,NULL,create_click_thread,(void *)activeWin);
	}
}

/* ################## MOVE ################## */
pthread_t move_pthread_id = 0;

void * create_move_thread(void * arg)
//void create_move_thread( )
{
	clickWindowLiveFlag = 0;

	pthread_mutex_lock(&move_mutex);
	char name[240];
	move_display = XOpenDisplay(NULL);
	Window win = (Window)arg;

#ifdef AddMoveControl
	if (!isExpectWindow(move_display,win,move_window_name))
		goto OVER;
#endif

	//judge whether the window exists.
	if (!isExistWindow(win))
		goto OVER;

	int ret = XGrabPointer(move_display,win,False,
			ButtonPressMask|ButtonReleaseMask|PointerMotionMask|FocusChangeMask|EnterWindowMask|LeaveWindowMask,
			 GrabModeAsync,GrabModeAsync,
			 win,
			 None,CurrentTime);

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

	int count = 0;

	moveWindowLiveFlag = 1;

	while(moveWindowLiveFlag)
	{
		XPeekEvent(move_display,&event);//If empty,blocking
		if (XPending(move_display) >= 2)
		{
			XNextEvent(move_display,&event);
			XSendEvent(move_display,win,False,mask,&event);
			count = 0;
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
		else if (XPending(move_display) == 1)
		{
			count++;
			if (count == 100)
			{
				moveWindowLiveFlag = 0;
				XNextEvent(move_display,&event);
				XSendEvent(move_display,win,False,mask,&event);
				count = 0;
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
			usleep(200);
		}

		XFlush(move_display);
	}

	static int i = 0;
OVER:
#ifdef DEBUG

	if (getWindowName(move_display,win,name))
		printf("%d  move grab over : %s\n\n\n",i++,name);
	else
		printf("%d move grab over\n\n\n",i++);
#endif
	XUngrabPointer(move_display,CurrentTime);
    XCloseDisplay(move_display);

	pthread_mutex_unlock(&move_mutex);
}

void handle_move_signal()
{
	if ((activeWin != 0) && (isCanGrab == 1))
	{
#ifdef DEBUG
		printf("move active win : 0x%x\n",(int)activeWin);
#endif
		int ret = pthread_create(&move_pthread_id,NULL,create_move_thread,(void *)activeWin);
	}
}

void * create_null_cursor_thread(void * arg)
{
	Display * display  = XOpenDisplay(NULL);

	while(1)
	{
		usleep(1000*200);
		//Set the null cursor
		if ((E5) && (activeWin != 0x0) && (isExistWindow(activeWin)))
		{
			  if (isExpectWindow(display,activeWin,whitelist_window))
			  {
					//Set the null cursor
					change_cursor_shape(display,activeWin);
					//Set the child null cursor
					get_child_win(display,activeWin);
			  }
		}
	}
	XCloseDisplay(display);
}

Bool get_child_win(Display * dpy,Window w)
{
	Window hadnoexist_window = 0;
	if (isExistWindow (w))
	{
		Window root_return,parent_return,*child_return = NULL,*child_backup_return = NULL;
		unsigned int count = 0;

		int ret;
		ret = XQueryTree(dpy,w,&root_return,&parent_return,&child_return,&count);

		if (count == 0)
		{
			if (child_return != NULL)
				free(child_return);

			return False;
		}

		child_backup_return = child_return;
		while (count != 0)
		{
//			printf("	count %d  child_return : 0x%xd\n",count,(int)*child_return);
			change_cursor_shape(dpy,*child_return);

			count--;
			get_child_win(dpy,*child_return);
			child_return++;
		}

		if (child_backup_return != NULL)
			free(child_backup_return);

		return True;
	}
	else
		return False;
}

Window get_parent_win(Display * dpy,Window w)
{
	Window root_return = 0x0,parent_return = 0x0,*child_return = NULL;
	unsigned int count = 0;

	if (isExistWindow (w))
	{
		int ret;
		ret = XQueryTree(dpy,w,&root_return,&parent_return,&child_return,&count);
		if ((count != 0) && (child_return))
			free(child_return);

	}
	return parent_return;
}

int main(int argc, char * argv[])
{

	pid_t pid,sid;
	int ret;

	/******************** Save the pid into file ********************/
	char pidInfo[20];
	int fd = open(PID_FILE,O_CREAT|O_RDWR|O_TRUNC,S_IREAD|S_IWRITE);
	sprintf(pidInfo,"%d",getpid());
	if (fd > 0)
	{
#ifdef DEBUG
		printf("pidInfo : %s\n",pidInfo);
#endif
		int length = (int)strlen(pidInfo);
		write(fd,pidInfo,length);
	}
	else
	{
		printf("It can't open %s\n",PID_FILE);
	}
	close(fd);


	/******************** Install signal ********************/
	signal(SIGALRM,handle_click_signal);
	signal(SIGUSR1,handle_move_signal);


	/******************** X Window : Init ********************/

	int error = 0;
	int event = 0;
	int status = 0;
	int focus_count = 0;

	XWindowAttributes windowattribute;

	focus_window = 0x0;
	pre_window = 0x0;

	//Open the display and XCB
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

	null_cursor = createnullcursor(display);

	//judge whether E5
	int screen_num = DefaultScreen(display);

	Window rootWindow = DefaultRootWindow(display);

	static int setFocusCount = 0;
	xcb_get_property_reply_t *prop = NULL;

	XGetWindowAttributes(display,rootWindow,&windowattribute);

	if ((screen_num == 0) && (windowattribute.width == 3200) && (windowattribute.height == 1280))
	{
		E5 = 1;
	}
#ifdef DEBUG
	printf("trans_coords : %d  %d\n",windowattribute.width,windowattribute.height);
	printf("screen number : %d\n",screen_num);
#endif
	int startNullCursorThreadFlag = 0;
	int revertInputFocus = 0;
	char command[128];
	int result;

	while(1)
	{
//		usleep(1000*50);
		usleep(1000*5);
		if (E5 && (display != NULL))
		{
			focus_window = 0x0;

			revertInputFocus = XGetInputFocus(display,&focus_window,&ret_revert);

			//focus_window maybe is equal to 0x1
			if ((focus_window >= 0x100))
			{
				if (isExistWindow(focus_window))
				{
#if 0
					if (revertInputFocus == RevertToPointerRoot)
					{
						focus_window = get_parent_win(display,focus_window);
					}
#endif

					XGetWindowAttributes(display,focus_window,&windowattribute);

					xcb_translate_coordinates_cookie_t trans_coords_cookie = xcb_translate_coordinates (
												xcb_dpy,
												focus_window,
												windowattribute.root,
												-(windowattribute.border_width),
												-(windowattribute.border_width));

					xcb_translate_coordinates_reply_t * trans_coords = NULL;

					trans_coords=xcb_translate_coordinates_reply (xcb_dpy, trans_coords_cookie, NULL);

					if (trans_coords == NULL)
						printf("trans_coords null \n ");

					isCanGrab = 1;
					//The window is in Main Screen
//					if (isExpectWindow(display,focus_window,whitelist_window) && isExistWindow(focus_window))
					if ((trans_coords) && (trans_coords->dst_x >= 1280) && (pre_window != focus_window) )
					{
						pre_window = focus_window;
						activeWin = pre_window;
						setFocusCount = 0;
						focus_count = 0;
						if (currentActiveWindowNumber < MAX)
						{
							activeWinStack[currentActiveWindowNumber] = activeWin;
							currentActiveWindowNumber++;
						}

						if (startNullCursorThreadFlag == 0)
						{
							pthread_t pthread_id = 0;

							int ret = pthread_create(&pthread_id,NULL,create_null_cursor_thread,NULL);

							startNullCursorThreadFlag = 1;
						}
					}
					//Handle EUP Window
					else if (isEUPWindow(display,focus_window))
					{
						isCanGrab == 0;
						//Set the null cursor
						change_cursor_shape(display,focus_window);

						//Get Focus and Raise the window.
						while(isEUPWindow(display,focus_window))
						{
							isCanGrab == 0;
//							sprintf(command,"xdotool windowfocus %d",(int)focus_window);
//							result = system(command);
//							sprintf(command,"xdotool windowraise %d",(int)focus_window);
//							result = system(command);

							sprintf(command,"xdotool windowactivate %d",(int)focus_window);
							result = system(command);
//							usleep(1000*100);
						}
						isCanGrab == 1;
					}
					//Handle the messagebox
					else if (isExpectWindow(display,focus_window,message_window))
					{
						isCanGrab == 0;
						while(isExpectWindow(display,focus_window,message_window))
						{
							isCanGrab == 0;
							printf("Messsage BOS\n");
//							sprintf(command,"xdotool windowfocus %d",(int)focus_window);
//							result = system(command);
//							sprintf(command,"xdotool windowraise %d",(int)focus_window);
//							result = system(command);
							sprintf(command,"xdotool windowactivate %d",(int)focus_window);
							result = system(command);
							usleep(1000*100);
						}
						isCanGrab == 1;
					}




					if (trans_coords)
						free(trans_coords);

					//Touch Device
				    if (isExpectWindow(display,focus_window,touch_device_name))
				    {
				    	//Set the null cursor
						change_cursor_shape(display,focus_window);
				    }

					//reset the focus
					if ((activeWin != 0x0) && (windowattribute.width != 1920))
					{
						focus_count++;
						//optimize
						if (focus_count%5  == 0)
						{
							if (isExpectWindow(display,activeWin,focus_window_name) && isExistWindow(activeWin))
							{
								sprintf(command,"xdotool windowfocus %d",(int)activeWin);
								result = system(command);
//								sprintf(command,"xdotool windowraise %d",(int)activeWin);
//								result = system(command);
//								sprintf(command,"xdotool windowactivate %d",(int)activeWin);
//								result = system(command);
							}
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
