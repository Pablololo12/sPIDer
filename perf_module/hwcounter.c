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

#define VFP_SPEC 0x75
#define ASE_SPEC 0x74
#define MEM_ACCESS 0x13
#define INST_RETIRED 0x08
#define DEF_SAMPLING_VALUE (1000000)

static struct kobject *files_kobject;
static u32 float_count[NR_CPUS] = {0};
static u32 simd_count[NR_CPUS] = {0};
static u32 mem_count[NR_CPUS] = {0};
static u32 inst_count[NR_CPUS] = {0};
static u32 float_count_prev[NR_CPUS] = {0};
static u32 simd_count_prev[NR_CPUS] = {0};
static u32 mem_count_prev[NR_CPUS] = {0};
static u32 inst_count_prev[NR_CPUS] = {0};
static struct perf_event *pe[NR_CPUS][4];
static DEFINE_PER_CPU(char, is_a73);
static int die_hw = 0;
static int a73_id[4] = {0};
static int num_a73 = 0;

static void periodic_task(struct work_struct *work);
static struct workqueue_struct *periodic_workqueue;
static DECLARE_DELAYED_WORK(task, periodic_task);

static int hwcounter_perf_event_initialize(int cpu);

static void read_from_registers(void)
{
	u32 total;
	u64 enabled, running;
	int i, aux;

	for (i=0; i<num_a73; i++) {
		aux = a73_id[i];
		if (IS_ERR(pe[aux][0]))
			hwcounter_perf_event_initialize(a73_id[i]);
		if (IS_ERR(pe[aux][0]))
			continue;
		total = (u32) perf_event_read_value(pe[aux][0], &enabled, &running);
		inst_count[aux] = total - inst_count_prev[aux];
		inst_count_prev[aux] = total;
		total = (u32) perf_event_read_value(pe[aux][1], &enabled, &running);
		mem_count[aux] = total - mem_count_prev[aux];
		mem_count_prev[aux] = total;
		total = (u32) perf_event_read_value(pe[aux][2], &enabled, &running);
		float_count[aux] = total - float_count_prev[aux];
		float_count_prev[aux] = total;
		total = (u32) perf_event_read_value(pe[aux][3], &enabled, &running);
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

static ssize_t float_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	int i=0;
	u64 aux = 0;
	for(i=0; i<num_possible_cpus(); i++){
		if(cpu_online(i)) {
			aux += float_count[i];
		}
	}
	return sprintf(buf, "%llu\n", aux);
}

static ssize_t simd_show(struct kobject *kobj, struct kobj_attribute *attr,
								char *buf)
{
	int i=0;
	u64 aux = 0;
	for(i=0; i<num_possible_cpus(); i++){
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
	for(i=0; i<num_possible_cpus(); i++){
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
	for(i=0; i<num_possible_cpus(); i++){
		if(cpu_online(i)) {
			aux += inst_count[i];
		}
	}
	return sprintf(buf, "%llu\n", aux);
}

static struct kobj_attribute float_attribute =__ATTR(float_count, 0444,
							float_show, NULL);
static struct kobj_attribute simd_attribute =__ATTR(simd_count, 0444,
							simd_show, NULL);
static struct kobj_attribute mem_attribute =__ATTR(mem_count, 0444,
							mem_show, NULL);
static struct kobj_attribute inst_attribute =__ATTR(inst_count, 0444,
							inst_show, NULL);

static struct perf_event_attr pea_INST_RETIRED = {
	.type		= PERF_TYPE_RAW,
	.config		= INST_RETIRED,
	.size		= sizeof(struct perf_event_attr),
	.disabled	= 0
};
static struct perf_event_attr pea_VFP_SPEC = {
	.type		= PERF_TYPE_RAW,
	.config		= VFP_SPEC,
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
	pe[cpu][0] = perf_event_create_kernel_counter(&pea_INST_RETIRED,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][0]))
		return PTR_ERR(pe[cpu][0]);
	pe[cpu][1] = perf_event_create_kernel_counter(&pea_MEM_ACCESS,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][1]))
		return PTR_ERR(pe[cpu][1]);
	pe[cpu][2] = perf_event_create_kernel_counter(&pea_VFP_SPEC,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][2]))
		return PTR_ERR(pe[cpu][2]);
	pe[cpu][3] = perf_event_create_kernel_counter(&pea_ASE_SPEC,cpu,
							NULL,NULL,NULL);
	if (IS_ERR(pe[cpu][3]))
		return PTR_ERR(pe[cpu][3]);
	return 0;
}

static int __init hwcounter_init(void)
{
	int error = 0,i;
	char is73;
	printk(KERN_DEBUG "Initializing HWcounter module\n");

	files_kobject = kobject_create_and_add("hwcounters",kernel_kobj);
	if(!files_kobject){
		error = -1;
		goto exit_hwcounter_init;
	}

	error = sysfs_create_file(files_kobject, &float_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	error = sysfs_create_file(files_kobject, &simd_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	error = sysfs_create_file(files_kobject, &mem_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	error = sysfs_create_file(files_kobject, &inst_attribute.attr);
	if (error)
		goto delete_sysfs_exit;

	printk(KERN_DEBUG "sysfs created succesfully\n");
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
	printk(KERN_DEBUG "Module initialized\n");
	return 0;

delete_sysfs_exit:
	kobject_put(files_kobject);
exit_hwcounter_init:
	return error;
}

static void delete_counters(void)
{
	int i;
	printk(KERN_DEBUG "Disabling Counters\n");
	
	for(i=0; i<num_possible_cpus(); i++) {
		if(pe[i][0]!=NULL) {
			perf_event_disable(pe[i][0]);
			perf_event_release_kernel(pe[i][0]);
		}
		if(pe[i][1]!=NULL) {
			perf_event_disable(pe[i][1]);
			perf_event_release_kernel(pe[i][1]);
		}
		if(pe[i][2]!=NULL) {
			perf_event_disable(pe[i][2]);
			perf_event_release_kernel(pe[i][2]);
		}
		if(pe[i][3]!=NULL) {
			perf_event_disable(pe[i][3]);
			perf_event_release_kernel(pe[i][3]);
		}
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
	printk(KERN_DEBUG "Goodbye!!\n");
}

MODULE_AUTHOR("Pablo Hernandez <pabloheralm@gmail.com>");
MODULE_DESCRIPTION("HWCounter is a kernel module that exposes \
	the hardware counters from the cpu");
MODULE_LICENSE("GPL");

late_initcall_sync(hwcounter_init);
module_exit(hwcounter_exit);
