
/*
 * init_controller initializes the controller
 * Params:
 *  - K the proportional value for the PID
 *  - Ti the integral value for the PID
 *  - Ts the sample time in seconds
 *  - Tdes the objective temperature
 *  - Fzero frequency zero
 *  - Tzero temperature zero
 *  - perturbance the perturbance value
 * Output:
 *  1 if ok
 */
int init_controller(double K, double Ti, double Ts, double Tdes, double Fzero,
		double Tzero, double perturbance);

double read_temp();
double FF_temp(double temp);
double FF_perturbance(double pert);
int select_freq(double freq);

/*
 * Starts the execution of the controller
 */
int start_controller();
