#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#define TARGET "cdoc"
#define MAIN "main.c"

void cflags(Cmd* cmd, bool debug) {
    if (debug) cmd_push_str(cmd, "-ggdb");
    else cmd_push_str(cmd, "-O2");
    cmd_push_str(cmd, "-std=c2x", "-Wall", "-Wextra");
}

void libs(Cmd* cmd) {
    cmd_push_str(cmd, "-L./lib/", "-l:libtree-sitter.a", "-l:libtree-sitter-c.a");
}

void cmd_main(Cmd* cmd, bool debug) {
    cmd_push_str(cmd, "gcc");
    cflags(cmd, debug);
    cmd_push_str(cmd, "-o", TARGET, MAIN);
    libs(cmd);
}

typedef struct {
    bool force;
    bool debug;
}Config;

Config parse_config(int argc, char** argv) {
    Config config = { .debug = false, .force = false };

    while (argc > 0) {
        char* arg = *argv++; argc--;

        if (arg[0] == '-') {
            if (strcmp(arg, "-F") == 0) config.force = true;
            else if (strcmp(arg, "-D") == 0) config.debug = true;
            else {
                printf("[WARN] unknown flag: %s\n", arg);
            } 
        }
    }

    return config;
}

int main(int argc, char** argv) {
    Cmd cmd = {};
    build_yourself_cflags(&cmd, &argc, &argv, "-std=c2x", "-Wall", "-Wextra");

    Config config = parse_config(argc, argv);

    if (config.force || need_rebuild(TARGET, SRCS(MAIN))) {
        cmd_main(&cmd, config.debug);
        if (!cmd_run_sync(&cmd, true)) return 1;
    }
    cmd.count = 0;

    cmd_free(&cmd);

    return 0;
}
