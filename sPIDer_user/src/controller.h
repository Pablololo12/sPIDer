
/**
 * init_controller initializes the controller
 * Params:
 *  - K the proportional value for the PID
 *  - Ti the integral value for the PID
 *  - Ts the sample time in seconds
 *  - Tdes the objective temperature
 *  - Fzero frequency zero
 *  - Tzero temperature zero
 *  - feed_fordw the start gain
 * Output:
 *  1 if ok
 */
int init_controller(double K, double Ti, double Ts, double Tde, double Fzer,
		double Tzer, double feed_fordw);

/**
 * Starts the execution of the controller
 */
int start_controller();
