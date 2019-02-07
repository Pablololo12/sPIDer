#ifdef DEBUG
#include <stdio.h>
#endif
#include <unistd.h>
#include "controller.h"
#include "pid.h"

static double Fzero = 0.0;
static double Tzero = 0.0;
static double Perturbance = 0.0;
static double Tdes = 0.0;
static long Sample = 0;

int init_controller(double K, double Ti, double Ts, double Tdes, double Fzero,
		double Tzero, double perturbance)
{

	if (pid_init(K,Ti,Ts)) {
#ifdef DEBUG
		printf("CONTROLLER Error initializing PID\n");
#endif
		return 0;
	}

	Fzero = Fzero;
	Tzero = Tzero;
	Perturbance = perturbance;
	Tdes = Tdes;
	Sample = (long)(Ts*1000);

	return 1;
}

double read_temp()
{
	return 0.0;
}

double FF_temp(double temp)
{
	return 0.0;
}

double FF_perturbance(double pert)
{
	return 0.0;
}

int select_freq(double freq)
{
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

		pipeL += FF_perturbance(Perturbance);

		pipeL += Fzero;

#ifdef DEBUG
		printf("CONTROLLER Freq selected: %f\n", pipeL);
#endif
		select_freq(pipeL);

		usleep(Sample);
	}

	return 0;
}

