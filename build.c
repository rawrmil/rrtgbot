#define NOB_STRIP_PREFIX

#define NOB_IMPLEMENTATION
#include "nob.h"
#undef NOB_IMPLEMENTATION

#define FLAG_IMPLEMENTATION
#include "flag.h"
#undef FLAG_IMPLEMENTATION

struct Flags {
	bool* o3;
	bool* sanitize;
} flags;

void FlagsParse(int argc, char** argv) {
	bool* f_help = flag_bool("help", 0, "help");
	flags.o3 = flag_bool("O3", false, "-O3 optimizations");
	flags.sanitize = flag_bool("sanitize", false, "sanitizers");

	if (!flag_parse(argc, argv)) {
    flag_print_options(stdout);
		flag_print_error(stderr);
		exit(1);
	}

	if (*f_help) {
    flag_print_options(stdout);
		exit(0);
	}
}

int main(int argc, char** argv) {
	NOB_GO_REBUILD_URSELF(argc, argv);

	FlagsParse(argc, argv);

	Cmd cmd = {0};

	if (!nob_mkdir_if_not_exists("out")) return 1;

	if (!nob_file_exists("credentials.h")) {
		nob_write_entire_file("credentials.h", "", 0);
	}

	if (needs_rebuild1("out/mongoose.o", "3rd_party/mongoose/mongoose.c")) {
		cmd_append(&cmd,
				"cc", "-c", "3rd_party/mongoose/mongoose.c", "-o", "out/mongoose.o",
				"-O3",
				"-DMG_TLS=MG_TLS_BUILTIN");
		if (!cmd_run(&cmd)) return 1;
	}

	cmd_append(&cmd,
			"cc", "main.c", "3rd_party/cJSON/cJSON.c", "-o", "out/rrtgbot",
			"out/mongoose.o",
			"-I./3rd_party");
	if (*flags.sanitize) { cmd_append(&cmd, "-fsanitize=address,undefined"); }
	if (*flags.o3) { cmd_append(&cmd, "-O3"); }
	if (!cmd_run(&cmd)) return 1;

	return 0;
}
