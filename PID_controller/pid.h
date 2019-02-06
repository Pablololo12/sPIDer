#ifndef _PID_LIB_
#define _PID_LIB_

#define FREQ_FILE "/sys/devices/system/cpu/cpufreq/policy4/scaling_available_frequencies"

// Choose the correct frequency
int which_freq(float);

// Update the temperature and return the choosen frequency 
int update_temp(float);

// initialize the constants for the formula
int initialize_pid(float[5]);

#endif
