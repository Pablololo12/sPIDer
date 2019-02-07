#ifdef DEBUG
#include <stdio.h>
#endif
#include "pid.h"

static double A = 0.0;
static double B = 0.0;

double pid_calculate (double error)
{
	static double error1 = 0.0;
	static double u1 = 0.0;

	double aux = u1 + A*error + B*error1;

	error1 = error;
	u1 = aux;

#ifdef DEBUG
	printf("PID Freq Calculated: %f\n", aux);
#endif

	return aux;
}

int pid_init(double K, double Ti, double Ts)
{
	A = K + (K/Ti)*Ts/2.0;
	B = -K + (K/Ti)*Ts/2.0;

#ifdef DEBUG
	printf("PID Initializing\n");
	printf("PID A: %f B: %f \n", A, B);
#endif
	return 1;
}
