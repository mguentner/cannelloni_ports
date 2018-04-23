/*
 * This is an example how to use cannelloni on embedded platforms
 * using the lwip stack
 *
 * Copyright (C) 2015-2018 Maximilian Güntner <code@sourcediver.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License Version 3 as
 * published by  the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */


#ifndef _CANNELLONI_H
#define _CANNELLONI_H

#include "stdint.h"

#define CANNELLONI_PORT 20000
#define CANNELLONI_REMOTE_ADDR(x) IP4_ADDR(x,10,1,0,2)
#define CANNELLONI_REMOTE_PORT 20000
#define CANNELLONI_CAN_BUFFER_SIZE 10
#define CANNELLONI_UDP_BUFFER_SIZE 10

/* Base size of a canfd_frame (canid + dlc) */
#define CANNELLONI_FRAME_BASE_SIZE 5
/* Size in byte of UDPDataPacket */
#define CANNELLONI_DATA_PACKET_BASE_SIZE 5

#define CANNELLONI_FRAME_VERSION 2
#define CANFD_FRAME              0x80

enum op_codes {DATA, ACK, NACK};

struct __attribute__((__packed__)) cannelloni_data_packet {
  /* Version */
  uint8_t version;
  /* OP Code */
  uint8_t op_code;
  /* Sequence Number */
  uint8_t seq_no;
  /* Number of CAN Messages in this packet */
  uint16_t count;
};

typedef uint32_t canid_t;

/* special address description flags for the CAN_ID */
#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

/* valid bits in CAN ID for frame formats */
#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */
#define CAN_ERR_MASK 0x1FFFFFFFU /* omit EFF, RTR, ERR flags */

#define CANFD_MAX_DLEN 8

struct canfd_frame  {
	canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t    len;     /* frame payload length in byte */
	uint8_t    flags;   /* additional flags for CAN FD */
	uint8_t    data[CANFD_MAX_DLEN] __attribute__((aligned(8)));
};

/* Helper function to get the real length of a frame */
uint8_t canfd_len(const struct canfd_frame *f);

void init_cannelloni(void);

void run_cannelloni(void);

void handle_cannelloni_frame(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port);

struct canfd_frame* get_can_tx_frame();
struct canfd_frame* get_can_rx_frame();

extern void (*can_tx_function)(struct canfd_frame *f);
extern void (*can_rx_function)();


#endif
