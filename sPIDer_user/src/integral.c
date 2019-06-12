#ifdef DEBUG
#include <stdio.h>
#endif
#include "integral.h"

static double A = 0.0;
static double u1 = 0.0;

double integral_calculate (double error)
{
	static double error1 = 0.0;

	double aux = u1 + A*error1;

	error1 = error;
	u1 = aux;

#ifdef DEBUG
	printf("Integral Calculated: %f\n", aux);
#endif

	return aux;
}

int integral_init(double K, double Ti, double Ts)
{
	A = (K/Ti)*Ts;

#ifdef DEBUG
	printf("Integral Initializing\n");
	printf("Integral A: %f\n", A);
#endif
	return 1;
}
