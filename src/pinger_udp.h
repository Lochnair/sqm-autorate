#ifndef PINGER_UDP_H
#define PINGER_UDP_H

#include <stdint.h>

struct udp_timestamp_hdr
{
	uint8_t type;
	uint8_t code;
	uint16_t checksum;
	uint16_t identifier;
	uint16_t sequence;
	uint32_t originateTime;
	uint32_t originateTimeNs;
	uint32_t receiveTime;
	uint32_t receiveTimeNs;
	uint32_t transmitTime;
	uint32_t transmitTimeNs;
};

#endif // PINGER_UDP_H