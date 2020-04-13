#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define MYDEV "temp_cdev"

static uint16_t temperature;
static DEFINE_MUTEX (temp_mutex);
static dev_t temp_dev;
static struct cdev *temp_cdev;
static struct class *temp_class;
static unsigned int count = 1;

	/* Temp sensor commands */
#define SKIP_ROM		0xCC
#define READ_ROM		0x33
#define CONVERT_T		0x44
#define READ_SCRATHPAD	0xBE

	/* Temp sensor configs */
#define PIN				4
#define GPIO_NAME		TEMP_PIN

#define WQ_NAME			"read_temperature_sensor"
static void work_handler(struct work_struct *work);

static struct workqueue_struct *led_wq;
static DECLARE_WORK(led_w, work_handler);

	/* Temp char dev */
static int temp_open(struct inode *inode, struct file *file);
static int temp_release(struct inode *inode, struct file *file);
static ssize_t temp_read(struct file *file, char __user *buf, size_t lbuf, loff_t *ppos);

static const struct file_operations temp_fops = {
	.owner = THIS_MODULE,
	.read = temp_read,
	.open = temp_open,
	.release = temp_release,
};

static int temp_open(struct inode *inode, struct file *file)
{
	static int counter = 1;
	
	pr_info("%s - successfully opened with MAJOR number: %d, MINOR number: %d\n", MYDEV, imajor(inode), iminor(inode));
	pr_info("%s being opened %d times since loaded, ref=%d\n", MYDEV, counter, module_refcount(THIS_MODULE));
	counter++;

	return 0;
}

static int temp_release(struct inode *inode, struct file *file)
{
	pr_info("Closing %s!\n", MYDEV);
	return 0;
}

static ssize_t temp_read(struct file *file, char __user *buf, size_t lbuf, loff_t *ppos)
{
	int nbytes = 0;
	int error = 0;

	if (lbuf > sizeof(uint16_t)) error--;

	if (!error) {
		int ret = queue_work(led_wq, &led_w);
		if (!ret) {
			pr_err("Delayed work not queued\n");
			error--;
		}
	}

	if (!error) {
		mutex_lock(&temp_mutex);
		nbytes = copy_to_user(buf, (void*)&temperature, sizeof(uint16_t));
		pr_info("READING nbytes:%d, temperature 0x%x\n", nbytes, temperature);
		mutex_unlock(&temp_mutex);
	}

	return nbytes;
}

	/* Temp driver logic */
static void bus_pin_init(void)
{
	int ret;
	
	/* Initialization */
	ret = gpio_request(PIN, "GPIO_NAME");
	if (ret) {
		pr_err("request GPIO %d failed\n", PIN);
	}

	/* Setting GPIO function */
	ret = gpio_direction_input(PIN);
	if (ret) {
		pr_err("gpio_direction_input GPIO %d failed\n", PIN);
	}
}

static int master_reset(void)
{
	static int rc = 0;
	
	gpio_direction_output(PIN, 0); 	/* pull the bus low */
	udelay(480);
	gpio_direction_output(PIN, 1); 	/* pull the bus high */
	gpio_direction_input(PIN);		/* release the bus */
	udelay(60); 					/* wait for the maximum DS18B20 reaction time */
	
	if (gpio_get_value(PIN) != 0) {
		if (rc > 4) {							/* try maximum 4 times */
			pr_info("DS18B20 NOT reset, stop trying\n");
			return 1;
		}
		
		pr_info("DS18B20 NOT reset, try again in 420 milisecond\n");
		udelay(480);							/* wait for the maximum DS18B20 signal time before retry reset */
		queue_work(led_wq, &led_w);			/* try again */
	}
	
	udelay(480);
	
	return 0;
}

static void write_bit(uint8_t bit)
{
	if (bit) {
		gpio_direction_output(PIN, 0); 	/* pull the bus low */
		udelay(1);			/* for at least 1 microsecond */
		gpio_direction_input(PIN);	/* release the bus */
		udelay(61); 			/* wait for the min pull up resistor time required by DS18B20*/
	}
	else {
		gpio_direction_output(PIN, 0); 	/* pull the bus low */
		udelay(60);			/* for at least 60 microsecond */
		gpio_direction_input(PIN);	/* release the bus */
		udelay(2); 			/* wait for enough time for the resistor to pull up */
	}
}

static uint8_t read_bit(void)
{
	uint8_t bit;
	gpio_direction_output(PIN, 0); 	/* pull the bus low */
	udelay(1);			/* for 2 microsecond */
	gpio_direction_input(PIN);	/* release the bus */
	udelay(13);
	
	if (gpio_get_value(PIN)) {	/* sample the bus for 0 if DS18B20 give 0 (stuck the bus low) and 1 if sensor give 1 (release the bus) */
		bit = 1;
		udelay(47);		/* wait for DS18B20 to release the buss */
	}
	else {
		bit = 0;
		udelay(47);		/* 47 more microseconds to be sure the bus is pulled up by the resistor and not touched by DS18B20 */
	}
	return bit;
}	

static void write_command(uint8_t command)
{
	int i;
	int mask = 1;

	for (i = 0; i < 8; i++) write_bit((command >> i) & mask);
}

static uint64_t read_rom(void)
{
	int i;
	uint64_t rom;
	rom = 0;
	
	for (i = 0; i < 64; i++) rom |= (read_bit() << i);
	pr_err("ROM VALUE: %ld\n", (long)rom); /*not an err just for readability in dmesg */
	
	return rom;
}	

static void read_temperature(void)
{
	int i;

	mutex_lock(&temp_mutex);
	temperature = 0;
	for (i = 0; i < 16; i++) temperature |= (read_bit() << i);
	pr_err("TEMPERATURE VALUE: 0x%x\n", temperature); /* raw hex value in kernel module only, not an err just for readability in dmesg */
	mutex_unlock(&temp_mutex);
	
	return;
}

static void work_handler(struct work_struct *work)
{
	/* read ROM */
	if (master_reset()) return;
	write_command(READ_ROM);
	read_rom();
	mdelay(1);
	
	/* init convertion */
	if (master_reset()) return;
	write_command(SKIP_ROM);
	write_command(CONVERT_T);

	while (read_bit() == 0) mdelay(1);
	
	/* when ready, read temp */
	if (master_reset()) return;
	write_command(SKIP_ROM);
	write_command(READ_SCRATHPAD);
	read_temperature();
	if (master_reset()) return;
}

static int __init temp_init(void)
{
	int error = 0;
	pr_info("Temp Module Init\n");

	if (alloc_chrdev_region(&temp_dev, 0, count, MYDEV) < 0)
	{
		pr_info("Region allocation failed\n");
		return -1;
	}

	if (!(temp_cdev = cdev_alloc())) 
	{
		pr_info("cdev_alloc() failed\n");
		unregister_chrdev_region(temp_dev, count);
		return -1;
	}	
	cdev_init(temp_cdev, &temp_fops);
	if (cdev_add(temp_cdev, temp_dev, count) < 0) 
	{
		pr_info("%s\n", "cdev_add failed");
		cdev_del(temp_cdev);
		unregister_chrdev_region(temp_dev, count);
		return -1;
	}

	temp_class = class_create(THIS_MODULE, "temp_class");
	device_create (temp_class, NULL, temp_dev, "%s", "temp_cdrv");
	
	bus_pin_init();

	led_wq = create_singlethread_workqueue(WQ_NAME);
	if (led_wq) {
		pr_info("WQ created\n");
	} else {
		pr_err("No workqueue created\n");
		error--; 
	}

	return error;
}

static void __exit temp_exit(void)
{
	cancel_work_sync(&led_w);
	destroy_workqueue(led_wq);
	
	gpio_free(PIN);

	device_destroy(temp_class, temp_dev);
	class_unregister(temp_class);
	class_destroy(temp_class);
	unregister_chrdev_region(temp_dev, count);

	pr_info("Goodbye, Temp Module Exit\n");
}

module_init(temp_init);
module_exit(temp_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Slaev");
