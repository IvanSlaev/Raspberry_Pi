#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

int main(int argc, const char *argv[])
{
	int length, temp_fd, rc;
	const char *nodename = "/dev/temp_cdrv";
	short temperature = 0;

	length = sizeof(short);

	temp_fd = open(nodename, O_RDONLY);
	
	rc = read(temp_fd, &temperature, length);
	printf("Raw temperature 0x%x\n", temperature);

	return 0;
}
