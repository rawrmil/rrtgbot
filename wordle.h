#ifndef WORDLE_H
#define WORDLE_H

#include "tgbot.h"
#include "wchar.h"
#include "mongoose/mongoose.h"

typedef struct Wordle {
	uint8_t word_index;
	uint8_t word[5];
	uint8_t words[40]; // 5 * 8
	uint8_t tiles[40]; // 5 * 8
	// 0 - unknown
	// 1 - letter doesn't exist
	// 2 - letter does exist
	// 3 - letter exists in this place
} Wordle;

typedef struct WordleWord {
	uint8_t difficulty;
	// 0 - unknown
	// 1 - easy
	// 2 - medium
	// 3 - hard
	// ? - funny
	uint8_t word[5];
} WordleWord;

typedef struct WordleWords {
	WordleWord* items;
	size_t count;
	size_t capacity;
} WordleWords;

extern WordleWords wordle_words;

void* WordleInit();
void WordleInitWords();
bool WordleMessage(int chat_id, char* text, void* data);

#endif /* WORDLE_H */

#ifdef WORDLE_IMPLEMENTATION

size_t ut8cplen(uint8_t c) {
	if ((c & 0x80) == 0x00) { return 1; } // 0xxxxxxx
	if ((c & 0xE0) == 0xC0) { return 2; } // 001xxxxx
	if ((c & 0xF0) == 0xE0) { return 3; } // 0001xxxx
	if ((c & 0xF8) == 0xF0) { return 4; } // 00001xxx
	return 0;
}

size_t ut8cp(char* buf, size_t len, uint32_t* cp_out) {
	if (len == 0) { return (size_t)-1; }
	size_t l = ut8cplen(buf[0]);
	if (l == 0) { return (size_t)-1; }
	if (l > len) { return (size_t)-1; }
	uint32_t cp = buf[0];
	switch (l) {
		case 2: cp &= 0x1F; break;
		case 3: cp &= 0x0F; break;
		case 4: cp &= 0x07; break;
	}
	for (size_t i = 1; i < l; i++) {
		if ((buf[i] & 0xC0) != 0x80) { return (size_t)-1; }
		cp = (cp << 6) | (buf[i] & 0x3F);
	}
	*cp_out = cp;
	return l;
}

size_t ut8cptobuf(uint32_t cp, uint8_t* out) {
	if (cp <= 0x7F) {
		// out: 0xxx'xxxx
		out[0] = (uint8_t)cp;
		return 1;
	} else if (cp <= 0x07FF) {
		// out: 110x'xxxx 10xx'xxxx
		out[0] = (0xC0) | (cp >> 6);
		out[1] = (0x80) | (cp & 0x3F);
		return 2;
	} else if (cp <= 0xFFFF) {
		// U+D800 - U+0FFF: surrogate pairs for UTF-16
		if (cp >= 0xD800 && cp <= 0xDFFF) { return 0; }
		// out: 1110'xxxx 10xx'xxxx 10xx'xxxx
		out[0] = 0xE0 | (cp >> 12);
		out[1] = 0x80 | ((cp >> 6) & 0x3F);
		out[2] = 0x80 | (cp & 0x3F);
		return 3;
	} else if (cp <= 0x0010FFFF) {
		// out: 1111'0xxx 10xx'xxxx 10xx'xxxx 10xx'xxxx
		out[0] = 0xF0 | (cp >> 18);
		out[1] = 0x80 | ((cp >> 12) & 0x3F);
		out[2] = 0x80 | ((cp >> 6) & 0x3F);
		out[3] = 0x80 | (cp & 0x3f);
		return 4;
	}
}

bool ut8cptosb(Nob_String_Builder* sb, uint32_t cp) {
	uint8_t out[4];
	size_t len;
	len = ut8cptobuf(cp, out);
	if (len == 0) { return false; }
	nob_da_append_many(sb, out, len);
}

WordleWords wordle_words;

uint8_t WordleCPToRuCode(uint32_t cp) {
	if (cp == (uint32_t)-1) { return (uint8_t)-1; }
	uint8_t rc = (uint8_t)-1;
	if (cp >= 0x430 && cp <= 0x44f) { rc = cp - 0x430; }
	if (cp >= 0x410 && cp <= 0x42f) { rc = cp - 0x410; }
	if (cp == 0x451 || cp == 0x401) { rc = 32; }
	return rc;
}

uint32_t WordleRuCodeToCP(uint8_t rc) {
	// а - 0, б - 1, в - 2, ..., я = 31, ё - 32
	if (rc > 32) { return (uint32_t)-1; }
	if (rc != 32) { return 0x430 + rc; }
	return 0x451;
}

void* WordleInit() {
	Wordle* wordle = calloc(1, sizeof(*wordle));
	if (wordle_words.count == 0) { return NULL; }
	Nob_String_Builder sb = {0};
	for (size_t k = 0; k < wordle_words.count; k++) { // Iteration limit
		size_t i = rand() % wordle_words.count;
		if (k == wordle_words.count - 1 || wordle_words.items[i].difficulty == 1) {
			for (size_t j = 0; j < 5; j++) {
				wordle->word[j] = wordle_words.items[i].word[j];
				ut8cptosb(&sb, WordleRuCodeToCP(wordle->word[j]));
			}
			break;
		}
	}
	MG_INFO(("WORD: %.*s\n", (int)sb.count, sb.items));
	nob_sb_free(sb);
	return (void*)wordle;
}

void WordleInitWords() {
	bool result = true;
	Nob_String_Builder fdata = {0};
	nob_read_entire_file("resources/wordle_bank.txt", &fdata);
	WordleWord curr = {0};
	size_t index = 0;
	for (size_t i = 0; i < fdata.count; ) {
		uint32_t cp;
		i += ut8cp(&fdata.items[i], fdata.count - i, &cp);
		if (cp == (uint32_t)-1) { nob_return_defer(false); }
		switch (cp) {
		case '\n':
			nob_da_append(&wordle_words, curr);
			index = 0;
			break;
		case '-': curr.difficulty = 0; break;
		case '1': curr.difficulty = 1; break;
		case '2': curr.difficulty = 2; break;
		case '3': curr.difficulty = 3; break;
		default:
			uint8_t lkp_i = WordleCPToRuCode(cp);
			if (index == 5) { nob_return_defer(false); }
			if (lkp_i == (uint8_t)-1) { nob_return_defer(false); }
			curr.word[index] = lkp_i;
			index++;
			break;
		}
	}
defer:
	if (!result) { MG_ERROR(("Wordle parsing error.")); }
	nob_sb_free(fdata);
	Nob_String_Builder sb = {0};
	for (size_t i = 0; i < wordle_words.count; i++) {
		nob_sb_append_cstr(&sb, " ");
		nob_sb_appendf(&sb, "%lu-", wordle_words.items[i].difficulty);
		for (size_t j = 0; j < 5; j++) {
			ut8cptosb(&sb, WordleRuCodeToCP(wordle_words.items[i].word[j]));
		}
		if (i == 10) { break; }
	}
	nob_sb_appendf(&sb, " ...\n");
	nob_sb_append_null(&sb);
	printf("Wordle words loaded:%.*s\n", (int)sb.count, sb.items);
	nob_sb_free(sb);
}

bool WordleMessage(int chat_id, char* text, void* data) {
	bool result = true;
	Wordle* wordle = (Wordle*)data;
	struct mg_str word = mg_str(text);
	size_t counter = 0;
	uint32_t cp;
	for (size_t i = 0; i < word.len; ) {
		i += ut8cp(&word.buf[i], word.len - i, &cp);
		//mg_hexdump(&word.buf[i], word.len - i);
		uint8_t rc = WordleCPToRuCode(cp);
		wordle->words[wordle->word_index * 5 + counter] = rc;
		if (rc == (uint8_t)-1) { nob_return_defer(false); }
		//printf("rc=%d,word[%d]=%d\n", rc, counter, wordle->word[counter]);
		if (rc == wordle->word[counter]) {
			wordle->tiles[wordle->word_index * 5 + counter] = 3;
		} else {
			for (size_t j = 0; j < 5; j++) {
				if (rc == wordle->word[j]) {
					wordle->tiles[wordle->word_index * 5 + counter] = 2;
					break;
				}
			}
		}
		counter++;
		if (counter == 5) { break; }
	}
	nob_temp_reset();
	if (counter != 5) { nob_return_defer(false); }
	bool found_word = false;
	for (size_t i = 0; i < wordle_words.count; i++) {
		if (memcmp(&wordle->words[wordle->word_index * 5], wordle_words.items[i].word, 5) == 0) {
			found_word = true;
			break;
		}
	}
	if (!found_word) { nob_return_defer(false); }
defer:
	Nob_String_Builder sb = {0};
	if (!result) {
		for (size_t j = 0; j < 5; j++) {
			wordle->tiles[wordle->word_index * 5 + j] = 1;
		}
		// TODO: TGBotSendf
		TGBotSendText(chat_id, "Ошибка недопустимое слово.");
		//TGBotSendText(chat_id, "Error invalid word.");
	} else {
		nob_sb_appendf(&sb, "Слово существует:\n");
		//nob_sb_appendf(&sb, "Word exists:\n");
		nob_sb_appendf(&sb, "```txt\n");
		//mg_hexdump(wordle->words, 40);
		//mg_hexdump(wordle->tiles, 40);
		for (size_t i = 0; i < wordle->word_index + 1; i++) {
			for (size_t j = 0; j < 5; j++) {
				ut8cptosb(&sb, WordleRuCodeToCP(wordle->words[i * 5 + j]));
			}
			nob_sb_appendf(&sb, " -> ");
			for (size_t j = 0; j < 5; j++) {
				switch (wordle->tiles[i * 5 + j]) {
				case 1:
					nob_sb_appendf(&sb, "⬜️");
					break;
				case 2:
					nob_sb_appendf(&sb, "🟨");
					break;
				case 3:
					nob_sb_appendf(&sb, "🟩");
					break;
				default:
					//NOB_UNREACHABLE("^here");
					nob_sb_appendf(&sb, "⬜️");
					break;
				}
			}
			nob_sb_appendf(&sb, "\n");
		}
		nob_sb_appendf(&sb, "```\n");
		bool guessed_right = true;
		for (size_t i = 0; i < 5; i++) {
			if (wordle->tiles[wordle->word_index * 5 + i] != 3) {
				guessed_right = false;
				break;
			}
		}
		if (guessed_right) {
				nob_sb_appendf(&sb, "Ты угадал! Это '");
				//nob_sb_appendf(&sb, "You guessed right! It's '");
				for (size_t j = 0; j < 5; j++) {
					ut8cptosb(&sb, WordleRuCodeToCP(wordle->word[j]));
				}
				nob_sb_appendf(&sb, "'");
		}
		nob_sb_append_null(&sb);
		TGBotSendTextMD(chat_id, sb.items);
		if (guessed_right) { return true; }
		if (wordle->word_index == 5) {
			sb.count = 0;
			nob_sb_appendf(&sb, "Попытки кончились. Ответ: '");
			//nob_sb_appendf(&sb, "No more tries. Answer: '");
			for (size_t j = 0; j < 5; j++) {
				ut8cptosb(&sb, WordleRuCodeToCP(wordle->word[j]));
			}
			nob_sb_appendf(&sb, "'");
			nob_sb_append_null(&sb);
			TGBotSendText(chat_id, sb.items);
			return true;
		}
		wordle->word_index++;
	}
	nob_sb_free(sb);
	return false;
}

#endif /* WORDLE_IMPLEMENTATION */
