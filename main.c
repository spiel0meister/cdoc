#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <dirent.h>
#ifndef DT_DIR
    #define DT_DIR 4
#endif // DT_DIR

#include "include/tree-sitter.h"
#include "include/tree-sitter-c.h"

#define SBUILDER_IMPLEMENTATION
#include "string_builder.h"

#define CBUILD_IMPLEMENTATION
#include "cbuild.h"

#include "da.h"

#define DOC_FUNCS_QUERY "((comment)* @comment . (declaration declarator: (function_declarator declarator: (identifier) @name)) @func)"
typedef struct {
    char name[1024];
    char func[1024];

    char* comment_start;
    size_t comment_len;
}FunctionDoc;

typedef struct {
    FunctionDoc* items;
    size_t count;
    size_t capacity;
}FunctionDocList;

#define DOC_TYPEDEF_QUERY "((comment)* @comment . (type_definition declarator: (type_identifier) @name) @struct)"
typedef struct {
    char name[1024];
    char typedef_[1024];

    char* comment_start;
    size_t comment_len;
}TypedefDoc;

typedef struct {
    TypedefDoc* items;
    size_t count;
    size_t capacity;
}TypedefDocList;

#define DOC_MACRO_QUERY "((comment)* @comment . [(preproc_def name: (identifier) @name) (preproc_function_def name: (identifier) @name)] @def)"
typedef struct {
    char name[1024];
    char def[1024];

    char* comment_start;
    size_t comment_len;
}MacroDoc;

typedef struct {
    MacroDoc* items;
    size_t count;
    size_t capacity;
}MacroDocList;

typedef struct {
    char name[256];
    char path[2048];
}File;

typedef struct {
    File* items;
    size_t count;
    size_t capacity;
}FileList;

char* readfile(const char* filepath) {
    FILE *f = fopen(filepath, "rb");
    fseek(f, 0, SEEK_END);
    size_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);  /* same as rewind(f); */

    char *string = malloc(fsize + 1);
    size_t _n = fread(string, fsize, 1, f);
    (void)_n;
    fclose(f);

    string[fsize] = 0;

    return string;
}

size_t node_get_text(char* out, TSNode node, const char* source) {
    uint32_t start_offset = ts_node_start_byte(node);
    uint32_t end_offset = ts_node_end_byte(node);
    int len = end_offset - start_offset;
    assert(len >= 0);
    memcpy(out, source + start_offset, len);
    return len;
}

void find_header_files(FileList* files, const char* path) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        switch (errno) {
            case EACCES:
            case EBADF:
                printf("[WARN] couldn't open directory %s: %s\n", path, strerror(errno));
                return;
            case EMFILE:
            case ENFILE:
                printf("[ERROR] couldn't open directory %s: %s\n", path, strerror(errno));
                exit(1);
            case ENOMEM:
                assert(0 && "Buy more ram lol!");
            case ENOENT:
            case ENOTDIR:
                assert(0);
        }
    }

    struct dirent* dirent_ = readdir(dir);
    while (dirent_ != NULL) {
        char path_[2048] = {};
        snprintf(path_, 2047, "%s/%s", path, dirent_->d_name);

        if (dirent_->d_type == DT_DIR) {
            if (dirent_->d_name[0] == '.') goto while_end;
            find_header_files(files, path_);
        } else {
            char* name = dirent_->d_name;
            while (name[1] != 0) name++;
            if (*name == 'h' && *(name - 1) == '.') {
                File file = {};
                memcpy(file.name, dirent_->d_name, 255);
                memcpy(file.path, path_, 2047);
                da_append(files, file);
            }
        }
while_end:
        dirent_ = readdir(dir);
    }

    closedir(dir);
}

int main(int argc, char** argv) {
    assert(argc > 0);
    const char* program = *argv++; argc--;

    if (argc == 0) {
        printf("Usage: %s <dirpath>\n", program);
        return 1;
    }
    const char* dirname = *argv++; argc--;

    FileList files = {};
    find_header_files(&files, dirname);

    TSParser* parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());

    if (!create_dir_if_not_exists("docs/")) return 1;
    da_foreach(&files, File, file) {
        printf("[INFO] processing %s\n", file->path);
        char* code = readfile(file->path);

        TSTree* tree = ts_parser_parse_string(parser, NULL, code, strlen(code));
        TSNode root = ts_tree_root_node(tree);

        uint32_t error_pos;
        TSQueryError q_error;
        TSQuery* query = ts_query_new(tree_sitter_c(), DOC_FUNCS_QUERY, strlen(DOC_FUNCS_QUERY), &error_pos, &q_error);
        assert(q_error == TSQueryErrorNone);

        TSQueryCursor* cursor = ts_query_cursor_new();
        ts_query_cursor_exec(cursor, query, root);

        FunctionDocList fdoc_list = {};
        TSQueryMatch match;
        while (ts_query_cursor_next_match(cursor, &match)) {
            FunctionDoc doc = {};
            size_t comment_len = 0;
            for (int i = 0; i < match.capture_count; ++i) {
                TSQueryCapture capture = match.captures[i];
                TSNode node = capture.node;
                uint32_t capture_name_len;
                const char* capture_name = ts_query_capture_name_for_id(query, capture.index, &capture_name_len);

                if (strcmp(capture_name, "comment") == 0) {
                    uint32_t start = ts_node_start_byte(node);
                    uint32_t end = ts_node_end_byte(node);
                    if (doc.comment_start == NULL) {
                        doc.comment_start = code + start;
                    }
                    comment_len += end - start;
                } else if (strcmp(capture_name, "func") == 0) {
                    node_get_text(doc.func, node, code);
                } else if (strcmp(capture_name, "name") == 0) {
                    node_get_text(doc.name, node, code);
                } else {
                    assert(0);
                }
            }
            doc.comment_len = comment_len;

            da_append(&fdoc_list, doc);
        }
        ts_query_delete(query);
        query = ts_query_new(tree_sitter_c(), DOC_TYPEDEF_QUERY, strlen(DOC_TYPEDEF_QUERY), &error_pos, &q_error);
        assert(q_error == TSQueryErrorNone);

        ts_query_cursor_exec(cursor, query, root);

        TypedefDocList structdoc_list = {};
        while (ts_query_cursor_next_match(cursor, &match)) {
            TypedefDoc doc = {};
            size_t comment_len = 0;
            for (int i = 0; i < match.capture_count; ++i) {
                TSQueryCapture capture = match.captures[i];
                TSNode node = capture.node;
                uint32_t capture_name_len;
                const char* capture_name = ts_query_capture_name_for_id(query, capture.index, &capture_name_len);

                if (strcmp(capture_name, "comment") == 0) {
                    uint32_t start = ts_node_start_byte(node);
                    uint32_t end = ts_node_end_byte(node);
                    if (doc.comment_start == NULL) {
                        doc.comment_start = code + start;
                    }
                    comment_len += end - start;
                } else if (strcmp(capture_name, "struct") == 0) {
                    node_get_text(doc.typedef_, node, code);
                } else if (strcmp(capture_name, "name") == 0) {
                    node_get_text(doc.name, node, code);
                } else {
                    assert(0);
                }
            }
            doc.comment_len = comment_len;

            da_append(&structdoc_list, doc);
        }
        ts_query_delete(query);
        query = ts_query_new(tree_sitter_c(), DOC_MACRO_QUERY, strlen(DOC_MACRO_QUERY), &error_pos, &q_error);
        assert(q_error == TSQueryErrorNone);

        ts_query_cursor_exec(cursor, query, root);

        MacroDocList macrodoc_list = {};
        while (ts_query_cursor_next_match(cursor, &match)) {
            MacroDoc doc = {};
            size_t comment_len = 0;
            for (int i = 0; i < match.capture_count; ++i) {
                TSQueryCapture capture = match.captures[i];
                TSNode node = capture.node;
                uint32_t capture_name_len;
                const char* capture_name = ts_query_capture_name_for_id(query, capture.index, &capture_name_len);

                if (strcmp(capture_name, "comment") == 0) {
                    uint32_t start = ts_node_start_byte(node);
                    uint32_t end = ts_node_end_byte(node);
                    if (doc.comment_start == NULL) {
                        doc.comment_start = code + start;
                    }
                    comment_len += end - start;
                } else if (strcmp(capture_name, "def") == 0) {
                    node_get_text(doc.def, node, code);
                } else if (strcmp(capture_name, "name") == 0) {
                    node_get_text(doc.name, node, code);
                } else {
                    assert(0);
                }
            }
            doc.comment_len = comment_len;

            da_append(&macrodoc_list, doc);
        }

        StringBuilder sb = sbuilder_init(1024);
        sbuilder_push_str(&sb, "# ", file->name, "\n");
        sbuilder_push_str(&sb, "## Struct Typedefs\n");
        da_foreach(&structdoc_list, TypedefDoc, doc) {
            sbuilder_push_str(&sb, "### ", doc->name, "\n");
            sbuilder_push_str(&sb, "```c\n", doc->typedef_, "\n```\n");
            if (doc->comment_start != NULL) {
                sbuilder_push_nstr(&sb, doc->comment_start, doc->comment_len);
                sbuilder_push_str(&sb, "\n\n");
            } else {
                sbuilder_push_str(&sb, "\n");
            }
        }

        sbuilder_push_str(&sb, "## Functions\n");
        da_foreach(&fdoc_list, FunctionDoc, doc) {
            sbuilder_push_str(&sb, "### ", doc->name, "\n");
            sbuilder_push_str(&sb, "```c\n", doc->func, "\n```\n");
            if (doc->comment_start != NULL) {
                sbuilder_push_nstr(&sb, doc->comment_start, doc->comment_len);
                sbuilder_push_str(&sb, "\n\n");
            } else {
                sbuilder_push_str(&sb, "\n");
            }
        }

        sbuilder_push_str(&sb, "## Macros\n");
        da_foreach(&macrodoc_list, MacroDoc, doc) {
            sbuilder_push_str(&sb, "### ", doc->name, "\n");
            sbuilder_push_str(&sb, "```c\n", doc->def, "\n```\n");
            if (doc->comment_start != NULL) {
                sbuilder_push_nstr(&sb, doc->comment_start, doc->comment_len);
                sbuilder_push_str(&sb, "\n\n");
            } else {
                sbuilder_push_str(&sb, "\n");
            }
        }


        char out_filepath[1024] = {};
        char* filename_md = path_with_ext(file->name, ".md");
        sprintf(out_filepath, "docs/%s", filename_md);
        FILE* out = fopen(out_filepath, "w");
        fwrite(sb.items, 1, sb.size, out);
        fclose(out);
        sbuilder_free(&sb);
        free(filename_md);

        da_free(&fdoc_list);
        da_free(&structdoc_list);
        da_free(&macrodoc_list);

        ts_query_delete(query);
        ts_query_cursor_delete(cursor);
        ts_tree_delete(tree);

        free(code);
    }

    ts_parser_delete(parser);

    return 0;
}
