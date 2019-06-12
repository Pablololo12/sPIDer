#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "controller.h"
#include "pid.h"
#include "integral.h"

#define FILE_TEMP "/sys/class/thermal/thermal_zone0/temp"
#define PATH_TO_CPU "/sys/devices/system/cpu/cpufreq/policy4/scaling_setspeed"
#define PATH_FREQS "/sys/devices/system/cpu/cpufreq/policy4/scaling_available_frequencies"

static double Fzero = 0.0;
static double Tzero = 0.0;
static double Tdes = 0.0;
static double FeedFordw = 0.0;
static long Sample = 0;

// Files to get temperature info and set frequencies
static int temp;
static int cpu_fd;
static FILE *freqs_fd;

static int NUM_FREQ = 0;
static long FREQS[30];

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

	freqs_fd = fopen(PATH_FREQS, "r");
	if (freqs_fd == NULL) {
		fprintf(stderr, "Error openning available freqs file\n");
		return -1;
	}

	return 1;
}

int init_controller(double K, double Ti, double Ts, double Tde, double Fzer,
		double Tzer, double feed_fordw, double K_i, double Ti_i)
{
#ifdef DEBUG
	int i;
#endif

	if (!pid_init(K,Ti,Ts)) {
#ifdef DEBUG
		fprintf(stderr,"CONTROLLER Error initializing PID\n");
#endif
		return 0;
	}

	if (!integral_init(K_i,Ti_i,Ts)) {
#ifdef DEBUG
		fprintf(stderr,"CONTROLLER Error initializing PID\n");
#endif
		return 0;
	}

	if (!open_files())
		return 0;

	// Read available frequencies
	while (!feof(freqs_fd))
		fscanf(freqs_fd, "%ld", &FREQS[NUM_FREQ++]);
	NUM_FREQ--;
	fclose(freqs_fd);

#ifdef DEBUG
	printf("-----------\n");
	for (i=0; i<NUM_FREQ; i++)
		printf("%ld\n",FREQS[i]);
	printf("-----------\n");
#endif

	Fzero = Fzer;
	Tzero = Tzer;
	Tdes = Tde;
	FeedFordw = feed_fordw;
	Sample = (long)(Ts*1000000.0);

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

long select_freq(double freq)
{
	long f;
	int i;
	char buff[10];
	long aux;

	f = (long) freq;
#ifdef DEBUG
	printf("PREV: %ld\n", f);
#endif
	if (f < FREQS[0]){
		f = FREQS[0];
		goto select_freq;
	}
	
	for (i=1; i<NUM_FREQ; i++) {
		if (f > FREQS[i])
			continue;
		//f = FREQS[i-1];
		//break;
		aux = (FREQS[i]+FREQS[i-1]) >> 1;
		if (f < aux)
			f = FREQS[i-1];
		else
			f = FREQS[i];
		break;
	}

	if (f > FREQS[NUM_FREQ-1])
		f = FREQS[NUM_FREQ-1];

select_freq:
	sprintf(buff,"%ld", f);
#ifdef DEBUG
	printf("POST: %ld\n", f);
#endif
	pwrite(cpu_fd, buff, strlen(buff),0);

	return f;
}

/* Outdated
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
	double freq_int = 0.0;
	double error = 0.0;
	long f = 0.0;
	while (1) {
		pipeL = Tdes;
		pipeL -= Tzero;

		aux = FF_temp(pipeL);

		pipeL = pipeL - (read_temp() - Tzero);
		error = pipeL;

#ifdef DEBUG
		printf("CONTROLLER Temp: %f %f \n", pipeL, -(read_temp()-Tzero));
#endif
		pipeL = pid_calculate(pipeL);

		pipeL += aux;

		pipeL += freq_int;

		pipeL += Fzero;

#ifdef DEBUG
		printf("CONTROLLER Freq selected: %f\n", pipeL);
#endif
		f = select_freq(pipeL);
		//update_freq((long)f-Fzero);

		if (abs(error) < 5.0)
			freq_int = integral_calculate(pipeL-f);
		else
			freq_int = integral_calculate(0.0);

		usleep(Sample);
	}

	return 0;
}

