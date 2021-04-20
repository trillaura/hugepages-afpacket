#include "perf.h"

unsigned long long rdclock(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void update_stats(struct stats *stats, uint64_t val)
{
	double delta;

	stats->n++;
	delta = val - stats->mean;
	stats->mean += delta / stats->n;
	stats->M2 += delta * (val - stats->mean);
}

static double avg_stats(struct stats *stats)
{
	return stats->mean;
}

/* perf_event_open syscall wrapper */
long
sys_perf_event_open(struct perf_event_attr *hw_event,
		    pid_t pid, int cpu, int group_fd, unsigned long flags)
{
	return syscall(__NR_perf_event_open, hw_event, pid, cpu,
		       group_fd, flags);
}

static inline pid_t gettid()
{
	return syscall(SYS_gettid);
}

static struct perf_event_attr default_attrs[] = {

	{.type = PERF_TYPE_SOFTWARE,.config = PERF_COUNT_SW_PAGE_FAULTS},

	{.type = PERF_TYPE_HW_CACHE,.config =
	 (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
	  (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
	{.type = PERF_TYPE_HW_CACHE,.config =
	 (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_READ << 8) |
	  (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},
	{.type = PERF_TYPE_HW_CACHE,.config =
	 (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
	  (PERF_COUNT_HW_CACHE_RESULT_ACCESS << 16))},
	{.type = PERF_TYPE_HW_CACHE,.config =
	 (PERF_COUNT_HW_CACHE_DTLB | (PERF_COUNT_HW_CACHE_OP_WRITE << 8) |
	  (PERF_COUNT_HW_CACHE_RESULT_MISS << 16))},

	{.type = PERF_TYPE_HARDWARE,.config = PERF_COUNT_HW_REF_CPU_CYCLES},
	{.type = PERF_TYPE_HARDWARE,.config = PERF_COUNT_HW_INSTRUCTIONS},
	{.type = PERF_TYPE_HARDWARE,.config = PERF_COUNT_HW_CACHE_MISSES},
	{.type = PERF_TYPE_HARDWARE,.config = PERF_COUNT_HW_CACHE_REFERENCES},
	//{.type = PERF_TYPE_HARDWARE,.config = PERF_COUNT_HW_REF_CPU_CYCLES},
	/* PERF_COUNT_HW_REF_CPU_CYCLES (since Linux 3.3) 
	 * Total cycles; not affected by CPU frequency scaling.
	 * */
};

int get_counters()
{
	return __LIBPERF_ARRAY_SIZE(default_attrs);
}

char *get_counter(int i)
{
	char counters[9][20]= { "page-faults", "dTLB-loads", "dTLB-load-misses",
	       "dTLB-stores", "dTLB-store-misses", "cycles", "instructions",
	       "cache-misses", "cache-references" };
	char *s = strdup(counters[i]);
	return s;
}

/* thread safe */
/* sets up a set of fd's for profiling code to read from */
struct libperf_data *libperf_initialize(pid_t pid, int cpu)
{
	int nr_counters = __LIBPERF_ARRAY_SIZE(default_attrs);

	int i;

	struct libperf_data *pd = malloc(sizeof(struct libperf_data));
	if (pd == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	if (pid == -1)
		pid = getpid(); // TODO or gettid() ?

	pd->group = -1;

	for (i = 0; i < (int) __LIBPERF_ARRAY_SIZE(pd->fds); i++)
		pd->fds[i] = -1;

	pd->pid = pid;
	pd->cpu = cpu;

	struct perf_event_attr *attrs =
	    malloc(nr_counters * sizeof(struct perf_event_attr));

	if (attrs == NULL) {
		perror("malloc");
		exit(EXIT_FAILURE);
	}

	memcpy(attrs, default_attrs, sizeof(default_attrs));
	pd->attrs = attrs;

	for (i = 0; i < nr_counters; i++) {
		attrs[i].size = sizeof(struct perf_event_attr);
		attrs[i].inherit = 1;
		attrs[i].disabled = 1;
		attrs[i].enable_on_exec = 0;
		pd->fds[i] = sys_perf_event_open(&attrs[i], pid, cpu, -1, 0);
		if (pd->fds[i] < 0) {
			fprintf(stderr, "At event %d/%d\n", i, nr_counters);
			perror("sys_perf_event_open");
			exit(EXIT_FAILURE);
		}

	}

	pd->wall_start = rdclock();
	return pd;
}

uint64_t get_cpuclock_counter(struct libperf_data *pd) {
	int *fds = pd->fds;
	int CPUCLOCK = 5;
	uint64_t count[3];
	uint64_t clock;

	assert(fds[CPUCLOCK] >= 0);

	clock = read(fds[CPUCLOCK], count, sizeof(uint64_t));
	assert(clock == sizeof(uint64_t));

	return count[0];
}

/* thread safe */
/* pass in int* from initialize function */
/* reads from fd's, prints out stats, and closes them all */
void libperf_finalize(struct libperf_data *pd)
{
	int i, result, nr_counters = __LIBPERF_ARRAY_SIZE(default_attrs);

	int *fds = pd->fds;

	uint64_t count[3];	/* potentially 3 values */

	struct stats event_stats[nr_counters];

	struct stats walltime_nsecs_stats;
	walltime_nsecs_stats.n = 0;
	walltime_nsecs_stats.mean = 0;

	for (i = 0; i < nr_counters; i++) {
		assert(fds[i] >= 0);
		result = read(fds[i], count, sizeof(uint64_t));
		assert(result == sizeof(uint64_t));

		update_stats(&event_stats[i], count[0]);

		close(fds[i]);
		fds[i] = -1;

		fprintf(stderr, "%14.0f %s\n",
			avg_stats(&event_stats[i]), get_counter(i));
	}

	update_stats(&walltime_nsecs_stats, rdclock() - pd->wall_start);
	fprintf(stderr, "%14.9f elapsed time\n", 
		avg_stats(&walltime_nsecs_stats) / 1e9);
	free(pd->attrs);
	free(pd);
}

uint64_t libperf_readcounter(struct libperf_data *pd, int counter)
{
	uint64_t value;

	assert(counter >= 0 && counter < __LIBPERF_MAX_COUNTERS);

	if (counter == __LIBPERF_MAX_COUNTERS)
		return (uint64_t) (rdclock() - pd->wall_start);

	assert(read(pd->fds[counter], &value, sizeof(uint64_t)) ==
	       sizeof(uint64_t));

	return value;
}

int libperf_enablecounter(struct libperf_data *pd, int counter)
{
	assert(counter >= 0 && counter < __LIBPERF_MAX_COUNTERS);
	if (pd->fds[counter] == -1)
		assert((pd->fds[counter] =
			sys_perf_event_open(&(pd->attrs[counter]), pd->pid,
					    pd->cpu, pd->group, 0)) != -1);

	return ioctl(pd->fds[counter], PERF_EVENT_IOC_ENABLE);
}

int libperf_disablecounter(struct libperf_data *pd, int counter)
{
	assert(counter >= 0 && counter < __LIBPERF_MAX_COUNTERS);
	if (pd->fds[counter] == -1)
		return 0;

	return ioctl(pd->fds[counter], PERF_EVENT_IOC_DISABLE);
}

void libperf_close(struct libperf_data *pd)
{
	int i, nr_counters = __LIBPERF_ARRAY_SIZE(default_attrs);

	for (i = 0; i < nr_counters; i++) {
		assert(pd->fds[i] >= 0);
		close(pd->fds[i]);
	}

	free(pd->attrs);
	free(pd);
}
