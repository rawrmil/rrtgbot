#ifndef WORDLE_H
#define WORDLE_H

#include "tgbot.h"
#include "wchar.h"
#include "mongoose/mongoose.h"

typedef struct Wordle {
	uint8_t word_index;
	uint8_t word[5];
	uint8_t words[30]; // 5 * 6
	uint8_t tiles[30]; // 5 * 6
	// 0 - unknown
	// 1 - letter doesn't exist
	// 2 - letter does exist
	// 3 - letter exists in this place
} Wordle;

void* WordleInit();
void WordleMessage(int chat_id, char* text, void* data);

#endif /* WORDLE_H */

#ifdef WORDLE_IMPLEMENTATION

void* WordleInit() {
	Wordle* wordle = calloc(1, sizeof(*wordle));
	wordle->word[0] = 0;
	wordle->word[1] = 1;
	wordle->word[2] = 2;
	wordle->word[3] = 3;
	wordle->word[4] = 4;
	return (void*)wordle;
}

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

uint8_t WordleCPToRuCode(uint32_t cp) {
	// а - 0, б - 1, в - 2, ..., я = 31, ё - 32
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

void WordleMessage(int chat_id, char* text, void* data) {
	bool result = true;
	Wordle* wordle = (Wordle*)data;
	struct mg_str word = mg_str(text);
	size_t counter = 0;
	uint32_t cp;
	for (size_t i = 0; i < word.len; ) {
		i += ut8cp(&word.buf[i], word.len - i, &cp);
		if (counter >= 6) { nob_return_defer(false); }
		//mg_hexdump(&word.buf[i], word.len - i);
		uint8_t rc = WordleCPToRuCode(cp);
		wordle->words[wordle->word_index * 5 + counter] = rc;
		//printf("rc=%lu\n", rc);
		//uint8_t out[4];
		//size_t l = ut8cptobuf(WordleRuCodeToCP(rc), out);
		//printf("letter=%.*s\n", (int)l, out);
		if (rc == (uint8_t)-1) { nob_return_defer(false); }
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
		if (counter > 5) { nob_return_defer(false); }
	}
	printf("\n");
	nob_temp_reset();
	if (counter != 5) { nob_return_defer(false); }
defer:
	Nob_String_Builder sb = {0};
	if (!result) {
		// TODO: TGBotSendf
		TGBotSendText(chat_id, "Error reading the word.");
	} else {
		if (wordle->word_index == 6) {
			TGBotSendText(chat_id, "No more tries.");
			return;
		}
		wordle->word_index++;
		nob_sb_appendf(&sb, "Word is valid:\n");
		nob_sb_appendf(&sb, "```txt\n");
		for (size_t i = 0; i < wordle->word_index; i++) {
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
					nob_sb_appendf(&sb, "⬜️");
					break;
				}
			}
			nob_sb_appendf(&sb, "\n");
		}
		nob_sb_appendf(&sb, "```\n");
		nob_sb_append_null(&sb);
		TGBotSendTextMD(chat_id, sb.items);
	}
	nob_sb_free(sb);
}

#endif /* WORDLE_IMPLEMENTATION */
