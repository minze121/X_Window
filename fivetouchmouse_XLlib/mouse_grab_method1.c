/***********************************************************************************************************************
#Copyright  : Xi'an EDAN INSTRUMENTS,INC.
#ProjectName: E5 project
#FileName   : mouse_grab.c
#Time       : 2014.08.29
#Author     : JiBo
#Function   :  
#             
#Note       : For now, This  program is only support optical mouse, don't support usb mouse.
#***********************************************************************************************************************/



#include <stdio.h>
#include <stdint.h>
#include <linux/input.h>
#include <linux/version.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>

#define MYSIG SIGRTMIN+1;

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"
#define MOUSE_NAME     "Mouse"

#ifndef EV_SYN
#define EV_SYN 0
#endif
#ifndef SYN_MT_REPORT
#define SYN_MT_REPORT 2
#endif

#define NAME_ELEMENT(element) [element] = #element

int fd;

struct mouselocation{
	int x;
	int y;
};


static const char * const events[EV_MAX + 1] = {
	[0 ... EV_MAX] = NULL,
	NAME_ELEMENT(EV_SYN),			NAME_ELEMENT(EV_KEY),
	NAME_ELEMENT(EV_REL),			NAME_ELEMENT(EV_ABS),
	NAME_ELEMENT(EV_MSC),			NAME_ELEMENT(EV_LED),
	NAME_ELEMENT(EV_SND),			NAME_ELEMENT(EV_REP),
	NAME_ELEMENT(EV_FF),			NAME_ELEMENT(EV_PWR),
	NAME_ELEMENT(EV_FF_STATUS),		NAME_ELEMENT(EV_SW),
};

static const char * const relatives[REL_MAX + 1] = {
	[0 ... REL_MAX] = NULL,
	NAME_ELEMENT(REL_X),			NAME_ELEMENT(REL_Y),
	NAME_ELEMENT(REL_Z),			NAME_ELEMENT(REL_RX),
	NAME_ELEMENT(REL_RY),			NAME_ELEMENT(REL_RZ),
	NAME_ELEMENT(REL_HWHEEL),
	NAME_ELEMENT(REL_DIAL),			NAME_ELEMENT(REL_WHEEL),
	NAME_ELEMENT(REL_MISC),
};

static const char * const misc[MSC_MAX + 1] = {
	[ 0 ... MSC_MAX] = NULL,
	NAME_ELEMENT(MSC_SERIAL),		NAME_ELEMENT(MSC_PULSELED),
	NAME_ELEMENT(MSC_GESTURE),		NAME_ELEMENT(MSC_RAW),
	NAME_ELEMENT(MSC_SCAN),
#ifdef MSC_TIMESTAMP
	NAME_ELEMENT(MSC_TIMESTAMP),
#endif
};

static const char * const repeats[REP_MAX + 1] = {
	[0 ... REP_MAX] = NULL,
	NAME_ELEMENT(REP_DELAY),		NAME_ELEMENT(REP_PERIOD)
};

static const char * const syns[3] = {
	NAME_ELEMENT(SYN_REPORT),
	NAME_ELEMENT(SYN_CONFIG),
#ifdef SYN_MT_REPORT
	NAME_ELEMENT(SYN_MT_REPORT)
#endif
};

static const char * const * const names[EV_MAX + 1] = {
	[0 ... EV_MAX] = NULL,
	[EV_SYN] = events,			[EV_REL] = relatives,			
	[EV_MSC] = misc,			[EV_REP] = repeats,
	
};

static int set_mouse_relmove(int fd,int rel_x,int rel_y);

void send_signal()
{
	pid_t pid = getpid();	
	int signo = MYSIG;
	int i;
	union sigval mysigval;
	struct mouselocation *plocation;
	
	plocation =(struct mouselocation *)malloc(sizeof(struct mouselocation));
	plocation->x=8;
	plocation->y=9;
	
	mysigval.sival_ptr = plocation;

	printf("pid =%d\n",pid);

	if ( sigqueue(pid,signo,mysigval) == -1){
		printf("send signal failed!\n");
		return;
	} else {
		printf("send sucessful!\n");
		sleep(1);
	}

}

void sigal_mvmouse(int signum, siginfo_t * info, void *myact)
{	
	int x,y;
	
	printf("receive a signal!\n");

	x = (( struct mouselocation *)info->si_value.sival_ptr)->x;
	y = (( struct mouselocation *)info->si_value.sival_ptr)->y; 
	printf("x=%d,y=%d\n",x,y);

	set_mouse_relmove(fd,x,y);
}

void install_signal()
{
	struct sigaction myAct;
	myAct.sa_flags = SA_SIGINFO;	
	myAct.sa_sigaction = sigal_mvmouse;
	
	int signo = MYSIG;
	
	if (sigaction(signo,&myAct, NULL) <0){
		printf("install sign failed!\n");
		return ;
	}

}

/**
  **Filter for the AutoDevProbe scandir on /dev/input.
  **@param dir The current directory entry provided by scandir.
  **@return Non-zero if the given directory entry starts with "event", or zero
  **otherwise.
  **
**/
static int is_event_device(const struct dirent *dir){

	return strncmp(EVENT_DEV_NAME,dir->d_name,5) == 0;
}
/**
  *Scans all /dev/input/event*,find the mouse device file.
**/

static char * scan_devices(void)
{
	struct dirent **namelist;
	int i,ndev,devnum,count;
	char * filename =  NULL;
								              							   
	ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device,alphasort);
	if (ndev <= 0)
		return NULL;
	
	for (i = 0,count = 0; i < ndev; i++){
		char fname[64];
		int fd = -1;
		char name[256] = "???";
		
		snprintf(fname, sizeof(fname),
				"%s/%s", DEV_INPUT_EVENT, namelist[i]->d_name);
		fd = open(fname, O_RDONLY);
		if (fd < 0)
			continue;
		ioctl(fd, EVIOCGNAME(sizeof(name)), name);

//		printf("%s:   %s\n",fname,name);
		if(strstr(name,MOUSE_NAME) !=NULL){
			printf("count:%d  %s-->%s\n",count,fname,name);
			asprintf(&filename, "%s",fname);
			count++;
		}
		close(fd);
		free(namelist[i]);
	}
	//if the mouse devices >1, user can have choose the one.
	if (count >1){
		printf("Select the device event number [0-%d]: ", ndev - 1);
		scanf("%d", &devnum);

		if (devnum >= ndev || devnum < 0)
			return NULL;
		
		asprintf(&filename, "%s/%s%d",
				DEV_INPUT_EVENT, EVENT_DEV_NAME,
				devnum);

	}
//	printf("filename:%s\n",filename);
	return filename;
}
/**
  *Set the mouse device is Grab model
  *@param fd The file descriptor to the device.
  * @return 0 if the grab was successful, or 1 otherwise.
**/
static int set_grab(int fd)
{
	int ret;

	ret = ioctl(fd, EVIOCGRAB, (void*)1);

	return ret;
}





/**
 * Print device events as they come in.
 *
 * @param fd The file descriptor to the device.
 * @return 0 on success or 1 otherwise.
 */
static int grab_events(int fd)
{
	struct input_event ev[64];
	int i, rd;

	while (1) {
		rd = read(fd, ev, sizeof(struct input_event) * 64);

		if (rd < (int) sizeof(struct input_event)) {
			printf("expected %d bytes, got %d\n", (int) sizeof(struct input_event), rd);
			perror("\n error reading");
			return 1;
		}

		for (i = 0; i < rd / sizeof(struct input_event); i++) {
			printf("Event: time %ld.%06ld, ", ev[i].time.tv_sec, ev[i].time.tv_usec);

			if (ev[i].type == EV_SYN) {
				if (ev[i].code == SYN_MT_REPORT)
					printf("++++++++++++++ %s ++++++++++++\n", syns[ev[i].code]);
				else
					printf("-------------- %s ------------\n", syns[ev[i].code]);
			} else {
				printf("type %d (%s), code %d (%s), ",
					ev[i].type,
					events[ev[i].type] ? events[ev[i].type] : "?",
					ev[i].code,
					names[ev[i].type] ? (names[ev[i].type][ev[i].code] ? names[ev[i].type][ev[i].code] : "?") : "?");
				if (ev[i].type == EV_MSC && (ev[i].code == MSC_RAW || ev[i].code == MSC_SCAN))
					printf("value %02x\n", ev[i].value);
				else
					printf("value %d\n", ev[i].value);
			}
		}
		send_signal();
	}
}


/**
 * move the mouse value(relative value)
 *
 * @param fd The file descriptor to the device.
 * @param rel_x: rel x value
 * @param rel_y: rel y value
 * @return 0 on success or 1 otherwise.
 */
static int set_mouse_relmove(int fd, int rel_x,int rel_y)
{
	int ret;
	struct input_event event;



	gettimeofday(&event.time, NULL);
	event.type = EV_REL;
	event.code = REL_X;
	event.value = rel_x;
	
	ret = write(fd,&event, sizeof(event));
	if(ret <0){
		printf("write rel x error!\n");
		return 1;
	}

	event.type = EV_REL;
	event.code = REL_Y;
	event.value = rel_y;
	
	ret = write(fd,&event,sizeof(event));
	if(ret <0){
		printf("write rel y  error!\n");
		return 1;
	}

	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	
	ret = write(fd,&event,sizeof(event));
	if(ret <0){
		printf("write event  error!\n");
		return 1;
	}
}

/**
 * move the mouse value(absolute value)
 *
 * @param fd The file descriptor to the device.
 * @param rel_x: abs x value
 * @param rel_y: abs y value
 * @return 0 on success or 1 otherwise.
 */
static int set_mouse_absmove(int fd, int abs_x,int abs_y)
{
	int ret;
	struct input_event event;

	if( fd <= 0){
		printf("fd is error!\n");
		return 1;
	}

	gettimeofday(&event.time, NULL);
	event.type = EV_ABS;
	event.code = ABS_X;
	event.value = abs_x;

	ret=write(fd,&event, sizeof(event));
	if(ret <0){
		printf("write ABS_X error!\n");
		return 1;
	}


	event.type = EV_ABS;
	event.code = ABS_Y;
	event.value = abs_y;
	
	ret = write(fd,&event,sizeof(event));
	if(ret <0){
		printf("write ABS_Y  error!\n");
		return 1;
	}
	
	
	event.type = EV_SYN;
	event.code = SYN_REPORT;
	event.value = 0;
	
	ret = write(fd,&event,sizeof(event));
	if(ret <0){
		printf("write event  error!\n");
		return 1;
	}
	
}



int main()
{
	char *filename;
	pid_t pid;	

	filename = scan_devices();
	if (filename == NULL){
		printf("Don't find the mouse device!\n");
		return EXIT_FAILURE;
	}
	printf("the main filename:%s\n",filename);

	
	if ((fd = open(filename, O_RDWR )) < 0) {
		printf("open failed!\n");
		if (errno == EACCES && getuid() != 0)
			fprintf(stderr, "You do not have access to %s. Try "
					"running as root instead.\n",
					filename);
		return EXIT_FAILURE;
	}

	if (set_grab(fd)) {
		printf("set grab is falled!\n");
		close(fd);
		return EXIT_FAILURE;
		}

	install_signal();
        
	grab_events(fd);		
	

	free(filename);


}

