// In order to use pread and pwrite
#define _XOPEN_SOURCE 500
#include <unistd.h>
///////////////////
#include "pid.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>

// Constants
enum {
        DESIRED_TEMP = 70000}; // Constant for the array of values

// TODO: Find by itself the correct thermal zone
#define FILE_TEMP "/sys/class/thermal/thermal_zone0/temp"

// Directory of cpus
#define PATH_TO_CPU "/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed"


// Files to get temperature info and set frequencies
static int temp;
static int cpu_fd;

/*
 * Function to open the temperature file and the freq files
 */
static int open_files(void)
{

	// open the temperature file
	temp = open(FILE_TEMP, O_RDONLY);
	if (temp == -1) {
		fprintf(stderr, "Error openning temperature file\n");
		return -1;
	}
	

	cpu_fd = open(PATH_TO_CPU, O_WRONLY);
	if (cpu_fd == -1) {
		fprintf(stderr, "Error openning cpu scaling\n");
		return -1;
	}

	return 1;
}

int main(int argc, char **argv)
{
	int aux = 0; // Auxiliar variable to keep temperature
	int new_freq = 0; // Auxiliar variable to save new frequency
	char freq_str[8]; // Char array used to write frequency
	char temp_buff[8]; // Char array used to read the current temperature
	float values[5];
	long interval;
	struct timespec tim;
	long temp_desired;

	values[0] = atof(argv[1]);
	values[1] = atof(argv[2]);
	values[2] = atof(argv[3]);
	values[3] = atof(argv[4]);
	values[4] = atof(argv[5]);

	interval = atol(argv[6])*1000L;
	tim.tv_sec  = 0;
	tim.tv_nsec = atol(argv[6])*1000L;

	temp_desired = atol(argv[7]);

	// initialize the PID controller
	if (initialize_pid(values)==-1) {
		return 1;
	}

	// Open control files
	if (open_files()==-1) {
		return 1;
	}

	printf("Successfully initialized\n");

	while (1) {
		// Get the current temperature
		pread(temp,temp_buff,8, 0);
		aux = (int) strtol(temp_buff,NULL,10);

		// Update the PID
		new_freq = update_temp((temp_desired-aux)/1000.0);

		sprintf(freq_str,"%d", new_freq);

		printf("%ld %d\n", aux, new_freq);

		pwrite(cpu_fd, freq_str, strlen(freq_str),0);

		usleep(interval);
	}

	return 0;
}
