#ifndef DISAGG_COMMON_H
#define DISAGG_COMMON_H

#include <linux/if_ether.h>

#define ETH_PROT_NR 0x9876

#define MTU 9000

// Use v2 as v3 would only poll on block-level
// But maybe change to v3 as it allows variable sized frames
#define TPACKET_VERSION TPACKET_V2
#define HDRLEN TPACKET2_HDRLEN

#define BLOCK_SIZE (1 << 22)
#define FRAME_SIZE (1 << 14)
#define BLOCK_NR (8)
#define FRAME_NR (BLOCK_NR * BLOCK_SIZE / FRAME_SIZE)

#endif // DISAGG_COMMON_H
