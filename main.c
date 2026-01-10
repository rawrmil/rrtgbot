#include <stdio.h>
#include <signal.h>
#include <stdbool.h>
#include <getopt.h>
#include <time.h>

#include "mongoose/mongoose.h"

#define NOB_IMPLEMENTATION
#include "nob.h"
#undef NOB_IMPLEMENTATION

#define FLAG_IMPLEMENTATION
#include "flag.h"
#undef FLAG_IMPLEMENTATION

#define BINARY_RW_IMPLEMENTATION
#include "binary_rw.h"
#undef BINARY_RW_IMPLEMENTATION

#include "credentials.h" /* Created by user */

#define TGBOT_IMPLEMENTATION
#include "tgbot.h"
#undef TGBOT_IMPLEMENTATION

#define ENUMS_IMPLEMENTATION
#include "enums.h"
#undef ENUMS_IMPLEMENTATION

// --- SUBSYSTEMS ---

#define WORDLE_IMPLEMENTATION
#include "wordle.h"
#undef WORDLE_IMPLEMENTATION

// --- TESTS ---

#define TESTS_IMPLEMENTATION
#include "tests.h"
#undef TESTS_IMPLEMENTATION

// --- UTILS ---

void RandomBytes(void *buf, size_t len) {
	int fd = open("/dev/urandom", O_RDONLY);
	assert(fd >= 0);
	size_t n = read(fd, buf, len);
	assert(n == len);
	close(fd);
}

// --- APP ---

struct Flags {
	uint64_t* tests_mode;
} flags;

void FlagsParse(int argc, char** argv) {
	mg_log_set(MG_LL_NONE);

	bool* f_help = flag_bool("help", 0, "help");
	uint64_t* f_ll = flag_uint64("log-level", 0, "none, error, info, debug, verbose (0, 1, 2, 3, 4)");
	flags.tests_mode = flag_uint64("tests-mode", 0, "0 - no tests, 1 - tests and exit, 2 - tests and open server");

	if (!flag_parse(argc, argv)) {
    flag_print_options(stdout);
		flag_print_error(stderr);
		exit(1);
	}

	if (*f_help) {
    flag_print_options(stdout);
		exit(0);
	}

	mg_log_set(*f_ll);
}

// --- EVENTS ---

typedef struct TGB_Chat {
	TGB_ChatMode mode;
	int id;
	void* mode_data;
} TGB_Chat;

typedef struct TGB_Chats {
	TGB_Chat* items;
	size_t count;
	size_t capacity;
} TGB_Chats;

TGB_Chats chats;

TGB_Chat* TGBotGetChatById(int id) {
	nob_da_foreach(TGB_Chat, chat, &chats) {
		if (chat->id == id) { return chat; }
	}
	return NULL;
}

cJSON* rm_empty;
cJSON* rm_exit;
cJSON* rm_help;

void HandleUserUnknownCommandMessage(TGB_Chat* chat) {
	TGBotSendTextMDReplyMarkup(chat->id, "Неизвестная команда. Помощь: /help", rm_help);
	//TGBotSendText(chat->id, "Unknown command.");
}

bool HandleUserCommand(TGB_Chat* chat, char* text) {
	if (strcmp(text, "/echo") == 0) {
		chat->mode = TGB_CM_ECHO;
		TGBotSendTextMDReplyMarkup(chat->id, "Чтобы выйти напишите /exit", rm_exit);
		//TGBotSendText(chat->id, "To exit echo mode type /exit");
		return true;
	}
	if (strcmp(text, "/foo") == 0) {
		TGBotSendText(chat->id, "бар");
		//TGBotSendText(chat->id, "bar");
		return true;
	}
	if (strcmp(text, "/wordle") == 0) {
		chat->mode = TGB_CM_WORDLE;
		chat->mode_data = WordleInit(chat->id);
		if (chat->mode_data == NULL) {
			TGBotSendText(chat->id, "Внутренняя ошибка Вордл.");
			//TGBotSendText(chat->id, "Wordle game internal error.");
			return true;
		}
		TGBotSendTextMDReplyMarkup(chat->id, "Игра Вордл началась. Чтобы выйти напишите /exit", rm_exit);
		//TGBotSendText(chat->id, "Wordle game started. To exit type /exit");
		return true;
	}
	HandleUserUnknownCommandMessage(chat);
	return false;
}

void HandleCommandExit(TGB_Chat* chat) {
	switch (chat->mode) {
		case TGB_CM_ECHO:
			TGBotSendTextMDReplyMarkup(chat->id, "Выход из режима 'эхо'.", rm_help);
			//case TGB_CM_ECHO: TGBotSendText(chat->id, "Exited echo.");
			break;
		case TGB_CM_WORDLE:
			TGBotSendTextMDReplyMarkup(chat->id, "Выход из Вордла.", rm_help);
			//case TGB_CM_WORDLE: TGBotSendText(chat->id, "Exited Wordle.");
			break;
	}
	free(chat->mode_data);
	chat->mode_data = NULL;
	chat->mode = TGB_CM_DEFAULT;
}

void HandleCommand(TGB_Chat* chat, char* text) {
	switch (chat->mode) {
		case TGB_CM_ECHO:
			TGBotSendText(chat->id, text);
			break;
		case TGB_CM_WORDLE:
			if (WordleMessage(chat->id, text, chat->mode_data)) { HandleCommandExit(chat); }
			break;
	}
}

void HandleUpdate(cJSON* update) {
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
		nob_da_append(&chats, new_chat);
		chat = &chats.items[chats.count - 1];
	}

	MG_INFO(("update_id=%d\n", update_id, text));

	if (!strcmp(text, "/start") || !strcmp(text, "/help")) {
		TGBotSendTextMDReplyMarkup(chat->id,
				"/help - помощь\n"
				"/wordle - Вордл (угадай слово из пяти букв)\n"
				"/echo - режим эхо\n"
				"/foo - для теста кароч",
				rm_help);
		HandleCommandExit(chat);
		return;
	}

	if (chat->mode == TGB_CM_DEFAULT) {
		if (text[0] == '/') { HandleUserCommand(chat, text); }
		else { HandleUserUnknownCommandMessage(chat); }
		return;
	}

	if (strcmp(text, "/exit") == 0) {
		HandleCommandExit(chat);
		return;
	}

	HandleCommand(chat, text);
}

// --- MAIN ---

bool is_closed;

void app_terminate(int sig) {
	TGBotSendText(TGBOT_ADMIN_CHAT_ID, "Server closed.");
	printf("TGBOT: CLOSING\n");
	is_closed = true;
}

int main(int argc, char* argv[]) {
	rm_empty = cJSON_Parse("{}");
	rm_exit = cJSON_Parse("{\"keyboard\":[[\"/exit\"]],\"resize_keyboard\":true}");
	rm_help = cJSON_Parse(
			"{\"keyboard\":["
				"[\"/help\",\"/wordle\"],"
				"[\"/echo\",\"/foo\"]"
			"],\"resize_keyboard\":true}");

	srand(nob_nanos_since_unspecified_epoch());

	WordleInitGame();
	FlagsParse(argc, argv);
	if (*flags.tests_mode != 0) {
		tgb.is_mocking = true;
		Tests();
		tgb.is_mocking = false;
		if (*flags.tests_mode == 1) { return 0; }
	}

	NOB_ASSERT(nob_mkdir_if_not_exists("dbs"));

	printf("log_level: %d\n", mg_log_level);

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	TGBotConnect(&mgr, HandleUpdate);

	signal(SIGINT, app_terminate);
	signal(SIGTERM, app_terminate);
	uint64_t last = mg_millis();
	while (!is_closed) {
		mg_mgr_poll(&mgr, 100);
#ifndef TGBOT_WEBHOOK_URL
		TGBotPoll();
#endif
	}
	TGBotClose(&mgr);

	// Closing
	mg_mgr_free(&mgr);
	printf("Server closed.\n");
	WordleCloseGame();
	cJSON_Delete(rm_empty);
	cJSON_Delete(rm_exit);
	cJSON_Delete(rm_help);

	return 0;
}
