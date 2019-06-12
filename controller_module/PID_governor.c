#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/thermal.h>
#include <linux/limits.h>
#include "../hwcounter/hwcounter.h"

/* PID_governor macros */
#define DEF_K_VALUE				(20000)
#define DEF_TI_VALUE			(250)
#define DEF_FF_VALUE			(32000)
#define DEF_T_ZERO_VALUE		(70000)
#define DEF_F_ZERO_VALUE		(1421000)
#define DEF_TEMP_OBJ			(80000)
#define DEF_SAMPLING_VALUE		(1000000)

/*
 * Struct of data for each CPU
 */
struct cpu_info_s {
	int error1;
	int u1;
	int A_int;
	int B_int;
	int A_float;
	int B_float;
	int A_mem;
	int B_mem;
	int type;
	int count;
	struct cpufreq_policy *cur_policy;
	struct delayed_work work;
	int cpu;
	unsigned int enable:1;
};
static DEFINE_PER_CPU(struct cpu_info_s, cs_cpu_info);


/****** Global data *******/

// dbs_mutex to protect data access.
static DEFINE_MUTEX(dbs_mutex);

static struct dbs_tuners {
	unsigned int sampling_rate;
	unsigned int K_int;
	unsigned int Ti_int;
	unsigned int FF_int;
	unsigned int T_zero_int;
	unsigned int F_zero_int;
	unsigned int T_int;
	unsigned int K_float;
	unsigned int Ti_float;
	unsigned int FF_float;
	unsigned int T_zero_float;
	unsigned int F_zero_float;
	unsigned int T_float;
	unsigned int K_mem;
	unsigned int Ti_mem;
	unsigned int FF_mem;
	unsigned int T_zero_mem;
	unsigned int F_zero_mem;
	unsigned int T_mem;
	unsigned int update;
	unsigned int type_check;

} dbs_tuners_ins = {
	.sampling_rate = DEF_SAMPLING_VALUE,
	.K_int = DEF_K_VALUE,
	.Ti_int = DEF_TI_VALUE,
	.FF_int = DEF_FF_VALUE,
	.T_zero_int = DEF_T_ZERO_VALUE,
	.F_zero_int = DEF_F_ZERO_VALUE,
	.T_int = DEF_TEMP_OBJ,
	.K_float = DEF_K_VALUE,
	.Ti_float = DEF_TI_VALUE,
	.FF_float = DEF_FF_VALUE,
	.T_zero_float = DEF_T_ZERO_VALUE,
	.F_zero_float = DEF_F_ZERO_VALUE,
	.T_float = DEF_TEMP_OBJ,
	.K_mem = DEF_K_VALUE,
	.Ti_mem = DEF_TI_VALUE,
	.FF_mem = DEF_FF_VALUE,
	.T_zero_mem = DEF_T_ZERO_VALUE,
	.F_zero_mem = DEF_F_ZERO_VALUE,
	.T_mem = DEF_TEMP_OBJ,
	.update = 10,
	.type_check = 0,
};

static struct thermal_zone_device *tz0;
static unsigned int dbs_enable;

int long_int_to_int(long long int big)
{
	int u;
	// Check for overflow
	if (big <= INT_MIN) {
		u = INT_MIN + 1;
	} else if (big >= INT_MAX) {
		u = INT_MAX - 1;
	} else {
		u = big;
	}
	return u;
}

static void update_discrete_values(struct cpu_info_s *this_info,
		int k, int ti, int ts, int *A, int *B)
{
	long aux;
	aux = (k*ts)/(ti<<1);
	// Aproximamos exp a 1 dado que los valores son muy pequenyos
	*A = aux;
	*B = -aux;
}

/********************* PID controler *********************/
static void dbs_check_cpu(struct cpu_info_s *this_info)
{
	int need_update, type, t0, f0, ff, A, B, temp_ac, T;
	int e1, u1, u, error;
	long long int acum, aux;
	struct cpufreq_policy *policy;
	// Lets start by check if there is an update to perform
	mutex_lock(&dbs_mutex);

	need_update = dbs_tuners_ins.update;
	if (need_update) {
		update_discrete_values(this_info, dbs_tuners_ins.K_int,
				dbs_tuners_ins.Ti_int, dbs_tuners_ins.sampling_rate,
				&this_info->A_int, &this_info->B_int);
		update_discrete_values(this_info, dbs_tuners_ins.K_float,
				dbs_tuners_ins.Ti_float, dbs_tuners_ins.sampling_rate,
				&this_info->A_float, &this_info->B_float);
		update_discrete_values(this_info, dbs_tuners_ins.K_float,
				dbs_tuners_ins.Ti_mem, dbs_tuners_ins.sampling_rate,
				&this_info->A_mem, &this_info->B_mem);
	}
	dbs_tuners_ins.update = 0;

	this_info->count++;
	if (this_info->count >= dbs_tuners_ins.type_check) {
		this_info->count = 0;
		this_info->type = get_type_program();
	}
	type = this_info->type;

	if (type == 1) { // float
		t0 = dbs_tuners_ins.T_zero_float;
		f0 = dbs_tuners_ins.F_zero_float;
		ff = dbs_tuners_ins.FF_float;
		A = this_info->A_float;
		B = this_info->B_float;
		T = dbs_tuners_ins.T_float;
	} else if (type == 2) { // int
		t0 = dbs_tuners_ins.T_zero_int;
		f0 = dbs_tuners_ins.F_zero_int;
		ff = dbs_tuners_ins.FF_int;
		A = this_info->A_int;
		B = this_info->B_int;
		T = dbs_tuners_ins.T_int;
	} else { // mem
		t0 = dbs_tuners_ins.T_zero_mem;
		f0 = dbs_tuners_ins.F_zero_mem;
		ff = dbs_tuners_ins.FF_mem;
		A = this_info->A_mem;
		B = this_info->B_mem;
		T = dbs_tuners_ins.T_mem;
	}

	mutex_unlock(&dbs_mutex);

	policy = this_info->cur_policy;
	
	thermal_zone_get_temp(tz0, &temp_ac);

	e1 = this_info->error1;
	u1 = this_info->u1;

	acum = T;
	acum -= t0;
	aux = ff*acum;
	acum = acum - (temp_ac - t0);
	error = acum;
	acum = u1 + A*acum + B*e1;

	acum += aux;
	acum += f0;

	u = long_int_to_int(acum);

	this_info->error1 = error;
	this_info->u1 = u;

	__cpufreq_driver_target(policy, u, CPUFREQ_RELATION_C);
}


/******************* Working queues *******************/
static void do_timer(struct work_struct *work)
{
	int delay;
	struct cpu_info_s *dbs_info =
		container_of(work, struct cpu_info_s, work.work);

	/* We want all CPUs to do sampling nearly on same jiffy */
	mutex_lock(&dbs_mutex);
	delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	mutex_unlock(&dbs_mutex);

	delay -= jiffies % delay;

	dbs_check_cpu(dbs_info);

	schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work, delay);
}

static inline void timer_init(struct cpu_info_s *dbs_info)
{
	/* We want all CPUs to do sampling nearly on same jiffy */
	int delay = usecs_to_jiffies(dbs_tuners_ins.sampling_rate);
	delay -= jiffies % delay;

	dbs_info->enable = 1;
	INIT_DEFERRABLE_WORK(&dbs_info->work, do_timer);
	schedule_delayed_work_on(dbs_info->cpu, &dbs_info->work, delay);
}

static inline void timer_exit(struct cpu_info_s *dbs_info)
{
	dbs_info->enable = 0;
	cancel_delayed_work_sync(&dbs_info->work);
}


/************************** sysfs interface ************************/

#define show_one(file_name, object)                         \
static ssize_t show_##file_name                             \
(struct kobject *kobj, struct attribute *attr, char *buf)   \
{                                                           \
	return sprintf(buf, "%u\n", dbs_tuners_ins.object);     \
}

#define store_one(filename, object)        \
static ssize_t store_##filename            \
(struct kobject *a, struct attribute *b,   \
 const char *buf, size_t count)            \
{                                          \
	unsigned int input;                    \
	int ret;                               \
	ret = sscanf(buf, "%u", &input);       \
	if (ret != 1) return -EINVAL;          \
	mutex_lock(&dbs_mutex);                \
	dbs_tuners_ins.object = input;         \
	dbs_tuners_ins.update = 1;             \
	mutex_unlock(&dbs_mutex);              \
	return count;                          \
}

#define rw_one(filename, object)    \
show_one(filename, object)          \
store_one(filename, object)         \
define_one_global_rw(filename)      \

rw_one(sampling_rate, sampling_rate);
rw_one(K_int, K_int);
rw_one(Ti_int, Ti_int);
rw_one(FF_int, FF_int);
rw_one(T_zero_int, T_zero_int);
rw_one(F_zero_int, F_zero_int);
rw_one(T_int, T_int);
rw_one(K_float, K_float);
rw_one(Ti_float, Ti_float);
rw_one(FF_float, FF_float);
rw_one(T_zero_float, T_zero_float);
rw_one(F_zero_float, F_zero_float);
rw_one(T_float, T_float);
rw_one(K_mem, K_mem);
rw_one(Ti_mem, Ti_mem);
rw_one(FF_mem, FF_mem);
rw_one(T_zero_mem, T_zero_mem);
rw_one(F_zero_mem, F_zero_mem);
rw_one(T_mem, T_mem);
rw_one(type_check, type_check);


static struct attribute *attributes[] = {
	&sampling_rate.attr,
	&K_int.attr,
	&Ti_int.attr,
	&FF_int.attr,
	&T_zero_int.attr,
	&F_zero_int.attr,
	&T_int.attr,
	&K_float.attr,
	&Ti_float.attr,
	&FF_float.attr,
	&T_zero_float.attr,
	&F_zero_float.attr,
	&T_float.attr,
	&K_mem.attr,
	&Ti_mem.attr,
	&FF_mem.attr,
	&T_zero_mem.attr,
	&F_zero_mem.attr,
	&T_mem.attr,
	&type_check.attr,
	NULL
};

static struct attribute_group attr_group = {
	.attrs = attributes,
	.name = "PID_governor",
};

/********************* end sysfs ******************************/

/**************** Governor *****************/

static int cpufreq_gov_pid_start(struct cpufreq_policy *policy)
{
	unsigned int cpu = policy->cpu;
	struct cpu_info_s *this_info;
	unsigned int j;
	int rc;

	this_info = &per_cpu(cs_cpu_info, cpu);

	if ((!cpu_online(cpu)) || (!policy->cur))
			return -EINVAL;

	mutex_lock(&dbs_mutex);

	for_each_cpu(j, policy->cpus) {
		struct cpu_info_s *j_dbs_info;
		j_dbs_info = &per_cpu(cs_cpu_info, j);
		j_dbs_info->cur_policy = policy;
		j_dbs_info->error1 = 0;
		j_dbs_info->u1 = 0;
	}

	dbs_enable++;
	/*
	 * Start the timerschedule work, when this governor
	 * is used for first time
	 */
	if (dbs_enable == 1) {
		rc = sysfs_create_group(cpufreq_global_kobject,
					&attr_group);
		if (rc) {
			mutex_unlock(&dbs_mutex);
			return rc;
		}

	}
	mutex_unlock(&dbs_mutex);

	timer_init(this_info);
	return 0;
}

static void cpufreq_gov_pid_stop(struct cpufreq_policy *policy)
{
	unsigned int cpu = policy->cpu;
	struct cpu_info_s *this_info;

	this_info = &per_cpu(cs_cpu_info, cpu);

	timer_exit(this_info);

	mutex_lock(&dbs_mutex);
	dbs_enable--;


	mutex_unlock(&dbs_mutex);
	if (!dbs_enable)
		sysfs_remove_group(cpufreq_global_kobject,
				&attr_group);
}


#ifndef CONFIG_CPU_FREQ_DEFAULT_GOV_PIDGOV
static
#endif
struct cpufreq_governor cpufreq_gov_pid = {
	.name			= "PID_GOVERNOR",
	.start			= cpufreq_gov_pid_start,
	.stop			= cpufreq_gov_pid_stop,
	.owner			= THIS_MODULE,
};

static int __init cpufreq_gov_dbs_init(void)
{
	tz0 = thermal_zone_get_zone_by_name("cls0");
	if (tz0 == NULL) return EFAULT;

	return cpufreq_register_governor(&cpufreq_gov_pid);
}

static void __exit cpufreq_gov_dbs_exit(void)
{
	cpufreq_unregister_governor(&cpufreq_gov_pid);
}

MODULE_AUTHOR("Pablo Hernandez <pabloheralm@gmail.com>");
MODULE_DESCRIPTION("PID_governor - A PID governor to keep"
	"the same temperature");
MODULE_LICENSE("GPL");

#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_PIDGOV
fs_initcall(cpufreq_gov_dbs_init);
#else
module_init(cpufreq_gov_dbs_init);
#endif
module_exit(cpufreq_gov_dbs_exit);
