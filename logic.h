#ifndef LOGIC_H_
#define LOGIC_H_

#include "wordle.h"

typedef struct TGB_Chat {
	// To disk
	int id;
	// Intermediate
	TGB_ChatMode mode;
	void* mode_data;
} TGB_Chat;

typedef struct TGB_Chats {
	TGB_Chat* items;
	size_t count;
	size_t capacity;
} TGB_Chats;

TGB_Chats chats;

TGB_Chat* TGBotGetChatById(int id);
bool HandleUserCommand(TGB_Chat* chat, char* text);
void HandleCommandExit(TGB_Chat* chat);
void HandleCommand(TGB_Chat* chat, char* text);
void HandleUpdate(cJSON* update);

#endif /* LOGIC_H_ */

#ifdef LOGIC_IMPLEMENTATION

TGB_Chat* TGBotGetChatById(int id) {
	// TODO: hashmap
	nob_da_foreach(TGB_Chat, chat, &chats) {
		if (chat->id == id) { return chat; }
	}
	return NULL;
}

char rm_empty[] = "{}";
char rm_exit[] = "{\"keyboard\":[[\"/exit\"]],\"resize_keyboard\":true}";
char rm_help[] =
			"{\"keyboard\":["
				"[\"/help\",\"/wordle\"],"
				"[\"/echo\",\"/foo\"]"
			"],\"resize_keyboard\":true}";
char rm_wordle[] =
			"{\"keyboard\":["
				"[\"/wordle_play\"],[\"/wordle_board\"],[\"/wordle_name\"]"
			"],\"resize_keyboard\":true}";

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
		TGBotSendTextMDReplyMarkup(chat->id,
				"/wordle\\_play - играть\n"
				"/wordle\\_board - лидерборд\n"
				"/wordle\\_name  - поменять ник\n",
				rm_wordle);
		return true;
	}
	if (strcmp(text, "/wordle_name") == 0) {
		chat->mode = TGB_CM_WORDLE_NAME;
		TGBotSendTextMDReplyMarkup(chat->id,
				"Введи ник (латинские буквы, цифры, 5 символов максимум, отмена /exit):",
				rm_exit);
		return true;
	}
	if (strcmp(text, "/wordle_board") == 0) {
		WordleLeaderboard(chat->id);
		return true;
	}
	if (strcmp(text, "/wordle_play") == 0) {
		chat->mode = TGB_CM_WORDLE_PLAY;
		chat->mode_data = WordleInitSession(chat->id);
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
		case TGB_CM_WORDLE_PLAY:
			TGBotSendTextMDReplyMarkup(chat->id, "Выход из Вордла.", rm_help);
			//TGBotSendText(chat->id, "Exited Wordle.");
			break;
		case TGB_CM_WORDLE_NAME:
			TGBotSendTextMDReplyMarkup(chat->id, "Выход из изменения имени.", rm_wordle);
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
		case TGB_CM_WORDLE_PLAY:
			if (WordleMessage(chat->id, text, chat->mode_data)) { HandleCommandExit(chat); }
			break;
		case TGB_CM_WORDLE_NAME:
			if (WordleNickname(chat->id, text)) {
				TGBotSendTextMDReplyMarkup(chat->id, "Ник обновлен.", rm_wordle);
				HandleCommandExit(chat);
				break;
			}
			TGBotSendTextMDReplyMarkup(chat->id, "Ник НЕ обновлен, проверь правильность.", rm_exit);
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
	cJSON* chat_username_json = cJSON_GetObjectItemCaseSensitive(chat_json, "username");
	if (!cJSON_IsString(chat_username_json)) { return; }
	int chat_id = chat_id_json->valueint;
	char* username = chat_username_json->valuestring;

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

#endif /* LOGIC_IMPLEMENTATION */
