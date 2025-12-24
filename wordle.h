#ifndef WORDLE_H
#define WORDLE_H

#include "tgbot.h"
#include "wchar.h"
#include "mongoose/mongoose.h"

typedef struct Wordle {
	uint8_t word_index;
	uint8_t word[5];
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

size_t utf8_char_len(uint8_t c) {
	if ((c & 0x80) == 0x00) { return 1; }
	if ((c & 0xE0) == 0xC0) { return 2; }
	if ((c & 0xF0) == 0xE0) { return 3; }
	if ((c & 0xF8) == 0xF0) { return 4; }
	return 0;
}

uint32_t utf8_to_codepoint(struct mg_str s, uint8_t* l_out) {
	if (s.len == 0) { return (uint32_t)-1; }
	size_t l = utf8_char_len(s.buf[0]);
	if (l == 0) { return (uint32_t)-1; }
	if (l > s.len) { return (uint32_t)-1; }
	uint32_t cp = s.buf[0];
	switch (l) {
		case 2: cp &= 0x1F; break;
		case 3: cp &= 0x0F; break;
		case 4: cp &= 0x07; break;
	}
	for (size_t i = 1; i < l; i++) {
		if ((s.buf[i] & 0xC0) != 0x80) { return (uint32_t)-1; }
		cp = (cp << 6) | (s.buf[i] & 0x3F);
	}
	*l_out = l;
	return cp;
}

uint8_t WordleLookupLetter(uint32_t cp) {
	// а - 0, б - 1, в - 2, ..., я = 32, й = 33, ё - 34
	if (cp == (uint32_t)-1) { return (uint8_t)-1; }
	uint8_t lkp_i = (uint8_t)-1;
	if (cp >= 0x430 && cp <= 0x44f) { lkp_i = cp - 0x430; }
	if (cp >= 0x410 && cp <= 0x42f) { lkp_i = cp - 0x410; }
	if (cp == 0x439 || cp == 0x419) { lkp_i = 33; }
	if (cp == 0x451 || cp == 0x401) { lkp_i = 34; }
	return lkp_i;
}

void WordleMessage(int chat_id, char* text, void* data) {
	bool result = true;
	Wordle* wordle = (Wordle*)data;
	struct mg_str word = mg_str(text);
	size_t counter = 0;
	for (size_t i = 0; i < word.len; ) {
		if (counter >= 6) { nob_return_defer(false); }
		//mg_hexdump(&word.buf[i], word.len - i);
		uint8_t l;
		uint8_t lkp_i = WordleLookupLetter(utf8_to_codepoint(mg_str(&word.buf[i]), &l));
		if (lkp_i == (uint8_t)-1) { nob_return_defer(false); }
		if (lkp_i == wordle->word[counter]) {
			wordle->tiles[wordle->word_index * 5 + counter] = 3;
		} else {
			for (size_t i = 0; i < 5; i++) {
				if (lkp_i == wordle->word[i]) {
					wordle->tiles[wordle->word_index * 5 + counter] = 2;
					break;
				}
			}
		}
		counter++;
		i += l;
	}
	printf("\n");
	nob_temp_reset();
	if (counter != 5) { nob_return_defer(false); }
defer:
	if (!result) {
		TGBotSendText(chat_id, "Error reading the word.");
	} else {
		if (wordle->word_index == 6) {
			TGBotSendText(chat_id, "No more tries.");
			return;
		}
		wordle->word_index++;
		Nob_String_Builder sb = {0};
		nob_sb_appendf(&sb, "Word is valid:\n");
		nob_sb_appendf(&sb, "```txt\n");
		for (size_t i = 0; i < wordle->word_index; i++) {
			nob_sb_appendf(&sb, "abcde -> ");
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
		nob_sb_free(sb);
	}
}

#endif /* WORDLE_IMPLEMENTATION */
