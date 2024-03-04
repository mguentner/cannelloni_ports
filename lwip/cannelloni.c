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

static void queue_init(frames_queue_t *q, struct canfd_frame *frames, size_t count)
{
  q->head = 0;
  q->tail = 0;
  q->count = count;
  q->frames = frames;
}

static struct canfd_frame *queue_put(frames_queue_t *q)
{
  if ((q->tail + 1) % q->count == q->head) {
    return NULL;
  }

  struct canfd_frame *frame = &(q->frames[q->tail]);
  q->tail = (q->tail + 1) % q->count;
  return frame;
}

static struct canfd_frame *queue_take(frames_queue_t *q)
{
  if (q->head == q->tail) {
    return NULL;
  }

  struct canfd_frame *frame = &(q->frames[q->head]);
  q->head = (q->head + 1) % q->count;
  return frame;
}

static struct canfd_frame *queue_peek(frames_queue_t *q)
{
  if (q->head == q->tail) {
    return NULL;
  }

  return &(q->frames[q->head]);
}

void init_cannelloni(cannelloni_handle_t* handle)
{
  handle->sequence_number = 0;
  handle->udp_rx_count = 0;

  queue_init(&handle->tx_queue, handle->Init.can_tx_buf, handle->Init.can_buf_size);
  queue_init(&handle->rx_queue, handle->Init.can_rx_buf, handle->Init.can_buf_size);

  handle->udp_pcb = udp_new();
  if (handle->udp_pcb == NULL) {
    return;
  }
  if (udp_bind(handle->udp_pcb, IP_ADDR_ANY, handle->Init.port)) {
    return;
  }
  udp_recv(handle->udp_pcb, handle_cannelloni_frame, (void*)handle);
}

void handle_cannelloni_frame(void *arg, struct udp_pcb *pcb, struct pbuf *p, const ip_addr_t *addr, uint16_t port)
{
  cannelloni_handle_t *const handle = (cannelloni_handle_t *const)arg;
  if (p != NULL) {
    struct cannelloni_data_packet *data;
    uint8_t error = 0;
    /* Check for OP Code */
    data = (struct cannelloni_data_packet*) p->payload;
    if (data->version != CANNELLONI_FRAME_VERSION) {
      /* Received wrong version */
      error = 1;
    }
    if (data->op_code != CNL_DATA) {
      /* Received wrong OP code */
      error = 1;
    }
    if (ntohs(data->count) == 0) {
      /* Received empty packet */
      error = 1;
    }
    if (!error) {
      uint8_t *rawData = ((uint8_t*)p->payload)+CANNELLONI_DATA_PACKET_BASE_SIZE;
      handle->udp_rx_count++;
      uint16_t i=0;
      for (i=0; i<ntohs(data->count); i++) {
        if (rawData-(uint8_t*)(p->payload)+CANNELLONI_FRAME_BASE_SIZE > p->tot_len) {
          /* Received incomplete packet */
          error = 1;
          break;
        }
        /* We got at least a complete canfd_frame header */
        struct canfd_frame *frame = queue_put(&handle->tx_queue);
        if (!frame) {
          /* Allocation error */
          error = 1;
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
            error = 1;
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

void transmit_udp_frame(cannelloni_handle_t* handle)
{
  struct canfd_frame *frame = queue_take(&handle->rx_queue);
  if (!frame) {
    return;
  }

  struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, 1200, PBUF_RAM);
  uint16_t length = 0;
  uint16_t frameCount = 0;
  uint8_t *data = (uint8_t*) p->payload + CANNELLONI_DATA_PACKET_BASE_SIZE;
  if (!p) {
    /* allocation error */
    return;
  }

  do {
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
  } while (frame = queue_take(&handle->rx_queue));

  struct cannelloni_data_packet *dataPacket = (struct cannelloni_data_packet*) p->payload;
  dataPacket->version = CANNELLONI_FRAME_VERSION;
  dataPacket->op_code = CNL_DATA;
  dataPacket->seq_no = handle->sequence_number++;
  dataPacket->count = htons(frameCount);

  length = (uint8_t*)data-(uint8_t*)p->payload;
  p->tot_len = length;
  p->len = length;

  udp_sendto(handle->udp_pcb, p, &(handle->Init.addr), handle->Init.remote_port);
  pbuf_free(p);
}

void transmit_can_frames(cannelloni_handle_t *const handle)
{
  if (!handle->Init.can_tx_fn)
    return;
  struct canfd_frame *frame = queue_peek(&handle->tx_queue);
  if (frame) {
    if (handle->Init.can_tx_fn(handle, frame)) {
      queue_take(&handle->tx_queue);
    }
  }
}

void receive_can_frames(cannelloni_handle_t* handle)
{
  if (!handle->Init.can_rx_fn) {
    return;
  }
  handle->Init.can_rx_fn(handle);
}

void run_cannelloni(cannelloni_handle_t *const handle)
{
  transmit_can_frames(handle);
  receive_can_frames(handle);
  transmit_udp_frame(handle);
}

struct canfd_frame *get_can_rx_frame(cannelloni_handle_t *const handle)
{
  return queue_put(&handle->rx_queue);
}

uint8_t canfd_len(const struct canfd_frame *f)
{
  return f->len & ~(CANFD_FRAME);
}
