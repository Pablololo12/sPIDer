#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "controller.h"

int main(int argc, char **argv)
{
	double k = 100000.0;
	double ti = 150.0;
	double ts = 0.5;
	double tdes = 70.0;
	double f = 1421000;
	double t = 70.0;
	double gain = 32000;
	int c;

	while ((c = getopt (argc, argv, "k:i:s:o:f:t:g:")) != -1) {
		switch (c) {
			case 'k':
				k = strtod(optarg,NULL);
				break;
			case 'i':
				ti = strtod(optarg,NULL);
				break;
			case 's':
				ts = strtod(optarg,NULL);
				break;
			case 'o':
				tdes = strtod(optarg,NULL);
				break;
			case 'f':
				f = strtod(optarg,NULL);
				break;
			case 't':
				t = strtod(optarg,NULL);
				break;
			case 'g':
				gain = strtod(optarg,NULL);
				break;
		}
	}

#ifdef DEBUG
	printf("%f %f %f %f %f %f %f\n", k, ti, ts, tdes, f, t, gain);
#endif

	init_controller(k, ti, ts, tdes, f, t, gain);
	start_controller();

	return 0;
}
