#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>

#define RED 17
#define BUTTON 18
#define GREEN 27
#define WQ_NAME "led_work_queue"
#define WQ_HZ_DELAY HZ/10
#define MAX_STATES 4

typedef enum {
	TL_NORMAL,
	TL_PEDESTRIAN
} TL_STATE;
TL_STATE tl_state = TL_NORMAL;

static void switch_to_normal(void);
static void switch_to_pedestrian(void);

typedef void (*switch_state_func)(void);
switch_state_func state_funcs[MAX_STATES] = { switch_to_normal, switch_to_pedestrian };

static int irq;

static void work_handler(struct work_struct *work);

static struct workqueue_struct *led_wq;
static DECLARE_DELAYED_WORK(led_w, work_handler);

static void switch_to_normal(void)
{
	tl_state = TL_NORMAL;
	enable_irq(irq);
}

static void switch_to_pedestrian(void)
{
	int ret;
	tl_state = TL_PEDESTRIAN;
	disable_irq_nosync(irq);
	ret = queue_delayed_work(led_wq, &led_w, WQ_HZ_DELAY);
}

static void work_handler(struct work_struct *work)
{
	msleep(2000);
	gpio_set_value(GREEN, 1);
	msleep(1000);
	gpio_set_value(RED, 0);
	msleep(4000);
	gpio_set_value(RED, 1);
	msleep(1000);
	gpio_set_value(GREEN, 0);

	state_funcs[TL_NORMAL]();

}
 
static irqreturn_t irq_handler_gpio(int irq, void *dev_id)
{
	state_funcs[TL_PEDESTRIAN]();

	return IRQ_HANDLED;
}

static int __init my_init(void)
{
	int ret;
	pr_info("Module Raspberry Pi\n");
	
	/* Initialization */
	ret = gpio_request(RED, "RED_LED");
	if (ret) {
		pr_err("request GPIO %d failed\n", RED);
	}
	ret = gpio_request(BUTTON, "BUTTON");
	if (ret) {
		pr_err("request GPIO %d failed\n", BUTTON);
	}
	ret = gpio_request(GREEN, "GREEN_LED");
	if (ret) {
		pr_err("request GPIO %d failed\n", GREEN);
	}

	/* Setting GPIO function */
	ret = gpio_direction_output(RED, 0);
	if (ret) {
		pr_err("gpio_direction_output GPIO %d failed\n", RED);
	}
	ret = gpio_direction_input(BUTTON);
	if (ret) {
		pr_err("gpio_direction_input GPIO %d failed\n", BUTTON);
	}
	ret = gpio_direction_output(GREEN, 0);
	if (ret) {
		pr_err("gpio_direction_output GPIO %d failed\n", GREEN);
	}

	irq = gpio_to_irq(BUTTON);
	if (!irq) {
		pr_err("gpio_to_irq failed \n");
	}	
	
	led_wq = create_singlethread_workqueue(WQ_NAME);
	if (led_wq) {
		pr_info("WQ created\n");
	} else {
		pr_err("No workqueue created\n");
	}

	ret = request_irq(irq, irq_handler_gpio, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "button", NULL);
	if (ret) {
		pr_err("irq request failed\n");
	}

	gpio_set_value(RED, 1);

	return 0;
}

static void __exit my_exit(void)
{
	pr_info("Goodbye, Module Raspberry Pi\n");

	gpio_set_value(RED, 0);
	gpio_free(RED);
	gpio_free(BUTTON);
	gpio_free(GREEN);

	cancel_delayed_work_sync(&led_w);

	destroy_workqueue(led_wq);
	free_irq(irq, NULL);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ivan Slaev");
