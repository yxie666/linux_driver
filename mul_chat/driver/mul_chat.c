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
#define MAJOR_NUM  101   //�豸��

struct dev
{
	struct cdev devm;         //�ַ��豸
	struct semaphore sem;
	wait_queue_head_t outq;   //�ȴ����У�ʵ����������
	int flag;                 //�������ѱ�־
	char buffer[MAXNUM + 1];  //�ַ�������
	char *rd, *wr, *end       //����д��βָ��
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
    /*major��0��ȥ��ʾҪ���ں˰������Զ�����һ�����ʵĿհ׵�û��ʹ�õ����豸��
    �ں�����ɹ�����ͻ᷵�ط�������豸�ţ��������ʧ�ܻ᷵�ظ���	*/
	dev_t dev = MKDEV(major, minor);
	if(major)
	{
		//��̬�����豸���
		result = register_chrdev_region(dev, 1, "mul_chat");
	}
	else
	{
		//��̬�����豸��
		result = alloc_chrdev_region(&dev, minor, 1, "mul_chat");
		major = MAJOR(dev);
	}
	
	if(result < 0)
		return result;
	
	/*�Զ������豸�ڵ�*/
	cdev_init(&mul_chat.devm, &mul_chat_fops);
	mul_chat.devm.owner = THIS_MODULE;
	err = cdev_add(&mul_chat.devm, dev, 1);
	if(err)
		printk(KERN_INFO "error %d adding char_mem device", err);
	else
	{
		printk("mul_chat register success\n");
		sema_init(&mul_chat.sem, 1);   //��ʼ���ź���
		init_waitqueue_head(&mul_chat.outq); //��ʼ���ȴ�����
		mul_chat.rd = mul_chat.buffer;       //��ָ��
		mul_chat.wr = mul_chat.buffer;       //дָ��
		mul_chat.end = mul_chat.buffer + MAXNUM;  //������βָ��
		mul_chat.flag = 0;                   //�������ѱ�־��0
	}
	
	mul_chat_class = class_create(THIS_MODULE, "mul_chat_class");
	device_create(mul_chat_class, NULL, dev, NULL, "mul_chat");
	printk(KERN_INFO "Create devices!\n");
	return 0;
}

static int mul_chat_open(struct inode* inode, struct file *filp)
{
	try_module_get(THIS_MODULE);    //ģ�������һ
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
	unregister_chrdev_region(MKDEV(major, 0), 1);  //ע���豸
	printk(KERN_INFO "Destroy devices!\n");
}

/*ssize_t read(struct file *filp, char *buf,size_t len, loff_t *off);
���� filp���ļ�ָ��,����len������������ݳ��ȡ�
���� buf��ָ���û��ռ�Ļ�����,������������߱��潫д�������,������һ������¶������ݵĿջ�������
����off��һ��ָ��long offset type(��ƫ��������)�������ָ��,�������ָ���û����ļ��н��д�ȡ������λ�á�
����ֵ�ǡ�signed size type(�з��ŵĳߴ�����)��

��Ҫ������,��Ҫ���ں˵�ַ�ռ���û���ַ�ռ�֮�䴫�����ݡ�
������ͨ���İ취����ָ��� memcpy����������Ĳ������������ԭ��,�������ں˿ռ���ֱ��ʹ���û��ռ��ַ��
�ں˿ռ��ַ���û��ռ��ַ֮��ܴ��һ���������,�û��ռ���ڴ��ǿɱ������ġ�
���ں˷����û��ռ�ָ��ʱ,���Ӧ��ҳ������Ѳ����ڴ�����,�����Ļ��ͻ����һ��ҳ��ʧЧ
*/
static ssize_t mul_chat_read(struct file *filp, char *buf, size_t len, loff_t *off)
{
	if(wait_event_interruptible(mul_chat.outq, mul_chat.flag) != 0)  //���ɶ�ʱ������������
	{
		return -ERESTARTSYS;
	}

    /*
    down_interruptible ������һ���ź��ж�,�� down ���������źŴ��͵����̡�
    ���������¶�ϣ���ź�������;����,���п��ܽ���һ���޷�ɱ���Ľ���,��������������Ԥ�ڵĽ����
    ����,�����ź��жϽ�ʹ���ź����Ĵ����ӻ�,��Ϊ������Ҫȥ��麯��(������ down_interruptible)�Ƿ��ѱ��жϡ�
    һ����˵,���ú������� 0 ʱ��ʾ�ɹ�,���ط� 0 ʱ���ʾ����
    ������������̱��ж�,���Ͳ������ź��� , ���,Ҳ�Ͳ��ܵ��� up �����ˡ�
    ���,���ź����ĵ��͵���ͨ���������������ʽ:
    if (down_interruptible (&sem))
        return -ERESTARTSYS;
    ����ֵ -ERESTARTSYS֪ͨϵͳ�������ź��жϡ�
    ��������豸�������ں˺����������³���,���߷��� -EINTR ��Ӧ�ó���,��ȡ����Ӧ�ó�������������źŴ������ġ�
    ��Ȼ,����������ַ�ʽ�жϲ����Ļ�,��ô����Ӧ�ڷ���ǰ�����������

    ʹ��down_interruptible����ȡ�ź����Ĵ��벻Ӧ��������Ҳ��ͼ��ø��ź����ĺ���,����ͻ�����������
    ������������е�ĳ�γ��������е��ź����ͷ�ʧ�ܵĻ�(���ܾ���һ�γ����صĽ��),
    ��ô�����κλ�ȡ���ź����ĳ��Զ������������
    */	
	if(down_interruptible(&mul_chat.sem))   // P����
	{
		return -ERESTARTSYS;
	}
	
	mul_chat.flag = 0;
	printk(KERN_INFO "into the function\n");
	printk(KERN_INFO "the rd is %c\n", *mul_chat.rd);  //��ָ��
	if(mul_chat.rd < mul_chat.wr)
	{
		len = min(len, (size_t)(mul_chat.wr - mul_chat.rd));  //���¶�д����
	}
	else
	{
		len = min(len, (size_t)(mul_chat.end - mul_chat.rd));
	}
	printk(KERN_INFO "the len is %d\n", len);

	/*
    read �� write ����Ҫ���Ĺ���,�������û���ַ�ռ���ں˵�ַ�ռ�֮������������ݵĿ�����
    ������������������ں˺����ṩ��,�������ڿ��������һ���ֽ�����,��Ҳ��ÿ�� read �� write ����ʵ�ֵĺ��Ĳ���:
    unsigned long copy_to_user(void *to, const void *from,unsigned long count);
    unsigned long copy_from_user(void *to, const void *from,unsigned long count);
    ��Ȼ��Щ��������Ϊ����ͨ���� memcpy ����,�������ں˿ռ������еĴ�������û��ռ�ʱ,��Ҫ���С�ġ�
    ��Ѱַ���û��ռ��ҳ����ܵ�ǰ�������ڴ�,���Ǵ���ҳ��ʧЧ�ĳ����ʹ���ʽ���ת��˯��,ֱ����ҳ�汻������������λ�á�
    ����,��ҳ�����ӽ����ռ�ȡ��ʱ,����������ͻᷢ�����������������д��Ա��˵,
    ������Ƿ����û��ռ���κκ����������ǿ������,���ұ����ܺ�������������������ִ�С�
    ���������ʹ���ź��������Ʋ������ʵ�ԭ��.
    ���������������ò����������ں˿ռ���û��ռ�֮�俽������,���ǻ�����û��ռ��ָ���Ƿ���Ч��
    ���ָ����Ч,�Ͳ�����п���;��һ����,����ڿ���������������Ч��ַ,������Ḵ�Ʋ������ݡ�
    �������������,����ֵ�ǻ�δ��������ڴ������ֵ��
    ������������Ĵ��󷵻�,�ͻ��ڷ���ֵ��Ϊ 0 ʱ,���� -EFAULT ���û���
    ��ֵ��ζ�ŷ����˴���,��ֵָ��������ʲô����,��������<linux/errno.h>�ж��塣
    ����������һЩ����:-EINTR(ϵͳ���ñ��ж�)�� -EFAULT (��Ч��ַ)��
    */
	if(copy_to_user(buf, mul_chat.rd, len))
	{
		printk(KERN_ALERT "copy failed\n");
		/*
        up�����ź�����ֵ,�������������ڵȴ��ź���תΪ����״̬�Ľ��̡�
        ����С��ʹ���ź��������ź������������ݱ����Ƕ���������,���Ҵ�ȡ��Щ���ݵ����д��붼�������Ȼ���ź�����
        */
		up(&mul_chat.sem);
		return -EFAULT;
	}
	
	printk(KERN_INFO "the read buffer is %s\n", mul_chat.buffer);
	mul_chat.rd = mul_chat.rd + len;
	if(mul_chat.rd == mul_chat.end)
	{
		mul_chat.rd = mul_chat.buffer; //�ַ�������ѭ��
	}
	up(&mul_chat.sem);  //V����
	return len;
	
}

static ssize_t mul_chat_write(struct file* filp, const char *buf, size_t len, loff_t *off)
{
	if(down_interruptible(&mul_chat.sem))   //P����
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
		up(&mul_chat.sem);  //V����
		return -EFAULT;
	}
	printk(KERN_INFO "the write buffer is %s\n", mul_chat.buffer);
	printk(KERN_INFO "the len of buffer is %d\n", strlen(mul_chat.buffer));
	mul_chat.wr = mul_chat.wr + len;
	if(mul_chat.wr == mul_chat.end)
	{
		mul_chat.wr = mul_chat.buffer;   //ѭ��
	}
	up(&mul_chat.sem);
	//V����
	mul_chat.flag = 1; //�������������Ի��Ѷ�����
	wake_up_interruptible(&mul_chat.outq);  //���Ѷ�����
	return len;
}

module_init(mul_chat_init);
module_exit(mul_chat_exit);
MODULE_LICENSE("GPL");