
#include <assert.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stropts.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <linux/perf_event.h>

#define __LIBPERF_MAX_COUNTERS 9
#define __LIBPERF_ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define PERF_CPU 87

/* lib struct */
struct libperf_data {
	int group;
	int fds[__LIBPERF_MAX_COUNTERS];
	struct perf_event_attr *attrs;
	pid_t pid;
	int cpu;
	unsigned long long wall_start;
};

struct stats {
	double n, mean, M2;
};


unsigned long long rdclock(void);
long sys_perf_event_open(struct perf_event_attr *hw_event,
		    pid_t pid, int cpu, int group_fd, unsigned long flags);
struct libperf_data *libperf_initialize(pid_t pid, int cpu);
void libperf_finalize(struct libperf_data *pd);
uint64_t libperf_readcounter(struct libperf_data *pd, int counter);
int libperf_enablecounter(struct libperf_data *pd, int counter);
int libperf_disablecounter(struct libperf_data *pd, int counter);
void libperf_close(struct libperf_data *pd);
int get_counters();
uint64_t get_cpuclock_counter(struct libperf_data *pd);
