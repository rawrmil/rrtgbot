#define NOB_STRIP_PREFIX

#define NOB_IMPLEMENTATION
#include "nob.h"
#undef NOB_IMPLEMENTATION

int main(int argc, char** argv) {

	NOB_GO_REBUILD_URSELF(argc, argv);
	
	Cmd cmd = {0};

	if (!nob_mkdir_if_not_exists("out")) return 1;

	if (!nob_file_exists("credentials.h")) {
		nob_write_entire_file("credentials.h", "", 0);
	}

	if (needs_rebuild1("out/mongoose.o", "3rd_party/mongoose/mongoose.c")) {
		cmd_append(&cmd,
				"cc", "-c", "3rd_party/mongoose/mongoose.c", "-o", "out/mongoose.o",
				"-DMG_TLS=MG_TLS_BUILTIN");
		if (!cmd_run(&cmd)) return 1;
	}

	cmd_append(&cmd,
			"cc", "main.c", "3rd_party/cJSON/cJSON.c", "-o", "out/rrtgbot",
			"out/mongoose.o",
			"-I./3rd_party");
	if (!cmd_run(&cmd)) return 1;

	return 0;
}
