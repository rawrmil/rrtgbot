#ifndef TESTS_H
#define TESTS_H

#include "nob.h"
#include "tgbot.h"
#include "wordle.h"

void Tests();

#endif /* TESTS_RW_H */

#ifdef TESTS_IMPLEMENTATION

uint64_t timer;

struct TestsVoidsToFree {
	void** items;
	size_t count;
	size_t capacity;
} voids_to_free;

void TestWordle10000Games() {
	void* wordle_data;
	for (int i = 0; i < 10000; i++) {
		int pi = i / 1000;
		NOB_ASSERT(wordle_data = WordleInitSession(pi));
		nob_temp_reset();
		size_t j = 0;
		for (; j < 6; j++) {
			if (WordleMessage(i, "бобёр", wordle_data)) {
				free(wordle_data);
				break;
			}
			for (char* msg = NULL;;) {
				TGB_QUEUE_POP(&tgb.mocking_q, &msg, NULL);
				if (msg == NULL) { break; }
				//printf("%s\n", msg);
				nob_da_append(&voids_to_free, msg);
			}
		}
	}
	while (voids_to_free.count != 0) {
		free(voids_to_free.items[0]);
		nob_da_remove_unordered(&voids_to_free, 0);
	}
	WordleLogPlayers();
}

#define RUN_TEST(func_) \
	do { \
		timer = nob_nanos_since_unspecified_epoch(); \
		func_(); \
		uint64_t timer_now = nob_nanos_since_unspecified_epoch(); \
		double secs = (double)(timer_now - timer) / NOB_NANOS_PER_SEC; \
		printf("["#func_"] %lf\n", secs); \
	} while(0)

void Tests() {
	printf("--- TESTING MODE ---\n");
	RUN_TEST(TestWordle10000Games);
	printf("--- TESTING ENDED ---\n");
}

#endif /* TESTS_IMPLEMENTATION */
