#include <stdio.h>
#include <string.h>

#include "src/foccci_can_interface.h"
#include "src/generic_can_frame.h"
#include "src/kangoo_bms_mon.h"
#include "src/kangoo_charge_mode.h"

struct ccsbox_settings {
	float limit_temp_C;
	float max_power_kwt;

	bool emulate_charge;
};

struct ccsbox {
	struct ccsbox_settings settings;

	struct kangoo_bms_mon	  bmsmon;
	struct kangoo_charge_mode chgmode;
	struct fci		  foccci;

	/** If set to true, will filter onboard charger messages */
	bool filter_bcb_messages;

	float limit_current_by_temp_A;

	bool emergency_stop;
	bool charging_ongoing;
};

void ccsbox_init(struct ccsbox *self)
{
	memset(self, 0U, sizeof(struct ccsbox));

	kangoo_bms_mon_init(&self->bmsmon);
	kangoo_charge_mode_init(&self->chgmode);
	fci_init(&self->foccci);

	/* TEST PURPOSES ONLY */
	self->filter_bcb_messages = true;
}

bool ccsbox_frame_is_filtered(struct ccsbox	       *self,
			      struct generic_can_frame *frame)
{
	bool filtered = false;

	if (!self->filter_bcb_messages) {
		filtered = false;
	} else if (((frame->len == 4) && (frame->id == 0x1C7)) ||
		   ((frame->len == 7) && (frame->id == 0x428)) ||
		   ((frame->len == 8) && (frame->id == 0x429)) ||
		   ((frame->len == 8) && (frame->id == 0x4F7))) {
		filtered = true;
	}

	return filtered;
}

/** Limits current based on BMS temperature report
 * Algorithm: */
void _ccsbox_limit_current_by_temp(struct ccsbox *self, float *limit_current)
{
	if (!limit_current)
		return;

	float bms_temp_C = kangoo_bms_get_temp_C(&self->bmsmon);
	float difference = bms_temp_C - self->settings.limit_temp_C;

	/* If bms temp is bigger than the temp limit:
	 * - Drop current by 20% per each degree of celsius. */
	if (difference > 0.0f) {
		/* Dereference pointer to modify the actual value */
		*limit_current *= (1.0f - (difference / 5.0f));
	}

	/* Current can't be less than zero */
	if (*limit_current < 0.0f) {
		*limit_current = 0.0f;
	}
}

void ccsbox_put_input_frame(struct ccsbox	     *self,
			    struct generic_can_frame *frame)
{
	kangoo_bms_mon_put_input_frame(&self->bmsmon, frame);
	fci_put_input_frame(&self->foccci, frame);
}

bool ccsbox_get_output_frame(struct ccsbox	      *self,
			     struct generic_can_frame *frame)
{
	bool success = false;

	struct generic_can_frame *chgmfr;
	chgmfr = kangoo_charge_mode_get_can_frame(&self->chgmode);

	/* Pull frames from charge manager & foccci can interface */
	if (chgmfr) {
		*frame	= *chgmfr;
		success = true;
	} else if (fci_get_output_frame(&self->foccci, frame)) {
		success = true;
	} else {
	}

	return success;
}

void ccsbox_box_update(struct ccsbox *self, const uint32_t delta_time_ms)
{
	float limit_current = 0;

	enum kangoo_bms_mon_event bms_event =
	    kangoo_bms_mon_update(&self->bmsmon, delta_time_ms);
	enum kangoo_charge_mode_event charge_mode_event =
	    kangoo_charge_mode_update(&self->chgmode, delta_time_ms);
	fci_step(&self->foccci, delta_time_ms);

	/* LIMITS */
	float max_input_kwt = kangoo_bms_get_max_input_kwt(&self->bmsmon);
	float voltage	    = kangoo_bms_get_voltage_V(&self->bmsmon);

	/* 1. Cap power to maximum allowed limit */
	if (max_input_kwt > self->settings.max_power_kwt) {
		max_input_kwt = self->settings.max_power_kwt;
	}

	/* 2. Calculate target current safely */
	if (voltage > 0.0f) {
		limit_current = (1000.0f * max_input_kwt) / voltage;
	} else {
		limit_current = 0.0f;
	}

	/* 3. Cap current to maximum hardware/configuration limit */
	if (limit_current > 120.0f) {
		limit_current = 120.0f;
	}

	_ccsbox_limit_current_by_temp(self, &limit_current);

	self->limit_current_by_temp_A = limit_current;

	if ((int)bms_event > 0) {
		printf("BMS_MON_EVENT:     %i\n", (int)bms_event);
	}

	if ((int)charge_mode_event > 0) {
		printf("CHARGE_MODE_EVENT: %i\n", (int)charge_mode_event);
	}

	/* EMERGENCY SITUATIONS */
	/* If BMS goes into sleep or emergency stop */
	if ((bms_event == KANGOO_BMS_MON_EVENT_SLEEP) ||
	    (self->emergency_stop)) {
		self->foccci.batt_flags	  &= ~FCI_BATT_FLAG_CHARGE_ALLOWED;
		self->foccci.batt_currq_A  = 0U;
	}

	/* If charge mode is running but charger is stopped */
	if (kangoo_charge_mode_is_running(&self->chgmode) &&
	    (self->foccci.chgr_state == FCI_CHGR_STATE_IDLE)) {
		kangoo_charge_mode_stop(&self->chgmode);
	}

	/* BEFORE CHARGING */
	/* If entered charging state and emulating charge mode */
	if (!self->charging_ongoing && kangoo_bms_is_awake(&self->bmsmon) &&
	    (self->foccci.chgr_state == FCI_CHGR_STATE_CHARGING_DC) &&
	    self->settings.emulate_charge) {
		if (kangoo_charge_mode_is_stopped(&self->chgmode)) {
			kangoo_charge_mode_start(&self->chgmode);
			self->foccci.batt_flags |=
			    FCI_BATT_FLAG_CHARGE_ALLOWED;
		}

		self->filter_bcb_messages = true;
		self->charging_ongoing	  = true;
	}

	/* If entered charging state and NOT emulating charge mode */
	if (!self->charging_ongoing && kangoo_bms_is_awake(&self->bmsmon) &&
	    (self->foccci.chgr_state == FCI_CHGR_STATE_CHARGING_DC) &&
	    !self->settings.emulate_charge) {
		self->foccci.batt_flags |= FCI_BATT_FLAG_CHARGE_ALLOWED;
		self->charging_ongoing	 = true;
	}

	/* Allow charging if charge mode is fully ready */
	/* if (charge_mode_event == KANGOO_CHARGE_MODE_EVENT_READY) { */
	/* This is only triggered after vehicle is fully ready to
	 *  charge. Doesn't work though, because it sets this flag
	    long after EVSE judges this as charge stop signal.
	   So we set this flag right after kangoo_charge_mode_start(...)
	   Not sure if it works either... */

	/* self->foccci.batt_flags |= FCI_BATT_FLAG_CHARGE_ALLOWED; */
	/*}*/

	/* WHILE CHARGING */
	/* While BMS is awake */
	if (kangoo_bms_is_awake(&self->bmsmon)) {
		self->foccci.batt_volt_V =
		    kangoo_bms_get_voltage_V(&self->bmsmon);
		self->foccci.batt_soc_pct =
		    kangoo_bms_get_soc_PCT(&self->bmsmon);

		if (!self->emergency_stop) {
			self->foccci.batt_currq_A =
			    self->limit_current_by_temp_A;
		} else {
			self->foccci.batt_currq_A = 0.0f;
		}
	}

	/* AFTER CHARGING */
	/* Stop filtering BCB messages if charge mode stopped */
	if (charge_mode_event == KANGOO_CHARGE_MODE_EVENT_STOP) {
		self->foccci.batt_flags	  &= ~FCI_BATT_FLAG_CHARGE_ALLOWED;
		self->filter_bcb_messages  = false;
	}

	/* While IDLE stop charge mode (if enabled) and set charge_ongoing to
	 * false */
	if (self->foccci.chgr_state == FCI_CHGR_STATE_IDLE) {
		if (kangoo_charge_mode_is_stopped(&self->chgmode)) {
			self->charging_ongoing = false;
		}

		kangoo_charge_mode_stop(&self->chgmode);

		self->emergency_stop = false;
	}
}
