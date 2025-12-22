#ifndef WORDLE_H
#define WORDLE_H

typedef struct Wordle {
	int test;
} Wordle;

void* WordleInit();
void WordleMessage(void* data);

#endif /* WORDLE_H */

#ifdef WORDLE_IMPLEMENTATION

void* WordleInit() {
	Wordle* wordle = calloc(1, sizeof(*wordle));
	wordle->test = 666;
	return (void*)wordle;
}

void WordleMessage(void* data) {
	Wordle* wordle = (Wordle*)data;
	printf("!!! %d\n", wordle->test);
}

#endif /* WORDLE_IMPLEMENTATION */
