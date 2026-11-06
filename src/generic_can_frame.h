#ifndef GENERIC_CAN_FRAME_GUARD
#define GENERIC_CAN_FRAME_GUARD

#include <stdint.h>

/******************************************************************************
 * GENERIC CAN BUS ABSTRACTION
 *****************************************************************************/
enum { GENERIC_CAN_FRAME_MAX_DATA_LEN = 8U };

/* TODO MAKE SAFE READ/WRITE */
struct generic_can_frame {
	uint32_t id;
	int8_t	 len;
	uint8_t	 data[GENERIC_CAN_FRAME_MAX_DATA_LEN];
};

void generic_can_frame_init(struct generic_can_frame *self) { self->len = -1; }

/*
void generic_can_frame_print(struct generic_can_frame *frame)
{
	int iter;

	(void)printf("ID: %08X, LEN: %i, [", (int)frame->id, frame->len);
	for (iter = 0; iter < frame->len; iter++) {
		(void)printf("0x%02X", frame->data[iter]);

		if (iter < (frame->len - 1)) {
			(void)printf(" ");
		}
	}

	(void)printf("]\n");

	(void)fflush(0);
}
*/

#endif /* GENERIC_CAN_FRAME_GUARD */
