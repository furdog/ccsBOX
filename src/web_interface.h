#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <Ticker.h>
#include <Update.h>
#include <WiFi.h>
#include <stdint.h>

/** Include generated index HTML */
#include "index.h"
#include "filesystem.h"

extern struct ccsbox ccsbox;

enum netmsg {
	NETMSG_MAX_KWT	      = 0U,
	NETMSG_EMULATE_CHARGE = 1U,
	NETMSG_EMERGENCY_STOP = 2U,
	NETMSG_WIFI_STOP      = 3U,
	NETMSG_DUMP_STATE     = 4U,
	NETMSG_CPU_RESET      = 5U,
	NETMSG_LIMIT_TEMP_C   = 6U,

	NETMSG_COUNT = 7U
};

static AsyncWebSocket ws_server("/ws");

/* CPU RESTART */
Ticker web_cpu_reset_ticker;

bool web_cpu_reset_scheduled = false;
void web_cpu_reset_delayed() { ESP.restart(); }
void web_cpu_reset()
{
	web_cpu_reset_scheduled = true;
	// TODO stop ccsbox
	web_cpu_reset_ticker.once(5.0, web_cpu_reset_delayed);
}
/* CPU RESTART */

void web_send_json(uint8_t client)
{
	String serialized;

	JsonDocument doc;
	JsonArray    array = doc.to<JsonArray>();
	JsonArray    narr;
	JsonObject   nobj;

	narr = array.add<JsonArray>();
	narr.add(NETMSG_MAX_KWT);
	narr.add(ccsbox.settings.max_power_kwt);

	narr = array.add<JsonArray>();
	narr.add(NETMSG_EMULATE_CHARGE);
	narr.add(ccsbox.settings.emulate_charge);

	narr = array.add<JsonArray>();
	narr.add(NETMSG_LIMIT_TEMP_C);
	narr.add(ccsbox.settings.limit_temp_C);

	/*printf("web_send_json_all, memory usage: %i bytes\n",
	       doc.memoryUsage());*/

	serializeJson(array, serialized);
	ws_server.text(client, serialized.c_str());
}

void web_recv_json(const char *payload)
{
	JsonDocument doc;
	JsonArray    array;
	uint8_t	     msg;
	JsonVariant  value;

	deserializeJson(doc, payload);

	array = doc.as<JsonArray>();
	msg   = array[0].as<int>();
	value = array[1];

	switch (msg) {
	case NETMSG_MAX_KWT:
		ccsbox.settings.max_power_kwt = value.as<float>();
		ccsbox_fs_commit_settings(&ccsbox);
		break;

	case NETMSG_EMULATE_CHARGE:
		ccsbox.settings.emulate_charge = value.as<int>();
		ccsbox_fs_commit_settings(&ccsbox);
		break;

	case NETMSG_EMERGENCY_STOP:
		ccsbox.emergency_stop = true;
		break;

	case NETMSG_WIFI_STOP:
		WiFi.disconnect(true);
		WiFi.mode(WIFI_OFF);
		break;

	case NETMSG_CPU_RESET:
		web_cpu_reset();
		return; /* Prevents default send action, down there. */

	case NETMSG_LIMIT_TEMP_C:
		ccsbox.settings.limit_temp_C = value.as<float>();
		ccsbox_fs_commit_settings(&ccsbox);
		break;

	default:
		break;
	}

	ws_server.textAll(payload);
}

/************************************************
 * WS
 ***********************************************/
void ws_receive_callback(unsigned char *text);

void ws_server_handler(AsyncWebSocket *server, AsyncWebSocketClient *client,
		       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
	(void)len;

	if (type == WS_EVT_CONNECT) {
		Serial.println("[WEBSOCKET] client connected");
		client->setCloseClientOnQueueFull(false);
		client->ping();
		web_send_json(client->id());
	} else if (type == WS_EVT_DISCONNECT) {
		Serial.println("[WEBSOCKET] client disconnected");
	} else if (type == WS_EVT_ERROR) {
		Serial.println("[WEBSOCKET] error");
	} else if (type == WS_EVT_PONG) {
		Serial.println("[WEBSOCKET] pong");
	} else if (type == WS_EVT_DATA) {
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		String	      msg  = "";
		if (info->final && info->index == 0 && info->len == len) {
			if (info->opcode == WS_TEXT) {
				data[len] = 0;
				web_recv_json((const char *)data);
				Serial.printf("[WEBSOCKET] text: %s\n",
					      (unsigned char *)data);
			}
		}
	}
}

/************************************************
 * UPDATER
 ***********************************************/
Ticker web_updater_ticker;
void   web_updater_reboot() { ESP.restart(); }
bool   web_update_success = false;

static void web_updater_handler(AsyncWebServerRequest *request,
				const String &filename, size_t index,
				uint8_t *data, size_t len, bool final)
{
	if (index == 0) {
		/* size_t filesize = request->header("X-FileSize").toInt();
		Serial.printf("Update: '%s' size %u\n", filename.c_str(),
		filesize); if (!Update.begin(filesize, U_FLASH)) { // pass the
		size provided */
		uint32_t free_space =
		    (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

		if (Update.begin(free_space)) {
			Update.printError(Serial);
		}
	}

	if (len) {
		Serial.printf("writing %d\n", len);
		if (Update.write(data, len) == len) {
		} else {
			Serial.printf("write failed to write %d\n", len);
		}
	}

	if (final) {
		if (Update.end(true)) {
			web_update_success = true;
			Serial.println("Update end success");
			web_updater_ticker.once(1.0, web_updater_reboot);
			AsyncWebServerResponse *response =
			    request->beginResponse(
				200, "text/plain",
				"Update successful. Rebooting...");
			response->addHeader("Access-Control-Allow-Origin",
					    "*");
			request->send(response);
		} else {
			web_update_success = false;
			Serial.println("Update end fail");
			Update.printError(Serial);
			AsyncWebServerResponse *response =
			    request->beginResponse(500, "text/plain",
						   "Update failed.");
			response->addHeader("Access-Control-Allow-Origin",
					    "*");
			request->send(response);
		}
	}
}

/************************************************
 * SERVER
 ***********************************************/
static AsyncWebServer web_server(80);
DNSServer	      dns_server;

static void web_handle_cors_preflight(AsyncWebServerRequest *request)
{
	AsyncWebServerResponse *response =
	    request->beginResponse(204, "text/plain");

	response->addHeader("Access-Control-Allow-Origin", "*");
	/* response->addHeader("Access-Control-Allow-Headers", "Content-Type,
	 * X-FileSize"); */

	request->send(response);
}

void web_send_index(AsyncWebServerRequest *request)
{
	AsyncWebServerResponse *response = request->beginResponse(
	    200, "text/html", index_html_gz, index_html_gz_len);
	response->addHeader("Content-Encoding", "gzip");
	request->send(response);
}

void web_send_ok(AsyncWebServerRequest *request) { request->send(200); }

void ccsbox_web_init_ap(const char *name)
{
	WiFi.mode(WIFI_AP);

	WiFi.softAPConfig(IPAddress(7, 7, 7, 7), IPAddress(7, 7, 7, 7),
			  IPAddress(255, 255, 255, 0));
	WiFi.softAP(name);

	web_server.onNotFound(web_send_index);
	web_server.on(
	    "/update", HTTP_POST, [](AsyncWebServerRequest *request) {},
	    web_updater_handler);
	web_server.on("/update", HTTP_OPTIONS, web_handle_cors_preflight);

	dns_server.setErrorReplyCode(DNSReplyCode::NoError);
	dns_server.start(53, "*", WiFi.softAPIP());

	ws_server.onEvent(ws_server_handler);
	web_server.addHandler(&ws_server);

	web_server.begin();
}

/************************************************
 * MAIN
 ***********************************************/
void ccsbox_web_update(struct ccsbox *self, const uint32_t delta_time_r1ms)
{
	dns_server.processNextRequest();
}

void ccsbox_web_update(struct ccsbox *self, const uint32_t delta_time_r1ms);
