#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <stdio.h>
#include <stdlib.h>

static const enum AVPixelFormat format = AV_PIX_FMT_YUV420P;
static const int height = 240;
static const int width = 320;
static const int new_height = 100;
static const int new_width = 200;

static AVFilterContext *buffersrc_ctx, *buffersink_ctx;
static AVFilterGraph *graph;
static AVFilterInOut *outputs, *inputs;

static void save_yuv_frame(AVFrame *frame, int frame_number)
{
    (void)frame_number;
    char filename[32];
    snprintf(filename, sizeof(filename), "frame.yuv");
    FILE *file = fopen(filename, "ab");

    uint32_t pitch_y = frame->linesize[0];
    uint32_t pitch_u = frame->linesize[1];
    uint32_t pitch_v = frame->linesize[2];

    uint8_t *y = frame->data[0];
    uint8_t *u = frame->data[1];
    uint8_t *v = frame->data[2];

    for (uint32_t i = 0; i < (uint32_t)frame->height; i++) {
        fwrite(y, frame->width, 1, file);
        y += pitch_y;
    }

    for (uint32_t i = 0; i < (uint32_t)frame->height/2; i++) {
        fwrite(u, frame->width/2, 1, file);
        u += pitch_u;
    }

    for (uint32_t i = 0; i < (uint32_t)frame->height/2; i++) {
        fwrite(v, frame->width/2, 1, file);
        v += pitch_v;
    }

    fclose(file);
}

static void fill_yuv_frame(AVFrame *frame, int frame_index, int width, int height)
{
    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            frame->data[0][y * frame->linesize[0] + x] = x + y + frame_index * 3;

    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + frame_index * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }
}

static int init_filters(const char *spec)
{
    int ret = 0;

    outputs = avfilter_inout_alloc();
    inputs = avfilter_inout_alloc();
    graph = avfilter_graph_alloc();

    if (!outputs || !inputs || !graph) {
        printf("Failed to allocate filter chain\n");
        goto end;
    }

    char args[512];
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             width, height, format, 1, 1, 1, 1);

    const AVFilter* buffersrc = avfilter_get_by_name("buffer");
    const AVFilter* buffersink = avfilter_get_by_name("buffersink");

    if ((ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
            args, NULL, graph)) < 0) {
        printf("Failed to create buffer source filter: %s\n", av_err2str(ret));
        goto end;
    }

    if ((ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, NULL, graph)) < 0) {
        printf("Failed to create buffer sink filter: %s\n", av_err2str(ret));
        goto end;
    }

    enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
    if ((ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                                   AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0) {
        printf("Failed to set pixel formats: %s\n", av_err2str(ret));
        goto end;
    }

    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;

    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;

    if ((ret = avfilter_graph_parse_ptr(graph, spec, &inputs, &outputs, NULL)) < 0) {
        printf("Could not parse filter chain '%s': %s\n", spec, av_err2str(ret));
        goto end;
    }

    if ((ret = avfilter_graph_config(graph, NULL)) < 0) {
        printf("Failed to configure graph: %s\n", av_err2str(ret));
        goto end;
    }

end:
    return ret;
}

static int apply_filters(AVFrame *frame)
{
    int ret = 0;
    AVFrame* filtered = NULL;

    if ((ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, 0)) < 0) {
        printf("Error feeding filter chain: %s\n", av_err2str(ret));
        goto end;
    }

    while (ret >= 0) {
        filtered = av_frame_alloc();
        if (!filtered) {
            ret = AVERROR(ENOMEM);
            break;
        }

        if ((ret = av_buffersink_get_frame(buffersink_ctx, filtered)) < 0) {
            // not an error
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                ret = 0;

            av_frame_free(&filtered);
            break;
        }

        // move filtered frame into frame, so caller can access it
        av_frame_unref(frame);
        av_frame_move_ref(frame, filtered);
        av_frame_free(&filtered);
    }
end:
    return ret;
}

int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc > 1 && argv[1]) {
        int level = atoi(argv[1]);
        av_log_set_level(level);
    }

    AVFrame* frame = NULL;
    for (int i = 0; i < 25; ++i) {
        frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_YUV420P;
        frame->width = width;
        frame->height = height;
        if ((ret = av_frame_get_buffer(frame, 32)) < 0) {
            printf("Failed to allocate frame buffer: %s\n", av_err2str(ret));
            goto end;
        }
        fill_yuv_frame(frame, i, width, height);

        char spec[128];
        snprintf(spec, sizeof(spec), "scale=w=%d:h=%d", new_width, new_height);
        if ((ret = init_filters(spec)) < 0)
            goto end;

        if ((ret = apply_filters(frame)) < 0)
            goto end;

        save_yuv_frame(frame, i);

        av_frame_free(&frame);
    }

    printf("ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s\n", av_get_pix_fmt_name(format), new_width, new_height, "frame.yuv");

end:
    av_frame_free(&frame);
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    avfilter_graph_free(&graph);

    return ret;
}
