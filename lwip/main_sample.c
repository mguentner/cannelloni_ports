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
#include "cannelloni_platform.h"

/* to be set in a timer routine */
extern volatile uint8_t tcp_timeout = 0;
extern volatile uint8_t arp_timeout = 0;

void init_system(void) {
  init_can();
  init_ethernet();
  init_lwip();
  init_cannelloni();
  can_tx_function = my_can_transmit; // see cannelloni_platform.h
  can_rx_function = my_can_receive;  // see cannelloni_platform.h
}

void main(void) {
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
    run_cannelloni();
  }
}

