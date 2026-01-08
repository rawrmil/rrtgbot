#ifndef TGBOT_RW_H
#define TGBOT_RW_H

#include "binary_rw.h"
#include "cJSON/cJSON.h"

#define TGBOT_API_HOST "api.telegram.org"
#define TGBOT_API_URL "https://"TGBOT_API_HOST"/"

#define X_TGB_RESP_TYPE \
	X(TGB_MT_UNKNOWN) \
	X(TGB_MT_SEND_MESSAGE) \
	X(TGB_MT_GET_UPDATES) \
	X(TGB_MT_SET_WEBHOOK) \
	X(TGB_MT_GET_WEBHOOK_INFO) \
	X(TGB_MT_DELETE_WEBHOOK) \
	X(TGB_MT_LENGTH)

#define X(name_) name_,
typedef enum TGB_RespType {
	X_TGB_RESP_TYPE
} TGB_RespType;
#undef X

#define TGB_QUEUE_CAPACITY 256

typedef void (*TGB_HandleUpdate)(cJSON*);

typedef struct TGB_RespQueue {
	TGB_RespType buf[TGB_QUEUE_CAPACITY];
	size_t head, tail;
} TGB_RespQueue;

typedef struct TGB_Bot {
	struct mg_connection* conn;
	bool is_connected;
	TGB_HandleUpdate fn;
	uint64_t last_poll_ms;
	uint64_t update_offset;
	TGB_RespQueue resp;
} TGB_Bot;

extern TGB_Bot tgb;

void TGBotConnect(struct mg_mgr* mgr, TGB_HandleUpdate);
void TGBotPoll();
void TGBotClose();

void TGBotSendText(uint64_t chat_id, char* text);
void TGBotSendTextMD(uint64_t chat_id, char* text);

#endif /* TGBOT_RW_H */

#ifdef TGBOT_IMPLEMENTATION

#define X(name_) #name_,
char* TGB_RESP_TYPE_NAMES[] = { X_TGB_RESP_TYPE };
#undef X

TGB_Bot tgb;

void TGBotAPISendJSON(char* method, char* action, char* buf, size_t len) {
	if (!tgb.is_connected) { MG_ERROR(("tg not connected")); return; } // TODO: add to queue
	char* msg = nob_temp_sprintf(
			"%s /bot"TGBOT_API_TOKEN"/%s HTTP/1.1\r\n"
			"Host: "TGBOT_API_HOST"\r\n"
			"Connection: keep-alive\r\n"
			"Content-Type: application/json\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%.*s", method, action, len, len, buf);
	mg_send(tgb.conn, msg, strlen(msg));
	nob_temp_reset();
}

void TGBotRespQueueAdd(TGB_RespType resp_type) {
	size_t last_tail = tgb.resp.tail;
	tgb.resp.tail = (tgb.resp.tail + 1) % TGB_QUEUE_CAPACITY;
	if (tgb.resp.tail == tgb.resp.head) { MG_ERROR(("msg queue overflow")); }
	tgb.resp.buf[last_tail] = resp_type;
}

TGB_RespType TGBotRespQueuePop() {
	if (tgb.resp.head == tgb.resp.tail) { return TGB_MT_UNKNOWN; }
	TGB_RespType resp_type = tgb.resp.buf[tgb.resp.head];
	tgb.resp.head = (tgb.resp.head + 1) % TGB_QUEUE_CAPACITY;
	return resp_type;
}

void TGBotSendGetWebhookInfo() {
	TGBotRespQueueAdd(TGB_MT_GET_WEBHOOK_INFO);
	TGBotAPISendJSON("GET", "getWebhookInfo", "", 0);
}

void TGBotSendDeleteWebhook() {
	TGBotRespQueueAdd(TGB_MT_DELETE_WEBHOOK);
	TGBotAPISendJSON("POST", "deleteWebhook", "", 0);
}

void TGBotSendGetUpdates() {
	TGBotRespQueueAdd(TGB_MT_GET_UPDATES);
	char* json =
		tgb.update_offset == 0 ?
		nob_temp_sprintf("{}") :
		nob_temp_sprintf("{\"offset\":\"%lu\"}", tgb.update_offset + 1);
	TGBotAPISendJSON("GET", "getUpdates", json, strlen(json));
	nob_temp_reset();
}

void TGBotSendText(uint64_t chat_id, char* text) {
	TGBotRespQueueAdd(TGB_MT_SEND_MESSAGE);
	cJSON *msg_json = cJSON_CreateObject();
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "chat_id", nob_temp_sprintf("%lu", chat_id)));
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "text", text));
	char* msg_str = cJSON_PrintUnformatted(msg_json);
	TGBotAPISendJSON("POST", "sendMessage", msg_str, strlen(msg_str));
	cJSON_Delete(msg_json);
	free(msg_str);
	nob_temp_reset();
}

void TGBotSendTextMD(uint64_t chat_id, char* text) {
	TGBotRespQueueAdd(TGB_MT_SEND_MESSAGE);
	cJSON *msg_json = cJSON_CreateObject();
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "chat_id", nob_temp_sprintf("%lu", chat_id)));
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "parse_mode", "Markdown"));
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "text", text));
	char* msg_str = cJSON_PrintUnformatted(msg_json);
	TGBotAPISendJSON("POST", "sendMessage", msg_str, strlen(msg_str));
	cJSON_Delete(msg_json);
	free(msg_str);
	nob_temp_reset();
}

void TGBotPoll() {
	uint64_t now = mg_millis();
	if (now - tgb.last_poll_ms > 2000) {
		MG_INFO(("POLL\n"));
		TGBotSendGetUpdates();
		tgb.last_poll_ms = now;
	}
}

void TGBotHandleTelegramResponse(void* ev_data) {
	MG_INFO(("MSG\n"));
	struct mg_http_message* hm = (struct mg_http_message*)ev_data;

	cJSON* msg = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
	if (!cJSON_IsObject(msg)) { return; }
	cJSON* res = cJSON_GetObjectItemCaseSensitive(msg, "result");
	if (!cJSON_IsArray(res)) { return; }
	cJSON* update;
	cJSON_ArrayForEach(update, res) { tgb.fn(update); }
	cJSON_Delete(msg);
}

void TGBotHandleTelegramMessage(struct mg_connection* c, void* ev_data) {
	int result = 200;
	struct mg_http_message* hm = (struct mg_http_message*)ev_data;
#ifdef TGBOT_WEBHOOK_URL
	struct mg_str* value = mg_http_get_header(hm, "X-Telegram-Bot-Api-Secret-Token");
	if (value == NULL) { MG_INFO(("webhook_secret: (nil)")); nob_return_defer(400); }
	//MG_INFO(("webhook_secret: %.*s", (int)value->len, value->buf));
	if (mg_strcmp(*value, mg_str(TGBOT_WEBHOOK_SECRET))) { nob_return_defer(400); }
#endif
	//printf("msg:%.*s\n", hm->message.len, hm->message.buf);
	cJSON* update = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
	if (!cJSON_IsObject(update)) { nob_return_defer(400); }
	tgb.fn(update);
defer:
	mg_http_reply(c, result, "", "");
	cJSON_Delete(update);
}

void TGBotWebhookEventHandler(struct mg_connection* c, int ev, void* ev_data) {
	switch (ev) {
		case MG_EV_HTTP_MSG:
			TGBotHandleTelegramMessage(c, ev_data);
			break;
		case MG_EV_ERROR:
			MG_INFO(("ERROR '%s'\n", ev_data));
			break;
	}
}

#ifdef TGBOT_WEBHOOK_URL
void TGBotSendSetWebhook() { // TODO: do backend in separate file
	TGBotRespQueueAdd(TGB_MT_SET_WEBHOOK);
	mg_http_listen(tgb.conn->mgr, "http://localhost:"TGBOT_WEBHOOK_PORT, TGBotWebhookEventHandler, NULL);
	char msg[] = "{\"url\":\""TGBOT_WEBHOOK_URL"\",\"secret_token\":\""TGBOT_WEBHOOK_SECRET"\"}";
	TGBotAPISendJSON("POST", "setWebhook", msg, strlen(msg));
	nob_temp_reset();
}
#endif

void TGBotEventHandler(struct mg_connection* c, int ev, void* ev_data) {
	switch (ev) {
		case MG_EV_CONNECT:
			MG_INFO(("CONNECTION\n"));
			struct mg_str ca = mg_file_read(&mg_fs_posix, "/etc/ssl/certs/ca-certificates.pem");
			struct mg_tls_opts opts = { .ca=ca, .name=mg_str("telegram.org") };
			mg_tls_init(c, &opts);
			break;
		case MG_EV_TLS_HS:
			MG_INFO(("HANDSHAKE\n"));
			tgb.is_connected = true;
			TGBotSendText(TGBOT_ADMIN_CHAT_ID, "Server started.");
			TGBotSendGetWebhookInfo();
#ifdef TGBOT_WEBHOOK_URL
			TGBotSendSetWebhook();
#else
			TGBotSendDeleteWebhook();
#endif
			break;
		case MG_EV_HTTP_MSG:
			struct mg_http_message* hm = (struct mg_http_message*)ev_data;
			TGB_RespType resp_type = TGBotRespQueuePop();
			//MG_INFO(("resp.len=%d,resp_type=%s\n", tgb.resp.len, TGB_MSG_TYPE_NAMES[resp_type]));
			MG_INFO(("%s:%.*s\n", TGB_RESP_TYPE_NAMES[resp_type], hm->body.len, hm->body.buf));
#ifndef TGBOT_WEBHOOK_URL
			if (resp_type == TGB_MT_GET_UPDATES) {
				TGBotHandleTelegramResponse(ev_data);
			}
#endif
			break;
		case MG_EV_CLOSE:
			MG_INFO(("CLOSE\n"));
			tgb.is_connected = false;
			TGBotConnect(c->mgr, tgb.fn);
			break;
		case MG_EV_ERROR:
			MG_INFO(("ERROR '%s'\n", ev_data));
			break;
	}
}

void TGBotConnect(struct mg_mgr* mgr, TGB_HandleUpdate fn) {
	Nob_String_Builder sb = {0};
	BReader br = {0};
	if (nob_file_exists("dbs/tgb_update_offset")) {
		NOB_ASSERT(nob_read_entire_file("dbs/tgb_update_offset", &sb));
		br.data = sb.items;
		br.count = sb.count;
		NOB_ASSERT(BReadU64(&br, &tgb.update_offset));
	}
	tgb.fn = fn;
	tgb.conn = mg_http_connect(mgr, TGBOT_API_URL, TGBotEventHandler, NULL);
	nob_sb_free(sb);
}

void TGBotClose(struct mg_mgr* mgr) {
	// Send pending messages
	for (size_t i = 0; i < 3; i++) {
		mg_mgr_poll(mgr, 1000);
	}
	// Drain
	tgb.is_connected = false;
	tgb.conn->is_closing = true;
	for (size_t i = 0; i < 3; i++) {
		mg_mgr_poll(mgr, 1000);
	}
	// Save state
	bw_temp.count = 0;
	BWriteU64(&bw_temp, tgb.update_offset);
	nob_write_entire_file("dbs/tgb_update_offset", bw_temp.items, bw_temp.count);
}

#endif /* TGBOT_IMPLEMENTATION */
