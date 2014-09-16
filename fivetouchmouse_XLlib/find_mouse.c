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
#include <sys/stat.h>
#include <signal.h>

#define MYSIG SIGRTMIN+1;

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"
#define MOUSE_NAME     "Mouse"

#define NAME_ELEMENT(element) [element] = #element


#define MOUSE_DEVICE_FILE "/home/devel/MOUSE_DEVICE_FILE"

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

	return filename;
}

int main()
{
	char *filename;

	filename = scan_devices();
	if (filename == NULL){
		printf("Don't find the mouse device!\n");
		return EXIT_FAILURE;
	}
	
	/******************** Save the MOUSE_DEVICE_FILE into file ********************/
	char mouse_device_info[30];
	int fd = open(MOUSE_DEVICE_FILE,O_CREAT|O_RDWR|O_TRUNC,S_IREAD|S_IWRITE);
	sprintf(mouse_device_info,"%s",filename);

	if (fd > 0)
	{
		int length = (int)strlen(mouse_device_info);
		write(fd,mouse_device_info,length);
	}
	else
	{
		printf("It can't open %s\n",MOUSE_DEVICE_FILE);
	}

	close(fd);

	chmod(MOUSE_DEVICE_FILE,S_IRWXU|S_IRWXG|S_IRWXO);

	chmod(filename,S_IRWXU|S_IRWXG|S_IRWXO);

	printf("mouse device file : %s\n",filename);

	free(filename);
}

