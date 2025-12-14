#ifndef TGBOT_RW_H
#define TGBOT_RW_H

#include "binary_rw.h"
#include "cJSON/cJSON.h"

#define TGBOT_API_HOST "api.telegram.org"
#define TGBOT_API_URL "https://"TGBOT_API_HOST"/"

void TGBotEventHandler(struct mg_connection* c, int ev, void* ev_data);
void TGBotConnect(struct mg_mgr* mgr);
void TGBotGet(struct mg_connection* c, char* action);
void TGBotPost(struct mg_connection* c, char* action, char* content_type, char* buf, size_t len);
void TGBotPoll();
void TGBotClose();

extern struct mg_connection* tgb_conn;
extern uint64_t tgb_last_poll_ms;
extern uint64_t tgb_update_offset;

#endif /* TGBOT_RW_H */

#ifdef TGBOT_IMPLEMENTATION

struct mg_connection* tgb_conn;
uint64_t tgb_last_poll_ms;
uint64_t tgb_update_offset;

void TGBotConnect(struct mg_mgr* mgr) {
	Nob_String_Builder sb = {0};
	BReader br = {0};
	if (nob_file_exists("dbs/tgb_update_offset")) {
		NOB_ASSERT(nob_read_entire_file("dbs/tgb_update_offset", &sb));
		br.data = sb.items;
		br.count = sb.count;
		NOB_ASSERT(BReadU64(&br, &tgb_update_offset));
	}
	tgb_conn = mg_http_connect(mgr, TGBOT_API_URL, TGBotEventHandler, NULL);
}

void TGBotGet(struct mg_connection* c, char* action) {
	mg_printf(c,
			"GET /bot"TGBOT_API_TOKEN"/%s HTTP/1.1\r\n"
			"Host: "TGBOT_API_HOST"\r\n"
			"Connection: keep-alive\r\n"
			"\r\n", action);
}

void TGBotPost(struct mg_connection* c, char* action, char* content_type, char* buf, size_t len) {
	char* msg = nob_temp_sprintf(
			"POST /bot"TGBOT_API_TOKEN"/%s HTTP/1.1\r\n"
			"Host: "TGBOT_API_HOST"\r\n"
			"Connection: keep-alive\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %zu\r\n"
			"\r\n"
			"%*.s", action, content_type, len, len, buf);
	printf("msg='%s'", msg);
	mg_send(c, msg, strlen(msg));
	nob_temp_reset();
}

void TGBotPoll() {
	uint64_t now = mg_millis();
	if (now - tgb_last_poll_ms > 1000) {
		MG_INFO(("TGBOT: POLL\n"));
		TGBotGet(tgb_conn, "getUpdates");
		tgb_last_poll_ms = now;
	}
}

void TGBotEventHandler(struct mg_connection* c, int ev, void* ev_data) {
	switch (ev) {
		case MG_EV_CONNECT:
			MG_INFO(("TGBOT: CONNECTION\n"));
			struct mg_str ca = mg_file_read(&mg_fs_posix, "/etc/ssl/certs/ca-certificates.pem");
			struct mg_tls_opts opts = { .ca=ca, .name=mg_str("telegram.org") };
			mg_tls_init(c, &opts);
			break;
		case MG_EV_TLS_HS:
			MG_INFO(("TGBOT: HANDSHAKE\n"));
			TGBotGet(c, "getMe");
			break;
		case MG_EV_HTTP_MSG:
			MG_INFO(("TGBOT: MSG\n"));
			struct mg_http_message* hm = (struct mg_http_message*)ev_data;
			mg_hexdump(hm->body.buf, hm->body.len);
			break;
		case MG_EV_ERROR:
			MG_INFO(("TGBOT: ERROR '%s'\n", ev_data));
			break;
	}
}

void TGBotClose() {
	bw_temp.count = 0;
	BWriteU64(&bw_temp, tgb_update_offset);
	nob_write_entire_file("dbs/tgb_update_offset", bw_temp.items, bw_temp.count);
}

#endif /* TGBOT_IMPLEMENTATION */
