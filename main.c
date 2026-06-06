#include "api.h"
#include "libs/argparse.h"
#include "libs/log.h"

int main(int argc, char *argv[])
{
    int status = 0;

    Stream *stream = (Stream *)calloc(1, sizeof(Stream));
    if (stream == NULL) {
        ERR("Failed to initialize Stream structure\n");
        return 1;
    }
    stream->type = UNINIT;

    argparse_option options[] = {
        ARG_BOOLEAN(NULL, "-h", "--help", "Show help information", NULL,
                    "help"),
        ARG_STR(&stream->cookie, "-c", "--cookie",
                "Set cookies for the request", NULL, NULL),
        ARG_STR(&stream->video_id, "-v", "--video", "Pass bvid or aid", NULL,
                NULL),
        ARG_STR(&stream->live_id, "-l", "--live", "Pass room id", NULL, NULL),
        ARG_INT(&stream->qn, "-q", "--qn", "Video/Live stream quality", NULL,
                NULL),
        ARG_INT(&stream->code, "-d", "--coder", "Video/Live stream coder", NULL,
                NULL),
        ARG_INT(&stream->audio, "-a", "--audio", "Video/Live audio quality",
                NULL, NULL),
        ARG_STR(&stream->output, "-o", "--output",
                "Output directory(include '/' or '\\')", NULL, NULL),
        ARG_END(),
    };

    argparse arg_parser;
    argparse_init(&arg_parser, options, 0);
    argparse_describe(&arg_parser, "bilicat",
                      "A tool for fetching BiliBili stream",
                      "Source Code: <https://github.com/inuyasha-660/bilicat>");
    argparse_parse(&arg_parser, argc, argv);

    if (arg_ismatch(&arg_parser, "help")) {
        argparse_info(&arg_parser);
        free_argparse(&arg_parser);
        return 0;
    }

    if (stream->video_id != NULL && stream->live_id == NULL) {
        stream->type = VIDEO;
    } else if (stream->video_id == NULL && stream->live_id != NULL) {
        stream->type = LIVE;
    } else {
        ERR("Invalid video/live argument\n");
        return 1;
    }

    if (stream_perform(stream) < 0) {
        status = 1;
    }
    free_argparse(&arg_parser);

    return status;
}
