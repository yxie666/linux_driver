#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/wait.h>
#include <linux/semaphore.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/device.h>
#include <linux/sched.h>

#define MAXNUM     100
#define MAJOR_NUM  101   //设备号

struct dev
{
	struct cdev devm;         //字符设备
	struct semaphore sem;
	wait_queue_head_t outq;   //等待队列，实现阻塞操作
	int flag;                 //阻塞唤醒标志
	char buffer[MAXNUM + 1];  //字符缓冲区
	char *rd, *wr, *end       //读，写，尾指针
}mul_chat;

static struct class *mul_chat_class;
static int major = MAJOR_NUM;
static int minor = 0;

static ssize_t mul_chat_read(struct file *, char*, size_t, loff_t *);
static ssize_t mul_chat_write(struct file *, const char *, size_t, loff_t *);
static int mul_chat_open(struct inode *inode, struct file *filp);
static int mul_chat_release(struct inode *inode, struct file *filp);

static struct file_operations mul_chat_fops = 
{
	.owner   = THIS_MODULE,
	.read    = mul_chat_read,
	.write   = mul_chat_write,
	.open    = mul_chat_open,
	.release = mul_chat_release,
};

static int __init mul_chat_init(void)
{
	int result = 0;
	int err = 0;
    /*major传0进去表示要让内核帮我们自动分配一个合适的空白的没被使用的主设备号
    内核如果成功分配就会返回分配的主设备号；如果分配失败会返回负数	*/
	dev_t dev = MKDEV(major, minor);
	if(major)
	{
		//静态申请设备编号
		result = register_chrdev_region(dev, 1, "mul_chat");
	}
	else
	{
		//动态分配设备号
		result = alloc_chrdev_region(&dev, minor, 1, "mul_chat");
		major = MAJOR(dev);
	}
	
	if(result < 0)
		return result;
	
	/*自动创建设备节点*/
	cdev_init(&mul_chat.devm, &mul_chat_fops);
	mul_chat.devm.owner = THIS_MODULE;
	err = cdev_add(&mul_chat.devm, dev, 1);
	if(err)
		printk(KERN_INFO "error %d adding char_mem device", err);
	else
	{
		printk("mul_chat register success\n");
		sema_init(&mul_chat.sem, 1);   //初始化信号量
		init_waitqueue_head(&mul_chat.outq); //初始化等待队列
		mul_chat.rd = mul_chat.buffer;       //读指针
		mul_chat.wr = mul_chat.buffer;       //写指针
		mul_chat.end = mul_chat.buffer + MAXNUM;  //缓冲区尾指针
		mul_chat.flag = 0;                   //阻塞唤醒标志置0
	}
	
	mul_chat_class = class_create(THIS_MODULE, "mul_chat_class");
	device_create(mul_chat_class, NULL, dev, NULL, "mul_chat");
	printk(KERN_INFO "Create devices!\n");
	return 0;
}

static int mul_chat_open(struct inode* inode, struct file *filp)
{
	try_module_get(THIS_MODULE);    //模块计数加一
	printk("This chrdev is in open\n");
	return 0;
}

static int mul_chat_release(struct inode *inode, struct file *filp)
{
	module_put(THIS_MODULE);
	printk("This chrdev is in release\n");
	return 0;
}

static void __exit mul_chat_exit(void)
{
	device_destroy(mul_chat_class, MKDEV(major, 0));
	class_destroy(mul_chat_class);
	cdev_del(&mul_chat.devm);
	unregister_chrdev_region(MKDEV(major, 0), 1);  //注销设备
	printk(KERN_INFO "Destroy devices!\n");
}

/*ssize_t read(struct file *filp, char *buf,size_t len, loff_t *off);
参数 filp是文件指针,参数len是请求传输的数据长度。
参数 buf是指向用户空间的缓冲区,这个缓冲区或者保存将写入的数据,或者是一个存放新读入数据的空缓冲区。
最后的off是一个指向“long offset type(长偏移量类型)”对象的指针,这个对象指明用户在文件中进行存取操作的位置。
返回值是“signed size type(有符号的尺寸类型)”

主要问题是,需要在内核地址空间和用户地址空间之间传输数据。
不能用通常的办法利用指针或 memcpy来完成这样的操作。由于许多原因,不能在内核空间中直接使用用户空间地址。
内核空间地址与用户空间地址之间很大的一个差异就是,用户空间的内存是可被换出的。
当内核访问用户空间指针时,相对应的页面可能已不在内存中了,这样的话就会产生一个页面失效
*/
static ssize_t mul_chat_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	if(wait_event_interruptible(mul_chat.outq, mul_chat.flag) != 0)  //不可读时，阻塞读进程
	{
		return -ERESTARTSYS;
	}

    /*
    down_interruptible 可以由一个信号中断,但 down 不允许有信号传送到进程。
    大多数情况下都希望信号起作用;否则,就有可能建立一个无法杀掉的进程,并产生其他不可预期的结果。
    但是,允许信号中断将使得信号量的处理复杂化,因为我们总要去检查函数(这里是 down_interruptible)是否已被中断。
    一般来说,当该函数返回 0 时表示成功,返回非 0 时则表示出错。
    如果这个处理过程被中断,它就不会获得信号量 , 因此,也就不能调用 up 函数了。
    因此,对信号量的典型调用通常是下面的这种形式:
    if (down_interruptible (&sem))
        return -ERESTARTSYS;
    返回值 -ERESTARTSYS通知系统操作被信号中断。
    调用这个设备方法的内核函数或者重新尝试,或者返回 -EINTR 给应用程序,这取决于应用程序是如何设置信号处理函数的。
    当然,如果是以这种方式中断操作的话,那么代码应在返回前完成清理工作。

    使用down_interruptible来获取信号量的代码不应调用其他也试图获得该信号量的函数,否则就会陷入死锁。
    如果驱动程序中的某段程序对其持有的信号量释放失败的话(可能就是一次出错返回的结果),
    那么其他任何获取该信号量的尝试都将阻塞在那里。
    */	
	if(down_interruptible(&mul_chat.sem))   // P操作
	{
		return -ERESTARTSYS;
	}
	
	mul_chat.flag = 0;
	printk(KERN_INFO "into the function\n");
	printk(KERN_INFO "the rd is %c\n", *mul_chat.rd);  //读指针
	if(mul_chat.rd < mul_chat.wr)
	{
		len = min(len, (size_t)(mul_chat.wr - mul_chat.rd));  //更新读写长度
	}
	else
	{
		len = min(len, (size_t)(mul_chat.end - mul_chat.rd));
	}
	printk(KERN_INFO "the len is %d\n", len);

	/*
    read 和 write 代码要做的工作,就是在用户地址空间和内核地址空间之间进行整段数据的拷贝。
    这种能力是由下面的内核函数提供的,它们用于拷贝任意的一段字节序列,这也是每个 read 和 write 方法实现的核心部分:
    unsigned long copy_to_user(void *to, const void *from,unsigned long count);
    unsigned long copy_from_user(void *to, const void *from,unsigned long count);
    虽然这些函数的行为很像通常的 memcpy 函数,但当在内核空间内运行的代码访问用户空间时,则要多加小心。
    被寻址的用户空间的页面可能当前并不在内存,于是处理页面失效的程序会使访问进程转入睡眠,直到该页面被传送至期望的位置。
    例如,当页面必须从交换空间取回时,这样的情况就会发生。对于驱动程序编写人员来说,
    结果就是访问用户空间的任何函数都必须是可重入的,并且必须能和其他驱动程序函数并发执行。
    这就是我们使用信号量来控制并发访问的原因.
    这两个函数的作用并不限于在内核空间和用户空间之间拷贝数据,它们还检查用户空间的指针是否有效。
    如果指针无效,就不会进行拷贝;另一方面,如果在拷贝过程中遇到无效地址,则仅仅会复制部分数据。
    在这两种情况下,返回值是还未拷贝完的内存的数量值。
    如果发现这样的错误返回,就会在返回值不为 0 时,返回 -EFAULT 给用户。
    负值意味着发生了错误,该值指明发生了什么错误,错误码在<linux/errno.h>中定义。
    比如这样的一些错误:-EINTR(系统调用被中断)或 -EFAULT (无效地址)。
    */
	if(copy_to_user(buf, mul_chat.rd, len))
	{
		printk(KERN_ALERT "copy failed\n");
		/*
        up递增信号量的值,并唤醒所有正在等待信号量转为可用状态的进程。
        必须小心使用信号量。被信号量保护的数据必须是定义清晰的,并且存取这些数据的所有代码都必须首先获得信号量。
        */
		up(&mul_chat.sem);
		return -EFAULT;
	}
	
	printk(KERN_INFO "the read buffer is %s\n", mul_chat.buffer);
	mul_chat.rd = mul_chat.rd + len;
	if(mul_chat.rd == mul_chat.end)
	{
		mul_chat.rd = mul_chat.buffer; //字符缓冲区循环
	}
	up(&mul_chat.sem);  //V操作
	return len;
	
}

static ssize_t mul_chat_write(struct file* filp, const char *buf, size_t len, loff_t *off)
{
	if(down_interruptible(&mul_chat.sem))   //P操作
	{
		return -ERESTARTSYS;
	}
	if(mul_chat.rd <= mul_chat.wr)
	{
		len = min(len, (size_t)(mul_chat.end - mul_chat.wr));
	}
	else
	{
		len = min(len, (size_t)(mul_chat.rd - mul_chat.wr - 1));
	}
	printk(KERN_INFO "the write len is %d\n", len);
	
	if(copy_from_user(mul_chat.wr, buf, len))
	{
		up(&mul_chat.sem);  //V操作
		return -EFAULT;
	}
	printk(KERN_INFO "the write buffer is %s\n", mul_chat.buffer);
	printk(KERN_INFO "the len of buffer is %d\n", strlen(mul_chat.buffer));
	mul_chat.wr = mul_chat.wr + len;
	if(mul_chat.wr == mul_chat.end)
	{
		mul_chat.wr = mul_chat.buffer;   //循环
	}
	up(&mul_chat.sem);
	//V操作
	mul_chat.flag = 1; //条件成立，可以唤醒读进程
	wake_up_interruptible(&mul_chat.outq);  //唤醒读进程
	return len;
}

module_init(mul_chat_init);
module_exit(mul_chat_exit);
MODULE_LICENSE("GPL");