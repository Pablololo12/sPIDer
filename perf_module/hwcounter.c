#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/perf_event.h>
#include <linux/notifier.h>
#include <linux/cpu.h>

// Definitions for hw counters
#define ASE_SPEC 0x74
#define MEM_ACCESS 0x13
#define INST_RETIRED 0x08
#define DEF_SAMPLING_VALUE (1000000)

// Vars for classifiying type of program
static s64 inter_int_=40;
static s64 mem_int_=-74;
static s64 simd_int_=-106;
static s64 inter_mem_=-8;
static s64 mem_mem_=11;
static s64 simd_mem_=-45;
static int type_of_program=1;

// Vars to read from counters
static struct kobject *files_kobject;
static u32 simd_count[NR_CPUS] = {0};
static u32 mem_count[NR_CPUS] = {0};
static u32 inst_count[NR_CPUS] = {0};
static u32 simd_count_prev[NR_CPUS] = {0};
static u32 mem_count_prev[NR_CPUS] = {0};
static u32 inst_count_prev[NR_CPUS] = {0};
static struct perf_event *pe[NR_CPUS][3];
static DEFINE_PER_CPU(char, is_a73);
static int die_hw = 0;
static int a73_id[4] = {0};
static int num_a73 = 0;

// Functions for periodic tasks
static void periodic_task(struct work_struct *work);
static struct workqueue_struct *periodic_workqueue;
static DECLARE_DELAYED_WORK(task, periodic_task);

// Declaration of hw perf counter
static int hwcounter_perf_event_initialize(int cpu);

static void free_counters(int cpu);

// Fast product by 100
inline s64 fast_100(s64 n)
{
	return (n << 6) + (n << 5) + (n << 2);
}

// Predict the type of program currently running globally
int predictor_int(s64 mem, s64 simd, s64 inst)
{
	s64 denomin, mem_num, int_num;
	s64 float_pred, int_pred, mem_pred;
	s64 e_c = 2;
	s64 aux1, aux2;

	// First we obtain the percentaje in integer
	mem = fast_100(mem)/inst;
	simd = fast_100(simd)/inst;

	// Here we calculate each numerator
	// e^(beta + x1*Y1 + x2*Y2)
	aux1 = inter_int_ + (mem * mem_int_)/100 + (simd * simd_int_)/100;
	aux2 = inter_mem_ + (mem * mem_mem_)/100 + (simd * simd_mem_)/100;
	// With this comparations we made a fast implementation of the e number
	if (aux1 >= 0) {
		if (aux1 < 62) {
			int_num = e_c << aux1;
		} else {
			int_num = (s64)(1) << 63;
		}
	} else {
		int_num = 0;
	}

	if (aux2 >= 0) {
		if (aux1 < 62) {
			mem_num = e_c << aux1;
		} else {
			mem_num = (s64)(1) << 63;
		}

	} else {
		mem_num = 0;
	}

	// Here we calculate the denominator
	// 1 + e^(beta1 + x1*Y1 + x2*Y2) + e^(beta2 + x3*Y1 + x4*Y2)
	denomin = 1 + int_num + mem_num;

	// Obtain each prediction
	float_pred = 100/denomin;
	int_pred = fast_100(int_num)/denomin;
	mem_pred = 100 - (float_pred + int_pred);
	//mem_pred = fast_100(mem_num)/denomin;

	if (float_pred > int_pred && float_pred > mem_pred) {
		return 1;
	} else if (int_pred > float_pred && int_pred > mem_pred) {
		return 2;
	} else {
		return 3;
	}
}

static void read_from_registers(void)
{
	u32 total;
	u64 enabled, running;
	int i, aux;

	for (i=0; i<num_a73; i++) {
		aux = a73_id[i];
		/*if (!cpu_online(aux) && !IS_ERR(pe[aux][0])) {
			free_counters(aux);
			pe[aux][0] = (struct perf_event *) -1;
			continue;
		}*/
		if (!cpu_online(aux))
			continue;
		if (IS_ERR(pe[aux][0]) || pe[aux][0]==NULL)
			hwcounter_perf_event_initialize(a73_id[i]);
		if (IS_ERR(pe[aux][0])) {
			printk(KERN_INFO "DEBUG HWCOUNTER: CPU %d Error %ld\n",aux,PTR_ERR(pe[aux][0]));
			continue;
		}
		total = (u32) perf_event_read_value(pe[aux][0], &enabled, &running);
		inst_count[aux] = total - inst_count_prev[aux];
		inst_count_prev[aux] = total;
		total = (u32) perf_event_read_value(pe[aux][1], &enabled, &running);
		mem_count[aux] = total - mem_count_prev[aux];
		mem_count_prev[aux] = total;
		total = (u32) perf_event_read_value(pe[aux][2], &enabled, &running);
		simd_count[aux] = total - simd_count_prev[aux];
		simd_count_prev[aux] = total;
	}

}

static void periodic_task(struct work_struct *work)
{
	read_from_registers();
	if (!die_hw)
		schedule_delayed_work(&task,
				usecs_to_jiffies(DEF_SAMPLING_VALUE));
}

static ssize_t simd_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	int i=0;
	u64 aux = 0;
	for (i=0; i<num_possible_cpus(); i++) {
		if(cpu_online(i)) {
			aux += simd_count[i];
		}
	}
	return sprintf(buf, "%llu\n", aux);
}

static ssize_t mem_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	int i=0;
	u64 aux = 0;
	for (i=0; i<num_possible_cpus(); i++) {
		if(cpu_online(i)) {
			aux += mem_count[i];
		}
	}
	return sprintf(buf, "%llu\n", aux);
}

static ssize_t inst_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	int i=0;
	u64 aux = 0;
	for (i=0; i<num_possible_cpus(); i++) {
		if(cpu_online(i)) {
			aux += inst_count[i];
		}
	}
	return sprintf(buf, "%llu\n", aux);
}

static ssize_t type_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	int i=0;
	u64 mem_aux = 0;
	u64 simd_aux = 0;
	u64 inst_aux = 0;

	for (i=0; i<num_possible_cpus(); i++) {
		if (cpu_online(i)) {
			mem_aux += mem_count[i];
			simd_aux += simd_count[i];
			inst_aux += inst_count[i];
		}
	}
	type_of_program = predictor_int(mem_aux,simd_aux,inst_aux);
	return sprintf(buf, "%d\n", type_of_program);
}

static struct kobj_attribute simd_attribute =__ATTR(simd_count, 0444,
							simd_show, NULL);
static struct kobj_attribute mem_attribute =__ATTR(mem_count, 0444,
							mem_show, NULL);
static struct kobj_attribute inst_attribute =__ATTR(inst_count, 0444,
							inst_show, NULL);
static struct kobj_attribute type_attribute =__ATTR(type_of_program, 0444,
							type_show, NULL);

static struct perf_event_attr pea_INST_RETIRED = {
	.type		= PERF_TYPE_RAW,
	.config		= INST_RETIRED,
	.size		= sizeof(struct perf_event_attr),
	.disabled	= 0
};
static struct perf_event_attr pea_ASE_SPEC = {
	.type		= PERF_TYPE_RAW,
	.config		= ASE_SPEC,
	.size		= sizeof(struct perf_event_attr),
	.disabled	= 0
};
static struct perf_event_attr pea_MEM_ACCESS = {
	.type		= PERF_TYPE_RAW,
	.config		= MEM_ACCESS,
	.size		= sizeof(struct perf_event_attr),
	.disabled	= 0
};

static int
cpu_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	int hotcpu = (unsigned long)hcpu;
	switch (action) {
	case CPU_UP_PREPARE:
	case CPU_UP_PREPARE_FROZEN:
	case CPU_ONLINE:
	case CPU_ONLINE_FROZEN:
		//hwcounter_perf_event_initialize(hotcpu);
		break;
	case CPU_UP_CANCELED:
	case CPU_UP_CANCELED_FROZEN:
	case CPU_DEAD:
	case CPU_DEAD_FROZEN:
		free_counters(hotcpu);
		break;
	}
	return NOTIFY_OK;
}

static struct notifier_block cpu_nfb = {
	.notifier_call = cpu_callback
};

static void initialize_registers(void * v)
{
	u32 val, model;
	int cpu;

	cpu = get_cpu();
	asm volatile("MRS %0, PMCR_EL0" : "=r" (val));
	model = (val >> 16) & 0xFF;
	if (model != 0x04) { // Check if the cpu is a Cortex A73
		get_cpu_var(is_a73) = 0;
		put_cpu_var(is_a73);
		printk(KERN_DEBUG "CPU %d is not a cortex a73", cpu);
	} else {
		get_cpu_var(is_a73) = 1;
		put_cpu_var(is_a73);
	}
	put_cpu();
}

static int hwcounter_perf_event_initialize(int cpu)
{
	printk(KERN_INFO "DEBUG HWCOUNTER: Initializing perf event for CPU: %d\n",
			cpu);
	pe[cpu][0] = perf_event_create_kernel_counter(&pea_INST_RETIRED,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][0]))
		return PTR_ERR(pe[cpu][0]);

	pe[cpu][1] = perf_event_create_kernel_counter(&pea_MEM_ACCESS,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][1]))
		return PTR_ERR(pe[cpu][1]);

	pe[cpu][2] = perf_event_create_kernel_counter(&pea_ASE_SPEC,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][2]))
		return PTR_ERR(pe[cpu][2]);
	return 0;
}

static int __init hwcounter_init(void)
{
	int error = 0,i;
	char is73;
	printk(KERN_INFO "DEBUG HWCOUNTER: Initializing HWcounter module\n");

	files_kobject = kobject_create_and_add("hwcounters",kernel_kobj);
	if(!files_kobject){
		error = -1;
		goto exit_hwcounter_init;
	}

	error = sysfs_create_file(files_kobject, &simd_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	error = sysfs_create_file(files_kobject, &mem_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	error = sysfs_create_file(files_kobject, &inst_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	error = sysfs_create_file(files_kobject, &type_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	printk(KERN_INFO "DEBUG HWCOUNTER: sysfs created succesfully\n");
	on_each_cpu(initialize_registers, NULL, 1);

	for (i=0; i<num_possible_cpus(); i++) {
		is73 = per_cpu(is_a73, i);
		if (is73)
			a73_id[num_a73++] = i;
	}

	for (i=0; i<num_a73; i++) {
		if (hwcounter_perf_event_initialize(a73_id[i])!=0) {
			error = -1;
			goto delete_sysfs_exit;
		}
	}
	periodic_workqueue = create_workqueue("HWcounter_work");
	schedule_delayed_work(&task, usecs_to_jiffies(DEF_SAMPLING_VALUE));
	register_cpu_notifier(&cpu_nfb);
	printk(KERN_INFO "DEBUG HWCOUNTER: Module initialized\n");
	return 0;

delete_sysfs_exit:
	kobject_put(files_kobject);
exit_hwcounter_init:
	return error;
}

static void free_counters(int cpu)
{
	printk(KERN_INFO "DEBUG HWCOUNTER: freeing CPU: %d\n", cpu);
	if(pe[cpu][0]!=NULL) {
		perf_event_disable(pe[cpu][0]);
		perf_event_release_kernel(pe[cpu][0]);
		pe[cpu][0]=NULL;
	}
	if(pe[cpu][1]!=NULL) {
		perf_event_disable(pe[cpu][1]);
		perf_event_release_kernel(pe[cpu][1]);
		pe[cpu][1]=NULL;
	}
	if(pe[cpu][2]!=NULL) {
		perf_event_disable(pe[cpu][2]);
		perf_event_release_kernel(pe[cpu][2]);
		pe[cpu][2]=NULL;
	}
}

static void delete_counters(void)
{
	int i;
	printk(KERN_INFO "DEBUG HWCOUNTER: Disabling Counters\n");
	
	for(i=0; i<num_possible_cpus(); i++) {
		free_counters(i);
	}
}

static void __exit hwcounter_exit(void)
{
	die_hw = 1;
	flush_scheduled_work();
	cancel_delayed_work(&task);
	if(periodic_workqueue != NULL){
		flush_workqueue(periodic_workqueue);
		destroy_workqueue(periodic_workqueue);
	}
	if(files_kobject != NULL){
		kobject_put(files_kobject);
	}
	delete_counters();
	printk(KERN_INFO "DEBUG HWCOUNTER: Goodbye!!\n");
}

MODULE_AUTHOR("Pablo Hernandez <pabloheralm@gmail.com>");
MODULE_DESCRIPTION("HWCounter is a kernel module that exposes \
	the hardware counters from the cpu");
MODULE_LICENSE("GPL");

late_initcall_sync(hwcounter_init);
module_exit(hwcounter_exit);
