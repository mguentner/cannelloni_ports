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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "udp.h"
#include "cannelloni.h"
#include "cannelloni_platform.h"

#pragma GCC optimize ("O0")

volatile struct canfd_frame can_tx_frames[CANNELLONI_CAN_BUFFER_SIZE];
volatile struct canfd_frame can_rx_frames[CANNELLONI_CAN_BUFFER_SIZE];

volatile int32_t can_tx_frames_pos = -1;
volatile int32_t can_rx_frames_pos = -1;

uint32_t cannelloni_sequence_number = 0;

struct udp_pcb *cannelloni_udp_pcb;

void (*can_tx_function)(struct canfd_frame *f) = NULL;
void (*can_rx_function)() = NULL;


void init_cannelloni()
{
  cannelloni_udp_pcb = udp_new();
  if (cannelloni_udp_pcb == NULL) {
    return;
  }
  if (udp_bind(cannelloni_udp_pcb, IP_ADDR_ANY, CANNELLONI_PORT)) {
    return;
  }
  udp_recv(cannelloni_udp_pcb, handle_cannelloni_frame, NULL);
}

uint32_t cannelloni_udp_rx_count;


void handle_cannelloni_frame(void *arg, struct udp_pcb *pcb, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
  if (p != NULL) {
    struct cannelloni_data_packet *data;
    uint8_t error = FALSE;
    /* Check for OP Code */
    data = (struct cannelloni_data_packet*) p->payload;
    if (data->version != CANNELLONI_FRAME_VERSION) {
      /* Received wrong version */
      error = TRUE;
    }
    if (data->op_code != DATA) {
      /* Received wrong OP code */
      error = TRUE;
    }
    if (ntohs(data->count) == 0) {
      /* Received empty packet */
      error = TRUE;
    }
    if (!error) {
      uint8_t *rawData = ((uint8_t*)p->payload)+CANNELLONI_DATA_PACKET_BASE_SIZE;
      cannelloni_udp_rx_count++;
      uint16_t i=0;
      for (i=0; i<ntohs(data->count); i++) {
        if (rawData-(uint8_t*)(p->payload)+CANNELLONI_FRAME_BASE_SIZE > p->tot_len) {
          /* Received incomplete packet */
          error = TRUE;
          break;
        }
        /* We got at least a complete canfd_frame header */
        struct canfd_frame *frame = get_can_tx_frame();
        if (!frame) {
          /* Allocation error */
          error = TRUE;
          break;
        }
        uint32_t debug = (rawData[0] << 24) | (rawData[1] << 16) | (rawData[2] << 8) | (rawData[3]);
        //debug = (uint32_t*)*rawData;
        frame->can_id = debug;
        /* += 4 */
        rawData += sizeof(canid_t);
        frame->len = *rawData;
        /* += 1 */
        rawData += sizeof(frame->len);
        /* If this is a CAN FD frame, also retrieve the flags */
        if (frame->len & CANFD_FRAME) {
          frame->flags = *rawData;
          /* += 1 */
          rawData += sizeof(frame->flags);
        }
        /* RTR Frames have no data section although they have a dlc */
        if ((frame->can_id & CAN_RTR_FLAG) == 0) {
          /* Check again now that we know the dlc */
          if (rawData-(uint8_t*)(p->payload)+canfd_len(frame) > p->tot_len) {
            /* Received incomplete packet / can header corrupt! */
            error = TRUE;
            break;
          }
          memcpy(frame->data, rawData, canfd_len(frame));
          rawData += canfd_len(frame);
        }
      }
    }
    pbuf_free(p);
  }
}

void transmit_udp_frame()
{
  if (can_rx_frames_pos >= 0) {
    struct ip_addr remote_addr;
    struct cannelloni_data_packet *dataPacket;

    CANNELLONI_REMOTE_ADDR(&remote_addr);
    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 1200, PBUF_RAM);
    uint16_t length = 0;
    uint16_t frameCount = 0;
    struct canfd_frame *frame;
    uint8_t *data = (uint8_t*) p->payload + CANNELLONI_DATA_PACKET_BASE_SIZE;
    if (!p) {
      /* allocation error */
      return;
    }

    while(can_rx_frames_pos >= 0) {
      frame = &can_rx_frames[can_rx_frames_pos];
      data[0] = frame->can_id >> 24 & 0x000000ff;
      data[1] = frame->can_id >> 16 & 0x000000ff;
      data[2] = frame->can_id >> 8  & 0x000000ff;
      data[3] = frame->can_id       & 0x000000ff;
      /* += 4 */
      data += sizeof(canid_t);
      *data = frame->len;
      /* += 1 */
      data += sizeof(frame->len);
      memcpy(data, frame->data, canfd_len(frame));
      data+=canfd_len(frame);
      frameCount++;
      can_rx_frames_pos--;
    }

    dataPacket = (struct cannelloni_data_packet*) p->payload;
    dataPacket->version = CANNELLONI_FRAME_VERSION;
    dataPacket->op_code = DATA;
    dataPacket->seq_no = cannelloni_sequence_number++;
    dataPacket->count = htons(frameCount);

    length = (uint8_t*)data-(uint8_t*)p->payload;
    p->tot_len = length;
    p->len = length;

    udp_sendto(cannelloni_udp_pcb, p, &remote_addr, CANNELLONI_REMOTE_PORT);
    pbuf_free(p);
  }
}

void transmit_can_frames()
{
  if (!can_tx_function)
    return;
  if (can_tx_frames_pos >= 0) {
    can_tx_function(&(can_tx_frames[can_tx_frames_pos]));
    can_tx_frames_pos--;
  }
}

void receive_can_frames()
{
  if (!can_rx_function) {
    return;
  }
  can_rx_function();
}

void run_cannelloni()
{
  transmit_can_frames();
  receive_can_frames();
  transmit_udp_frame();
}

struct canfd_frame* get_can_tx_frame()
{
  if (can_tx_frames_pos < CANNELLONI_CAN_BUFFER_SIZE) {
    can_tx_frames_pos++;
    return &(can_tx_frames[can_tx_frames_pos]);
  }
  return NULL;
}

struct canfd_frame* get_can_rx_frame()
{
  if (can_rx_frames_pos < CANNELLONI_CAN_BUFFER_SIZE) {
    can_rx_frames_pos++;
    return &(can_rx_frames[can_rx_frames_pos]);
  }
  return NULL;
}

uint8_t canfd_len(const struct canfd_frame *f)
{
  return f->len & ~(CANFD_FRAME);
}
