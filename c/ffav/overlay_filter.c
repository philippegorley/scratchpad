#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/pixdesc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/**
 * Complex video filter example
 *
 * Downsizes first input and overlays it over the second input
 */

#define FRAME_WIDTH 1280
#define FRAME_HEIGHT 720
#define FRAME_FORMAT AV_PIX_FMT_YUV420P
#define FRAME_COUNT 100
#define OUTPUT_FILE "output.yuv"

typedef struct FilteringContext {
    const char *desc;
    int initialized;
    int failed;
    AVFilterGraph *graph;
    int nb_inputs;
    AVFilterContext **inputs;
    int nb_outputs;
    AVFilterContext **outputs;
} FilteringContext;

static void save_yuv_frame(AVFrame *frame)
{
    char filename[32];
    snprintf(filename, sizeof(filename), OUTPUT_FILE);
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

static AVFrame *get_dummy_frame(int width, int height, int frame_index, int value)
{
    if (frame_index >= FRAME_COUNT)
        return NULL;

    int ret = 0;
    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return NULL;

    frame->format = FRAME_FORMAT;
    frame->width = width;
    frame->height = height;

    if ((ret = av_frame_get_buffer(frame, 0)) < 0) {
        printf("Failed to allocate frame buffer: %s\n", av_err2str(ret));
        av_frame_free(&frame);
        return NULL;
    }

    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
            frame->data[0][y * frame->linesize[0] + x] = x + y + frame_index * 3;

    for (int y = 0; y < height / 2; ++y) {
        for (int x = 0; x < width / 2; ++x) {
            frame->data[1][y * frame->linesize[1] + x] = (128 + y + frame_index) * value * 2;
            frame->data[2][y * frame->linesize[2] + x] = (64 + x + frame_index) * value * 5;
        }
    }

    return frame;
}

AVFrame *read_input_pad(FilteringContext *fc, AVFilterContext *fctx)
{
    // need info to init filters, return a frame that has this info set
    (void)fc;
    (void)fctx;
    return get_dummy_frame(FRAME_WIDTH, FRAME_HEIGHT, 0, 0);
}

static int init_input_filter(FilteringContext *fc, AVFilterInOut *in)
{
    int ret = 0;
    AVFrame *frame = read_input_pad(fc, in->filter_ctx);
    if (!frame)
        return AVERROR(EAGAIN);

    AVBufferSrcParameters *params = av_buffersrc_parameters_alloc();
    if (!params)
        return AVERROR(ENOMEM);

    char *filter_name = NULL;
    enum AVMediaType type = avfilter_pad_get_type(in->filter_ctx->input_pads, in->pad_idx);
    if (type == AVMEDIA_TYPE_VIDEO) {
        params->format = frame->format;
        params->time_base = AV_TIME_BASE_Q;
        params->width = frame->width;
        params->height = frame->height;
        params->sample_aspect_ratio = frame->sample_aspect_ratio;
        params->frame_rate.num = 25;
        params->frame_rate.den = 1;
        filter_name = "buffer";
    } else if (type == AVMEDIA_TYPE_AUDIO) {
        params->format = AV_SAMPLE_FMT_S16;
        params->time_base = AV_TIME_BASE_Q;
        params->sample_rate = frame->sample_rate;
        params->channel_layout = frame->channel_layout;
        filter_name = "abuffer";
    } else {
        printf("Only video and audio filters are supported\n");
        return AVERROR(EINVAL);
    }

    const AVFilter *filter = avfilter_get_by_name(filter_name);
    AVFilterContext *buffersrc_ctx = NULL;
    if (filter) {
        char name[128];
        snprintf(name, sizeof(name), "buffersrc_%s_%d", in->name, in->pad_idx);
        buffersrc_ctx = avfilter_graph_alloc_filter(fc->graph, filter, name);
    }
    if (!buffersrc_ctx) {
        printf("Failed to allocate buffer source filter\n");
        av_free(params);
        return -1;
    }
    ret = av_buffersrc_parameters_set(buffersrc_ctx, params);
    av_free(params);
    if (ret < 0)
        return ret;

    if ((ret = avfilter_init_str(buffersrc_ctx, NULL)) < 0) {
        printf("Failed to initialize buffer source: %s\n", av_err2str(ret));
        return ret;
    }

    if ((ret = avfilter_link(buffersrc_ctx, 0, in->filter_ctx, in->pad_idx)) < 0) {
        printf("Failed to link buffer source to graph: %s\n", av_err2str(ret));
        return ret;
    }

    ++fc->nb_inputs;
    fc->inputs = av_realloc_array(fc->inputs, fc->nb_inputs, sizeof(AVFilterContext*));
    fc->inputs[fc->nb_inputs - 1] = buffersrc_ctx;

    return ret;
}

static int init_output_filter(FilteringContext *fc, AVFilterInOut *out)
{
    int ret = 0;
    const AVFilter *buffersink;
    AVFilterContext *buffersink_ctx = NULL;
    enum AVMediaType type = avfilter_pad_get_type(out->filter_ctx->input_pads, out->pad_idx);

    if (type == AVMEDIA_TYPE_VIDEO) {
        buffersink = avfilter_get_by_name("buffersink");
    } else if (type == AVMEDIA_TYPE_AUDIO) {
        buffersink = avfilter_get_by_name("abuffersink");
    } else {
        printf("Only video and audio filters are supported\n");
        return AVERROR(EINVAL);
    }

    if ((ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
            NULL, NULL, fc->graph)) < 0) {
        printf("Failed to create buffer sink filter: %s\n", av_err2str(ret));
        return ret;
    }

    if ((ret = avfilter_link(out->filter_ctx, out->pad_idx, buffersink_ctx, 0)) < 0) {
        printf("Could not link buffer source to graph: %s\n", av_err2str(ret));
        return ret;
    }

    ++fc->nb_outputs;
    fc->outputs = av_realloc_array(fc->outputs, fc->nb_outputs, sizeof(AVFilterContext*));
    fc->outputs[fc->nb_outputs - 1] = buffersink_ctx;

    return ret;
}

static int init_graph(FilteringContext *fc)
{
    int ret = 0;
    AVFilterInOut *inputs = NULL;
    AVFilterInOut *outputs = NULL;

    fc->graph = avfilter_graph_alloc();
    if (!fc->graph)
        return -1;

    printf("Parsing: %s\n", fc->desc);
    if ((ret = avfilter_graph_parse2(fc->graph, fc->desc, &inputs, &outputs)) < 0) {
        printf("Failed to parse filter graph: %s\n", av_err2str(ret));
        fc->failed = 1;
        goto end;
    }

    for (AVFilterInOut *current = outputs; current; current = current->next) {
        if ((ret = init_output_filter(fc, current)) < 0) {
            printf("Failed to create output for graph: %s\n", av_err2str(ret));
            goto end;
        }
    }

    for (AVFilterInOut *current = inputs; current; current = current->next) {
        if ((ret = init_input_filter(fc, current)) < 0) {
            printf("Failed to create input for graph: %s\n", av_err2str(ret));
            fc->failed = 1;
            goto end;
        }
    }

    if ((ret = avfilter_graph_config(fc->graph, NULL)) < 0) {
        printf("Failed to configure filter chain: %s\n", av_err2str(ret));
        fc->failed = 1;
        goto end;
    }

    fc->initialized = 1;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

static int read_output(FilteringContext *fc)
{
    int ret = 0;
    for (int i = 0; i < fc->nb_outputs; ++i) {
        AVFrame *frame = av_frame_alloc();
        ret = av_buffersink_get_frame_flags(fc->outputs[i], frame, 0);
        if (ret >= 0) {
            save_yuv_frame(frame);
        } else if (ret == AVERROR(EAGAIN)) {
            printf("No frame available in sink\n");
            ret = 0;
        } else if (ret == AVERROR_EOF) {
            printf("EOF received\n");
        } else {
            printf("Error occurred while pulling frame from filters: %s\n", av_err2str(ret));
            fc->failed = 1;
        }
        av_frame_free(&frame);
    }
    return ret;
}

static int feed_input(FilteringContext *fc, int eof, int frame_index)
{
    int ret = 0;
    for (int i = 0; i < fc->nb_inputs; ++i) {
        int requested = av_buffersrc_get_nb_failed_requests(fc->inputs[i]);
        if (requested > 0)
            read_input_pad(fc, fc->inputs[i]);

        AVFrame *frame = NULL;
        if (!eof)
            frame = get_dummy_frame(FRAME_WIDTH, FRAME_HEIGHT, frame_index, i);
        if ((ret = av_buffersrc_add_frame(fc->inputs[i], frame)) < 0) {
            printf("Could not pass frame to filter chain: %s\n", av_err2str(ret));
            return ret;
        }
    }
    return ret;
}

static void process(FilteringContext *fc)
{
    if (!fc->initialized)
        init_graph(fc);

    int frame_index = 0;
    while (fc->initialized) {
        int o = read_output(fc);
        int i = feed_input(fc, 0, frame_index);
        if ((o < 0 && i < 0) || o == AVERROR_EOF)
            break;
        ++frame_index;
    }

    if (!fc->failed) {
        feed_input(fc, 1, frame_index);
    }
}

int main(int argc, char *argv[])
{
    int ret = 0;
    FilteringContext *fc = NULL;
    const char *filterspec = "[in1] scale=iw/4:ih/4 [mid1]; [in2] [mid1] overlay=main_w-overlay_w-10:main_h-overlay_h-10:shortest=1 [out1]";

    if (argc > 1 && argv[1]) {
        int level = atoi(argv[1]);
        av_log_set_level(level);
    }

    unlink(OUTPUT_FILE);

    fc = av_mallocz(sizeof(*fc));
    if (!fc)
        goto end;
    fc->desc = av_strdup(filterspec);

    process(fc);

    printf("Play the output file with the command:\nffplay -f rawvideo -pixel_format %s -video_size %dx%d %s\n",
           av_get_pix_fmt_name(FRAME_FORMAT), FRAME_WIDTH, FRAME_HEIGHT, OUTPUT_FILE);
end:
    if (fc) {
        avfilter_graph_free(&fc->graph);
        av_free(fc->inputs);
        av_free(fc->outputs);
        av_freep(&fc->desc);
    }
    av_free(fc);

    return (ret < 0 ? 1 : 0);
}
