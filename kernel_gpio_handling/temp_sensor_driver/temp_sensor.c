#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#define SKIP_ROM 	0xCC
#define READ_ROM 	0x33
#define CONVERT_T 	0x44
#define READ_SCRATHPAD 	0xBE

#define PIN 		4
#define GPIO_NAME 	TEMP_PIN
#define WQ_NAME 	"work_queue"
#define WQ_HZ_DELAY 	HZ/10

static void work_handler(struct work_struct *work);

static struct workqueue_struct *led_wq;
static DECLARE_DELAYED_WORK(led_w, work_handler);

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
	gpio_direction_input(PIN);	/* release the bus */
	udelay(60); 			/* wait for the maximum DS18B20 reaction time */
	
	if (gpio_get_value(PIN) != 0) {
		if (rc > 4) {							/* try maximum 4 times */
			pr_info("DS18B20 NOT reset, stop trying\n");
			return 1;
		}
		
		pr_info("DS18B20 NOT reset, try again in 420 milisecond\n");
		udelay(480);							/* wait for the maximum DS18B20 signal time before retry reset */
		queue_delayed_work(led_wq, &led_w, WQ_HZ_DELAY);		/* try again */
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
	int mask;

	mask = 1;
	
	for (i = 0; i < 8; i++)
	{
		write_bit((command >> i) & mask);
	}
}

static uint64_t read_rom(void)
{
	int i;
	uint64_t rom;
	rom = 0;
	
	for (i = 0; i < 64; i++)
	{
		rom |= (read_bit() << i);
	}
	pr_err("ROM VALUE: %ld\n", (long)rom); /*not an err just for readability in dmesg */
	
	return rom;
}	

static uint16_t read_temp(void)
{
	int i;
	uint16_t temp;
	temp = 0;
	
	for (i = 0; i < 16; i++)
	{
		temp |= (read_bit() << i);
	}
	pr_err("TEMPERATURE VALUE: 0x%x\n", temp); /* raw hex value in kernel module only, not an err just for readability in dmesg */
	return temp;
}

static void work_handler(struct work_struct *work)
{
	/* read ROM */
	if (master_reset())
	{
		return;
	}
	write_command(READ_ROM);
	read_rom();
	mdelay(1);
	
	/* init convertion */
	if (master_reset())
	{
		return;
	}
	write_command(SKIP_ROM);
	write_command(CONVERT_T);
	while (read_bit() == 0)
	{
		mdelay(1);
	}
	
	/* when ready, read temp */
	if (master_reset())
	{
		return;
	}
	write_command(SKIP_ROM);
	write_command(READ_SCRATHPAD);
	read_temp();
	if (master_reset())
	{
		return;
	}
}

static int __init my_init(void)
{
	int ret;
	pr_info("Module Raspberry Pi\n");
	
	bus_pin_init();

	led_wq = create_singlethread_workqueue(WQ_NAME);
	if (led_wq) {
		pr_info("WQ created\n");
	} else {
		pr_err("No workqueue created\n");
	}

	ret = queue_delayed_work(led_wq, &led_w, WQ_HZ_DELAY);

	return 0;
}

static void __exit my_exit(void)
{
	pr_info("Goodbye, Module Raspberry Pi\n");
	
	cancel_delayed_work_sync(&led_w);
	destroy_workqueue(led_wq);
	
	gpio_free(PIN);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Slaev");
