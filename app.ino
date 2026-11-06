#include "ccsbox.h"
#include "src/filesystem.h"
#include "src/generic_can_frame.h"
#include "src/hal.h"
#include "src/web_interface.h"

struct ccsbox ccsbox;

void background_tasks(void *pv_parameters)
{
	static struct delta_time dt;

	delta_time_init(&dt);
	delta_time_update_ms(&dt, millis());

	hal_led_init();
	ccsbox_web_init_ap("ccsBOX");

	while (1) {
		uint32_t dt_ms = delta_time_update_ms(&dt, millis());

		hal_led_update(dt_ms);
		ccsbox_fs_update(&ccsbox);
		ccsbox_web_update(&ccsbox, dt_ms);
		vTaskDelay(1);
	}
}

void setup()
{
	Serial.begin(921600);
	delay(5000);

	ccsbox_init(&ccsbox);
	ccsbox_fs_init(&ccsbox);

	hal_init_twai(&twai_bus_0);
	hal_init_twai(&twai_bus_1);

	xTaskCreate(background_tasks, "background_tasks", 10000, NULL, 1,
		    NULL);
}

void loop()
{
	static struct delta_time dt;
	uint32_t		 dt_ms = delta_time_update_ms(&dt, millis());

	/* General purpose, used by both reception and transmission */
	struct generic_can_frame frame;

	/* Update TWAI driver state */
	hal_twai_update(dt_ms);

	/* First bus (dual can) */
	if (hal_twai_recv(&twai_bus_0, &frame)) {
		ccsbox_put_input_frame(&ccsbox, &frame);

		/* Forward frame to the second bus. */
		if (!ccsbox_frame_is_filtered(&ccsbox, &frame)) {
			hal_twai_send(&twai_bus_1, &frame);
		}
	}

	/* Second bus (dual can) */
	if (hal_twai_recv(&twai_bus_1, &frame)) {
		ccsbox_put_input_frame(&ccsbox, &frame);

		/* Forward frame to the second bus. */
		if (!ccsbox_frame_is_filtered(&ccsbox, &frame)) {
			hal_twai_send(&twai_bus_0, &frame);
		}
	}

	/* Send to both buses. TODO judge where to send data. */
	if (ccsbox_get_output_frame(&ccsbox, &frame)) {
		hal_twai_send(&twai_bus_0, &frame);
		hal_twai_send(&twai_bus_1, &frame);
	}

	ccsbox_box_update(&ccsbox, dt_ms);
}
