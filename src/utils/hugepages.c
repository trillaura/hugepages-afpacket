#include "hugepages.h"

size_t get_hugepage_size()
{
#define HP_1G  (1048576*1024)
#define HP_16M (16384*1024)
#define HP_4M  (4096*1024)
#define HP_2M  (2048*1024)

	struct stat s;
	if (stat("/sys/kernel/mm/hugepages/hugepages-1048576kB/nr_hugepages", &s) == 0)
		return HP_1G;
	if (stat("/sys/kernel/mm/hugepages/hugepages-16384kB/nr_hugepages", &s) == 0)
		return HP_16M;
	if (stat("/sys/kernel/mm/hugepages/hugepages-4096kB/nr_hugepages", &s) == 0)
		return HP_4M;
	if (stat("/sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages", &s) == 0)
		return HP_2M;
	return 0;
}

char *get_hugepages_mountpoint()
{
	FILE *mp;
	char *line = NULL, *mountpoint = NULL;
	ssize_t read;
	
	size_t len = 0;
	mp = fopen("/proc/mounts", "r");
	if (!mp) {
		perror("fopen");
		exit(1);
	}

	while ((read = getline(&line, &len, mp)) != -1) {
		char mbuff[256];
		if (sscanf(line, "hugetlbfs %s", mbuff) == 1) {
			mountpoint = strdup(mbuff);
			break;
		}
	}
	free(line);
	fclose(mp);
	return mountpoint;
}
