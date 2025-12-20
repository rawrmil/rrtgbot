#ifndef TGBOT_RW_H
#define TGBOT_RW_H

#include "binary_rw.h"
#include "cJSON/cJSON.h"

#define TGBOT_API_HOST "api.telegram.org"
#define TGBOT_API_URL "https://"TGBOT_API_HOST"/"

#define X_TGB_CHAT_MODE \
	X(TGB_CM_DEFAULT) \
	X(TGB_CM_ECHO) \
	X(TGB_CM_FOO) \
	X(TGB_CM_LENGTH)

#define X(name_) name_,
typedef enum TGB_ChatMode {
	X_TGB_CHAT_MODE
} TGB_ChatMode;
#undef X
extern char* TGB_CHAT_MODE_NAMES[];

typedef struct TGB_Chat {
	TGB_ChatMode mode;
	int id;
} TGB_Chat;

typedef struct TGB_Chats {
	TGB_Chat* items;
	size_t count;
	size_t capacity;
} TGB_Chats;

typedef struct TGB_Bot {
	struct mg_connection* conn;
	uint64_t last_poll_ms;
	uint64_t update_offset;
	TGB_Chats chats;
} TGB_Bot;

extern TGB_Bot tgb;

void TGBotConnect(struct mg_mgr* mgr);
void TGBotPoll();
void TGBotClose();

void TGBotSendText(uint64_t chat_id, char* text);

#endif /* TGBOT_RW_H */

#ifdef TGBOT_IMPLEMENTATION

#define X(name_) #name_,
char* TGB_CHAT_MODE_NAMES[] = { X_TGB_CHAT_MODE };
#undef X

TGB_Bot tgb;

void TGBotGet(struct mg_connection* c, char* action, char* content_type, char* buf, size_t len) {
	char* msg = nob_temp_sprintf(
			"GET /bot"TGBOT_API_TOKEN"/%s HTTP/1.1\r\n"
			"Host: "TGBOT_API_HOST"\r\n"
			"Connection: keep-alive\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%.*s", action, content_type, len, len, buf);
	mg_send(c, msg, strlen(msg));
	nob_temp_reset();
}

void TGBotPost(struct mg_connection* c, char* action, char* content_type, char* buf, size_t len) {
	char* msg = nob_temp_sprintf(
			"POST /bot"TGBOT_API_TOKEN"/%s HTTP/1.1\r\n"
			"Host: "TGBOT_API_HOST"\r\n"
			"Connection: keep-alive\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%.*s", action, content_type, len, len, buf);
	mg_send(c, msg, strlen(msg));
	nob_temp_reset();
}

void TGBotSendText(uint64_t chat_id, char* text) {
	cJSON *msg_json = cJSON_CreateObject();
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "chat_id", nob_temp_sprintf("%lu", chat_id)));
	NOB_ASSERT(cJSON_AddStringToObject(msg_json, "text", text));
	char* msg_str = cJSON_PrintUnformatted(msg_json);
	TGBotPost(tgb.conn, "sendMessage", "application/json", msg_str, strlen(msg_str));
	free(msg_str);
	nob_temp_reset();
}

void TGBotPoll() {
	uint64_t now = mg_millis();
	if (now - tgb.last_poll_ms > 2000) {
		MG_INFO(("POLL\n"));
		char* json =
			tgb.update_offset == 0 ?
			nob_temp_sprintf("") :
			nob_temp_sprintf("{\"offset\":\"%lu\"}", tgb.update_offset + 1);
		TGBotGet(tgb.conn, "getUpdates", "application/json", json, strlen(json));
		nob_temp_reset();
		tgb.last_poll_ms = now;
	}
}

TGB_Chat* TGBotGetChatById(int id) {
	nob_da_foreach(TGB_Chat, chat, &tgb.chats) {
		if (chat->id == id) { return chat; }
	}
	return NULL;
}

bool TGBotUserHandleCommand(TGB_Chat* chat, char* text) {
	if (strcmp(text, "/echo") == 0) {
		chat->mode = TGB_CM_ECHO;
		TGBotSendText(chat->id, "To exit echo mode type /exit");
		return true;
	}
	if (strcmp(text, "/foo") == 0) {
		TGBotSendText(chat->id, "bar");
		return true;
	}
	TGBotSendText(chat->id, "Unknown command.");
	return false;
}

void TGBotHandleUpdate(cJSON* update) {
	cJSON* update_id_json = cJSON_GetObjectItemCaseSensitive(update, "update_id");
	if (!cJSON_IsNumber(update_id_json)) { return; }
	int update_id = update_id_json->valueint;
	if ((uint64_t)update_id <= tgb.update_offset) { return; }
	tgb.update_offset = (uint64_t)update_id;

	cJSON* message_json = cJSON_GetObjectItemCaseSensitive(update, "message");
	if (!cJSON_IsObject(message_json)) { return; }
	cJSON* text_json = cJSON_GetObjectItemCaseSensitive(message_json, "text");
	if (!cJSON_IsString(text_json)) { return; }
	char* text = text_json->valuestring;

	cJSON* chat_json = cJSON_GetObjectItemCaseSensitive(message_json, "chat");
	if (!cJSON_IsObject(chat_json)) { return; }
	cJSON* chat_id_json = cJSON_GetObjectItemCaseSensitive(chat_json, "id");
	if (!cJSON_IsNumber(chat_id_json)) { return; }
	int chat_id = chat_id_json->valueint;

	TGB_Chat* chat = TGBotGetChatById(chat_id);
	if (chat == NULL) {
		TGB_Chat new_chat = {0};
		new_chat.mode = TGB_CM_DEFAULT;
		new_chat.id = chat_id;
		nob_da_append(&tgb.chats, new_chat);
		TGBotSendText(chat_id, "Hello, stranger.");
		return;
	}
	switch (chat->mode) {
		case TGB_CM_DEFAULT:
			if (text[0] == '/' && TGBotUserHandleCommand(chat, text)) {
				return;
			}
			TGBotSendText(chat_id, "Unknown command.");
			break;
		case TGB_CM_ECHO:
			if (strcmp(text, "/exit") == 0) {
				TGBotSendText(chat_id, "Exited echo.");
				chat->mode = TGB_CM_DEFAULT;
				return;
			}
			TGBotSendText(chat_id, text);
			break;
	}

	//MG_INFO(("%d: '%s'\n", update_id, text));
	MG_INFO(("update_id=%d\n", update_id, text));
}

void TGBotHandleTelegramResponse(void* ev_data) {
	MG_INFO(("MSG\n"));
	struct mg_http_message* hm = (struct mg_http_message*)ev_data;

	cJSON* msg = cJSON_ParseWithLength(hm->body.buf, hm->body.len);
	if (!cJSON_IsObject(msg)) { return; }
	cJSON* res = cJSON_GetObjectItemCaseSensitive(msg, "result");
	if (!cJSON_IsArray(res)) { return; }
	cJSON* update;
	cJSON_ArrayForEach(update, res) { TGBotHandleUpdate(update); }
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
	TGBotHandleUpdate(update);
defer:
	mg_http_reply(c, result, "", "");
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
			TGBotSendText(TGBOT_ADMIN_CHAT_ID, "Server started.");
			TGBotGet(c, "getWebhookInfo", "application/json", "", 0);
#ifdef TGBOT_WEBHOOK_URL
			mg_http_listen(c->mgr, "http://localhost:6766", TGBotWebhookEventHandler, NULL);
			char msg[] = "{\"url\":\""TGBOT_WEBHOOK_URL"\",\"secret_token\":\""TGBOT_WEBHOOK_SECRET"\"}";
			TGBotPost(c, "setWebhook", "application/json", msg, strlen(msg));
			nob_temp_reset();
#else
			TGBotPost(c, "deleteWebhook", "application/json", "", 0);
#endif
			break;
		case MG_EV_HTTP_MSG:
			//struct mg_http_message* hm = (struct mg_http_message*)ev_data;
			//printf("msg:%.*s\n", hm->message.len, hm->message.buf);
			// TODO: show info responces from server
#ifndef TGBOT_WEBHOOK_URL
			TGBotHandleTelegramResponse(ev_data);
#endif
			break;
		case MG_EV_ERROR:
			MG_INFO(("ERROR '%s'\n", ev_data));
			break;
	}
}

void TGBotConnect(struct mg_mgr* mgr) {
	Nob_String_Builder sb = {0};
	BReader br = {0};
	if (nob_file_exists("dbs/tgb_update_offset")) {
		NOB_ASSERT(nob_read_entire_file("dbs/tgb_update_offset", &sb));
		br.data = sb.items;
		br.count = sb.count;
		NOB_ASSERT(BReadU64(&br, &tgb.update_offset));
	}
	tgb.conn = mg_http_connect(mgr, TGBOT_API_URL, TGBotEventHandler, NULL);
}

void TGBotClose() {
	bw_temp.count = 0;
	BWriteU64(&bw_temp, tgb.update_offset);
	nob_write_entire_file("dbs/tgb_update_offset", bw_temp.items, bw_temp.count);
}

#endif /* TGBOT_IMPLEMENTATION */
