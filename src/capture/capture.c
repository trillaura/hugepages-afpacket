#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <linux/ip.h>
#include <fcntl.h>
#include <errno.h>

#include "murmur3.h"
#include "hugepages.h"
#include "time.h"
#include "perf.h"

#ifndef likely
# define likely(x) __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
# define unlikely(x) __builtin_expect(!!(x), 0)
#endif

#define FREQ_DELAY 100

struct huge_mm {
	unsigned long usr_addr;
	char *filename;
	int hfd;
};

struct block_desc {
	uint32_t version;
	uint32_t offset_to_priv;
	struct tpacket_hdr_v1 h1;
};

struct ring {
	struct iovec *rd;
	uint8_t *map;
	struct huge_mm h_mm;
	struct tpacket_req3 req;
};

static uint64_t packets_total = 0, bytes_total = 0;
static uint64_t max_cpuclock = 0;
static uint64_t min_cpuclock = -1;
static uint64_t avg_cpuclock = 0;
static uint64_t var_cpuclock = 0;

static sig_atomic_t sigint = 0;

static void sighandler(int num)
{
	num = num;
	sigint = 1;
}

static void teardown_socket(struct ring *ring, int fd)
{
	int err;

	if (ring->h_mm.usr_addr)
		munmap((void *)ring->h_mm.usr_addr, 
			ring->req.tp_block_size * ring->req.tp_block_nr);

	if (fd > 0) {
		err = close(fd);
		if (err < 0)
			perror("closing socket");
	}

	if (ring->h_mm.hfd > 0)
		close(ring->h_mm.hfd);

	if (ring->h_mm.filename)
		unlink(ring->h_mm.filename);

	if (ring->rd)
		free(ring->rd);
}

static int setup_hugepages(int fd, struct ring *ring, unsigned int size)
{	
	char *hmountpoint;
	void *shm_hugepages;
	unsigned long user_addr;
	unsigned int hpagesize;
	char filename[256];
	int hfd;
	int err;
       
	hmountpoint = get_hugepages_mountpoint();
	if (!hmountpoint) {
		perror("hugepage mount");
		return -1;
	}
	
	hpagesize = get_hugepage_size();
	if (!hpagesize) {
		perror("hugepage size");
		return -1;
	}
	
	snprintf(filename, 256, "%s/buffer_%d", hmountpoint, getpid());
	hfd = open(filename, O_CREAT | O_RDWR, 0755);
	if (!hfd) {
		perror("hugepage open");
		return -1;
	}

	ring->h_mm.filename = strdup(filename);
	ring->h_mm.hfd = hfd;

	if (size % hpagesize)
		size += (hpagesize - (size % hpagesize));
	
	shm_hugepages =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, hfd, 0);
	if (shm_hugepages == MAP_FAILED) {
		perror("hugepage mmap");
		return -1;
	}

	user_addr = (unsigned long) shm_hugepages;

	ring->h_mm.usr_addr = user_addr;

	err = setsockopt(fd, SOL_PACKET, 24, &user_addr, sizeof(user_addr));
	if (err < 0) {
		perror("hugetlb enable setsockopt");
		return -1;
	}
	return 0;
}

static int setup_hugesocket(struct ring *ring, char *netdev, int option, int nblocks)
{
	int err, fd, v = TPACKET_V3;
	unsigned int i;
	struct sockaddr_ll ll;
	unsigned int blocksiz = (1 << 22), framesiz = 1 << 11;
	unsigned int blocknum = nblocks;
	unsigned int timeout = 60;

	fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (fd < 0) {
		perror("socket");
		return 0;
	}

	err = setsockopt(fd, SOL_PACKET, PACKET_VERSION, &v, sizeof(v));
	if (err < 0) {
		perror("setsockopt");
		teardown_socket(ring, fd);
		return 0;
	}

	memset(&ring->req, 0, sizeof(ring->req));
	ring->req.tp_block_size = blocksiz;
	ring->req.tp_frame_size = framesiz;
	ring->req.tp_block_nr = blocknum;
	ring->req.tp_frame_nr = (blocksiz * blocknum) / framesiz;
	ring->req.tp_retire_blk_tov = timeout;
	ring->req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
	if (option == 25) {
		err = setup_hugepages(fd, ring, blocknum*blocksiz);
		if (err < 0) {
			perror("allocating ring");
			teardown_socket(ring, fd);
			return 0;
		}
	}
	
	err = setsockopt(fd, SOL_PACKET, option, &ring->req,
			 sizeof(ring->req));
	if (err < 0) {
		perror("allocating ring");
		teardown_socket(ring, fd);
		return 0;
	}

	if (option != 25) {
		ring->map = (uint8_t *) mmap(NULL, ring->req.tp_block_size * ring->req.tp_block_nr,
				 PROT_READ | PROT_WRITE, MAP_SHARED, fd,
				 0);
		if (ring->map == MAP_FAILED) {
			perror("mmap");
			teardown_socket(ring, fd);
			return 0;
		}
	} else
		ring->map = (uint8_t *) ring->h_mm.usr_addr;

	ring->rd = malloc(ring->req.tp_block_nr * sizeof(*ring->rd));
	if (!ring->rd) {
		perror("malloc");
		teardown_socket(ring, fd);
		return 0;
	}

	for (i = 0; i < ring->req.tp_block_nr; ++i) {
		ring->rd[i].iov_base =
	    		ring->map + (i * ring->req.tp_block_size);
			ring->rd[i].iov_len = ring->req.tp_block_size;
	}

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = PF_PACKET;
	ll.sll_protocol = htons(ETH_P_ALL);
	ll.sll_ifindex = if_nametoindex(netdev);
	ll.sll_hatype = 0;
	ll.sll_pkttype = 0;
	ll.sll_halen = 0;

	err = bind(fd, (struct sockaddr *)&ll, sizeof(ll));
	if (err < 0) {
		perror("bind");
		teardown_socket(ring, fd);
		return 0;
	}

	return fd;
}

static uint32_t __hash = 0U;
static void hash(struct tpacket3_hdr *ppd, const int block_num)
{
	unsigned long len = ppd->tp_snaplen;
	uint32_t seed = 1234, rxhash = 0;

	MurmurHash3_x86_32(ppd, len, seed, &rxhash);

	__hash += rxhash;
	if (0)
		printf("[b#%d] rxhash: 0x%x\n", block_num, rxhash);
}

static void delayPacket(long delay)
{
	if (0)
		printf("Delaying...");
	int res;
	clock_t start = clock();
	long i, j;
	for (i = 1; i <= delay; ++i) {
		for (j = 0; j < 1000; ++j) {
			__asm__("imull %%ebx, %%eax;" 
					: "=a" (res): "a" (j), "b" (j+1));	
		}
	}
	clock_t end = clock();
	if (0)
		printf("%lf seconds --> ",(double) (end-start)/CLOCKS_PER_SEC);	
}

static void update_cpuclock_stats(uint64_t slot)
{
	double delta;

	if (!slot)
		return;

	if (slot > max_cpuclock)
		max_cpuclock = slot;

	if (slot < min_cpuclock)
		min_cpuclock = slot;

	if (avg_cpuclock > (avg_cpuclock + slot)) {
		printf("Wrap\n");
		exit(EXIT_FAILURE);
	}

	if (var_cpuclock > (var_cpuclock + slot*slot)) {
		printf("Wrap\n");
		exit(EXIT_FAILURE);
	}

	avg_cpuclock += slot;
	var_cpuclock += slot*slot;
}

static uint64_t get_cpuclock(void)
{
	uint64_t val;

	asm volatile ("rdtscp \n\t"
				"shl $32,%%rdx\n\t"
				"or %%rdx,%%rax\n\t"
				: "=&a" (val) : "0" (0) : "%rcx", "%rdx");
	return val;
}

static void walk_block(struct block_desc *pbd, const int block_num, 
		long delay, struct libperf_data *pd)
{
	int num_pkts = pbd->h1.num_pkts, i;
	unsigned long bytes = 0;
	struct tpacket3_hdr *ppd;
	uint64_t start, end;
	

	ppd = (struct tpacket3_hdr *)((uint8_t *) pbd +
				      pbd->h1.offset_to_first_pkt);
	for (i = 0; i < num_pkts; ++i) {
		
		if (!((packets_total + i) % FREQ_DELAY) &&
				packets_total) 
			delayPacket(delay);

		start = get_cpuclock();
		hash(ppd, block_num);
		end = get_cpuclock();

		if (end < start)
			update_cpuclock_stats(start - end + 1);
		else
			update_cpuclock_stats(end - start);

		bytes += ppd->tp_snaplen;

		ppd = (struct tpacket3_hdr *)
			((uint8_t *) ppd + ppd->tp_next_offset);
	}

	packets_total += num_pkts;
	bytes_total += bytes;
}

static void flush_block(struct block_desc *pbd)
{
	pbd->h1.block_status = TP_STATUS_KERNEL;
}

static double avg()
{
	return (double) avg_cpuclock/packets_total;
}

static double var(double m)
{
	double avg_v = (double) var_cpuclock/packets_total;
	
	return avg_v - m*m;
}


int main(int argc, char **argp)
{
	int fd, err;
	socklen_t len;
	struct ring ring;
	struct pollfd pfd;
	unsigned int block_num = 0, blocks = 64;
	long delay = 1;
	struct block_desc *pbd;
	struct tpacket_stats_v3 stats;

	if (argc != 5) {
		fprintf(stderr, "Usage: %s <interface> <option> <delay> <#blocks>\n \
				\twhere option is the type of block allocation:\n \
			        \t\t - 5: original rx_ring;\n \
			        \t\t - 25: hugetlbfs;\n \
				\t\t - 26: transparent hugepages;\n \
				\t\t - 27: contiguous (no retry);\n \
				\t\t - 28: discontiguous;\n \
			        \t\t - 29: contiguous (with retry);\n \
				\twhere delay is the number of times to repeat 1000\n\t \
				multiplication on a packet every 100 packets\n"
			, argp[0]);
 		return EXIT_FAILURE;
	}

	signal(SIGINT, sighandler);

	delay = atol(argp[3]);
	blocks = atoi(argp[4]);

	memset(&ring, 0, sizeof(ring));
	fd = setup_hugesocket(&ring, argp[1], atoi(argp[2]), blocks);
	if (!fd) {
		perror("socket setup");
		exit(1);
	}

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = fd;
	pfd.events = POLLIN | POLLERR;
	pfd.revents = 0;

	struct libperf_data *pd = libperf_initialize(getpid(), PERF_CPU);

	int i, nr_counters = get_counters();
	for (i=0; i< nr_counters; ++i)
		libperf_enablecounter(pd, i);

	while (likely(!sigint)) {
		pbd = (struct block_desc *)ring.rd[block_num].iov_base;

		if ((pbd->h1.block_status & TP_STATUS_USER) == 0) {
retry:
			err = poll(&pfd, 1, 10000);
			if (err < 0) {
				perror("poll");
				exit(1);
			}
			if (!err)
				goto retry;
			continue;
		}

		walk_block(pbd, block_num, delay, pd);
		flush_block(pbd);
		block_num = (block_num + 1) % blocks;
	}

	for (i=0; i < nr_counters; ++i)
		libperf_disablecounter(pd, i);

	libperf_finalize(pd);

	len = sizeof(stats);
	err = getsockopt(fd, SOL_PACKET, PACKET_STATISTICS, &stats, &len);
	if (err < 0) {
		perror("getsockopt");
		exit(1);
	}

	printf("__hash: %u", __hash);
	fflush(stdout);
	
	double a = avg();

	fprintf
	    (stderr, "\n \
	     %lu upackets\n \
	     %u kpackets\n \
	     %lu bytes\n \
	     %u dropped\n \
	     %lu maxcpucycles\n \
	     %lu mincpucycles\n \
	     %lf avgcpucycles\n \
	     %lf varcpucycles\n \
	     \n\n",
	     packets_total, stats.tp_packets, bytes_total,
	     stats.tp_drops, max_cpuclock, min_cpuclock, 
	     a, var(a));

	teardown_socket(&ring, fd);
	return 0;
}

