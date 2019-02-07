#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdio.h>
#include "controller.h"

int main(int argc, char **argv)
{

	init_controller(250000.0, 150.0, 0.5, 70.0, 916000.0, 60.0, 0.0);
	start_controller();

	return 0;
}
