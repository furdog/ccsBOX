/** CAN Interface between CcsBOX and FOCCCI hardware based on ccs32clara
 * firmware. Shortly abbreviated as "fci"
 *
 * What does it do?
 * - Receives data from the upper software layer and forwards it to foccci via
 * CAN interface
 * - Receives data from foccci via CAN interface and exposes it to upper
 * software layer
 *
 * How?
 * - Preconfigured CAN mapping on ccs32clara
 * - Higher software layer controls this module and manages CAN frames
 *
 * What's the foccci configuration (CAN mapping)?
 * - Message ID: 0x778 (Foccci transmits to ccsBOX)
 * can t PortState 1912 63 -8 1.0 0
 *
 * - Message ID: 0x777 (Foccci receives from ccsBOX)
 * can r BatteryVoltage  1911 63 -16 0.5 0
 * can r ChargeCurrent   1911 47 -16 0.5 0
 * can r soc             1911 31 -8  1.0 0
 * can r enable          1911 23 -8  1.0 0
 * can r TargetVoltage   1911 15 -16 0.5 0 */
#ifndef FOCCI_CAN_INTERFACE_H
#define FOCCI_CAN_INTERFACE_H
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "generic_can_frame.h"

#define FCI_CCSBOX_ID 0x777U
#define FCI_FOCCI_ID 0x778U
#define FCI_FRAME_LEN 8U
#define FCI_SEND_INTERVAL_MS 100U

/** Flags from battery. */
enum fci_batt_flags {
	FCI_BATT_FLAG_CHARGE_ALLOWED = 1U /**< Does battery allow charging? */
};

/** Foccci CCS charger state. */
enum fci_chgr_state {
	FCI_CHGR_STATE_IDLE	   = 0, /** Present in CCS DC */
	FCI_CHGR_STATE_PLUGGEDIN   = 1,
	FCI_CHGR_STATE_READY	   = 2,
	FCI_CHGR_STATE_CHARGING_AC = 3,
	FCI_CHGR_STATE_CHARGING_DC = 4, /** Present in CCS DC */
	FCI_CHGR_STATE_STOPPING	   = 5,
	FCI_CHGR_STATE_UNLOCK	   = 6,
	FCI_CHGR_STATE_PORTERROR   = 7
};

struct fci {
	uint8_t chgr_state;

	float	batt_volt_V;
	float	batt_currq_A;
	float	batt_soc_pct;
	uint8_t batt_flags;
	float	batt_target_voltage_V;

	bool	 send_allowed;
	uint32_t send_interval_r1ms;
};

/* Prototypes */
void fci_init(struct fci *self);
bool fci_put_input_frame(struct fci *self, struct generic_can_frame *frame);
bool fci_get_output_frame(struct fci *self, struct generic_can_frame *frame);
void fci_step(struct fci *self, uint32_t delta_time_r1ms);

#ifndef FOCCI_CAN_INTERFACE_NO_IMPL
void fci_init(struct fci *self)
{
	(void)memset(self, 0, sizeof(struct fci)); /* NOLINT */
}

bool fci_put_input_frame(struct fci *self, struct generic_can_frame *frame)
{
	bool success = true; /* TODO */

	if ((frame->id == (uint32_t)FCI_FOCCI_ID) &&
	    (frame->len == (int8_t)FCI_FRAME_LEN)) {
		self->chgr_state = frame->data[7U]; /* NOLINT */
	}

	return success;
}

bool fci_get_output_frame(struct fci *self, struct generic_can_frame *frame)
{
	bool success = false;

	/* NOLINTBEGIN(readability-magic-numbers) */
	if (self->send_allowed) {
		const int16_t batt_volt_raw =
		    (int16_t)(self->batt_volt_V * 2.0F);
		const int16_t batt_cur_raw =
		    (int16_t)(self->batt_currq_A * 2.0F);
		const int16_t batt_soc_raw   = (int16_t)self->batt_soc_pct;
		const int16_t batt_flags_raw = (int16_t)self->batt_flags;
		const int16_t batt_target_voltage_V =
		    (int16_t)(self->batt_target_voltage_V * 2.0F);

		self->send_allowed = false;

		/* I use uint32_t to prevent promotion to signed integer */
		frame->id	= FCI_CCSBOX_ID;
		frame->len	= FCI_FRAME_LEN;
		frame->data[7U] = ((uint32_t)batt_volt_raw >> 0U) & 0xFFU;
		frame->data[6U] = ((uint32_t)batt_volt_raw >> 8U) & 0xFFU;
		frame->data[5U] = ((uint32_t)batt_cur_raw >> 0U) & 0xFFU;
		frame->data[4U] = ((uint32_t)batt_cur_raw >> 8U) & 0xFFU;
		frame->data[3U] = ((uint32_t)batt_soc_raw >> 0U) & 0xFFU;
		frame->data[2U] = ((uint32_t)batt_flags_raw >> 0U) & 0xFFU;
		frame->data[1U] =
		    ((uint32_t)batt_target_voltage_V >> 0U) & 0xFFU;
		frame->data[0U] =
		    ((uint32_t)batt_target_voltage_V >> 8U) & 0xFFU;

		success = true;
	}
	/* NOLINTEND(readability-magic-numbers) */

	return success;
}

void fci_step(struct fci *self, const uint32_t delta_time_r1ms)
{
	self->send_interval_r1ms += delta_time_r1ms;

	/** After FCI_SEND_INTERVAL_MS allow frame send */
	if (self->send_interval_r1ms >= FCI_SEND_INTERVAL_MS) {
		self->send_interval_r1ms = 0U;
		self->send_allowed	 = true;
	}
}

#endif /* FOCCI_CAN_INTERFACE_NO_IMPL */
#endif /* FOCCI_CAN_INTERFACE_H */
