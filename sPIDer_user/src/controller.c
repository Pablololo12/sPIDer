#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "controller.h"
#include "pid.h"

#define FILE_TEMP "/sys/class/thermal/thermal_zone0/temp"
#define PATH_TO_CPU "/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed"

static double Fzero = 0.0;
static double Tzero = 0.0;
static double Tdes = 0.0;
static double FeedFordw = 0.0;
static long Sample = 0;

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

int init_controller(double K, double Ti, double Ts, double Tdes, double Fzero,
		double Tzero, double feed_fordw)
{

	if (pid_init(K,Ti,Ts)) {
#ifdef DEBUG
		fprintf(stderr,"CONTROLLER Error initializing PID\n");
#endif
		return 0;
	}

	if (!open_files())
		return 0;

	Fzero = Fzero;
	Tzero = Tzero;
	Tdes = Tdes;
	FeedFordw = feed_fordw;
	Sample = (long)(Ts*1000);

	return 1;
}

double read_temp()
{
	char buff[8];
	double aux = 0.0;
	// Get the current temperature
	pread(temp,buff,8, 0);
	aux = (double) strtod(buff,NULL);
	return aux / 1000.0;
}

double FF_temp(double temp)
{
	return temp*FeedFordw;
}

int select_freq(double freq)
{
	int f;
	char buff[10];

	f = (int) freq;
	sprintf(buff,"%d", f);
#ifdef DEBUG
	printf("%ld %d\n", aux, new_freq);
#endif
	pwrite(cpu_fd, buff, strlen(buff),0);

	return 1;
}

/*
 *                                                                 +Perturbance
 *                                               +----+            |
 *                                         +---+ | FF | <----------+
 *                                         |     +----+            V
 *                                         |                    +----+
 *      Tzero         +----+               |                    | GP |  Tzero
 *        +  +------> | FF | +------+      |      +Fzero        +--+-+    +
 *        |  |        +----+        |      |      |                |      |
 *Tdes    v  |         +-----+      v      V      v     +---+      V      v
 *   +-> +- +-> +- +-> | PID | +-> ++ +-> ++ +-> ++ +-> | G | +-> ++ +-> +- ++->
 *               ^     +-----+                          +---+                |
 *               |                                                           |
 *               |                                                           |
 *               +-----------------------------------------------------------+
 **/
int start_controller()
{
	double pipeL = 0.0;
	double aux = 0.0;
	while (1) {
		pipeL = Tdes;
		pipeL -= Tzero;
		
		aux = FF_temp(pipeL);

		pipeL -= read_temp() - Tzero;

		pipeL = pid_calculate(pipeL);

		pipeL += aux;

		pipeL += Fzero;

#ifdef DEBUG
		printf("CONTROLLER Freq selected: %f\n", pipeL);
#endif
		select_freq(pipeL);

		usleep(Sample);
	}

	return 0;
}

