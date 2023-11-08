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

#include "cannelloni.h"

#define CNL_BUF_SIZE 16

cannelloni_handle_t cannelloni_handle;
struct canfd_frame tx_buf[CNL_BUF_SIZE];
struct canfd_frame rx_buf[CNL_BUF_SIZE];

/* to be set in a timer routine */
extern volatile uint8_t tcp_timeout = 0;
extern volatile uint8_t arp_timeout = 0;

void my_can_transmit(struct canfd_frame *frame) {
  //write data in canfd_frame to interface...
    uint32_t dataLow = frame->data[0] << 24 |
    		           frame->data[1] << 16 |
    		           frame->data[2] << 8 |
    		           frame->data[3];
    uint32_t dataHigh = frame->data[4] << 24 |
	           	   	    frame->data[5] << 16 |
	           	   	    frame->data[6] << 8 |
	           	   	    frame->data[7];
}

void my_can_receive(void) {
  if (is_can_frame_pending()) {
    CanMessage msg;
    read_can_frame_from_controller(&msg);
		struct canfd_frame *frame = get_can_rx_frame(&cannelloni_handle);
		if (frame) {
			frame->can_id = msg.id;
			frame->len = msg.len;
			frame->data[0] = msg.data[0] >> 24 & 0x000000ff;
			frame->data[1] = msg.data[0] >> 16 & 0x000000ff;
			frame->data[2] = msg.data[0] >> 8  & 0x000000ff;
			frame->data[3] = msg.data[0]       & 0x000000ff;
			frame->data[4] = msg.data[1] >> 24 & 0x000000ff;
			frame->data[5] = msg.data[1] >> 16 & 0x000000ff;
			frame->data[6] = msg.data[1] >> 8  & 0x000000ff;
			frame->data[7] = msg.data[1]       & 0x000000ff;
		}
	}
}

void init_system(void) {
  IP4_ADDR(&cannelloni_handle.Init.addr, 10, 10, 10, 10);
  cannelloni_handle.Init.can_buf_size = CNL_BUF_SIZE;
  cannelloni_handle.Init.can_rx_buf = rx_buf;
  cannelloni_handle.Init.can_rx_fn = NULL;
  cannelloni_handle.Init.can_tx_buf = tx_buf;
  cannelloni_handle.Init.can_tx_fn = NULL;
  cannelloni_handle.Init.port = 20000;
  cannelloni_handle.Init.remote_port = 20000;

  init_can();
  init_ethernet();
  init_lwip();
  init_cannelloni(&cannelloni_handle);
}

void _main(void) {
  init_system();
  while(1) {
    if (tcp_timeout) {
      tcp_tmr();
      tcp_timeout = 0;
    }
    if (arp_timeout) {
      etharp_tmr();
      ip_reass_tmr();
    }
    run_cannelloni(&cannelloni_handle);
  }
}

