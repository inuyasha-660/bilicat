#include <stddef.h>
#include <stdint.h>

typedef struct Stream Stream;
typedef struct Pages Pages;
typedef enum TYPE TYPE;

enum TYPE {
    UNINIT,
    VIDEO,
    LIVE,
};

struct Stream {
    TYPE type;
    char *cookie;
    char *video_id;
    char *live_id;
    char *mid;
    int  qn;
    int  code;
    int  audio;
    char *output;
};

struct Buffer {
    char  *memory;
    size_t size;
};

struct Pages {
    size_t num;
    uint64_t *cid;
    char **part;
};

int stream_perform(Stream *stream);
