#include "generic_can_frame.h"
#include <stdint.h>

#if HARDWARE == esp32c6
#define TWAI_BUS_0_TX GPIO_NUM_18
#define TWAI_BUS_0_RX GPIO_NUM_19

#define TWAI_BUS_1_TX GPIO_NUM_14
#define TWAI_BUS_1_RX GPIO_NUM_15
#endif

/******************************************************************************
 * DELTA TIME
 *****************************************************************************/
struct delta_time {
	uint32_t _timestamp_prev;
};

void delta_time_init(struct delta_time *self) { self->_timestamp_prev = 0; }

uint32_t delta_time_update_ms(struct delta_time *self, uint32_t timestamp)
{
	uint32_t delta_time_ms = 0;

	delta_time_ms	      = timestamp - self->_timestamp_prev;
	self->_timestamp_prev = timestamp;

	return delta_time_ms;
}

/******************************************************************************
 * LED
 *****************************************************************************/
static bool led_state	= false;
static bool led2_state	= false;
static bool rapid_blink = false;

#include <Adafruit_NeoPixel.h>

#define PIN_WS2812B 8
#define NUM_PIXELS 1

Adafruit_NeoPixel ws2812b(NUM_PIXELS, PIN_WS2812B, NEO_GRB + NEO_KHZ800);

void hal_led_init()
{
	ws2812b.begin(); // initialize WS2812B strip object (REQUIRED)
}

extern bool web_cpu_reset_scheduled;
extern bool web_update_success;

void hal_led_update(uint32_t delta_time_ms)
{
	if (rapid_blink || web_update_success || web_cpu_reset_scheduled) {
		static uint8_t rapid_blink_timer = 0;

		rapid_blink_timer += delta_time_ms;

		if (rapid_blink_timer >= 50) {
			rapid_blink_timer = 0;
			led_state	  = !led_state;
			led2_state	  = !led2_state;

			ws2812b.setPixelColor(
			    0, ws2812b.Color(led_state ? 255 : 0, 0,
					     led2_state ? 255 : 0));
			ws2812b.show();
		}
	}
}

/******************************************************************************
 * KANGOO_CAN_FILTER_INIT_ESP32_TWAI
 *****************************************************************************/
/* ESP32 specific CAN driver */
#include "driver/gpio.h"
#include "driver/twai.h"

twai_handle_t twai_bus_0;
twai_handle_t twai_bus_1;
bool	      twai_bus_0_recovery = false;
bool	      twai_bus_1_recovery = false;

void hal_init_twai(twai_handle_t *bus)
{
	esp_err_t code;

	assert(bus == &twai_bus_0 || bus == &twai_bus_1);

	// Config
	// twai_handle_t *bus    = &twai_bus_0;
	gpio_num_t bus_tx =
	    (gpio_num_t)((bus == &twai_bus_0) ? TWAI_BUS_0_TX : TWAI_BUS_1_TX);
	gpio_num_t bus_rx =
	    (gpio_num_t)((bus == &twai_bus_0) ? TWAI_BUS_0_RX : TWAI_BUS_1_RX);
	uint8_t bus_id = (bus == &twai_bus_0) ? 0 : 1;

	twai_general_config_t g_config =
	    TWAI_GENERAL_CONFIG_DEFAULT(bus_tx, bus_rx, TWAI_MODE_NORMAL);
	twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
	twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

	// Install driver for TWAI bus 0
	g_config.controller_id = bus_id;
	code = twai_driver_install_v2(&g_config, &t_config, &f_config, bus);
	if (code == ESP_OK) {
		printf("TWAI driver installed\n");
	} else {
		printf("Failed to install driver (%s)\n",
		       esp_err_to_name(code));
		return;
	}
	// Start TWAI driver
	code = twai_start_v2(*bus);
	if (code == ESP_OK) {
		printf("TWAI driver started\n");
	} else {
		printf("Failed to start driver (%s)\n", esp_err_to_name(code));
		return;
	}

	twai_reconfigure_alerts_v2(*bus, TWAI_ALERT_BUS_OFF, NULL);
}

/* Call only in bus off state */
void hal_kill_esp32_twai(twai_handle_t *bus)
{
	assert(bus == &twai_bus_0 || bus == &twai_bus_1);

	twai_driver_uninstall_v2(*bus);
}

/* TODO proper bound checking */
void hal_twai_send(twai_handle_t *bus, struct generic_can_frame *frame)
{
	assert(bus == &twai_bus_0 || bus == &twai_bus_1);

	bool *recovery =
	    (bus == &twai_bus_0) ? &twai_bus_0_recovery : &twai_bus_1_recovery;

	if (frame->len > -1 || *recovery) {
		int8_t i;

		// Configure message to transmit
		twai_message_t msg;

		// Message type and format settings
		msg.extd = 0; // Standard vs extended format
		msg.rtr	 = 0; // Data vs RTR frame
		msg.ss	 = 0; // Whether the message is single shot (i.e., does
			      // not repeat on error)
		msg.self = 0; // Whether the message is a self reception
			      // request (loopback)
		msg.dlc_non_comp = 0; // DLC is less than 8

		// Message ID and payload
		msg.identifier	     = frame->id;
		msg.data_length_code = frame->len;

		for (i = 0; i < frame->len; i++) {
			msg.data[i] = frame->data[i];
		}

		// Queue message for transmission
		twai_transmit_v2(*bus, &msg, 0);
	}
}

/* TODO proper bound checking */
bool hal_twai_recv(twai_handle_t *bus, struct generic_can_frame *frame)
{
	bool success = false;

	assert(bus == &twai_bus_0 || bus == &twai_bus_1);

	twai_message_t msg;

	if (twai_receive_v2(*bus, &msg, 0) == ESP_OK &&
	    msg.data_length_code <= 8) {
		int8_t i = 0;

		frame->id = msg.identifier;

		if (!(msg.rtr)) {
			for (i = 0; i < msg.data_length_code; i++) {
				frame->data[i] = msg.data[i];
			}
		}

		frame->len = i;
		success = true;
	} else {
		frame->len = -1;
	}

	return success;
}

void hal_twai_update(uint32_t delta_time_ms)
{
	uint32_t alerts;

	alerts = 0;
	twai_read_alerts_v2(twai_bus_0, &alerts, 0);

	if (alerts & TWAI_ALERT_BUS_OFF) {
		twai_bus_0_recovery = true;
		printf("TWAI_ALERT_BUS_OFF\n");

		twai_reconfigure_alerts_v2(twai_bus_0, 0, NULL);
		hal_kill_esp32_twai(&twai_bus_0);
		hal_init_twai(&twai_bus_0);
		twai_bus_0_recovery = false;
	}

	alerts = 0;
	twai_read_alerts_v2(twai_bus_1, &alerts, 0);

	if (alerts & TWAI_ALERT_BUS_OFF) {
		twai_bus_1_recovery = true;
		printf("TWAI_ALERT_BUS_OFF\n");

		twai_reconfigure_alerts_v2(twai_bus_1, 0, NULL);
		hal_kill_esp32_twai(&twai_bus_1);
		hal_init_twai(&twai_bus_1);
		twai_bus_1_recovery = false;
	}
}
