# Huge pages impact analisys on high speed network packet capture in the Linux kernel

_Master thesis for the course of Computer Science and Information Engineering_
_Advisor: Marco Cesati_
_Co-advisors: Emiliano Betti, Pierpaolo Santucci_

## Abstract
 The target we want to achieve with this experimental work is to change the management of the kernel side allocation of the socket buffer for reception of a high rate of incoming packets. We were inspired from existing frameworks for high-performance packet capture, finding in the huge page table mechanism a possible improvement for the operations on the memory resources.  

 The Linux kernel supports huge pages as optimization of the memory access, reducing the overhead caused by the TLB miss events. We studied two solutions to extends the features of the AF\_PACKET domain module of the Linux OS, working on the kernel version 4.20.  

 Changing this subsystem impacts on the most common applications used for traffic analysis, as Wireshark, Snort or tcpdump. Indeed, it is important to work for integration with those softwares to avoid the isolation of the solution. These softwares uses the pcap library that implements higher level APIs for the packet sockets.

 Hence, this library is the means by which it is possible to complete the integration. But since adapting a stable, widespread code, requires cascaded updates, this has a not negligible cost. Therefore, we move towards an alternative way, resorting to the LD\_PRELOAD trick. It allows to execute a preloaded implementation of a no-static function before the original one.  

 In order to prepare a realistic environment to evaluate the solutions, we considered a synthetic network traffic using the Linux module pktgen properly configured. We produced a pcap trace file with tcpdump to repeat the test with the same kind of incoming packets.

 Furthermore, in the user application that implements the capture process, we introduced a delay factor every a defined number of packets. The delay allows to simulate the processing time spent on the packets of interest and to study the behavior of the buffer when it accumulates packets.

## Content

* docs/: documentation (slides, thesis text)

* src/kernel4.20-patch: kernel patch (git diff) + added header
* src/pktgen.sh: script that generates packets using the kernel synthetic packet generator
* src/replay.sh: script that sends packets using tcpreplay
* src/run.sh: script that runs the whole process: capturing packets, sending packet trace, dumping performance counters
* src/capture, src/utils: application that exploits the features offered by the patch
* src/plot, src/stats: scripts for plotting test measurements

## License

MIT
