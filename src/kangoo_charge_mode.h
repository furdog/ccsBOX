/**
 * @file kangoo_charge_mode.h
 * @brief Automata that emulates BCB messages, specifically for charging.
 * 	  Used to set vehicle into charging mode
 * 		(similarly if charging plug is inserted).
 *	  Assumes real BCB to be disconnected from CAN communication line
 * 		DURING operation of an emulator and restored AFTER.
 * 	  MAGIC WARNING: This piece of software is a reverse engineering!
 *
 * 	  USE IT AT YOUR OWN RISK!
 */
#ifndef KANGOO_CHARGE_MODE_GUARD
#define KANGOO_CHARGE_MODE_GUARD

#include <stdbool.h>
#include <stdint.h>

#include "generic_can_frame.h"

/******************************************************************************
 * CLASS
 *****************************************************************************/
enum kangoo_charge_mode_state {
	KANGOO_CHARGE_MODE_STATE_IDLE,
	KANGOO_CHARGE_MODE_STATE_START,
	KANGOO_CHARGE_MODE_STATE_WAIT_RELAY_ON,
	KANGOO_CHARGE_MODE_STATE_WAIT_TURNERS_ON,
	KANGOO_CHARGE_MODE_STATE_WAIT_START,
	KANGOO_CHARGE_MODE_STATE_NORMAL_CYCLE,
	KANGOO_CHARGE_MODE_STATE_WAIT_RELAY_OFF,
	KANGOO_CHARGE_MODE_STATE_WAIT_STOP,
	KANGOO_CHARGE_MODE_STATE_STOP
};

enum kangoo_charge_mode_event {
	KANGOO_CHARGE_MODE_EVENT_NONE,
	KANGOO_CHARGE_MODE_EVENT_START,
	KANGOO_CHARGE_MODE_EVENT_READY,
	KANGOO_CHARGE_MODE_EVENT_STOP
};

struct kangoo_charge_mode {
	enum kangoo_charge_mode_state _state;

	struct generic_can_frame _frames[4];
	int8_t			 _frame_counter;

	uint32_t _timer_10ms;
	uint32_t _timer_100ms;
	uint32_t _timer_ms;
};

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
bool kangoo_charge_mode_is_started(struct kangoo_charge_mode *self)
{
	return (self->_state != KANGOO_CHARGE_MODE_STATE_IDLE);
}

bool kangoo_charge_mode_is_preparing(struct kangoo_charge_mode *self)
{
	return ((self->_state >= KANGOO_CHARGE_MODE_STATE_START) &&
		(self->_state < KANGOO_CHARGE_MODE_STATE_NORMAL_CYCLE));
}

bool kangoo_charge_mode_is_running(struct kangoo_charge_mode *self)
{
	return (self->_state == KANGOO_CHARGE_MODE_STATE_NORMAL_CYCLE);
}

bool kangoo_charge_mode_is_stopping(struct kangoo_charge_mode *self)
{
	return ((self->_state > KANGOO_CHARGE_MODE_STATE_NORMAL_CYCLE) &&
		(self->_state <= KANGOO_CHARGE_MODE_STATE_STOP));
}

bool kangoo_charge_mode_is_stopped(struct kangoo_charge_mode *self)
{
	return (self->_state == KANGOO_CHARGE_MODE_STATE_IDLE);
}

void kangoo_charge_mode_init(struct kangoo_charge_mode *self)
{
	self->_state	     = KANGOO_CHARGE_MODE_STATE_IDLE;
	self->_frame_counter = -1;

	self->_timer_10ms  = 0U;
	self->_timer_100ms = 0U;
	self->_timer_ms	   = 0U;
}

void kangoo_charge_mode_start(struct kangoo_charge_mode *self)
{
	struct generic_can_frame msg_init[4] = {
	    {0x1C7, 4, {0xAA, 0x20, 0x00, 0x00, /* */ 0x00, 0x00, 0x00, 0x00}},
	    {0x428, 7, {0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x80 /* */, 0x00}},
	    {0x429, 8,	     {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x3F, 0x3F, 0xFF}},
	    {0x4F7, 8,	     {0x00, 0x00, 0x28, 0x00, 0x90, 0x00, 0x00, 0x00}}
	};

	self->_state = KANGOO_CHARGE_MODE_STATE_START;

	self->_frames[0] = msg_init[0];
	self->_frames[1] = msg_init[1];
	self->_frames[2] = msg_init[2];
	self->_frames[3] = msg_init[3];

	self->_timer_10ms  = 10U;
	self->_timer_100ms = 100U;
	self->_timer_ms	   = 0U;
}

void kangoo_charge_mode_stop(struct kangoo_charge_mode *self)
{
	/* If not stopping or not stopped */
	if (!(kangoo_charge_mode_is_stopping(self) ||
	      kangoo_charge_mode_is_stopped(self))) {
		self->_state = KANGOO_CHARGE_MODE_STATE_WAIT_RELAY_OFF;

		self->_frames[1].data[0] = 0x00;
		self->_frames[1].data[6] = 0x00;
		self->_frames[3].data[2] = 0x28;
		self->_frames[3].data[4] = 0x90;

		self->_timer_ms = 0U;
	}
}

void kangoo_charge_mode_stop_immediate(struct kangoo_charge_mode *self)
{
	if (!kangoo_charge_mode_is_stopped(self)) {
		self->_state = KANGOO_CHARGE_MODE_STATE_STOP;
	}
}

struct generic_can_frame *
kangoo_charge_mode_get_can_frame(struct kangoo_charge_mode *self)
{
	struct generic_can_frame *frame = NULL;

	if (self->_frame_counter > -1) {
		frame = &self->_frames[self->_frame_counter];
		self->_frame_counter--;
	}

	return frame;
}

enum kangoo_charge_mode_event
kangoo_charge_mode_update(struct kangoo_charge_mode *self,
			  const uint32_t	     delta_time_ms)
{
	enum kangoo_charge_mode_event event = KANGOO_CHARGE_MODE_EVENT_NONE;

	if (!kangoo_charge_mode_is_stopped(self)) {
		self->_timer_ms	   += delta_time_ms;
		self->_timer_10ms  += delta_time_ms;
		self->_timer_100ms += delta_time_ms;
	}

	switch (self->_state) {
	case KANGOO_CHARGE_MODE_STATE_START:
		self->_state = KANGOO_CHARGE_MODE_STATE_WAIT_RELAY_ON;
		event	     = KANGOO_CHARGE_MODE_EVENT_START;
		break;

	case KANGOO_CHARGE_MODE_STATE_WAIT_RELAY_ON:
		if (self->_timer_ms < 640U) {
			break;
		}

		self->_frames[3].data[2] = 0x2C;
		self->_state = KANGOO_CHARGE_MODE_STATE_WAIT_TURNERS_ON;
		break;

	case KANGOO_CHARGE_MODE_STATE_WAIT_TURNERS_ON:
		if (self->_timer_ms < 1640U) {
			break;
		}

		self->_frames[3].data[4] = 0x28; /* 0x29 - enables cooler. */
		self->_state		 = KANGOO_CHARGE_MODE_STATE_WAIT_START;
		break;

	case KANGOO_CHARGE_MODE_STATE_WAIT_START:
		if (self->_timer_ms < 4000U) {
			break;
		}

		self->_frames[1].data[0] = 0x20;
		self->_frames[1].data[3] = 0x10; /* enables charger counter? */
		self->_state = KANGOO_CHARGE_MODE_STATE_NORMAL_CYCLE;
		event	     = KANGOO_CHARGE_MODE_EVENT_READY;
		break;

	case KANGOO_CHARGE_MODE_STATE_NORMAL_CYCLE:
		/* Normally do nothing here. */
		break;

	case KANGOO_CHARGE_MODE_STATE_WAIT_RELAY_OFF:
		if (self->_timer_ms < 300U) {
			break;
		}

		self->_frames[3].data[4] = 0xB0;
		self->_state		 = KANGOO_CHARGE_MODE_STATE_WAIT_STOP;
		break;

	case KANGOO_CHARGE_MODE_STATE_WAIT_STOP:
		if (self->_timer_ms < 940U) {
			break;
		}

		self->_state = KANGOO_CHARGE_MODE_STATE_STOP;
		break;

	case KANGOO_CHARGE_MODE_STATE_STOP:
		self->_state = KANGOO_CHARGE_MODE_STATE_IDLE;
		event	     = KANGOO_CHARGE_MODE_EVENT_STOP;
		break;

	default:
		event = KANGOO_CHARGE_MODE_EVENT_NONE;
		break;
	}

	/* set 0x1C7 every 10ms */
	if (self->_timer_10ms >= 10U) {
		self->_timer_10ms = 0U;

		/* We should not interfere if other messages are set */
		if (self->_frame_counter < 0) {
			self->_frame_counter = 0;
		}
	}

	/* Set all messages every 100ms. */
	if (self->_timer_100ms >= 100U) {
		self->_timer_100ms   = 0U;
		self->_frame_counter = 3;
	}

	return event;
}

#endif /* KANGOO_CHARGE_MODE_GUARD */
