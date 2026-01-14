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

#define LOGIC_IMPLEMENTATION
#include "logic.h"
#undef LOGIC_IMPLEMENTATION

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

// --- MAIN ---

bool is_closed;

void app_terminate(int sig) {
	TGBotSendText(TGBOT_ADMIN_CHAT_ID, "Server closed.");
	printf("TGBOT: CLOSING\n");
	is_closed = true;
}

int main(int argc, char* argv[]) {

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
	WordleDeinitGame();

	return 0;
}
