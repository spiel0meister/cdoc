#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#define TARGET "cdoc"
#define MAIN "main.c"

void cflags(Cmd* cmd, bool debug) {
    if (debug) cmd_push_str(cmd, "-ggdb");
    cmd_push_str(cmd, "-std=c2x", "-Wall", "-Wextra");
}

void libs(Cmd* cmd) {
    cmd_push_str(cmd, "-L./", "-L./tree-sitter-c/", "-l:libtree-sitter.a", "-l:libtree-sitter-c.a");
}

void cmd_main(Cmd* cmd) {
    cmd_push_str(cmd, "gcc");
    cflags(cmd, true);
    cmd_push_str(cmd, "-o", TARGET, MAIN);
    libs(cmd);
}

int main(int argc, char** argv) {
    Cmd cmd = {};
    build_yourself_cflags(&cmd, &argc, &argv, "-std=c2x", "-Wall", "-Wextra");

    if (need_rebuild(TARGET, SRCS(MAIN))) {
        cmd_main(&cmd);
        if (!cmd_run_sync(&cmd, true)) return 1;
    }
    cmd.count = 0;

    return 0;
}
