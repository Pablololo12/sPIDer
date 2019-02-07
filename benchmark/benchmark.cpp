#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "Eigen/Dense"
#include <iostream>
#include <sched.h>

using namespace Eigen;
struct thread_result
{
	double time;
};

enum {
	VECTOR_ALIGNMENT = 16,
	};

static double
now_ms(void)
{
	struct timespec res;
	clock_gettime(CLOCK_REALTIME, &res);
	return 1000.0*res.tv_sec + (double)res.tv_nsec/1e6;
}

void *calculate_eigen_aux(void *arg)
{
	float temp;
	double t0, t1;
	int matrix_size = (long) arg;
	struct thread_result *tr = (struct thread_result*) malloc(sizeof(struct thread_result));
	MatrixXf m1(matrix_size, matrix_size);
	MatrixXf m2 = MatrixXf::Random(matrix_size,matrix_size);
	MatrixXf m3 = MatrixXf::Random(matrix_size,matrix_size);
	t0 = now_ms();
	// Only for characterize
	while(1)
		m1.noalias() = m2*m3;
	t1 = now_ms();
	tr->time = t1-t0;
	pthread_exit(tr);
}

double calculate_eigen(int num_threads, int matrix_size, int iter)
{
	pthread_t threads[num_threads];
	int i, d;
	double time = 0.0;
	double t0, t1;

	t0 = now_ms();
	for (d=0; d<iter; d++) {
		time = 0.0;
		for (i=0;i<num_threads;i++) {
			pthread_create(&threads[i], NULL, calculate_eigen_aux, (void *) matrix_size);
		}
		for(i=0;i<num_threads;i++) {
			struct thread_result *resp;
			pthread_join(threads[i],(void**)&resp);
			time = time + resp->time;
			free(resp);
		}
		printf("%f\n",time/num_threads);
	}
	t1 = now_ms();
	return t1-t0;
}

int check_affinity()
{
	cpu_set_t mask;
	long nproc, i;

	sched_getaffinity(0, sizeof(cpu_set_t), &mask);
	nproc = sysconf(_SC_NPROCESSORS_ONLN);
	printf("sched_getaffinity = ");
	for (i = 0; i < nproc; i++) {
		printf("%d ", CPU_ISSET(i, &mask));
	}
	printf("\n");
}

int main (int argc, char **argv)
{
	double time_eigen=0.0;
	int num_threads = 1;
	int matrix_size = 256;
	int opt;
	int iter=10;
	char *cpu_mask = NULL;
	char *aux;
	cpu_set_t mask;

	while ((opt = getopt (argc, argv, "t:m:c:i:")) != -1){
		switch(opt) {
			case 't':
				num_threads = atoi(optarg);
				break;
			case 'm':
				matrix_size = atoi(optarg);
				break;
			case 'c':
				cpu_mask = optarg;
				if(cpu_mask[0]=='a') cpu_mask=NULL;
				break;
			case 'i':
				iter = atoi(optarg);
				break;
		}
	}

	if (cpu_mask != NULL) {
		CPU_ZERO(&mask);
		aux = strtok (cpu_mask,"\",");
		while (aux != NULL)
		{
			CPU_SET(atoi(aux), &mask);
			aux = strtok (NULL, "\",");
		}
		sched_setaffinity(0, sizeof(cpu_set_t), &mask);
		//check_affinity();
	}

	time_eigen = calculate_eigen(num_threads,matrix_size,iter);
	printf("%f\n",time_eigen);
	return 0;
}
