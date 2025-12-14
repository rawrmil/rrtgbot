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
	// ...
} flags;

void FlagsParse(int argc, char** argv) {
	mg_log_set(MG_LL_NONE);

	bool* f_help = flag_bool("help", 0, "help");
	uint64_t* f_ll = flag_uint64("log-level", 0, "none, error, info, debug, verbose (0, 1, 2, 3, 4)");

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

// --- MAIN ---

bool is_closed;
uint64_t started_closing;

void app_terminate(int sig) {
	TGBotSendText(TGBOT_ADMIN_CHAT_ID, "Server closed.");
	started_closing = mg_millis();
}

int main(int argc, char* argv[]) {
	FlagsParse(argc, argv);

	NOB_ASSERT(nob_mkdir_if_not_exists("dbs"));

	printf("log_level: %d\n", mg_log_level);

	struct mg_mgr mgr;
	mg_mgr_init(&mgr);
	TGBotConnect(&mgr);

	//printf("tgb_update_offset=%lu\n", tgb_update_offset);
	//tgb_update_offset = 5555;
	//printf("tgb_update_offset=%lu\n", tgb_update_offset);

	signal(SIGINT, app_terminate);
	signal(SIGTERM, app_terminate);
	while (!is_closed) {
		mg_mgr_poll(&mgr, 500);
		TGBotPoll();
		uint64_t now = mg_millis();
		if (started_closing != 0 && now - started_closing > 1000) {
			MG_INFO(("TGBOT: POLL\n"));
			is_closed = true;
		}
	}

	// Closing
	mg_mgr_free(&mgr);
	printf("Server closed.\n");
	TGBotClose();

	return 0;
}
