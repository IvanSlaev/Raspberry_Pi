#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>

#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)

#define INP_GPIO(g)   *(gpio + ((g)/10)) &= ~(7<<(((g)%10)*3))
#define OUT_GPIO(g)   *(gpio + ((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio + (((g)/10))) |= (((a)<=3?(a) + 4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET(g)  *(gpio + 7) = 1 << g
#define GPIO_CLR(g)  *(gpio + 10) = 1 << g

#define GET_GPIO(g) (*(gpio + 13) >> g) & 1

#define RED 17
#define GREEN 27

#define BUTTON 18

static int  mem_fd;
static void *gpio_map;
 
// I/O access
static volatile unsigned *gpio;

enum
{
	NORMAL,
	PEDESTRIAN
} TL_STATES;

void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }
 
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,                 //Any address in our space will do
      BLOCK_SIZE,              //Map length
      PROT_READ|PROT_WRITE,    // Enable reading & writting to mapped memory
      MAP_SHARED,           //Shared with other processes
      mem_fd,                 //File to map
      GPIO_BASE              //Offset to GPIO peripheral
   );
 
   close(mem_fd); //No need to keep mem_fd open after mmap
 
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
 
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
 
 
} // setup_io

void irq_handler()
{
	if (GET_GPIO(BUTTON))
	{
		TL_STATES = PEDESTRIAN;
	}
}

void event_handler()
{
	switch (TL_STATES)
	{
		case NORMAL:
			GPIO_CLR(RED);
			GPIO_SET(GREEN);
			break;
			
		case PEDESTRIAN:
			sleep(3);
			GPIO_SET(RED);
			sleep(2);
			GPIO_CLR(GREEN);
			sleep(5);
			GPIO_SET(GREEN);
			sleep(2);
			TL_STATES = NORMAL;
			break;
	}
}

void prog_sig_handler()
{
	printf("Traffic light is set off\n");
	fflush(stdout);	
	GPIO_SET(RED);
	GPIO_SET(GREEN);
	sleep(10);
	printf("Don't be silly, it is not a real traffic light\n");
	GPIO_CLR(RED);
	GPIO_CLR(GREEN);
	exit(1);
}

int main(int argc, const char *argv[])
{
	signal (SIGINT, prog_sig_handler);
	
	setup_io();
	
	INP_GPIO(RED);
	OUT_GPIO(RED);
	
	INP_GPIO(GREEN);
	OUT_GPIO(GREEN);
	
	GPIO_CLR(RED);
	GPIO_CLR(GREEN);

	TL_STATES = NORMAL;
	
	while (1)
	{
		irq_handler();
		event_handler();
	}

	
    return 0;
}
