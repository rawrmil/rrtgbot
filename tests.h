#ifndef TESTS_H
#define TESTS_H

#include "nob.h"
#include "tgbot.h"
#include "wordle.h"

void Tests();

#endif /* TESTS_RW_H */

#ifdef TESTS_IMPLEMENTATION

uint64_t timer;

struct TestsJSONsToFree {
	cJSON** items;
	size_t count;
	size_t capacity;
} jsons_to_free;

void TestWordle10000Games() {
	void* wordle_data;
	for (int i = 0; i < 10000; i++) {
		NOB_ASSERT(wordle_data = WordleInit());
		size_t j = 0;
		for (; j < 100; j++) {
			if (WordleMessage(i, "дочка", wordle_data)) {
				free(wordle_data);
				break;
			}
			for (cJSON* msg_json = NULL;;) {
				TGB_QUEUE_POP(&tgb.mocking_q, &msg_json, NULL);
				if (msg_json == NULL) { break; }
				//char* msg = cJSON_PrintUnformatted(msg_json);
				//printf("%s\n", msg);
				//free(msg);
				//cJSON_Delete(msg_json);
				nob_da_append(&jsons_to_free, msg_json);
			}
		}
		NOB_ASSERT(j < 100);
	}
	while (jsons_to_free.count != 0) {
		cJSON_Delete(jsons_to_free.items[0]);
		nob_da_remove_unordered(&jsons_to_free, 0);
	}
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
	tgb.is_mocking = true;
	printf("--- TESTING MODE ---\n");
	RUN_TEST(TestWordle10000Games);
	printf("--- TESTING ENDED ---\n");
}

#endif /* TESTS_IMPLEMENTATION */
