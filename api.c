#include "api.h"
#include "libs/log.h"
#include <curl/curl.h>
#include <errno.h>
#include <string.h>
#include <yyjson.h>

#define USER_AGENT                                                          \
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:152.0) Gecko/20100101 " \
    "Firefox/152.0"

#define REFERER "Referer: https://www.bilibili.com"
#define HOST "Host: api.bilibili.com"
#define CONNECTION "Connection: keep-alive"
#define ACCEPT_LANG "Accept-Language: en-US,en;q=0.9"
#define ACCEPT \
    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8"

// 获取视频详细信息
static const char *API_VIDEO_DETAIL =
    "https://api.bilibili.com/x/web-interface/wbi/view/detail?need_view=1";

// 请求所有可用 DASH 流
static const char *API_VIDEO_STREAM =
    "https://api.bilibili.com/x/player/wbi/"
    "playurl?fnver=0&fnval=4048&fourk=1&from_client=BROWSER&need_"
    "fragment=false";

static size_t curl_read_cb(char *content, size_t size, size_t nmemb,
                           void *userp)
{
    size_t         realsize = size * nmemb;
    struct Buffer *buffer = (struct Buffer *)userp;

    buffer->memory = realloc(buffer->memory, buffer->size + realsize + 1);
    if (!buffer->memory) {
        ERR("Failed to reallocate memory for buffer->menory\n");
        return 0;
    }

    memcpy(buffer->memory + buffer->size, content, realsize);
    buffer->size += realsize;

    return realsize;
}

void pages_free(Pages *pages)
{
    if (pages == NULL) {
        return;
    }

    for (size_t i = 0; i < pages->num; i++) {
        if (pages->part[i] != NULL) {
            free(pages->part[i]);
        }
    }
    free(pages->cid);
    free(pages);
}

int video_download_and_merge(char *cookie, char *stream_url, char *audio_url,
                             char *filename, char *dir)
{
    int   status = 0;
    CURL *curl = curl_easy_init();
    if (curl) {
        CURLcode       code;
        struct Buffer *buffer = (struct Buffer *)malloc(sizeof(struct Buffer));
        buffer->memory = NULL;
        buffer->size = 0;

        struct curl_slist *header = NULL;
        header = curl_slist_append(header, USER_AGENT);
        header = curl_slist_append(header, REFERER);

        char *stream_outname = NULL;
        char *audio_outname = NULL;
        char *video_outname = NULL;
        if (dir != NULL) {
            asprintf(&stream_outname, "%s%s_stream.m4s", dir, filename);
            asprintf(&audio_outname, "%s%s_audio.m4s", dir, filename);
            asprintf(&video_outname, "%s%s.mp4", dir, filename);
        } else {
            asprintf(&stream_outname, "%s_stream.m4s", filename);
            asprintf(&audio_outname, "%s_audio.m4s", filename);
            asprintf(&video_outname, "%s.mp4", filename);
        }

        FILE *stream_outfile = fopen(stream_outname, "w");
        FILE *audio_outfile = fopen(audio_outname, "w");
        if (stream_outfile == NULL) {
            ERR("Failed to open %s\n", stream_outname);
            ERR("%s\n", strerror(errno));
            status = -1;
            goto end;
        }
        if (audio_outfile == NULL) {
            ERR("Failed to open %s\n", audio_outname);
            ERR("%s\n", strerror(errno));
            fclose(stream_outfile);
            status = -1;
            goto end;
        }

        curl_easy_setopt(curl, CURLOPT_URL, stream_url);
        curl_easy_setopt(curl, CURLOPT_COOKIE, cookie);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, stream_outfile);

        INFO("[GET] %s\n", stream_url);
        code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            ERR("%s\n", curl_easy_strerror(code));
            status = -1;
            goto end;
        }
        fclose(stream_outfile);

        curl_easy_setopt(curl, CURLOPT_URL, audio_url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, audio_outfile);

        INFO("[GET] %s\n", audio_url);
        code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            ERR("%s\n", curl_easy_strerror(code));
            status = -1;
            goto end;
        }
        fclose(audio_outfile);

        char *command_merge = NULL;
        asprintf(&command_merge, "ffmpeg -y -i \"%s\" -i \"%s\" -c copy \"%s\"",
                 stream_outname, audio_outname, video_outname);
        if (system(command_merge) < 0) {
            ERR("Failed to merge %s & %s \n", stream_outname, audio_outname);
            ERR("%s\n", strerror(errno));
            status = -1;
            goto end;
        }
        free(command_merge);

    end:
        free(video_outname);
        free(stream_outname);
        free(audio_outname);
        curl_slist_free_all(header);
        curl_easy_cleanup(curl);
    }

    return status;
}

int video_stream_fetch_finish(Stream *stream, struct Buffer *buffer,
                              char **stream_url, char **audio_url)
{
    int status = 0;

    yyjson_doc *doc = yyjson_read(buffer->memory, buffer->size, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (yyjson_is_null(root)) {
        ERR("Failed to get root\n");
        status = -1;
        goto end;
    }

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *dash = yyjson_obj_get(data, "dash");
    if (yyjson_is_null(data) || yyjson_is_null(dash)) {
        ERR("Failed to get data/dash\n");
        status = -1;
        goto end;
    }

    yyjson_val *video = yyjson_obj_get(dash, "video");
    yyjson_val *audio = yyjson_obj_get(dash, "audio");
    if (yyjson_is_null(video) || yyjson_is_null(audio)) {
        ERR("Failed to get video/audio\n");
        status = -1;
        goto end;
    }

    size_t      idx, max;
    yyjson_val *video_item;
    yyjson_arr_foreach(video, idx, max, video_item)
    {
        yyjson_val *id = yyjson_obj_get(video_item, "id");
        yyjson_val *codecid = yyjson_obj_get(video_item, "codecid");
        if (yyjson_is_null(id) || yyjson_is_null(codecid)) {
            ERR("Failed to get video:id/video:codecid\n");
            break;
        }

        if (stream->qn == 0 || yyjson_get_int(id) == stream->qn) {
            if (stream->code == 0 || yyjson_get_int(codecid) == stream->code) {

                // 使用 backupUrl 内的链接以避免不稳定的 mcdn
                yyjson_val *backupurl = yyjson_obj_get(video_item, "backupUrl");
                yyjson_val *url0 = yyjson_arr_get_first(backupurl);
                if (!yyjson_is_null(url0)) {
                    *stream_url = strdup(yyjson_get_str(url0));
                    break;
                }
            }
        }
    }

    idx = 0, max = 0;
    yyjson_val *audio_item;
    yyjson_arr_foreach(audio, idx, max, audio_item)
    {
        yyjson_val *id = yyjson_obj_get(audio_item, "id");
        if (yyjson_is_null(id)) {
            ERR("Failed to get audio:id\n");
            break;
        }

        if (stream->audio == 0 || yyjson_get_int(id) == stream->audio) {
            // 同上，取 backupUrl
            yyjson_val *backupurl = yyjson_obj_get(audio_item, "backupUrl");
            yyjson_val *url0 = yyjson_arr_get_first(backupurl);
            if (!yyjson_is_null(url0)) {
                *audio_url = strdup(yyjson_get_str(url0));
                break;
            }
        }
    }

end:
    yyjson_doc_free(doc);
    return status;
}

int video_stream_fetch(Stream *stream, Pages *pages)
{
    if (stream == NULL || pages == NULL) {
        ERR("stream/pages is NULL\n");
        return -1;
    }

    int   status = 0;
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        ERR("Failed to initialize curl\n");
        return -1;
    }

    CURLcode       code;
    struct Buffer *buffer = (struct Buffer *)malloc(sizeof(struct Buffer));
    buffer->memory = NULL;
    buffer->size = 0;

    struct curl_slist *header = NULL;
    header = curl_slist_append(header, USER_AGENT);
    header = curl_slist_append(header, REFERER);
    header = curl_slist_append(header, HOST);
    header = curl_slist_append(header, CONNECTION);
    header = curl_slist_append(header, ACCEPT);
    header = curl_slist_append(header, ACCEPT_LANG);

    curl_easy_setopt(curl, CURLOPT_COOKIE, stream->cookie);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_read_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

    char *base_url = NULL;
    if (strstr(stream->video_id, "BV")) {
        asprintf(&base_url, "%s&bvid=%s", API_VIDEO_STREAM, stream->video_id);
    } else {
        asprintf(&base_url, "%s&aid=%s", API_VIDEO_STREAM, stream->video_id);
    }

    for (size_t i = 0; i < pages->num; i++) {
        char *url = NULL;
        char *stream_url = NULL, *audio_url = NULL;

        asprintf(&url, "%s&cid=%lu", base_url, pages->cid[i]);
        curl_easy_setopt(curl, CURLOPT_URL, url);

        INFO("[GET] %s\n", url);

        code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            ERR("Failed to fetch %s\n", url);
            ERR("%s\n", curl_easy_strerror(code));
            status = -1;
            goto end;
        }

        if (video_stream_fetch_finish(stream, buffer, &stream_url, &audio_url) <
            0) {
            ERR("Failed to parse stream_url/audio_url\n");
            goto end;
        }
        if (stream_url == NULL || audio_url == NULL) {
            ERR("Failed to get stream_url/audio_url\n");
            goto end;
        }

        if (video_download_and_merge(stream->cookie, stream_url, audio_url,
                                     pages->part[i], stream->output) < 0) {
            ERR("Failed to download and merge video\n");
        }

    end:
        if (stream_url != NULL) {
            free(stream_url);
        }
        if (audio_url != NULL) {
            free(audio_url);
        }
        free(url);
    }

    free(buffer->memory);
    free(buffer);
    free(base_url);
    curl_slist_free_all(header);
    curl_easy_cleanup(curl);
    return status;
}

int video_print_info_finish(struct Buffer *buffer, Pages *pages)
{
    if (buffer == NULL || buffer->size == 0) {
        ERR("buffer is NULL\n");
        return -1;
    }

    int         status = 0;
    yyjson_doc *doc = yyjson_read(buffer->memory, buffer->size, 0);
    yyjson_val *root = yyjson_doc_get_root(doc);
    if (root == NULL) {
        ERR("Failed to get root of video information doc\n");
        status = -1;
        goto end;
    }

    yyjson_val *code = yyjson_obj_get(root, "code");
    if (yyjson_is_null(code) || yyjson_get_int(code) != 0) {
        ERR("Failed to get video information: code %d\n", yyjson_get_int(code));
        goto end;
    }

    yyjson_val *data = yyjson_obj_get(root, "data");
    yyjson_val *view = yyjson_obj_get(data, "View");
    if (yyjson_is_null(data) || yyjson_is_null(view)) {
        ERR("Failed to get data or View\n");
        goto end;
    }

    yyjson_val *bvid = yyjson_obj_get(view, "bvid");
    yyjson_val *aid = yyjson_obj_get(view, "aid");
    yyjson_val *title = yyjson_obj_get(view, "title");
    if (yyjson_is_null(bvid) || yyjson_is_null(aid) || yyjson_is_null(title)) {
        ERR("Failed to get bvid/aid/title\n");
        goto end;
    }

    yyjson_val *owner = yyjson_obj_get(view, "owner");
    yyjson_val *mid = yyjson_obj_get(owner, "mid");
    yyjson_val *name = yyjson_obj_get(owner, "name");
    if (yyjson_is_null(owner) || yyjson_is_null(mid) || yyjson_is_null(name)) {
        ERR("Failed to get owner/mid/name\n");
        goto end;
    }

    yyjson_val *view_pages = yyjson_obj_get(view, "pages");
    if (yyjson_is_null(view_pages)) {
        ERR("Failed to get pages\n");
        goto end;
    }

    INFO("Title: %s\n", yyjson_get_str(title));
    INFO("Author: %s mid: %lu\n", yyjson_get_str(name), yyjson_get_uint(mid));
    INFO("bvid: %s aid: %lu\n", yyjson_get_str(bvid), yyjson_get_uint(aid));

    size_t      idx, max;
    yyjson_val *page;
    yyjson_arr_foreach(view_pages, idx, max, page)
    {
        yyjson_val *cid = yyjson_obj_get(page, "cid");
        yyjson_val *part = yyjson_obj_get(page, "part");
        if (yyjson_is_null(cid) || yyjson_is_null(part)) {
            ERR("Failed to get cid/part\n");
            goto end;
        }

        INFO("part: %s cid: %lu\n", yyjson_get_str(part), yyjson_get_uint(cid));

        pages->cid = realloc(pages->cid, (pages->num + 1) * sizeof(uint64_t));
        pages->part = realloc(pages->part, (pages->num + 1) * sizeof(char *));
        pages->cid[pages->num] = yyjson_get_uint(cid);
        pages->part[pages->num] = strdup(yyjson_get_str(part));

        ++pages->num;
    }

end:
    yyjson_doc_free(doc);
    return status;
}

int video_print_info(Stream *stream, Pages *pages)
{
    if (stream == NULL || stream->video_id == NULL) {
        ERR("video_id is NULL\n");
        return -1;
    }

    char *url = NULL;
    if (strstr(stream->video_id, "BV")) {
        asprintf(&url, "%s&bvid=%s", API_VIDEO_DETAIL, stream->video_id);
    } else {
        asprintf(&url, "%s&aid=%s", API_VIDEO_DETAIL, stream->video_id);
    }

    int   status = 0;
    CURL *curl = curl_easy_init();
    if (curl) {
        CURLcode       code;
        struct Buffer *buffer = (struct Buffer *)malloc(sizeof(struct Buffer));
        buffer->memory = NULL;
        buffer->size = 0;

        struct curl_slist *header = NULL;
        header = curl_slist_append(header, USER_AGENT);
        header = curl_slist_append(header, REFERER);
        header = curl_slist_append(header, HOST);
        header = curl_slist_append(header, CONNECTION);
        header = curl_slist_append(header, ACCEPT);
        header = curl_slist_append(header, ACCEPT_LANG);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_COOKIE, stream->cookie);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_read_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);

        code = curl_easy_perform(curl);
        if (code != CURLE_OK) {
            ERR("Failed to get video information\n");
            ERR("%s\n", curl_easy_strerror(code));
            status = -1;
        } else {
            status = video_print_info_finish(buffer, pages);
        }

        free(buffer->memory);
        free(buffer);
        curl_slist_free_all(header);
        curl_easy_cleanup(curl);
    }

    free(url);
    return status;
}

int stream_perform(Stream *stream)
{
    if (stream == NULL || stream->type == UNINIT) {
        ERR("stream is NULL or stream->type == UNINIT\n");
        return -1;
    }
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int status = 0;
    switch (stream->type) {
    case VIDEO: {
        Pages *pages = calloc(1, sizeof(Pages));
        status = video_print_info(stream, pages);
        if (status < 0) {
            ERR("Aborted with code %d\n", status);
            break;
        }

        status = video_stream_fetch(stream, pages);
        if (status < 0) {
            ERR("Aborted with code %d\n", status);
        }

        pages_free(pages);
        break;
    }
    case LIVE: {
        break;
    }
    default: {
        ERR("Invalid type of stream\n");
        return -1;
    }
    }

    curl_global_cleanup();
    return status;
}
