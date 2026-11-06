/**
 * @file kangoo_bms_mon.h
 * @brief Automata that monitors BMS state.
 * 	  MAGIC WARNING: This piece of software is a reverse engineering!
 * 	  USE IT AT YOUR OWN RISK!
 */
#ifndef KANGOO_BMS_MON_GUARD
#define KANGOO_BMS_MON_GUARD

#include <stdbool.h>
#include <stdint.h>

#include "generic_can_frame.h"

/******************************************************************************
 * CLASS
 *****************************************************************************/
enum kangoo_bms_mon_state { KANGOO_BMS_STATE_SLEEP, KANGOO_BMS_STATE_AWAKE };

enum kangoo_bms_mon_event {
	KANGOO_BMS_MON_EVENT_NONE,
	KANGOO_BMS_MON_EVENT_SLEEP,
	KANGOO_BMS_MON_EVENT_AWAKE
};

struct kangoo_bms_mon {
	enum kangoo_bms_mon_state _state;

	bool _data_received;

	float _current_A;
	float _voltage_V;
	float _soc_PCT;
	float _max_input_kwt;
	float _temp_C;

	float _min_cell_V;
	float _max_cell_V;

	uint32_t _timer_ms;
};

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void kangoo_bms_mon_init(struct kangoo_bms_mon *self)
{
	self->_state = KANGOO_BMS_STATE_SLEEP;

	self->_data_received = false;

	self->_current_A     = 0.0;
	self->_voltage_V     = 0.0;
	self->_soc_PCT	     = 0.0;
	self->_max_input_kwt = 0.0;
	self->_temp_C	     = 0.0;

	self->_timer_ms = 0;
}

void kangoo_bms_mon_put_input_frame(struct kangoo_bms_mon    *self,
				    struct generic_can_frame *frame)
{
	switch (frame->id) {
	case 0x155:
		self->_current_A = ((uint16_t)frame->data[1] << 8) |
				   ((uint16_t)frame->data[2] << 0);

		self->_current_A =
		    (((uint16_t)self->_current_A & 0x0FFFU) / 4.0) - 500;

		self->_soc_PCT = ((uint16_t)frame->data[4] << 8) |
				 ((uint16_t)frame->data[5] << 0);

		self->_soc_PCT /= 400.0;

		self->_voltage_V = ((uint16_t)frame->data[6] << 8) |
				   ((uint16_t)frame->data[7] << 0);

		self->_voltage_V /= 2.0;

		self->_data_received = true;
		break;

	case 0x424:
		/* Actually that's a recuperation max input kwt */
		self->_max_input_kwt = (frame->data[2] / 2U);

		self->_temp_C = (frame->data[4] - 40);

		self->_data_received = true;
		break;

	case 0x425:
		self->_min_cell_V =
		    ((((uint16_t)frame->data[6] & 0x0001) << 8 |
		      frame->data[7]) +
		     100) /
		    100.0;
		self->_max_cell_V =
		    (((((uint16_t)frame->data[4] & 0x0003) << 8 |
		       frame->data[5]) >>
		      1) +
		     100) /
		    100.0;

		self->_data_received = true;
		break;

	default:
		break;
	}
}

bool kangoo_bms_is_sleep(struct kangoo_bms_mon *self)
{
	return self->_state == KANGOO_BMS_STATE_SLEEP;
}

bool kangoo_bms_is_awake(struct kangoo_bms_mon *self)
{
	return self->_state == KANGOO_BMS_STATE_AWAKE;
}

float kangoo_bms_get_soc_PCT(struct kangoo_bms_mon *self)
{
	return self->_soc_PCT;
}

float kangoo_bms_get_max_input_kwt(struct kangoo_bms_mon *self)
{
	return self->_max_input_kwt;
}

float kangoo_bms_get_voltage_V(struct kangoo_bms_mon *self)
{
	return self->_voltage_V;
}

float kangoo_bms_get_current_A(struct kangoo_bms_mon *self)
{
	return self->_current_A;
}

float kangoo_bms_get_power_W(struct kangoo_bms_mon *self)
{
	return self->_voltage_V * self->_current_A;
}

float kangoo_bms_get_temp_C(struct kangoo_bms_mon *self)
{
	return self->_temp_C;
}

float kangoo_bms_get_min_cell_V(struct kangoo_bms_mon *self)
{
	return self->_min_cell_V;
}

float kangoo_bms_get_max_cell_V(struct kangoo_bms_mon *self)
{
	return self->_max_cell_V;
}

enum kangoo_bms_mon_event kangoo_bms_mon_update(struct kangoo_bms_mon *self,
						const uint32_t delta_time_ms)
{
	enum kangoo_bms_mon_event event = KANGOO_BMS_MON_EVENT_NONE;

	if (self->_data_received) {
		self->_timer_ms = 0;
	} else {
		self->_timer_ms += delta_time_ms;
	}

	switch (self->_state) {
	case KANGOO_BMS_STATE_SLEEP:
		/* Enter awake mode,
		 * exit switch if we got any can message here */
		if (self->_data_received) {
			self->_state = KANGOO_BMS_STATE_AWAKE;
			event	     = KANGOO_BMS_MON_EVENT_AWAKE;
		}

		break;

	case KANGOO_BMS_STATE_AWAKE:
		/* Enter sleep mode
		 * if messages missing during 10000ms period */
		if (self->_timer_ms >= 10000U) {
			self->_state = KANGOO_BMS_STATE_SLEEP;
			event	     = KANGOO_BMS_MON_EVENT_SLEEP;

			/** TODO clear all vars to 0 */
		}

		break;

	default:
		break;
	}

	self->_data_received = false;
	return event;
}

#endif /* KANGOO_BMS_MON_GUARD */
