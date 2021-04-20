#ifdef CONFIG_PACKET_HUGE

#ifndef __PACKET_ALLOCATION_INTERNAL_H__
#define __PACKET_ALLOCATION_INTERNAL_H__

/* 	New options 	*/

#define PACKET_HUGETLB_ENABLE 	24
#define PACKET_HUGETLB_RING 	25
#define PACKET_TRANSHUGE_RING 	26
#define PACKET_GFPNORETRY_RING 	27
#define PACKET_VZALLOC_RING 	28
#define PACKET_GFPRETRY_RING 	29

/* 	Huge block memory descriptor 	*/
struct huge_mm {
	unsigned long 		shmaddr;
	struct page 		***hblocks;
	int 			npages;
	int 			thp_block;
};

#endif
#endif
