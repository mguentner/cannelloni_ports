#include "cannelloni.h"

#include "stdlib.h"

void init_can() {
}

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

void my_can_receive() {
  if (is_can_frame_pending())
    CanMessage msg;
    read_can_frame_from_controller(&msg);
		struct canfd_frame *frame = get_can_rx_frame();
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
