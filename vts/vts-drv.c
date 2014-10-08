#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>

//IOCTL DEFINE
#define TYPE_MAGIC_NR 'M'
#define MAX_IOC_NR 0x100
#define LEFT_CLICK _IOR(TYPE_MAGIC_NR,0,int)

#define VTS_MISCDEV_MINOR	183 

struct input_dev *input_dev;

static int vts_open(struct inode *inode,struct file *filp) {
	return 0;
}

static int buf[2];
//static int vts_ioctl(struct inode * inode, struct file * flip, unsigned int cmd, unsigned long arg)
static long vts_ioctl(struct file * flip, unsigned int cmd, unsigned long arg)
{
	int err = 0;

	//1 check command valid
	if (!(_IOC_TYPE(cmd) & TYPE_MAGIC_NR))
		return -EINVAL;

	if (_IOC_NR(cmd) > MAX_IOC_NR)
		return -EINVAL;

	//2. check param valid
	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		err = !access_ok(VERIFY_WRITE,(void __user *)arg,4);
	}
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		err = !access_ok(VERIFY_READ,(void __user *)arg,4);
	}

	if (err)
		return -EFAULT;
//#define BTN_LEFT		0x110
//#define BTN_RIGHT		0x111
	//implement the command
	switch(cmd)
	{
	case LEFT_CLICK:
		copy_from_user(buf, (int *)arg, sizeof(buf));
		static int count = 0;
		input_report_key(input_dev, BTN_LEFT, 1);
		input_sync(input_dev);

		input_report_key(input_dev, BTN_LEFT, 0);
		input_sync(input_dev);

//		printk("%d LEFT_CLICK %d %d\n",count++,buf[0],buf[1]);
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t vts_write(struct file *filp,const char *buf,size_t count,loff_t
		*ppos) {

	int x, y;
	int kbuf[2];

	copy_from_user(kbuf, buf, sizeof(kbuf));

	x = kbuf[0];
	y = kbuf[1];

	input_report_abs(input_dev, ABS_X, x);
	input_report_abs(input_dev, ABS_Y, y);
	input_sync(input_dev);

	return count;
}

int vts_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations vts_fops = {
	.owner   = THIS_MODULE,
	.open    = vts_open,
	.write   = vts_write,
//	.ioctl   = vts_ioctl,
//	.unlocked_ioctl   = vts_ioctl,
	.release = vts_release
};

static struct miscdevice vts_miscdev = {
	.minor		= VTS_MISCDEV_MINOR,
	.name		= "VTS",
	.nodename	= "vts",
	.fops		= &vts_fops,
};

static int __init vts_init(void)
{	
	int err;

	// Register an event device
	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		goto err3;
	}

	input_dev->name = "vts";

	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_SYN)| BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_MOUSE);
//	input_dev->keybit[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_LEFT) |BIT_MASK(BTN_MIDDLE) | BIT_MASK(BTN_RIGHT);

	input_set_abs_params(input_dev, ABS_X,  0, 3200, 0, 0);
	input_set_abs_params(input_dev, ABS_Y,  0, 1280, 0, 0);

	err = input_register_device(input_dev);
	if (err) {
		printk("%s: input_register_device fails.\n", __func__);
		goto err2;
	}

	// Register a misc device 
	err = misc_register(&vts_miscdev);
	if (err) {
		goto err1;
	}

	printk("%s OK\n", __func__);

	return 0;

err1:
	input_unregister_device(input_dev);
err2:
	input_free_device(input_dev);
err3:
	return err;
}

static void __exit vts_exit(void) {
	misc_deregister(&vts_miscdev);
	input_unregister_device(input_dev);
	input_free_device(input_dev);
}


module_init(vts_init);
module_exit(vts_exit);

MODULE_LICENSE("Dual BSD/GPL");
