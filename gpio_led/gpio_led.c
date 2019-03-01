#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <mach/gpio.h>

#define DEVICE_NAME  "gpio_led"
#define my_led       IMX_GPIO_NR(7, 13)

static long gpio_led_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	switch(cmd)
	{
		case 0:
		case 1:
			if(arg > 5)
			{
				return -EINVAL;
			}
			
			gpio_set_value(my_led, !cmd);
			break;
		default: return -EINVAL;
	}
	return 0;
}

static struct file_operations gpio_led_dev_fops={
	.owner          = THIS_MODULE,
	.unlocked_ioctl = gpio_led_ioctl,
};

static struct miscdevice gpio_led_dev = {
	.minor      = MISC_DYNAMIC_MINOR,
	.name       = DEVICE_NAME,
	.fops       = &gpio_led_dev_fops,
};

static int __init gpio_led_dev_init(void)
{
	int ret;
	gpio_free(my_led);
	
	ret = gpio_request(my_led, "gpio_led"); //第一个参数，为要申请的引脚，第二个为你要定义的名字
	gpio_direction_output(my_led, 1);       //设定是什么设备，第二为改引脚为输入还是输出
	gpio_set_value(my_led, 1);              //初始化值
	
	ret = misc_register(&gpio_led_dev);
	printk(DEVICE_NAME "\tInitialized\n");
	return ret;
}

static void __exit gpio_led_dev_exit(void)
{
	gpio_free(my_led);
	misc_deregister(&gpio_led_dev);
	printk(DEVICE_NAME "\tRelease\n");
}

module_init(gpio_led_dev_init);
module_exit(gpio_led_dev_exit);
MODULE_LICENSE("GPL");
