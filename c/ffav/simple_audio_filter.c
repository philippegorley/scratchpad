#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/md5.h>
#include <libavutil/opt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const enum AVSampleFormat format = AV_SAMPLE_FMT_S16;
static const int nb_samples = 1024;
static const int64_t channel_layout = AV_CH_LAYOUT_STEREO;
static const int sample_rate = 44100;

static AVFilterContext *buffersrc_ctx, *buffersink_ctx;
static AVFilterGraph *graph;
static AVFilterInOut *outputs, *inputs;

static void fill_samples(uint16_t* samples, int sampleRate, int nbSamples, int nbChannels, float tone)
{
    const float pi = 3.14159265358979323846264338327950288;
    const float tincr = 2 * pi * tone / sampleRate;
    float t = 0;

    for (int i = 0; i < 200; ++i) {
        for (int j = 0; j < nbSamples; ++j) {
            samples[2 * j] = (int)(sin(t) * 10000);
            for (int k = 1; k < nbChannels; ++k) {
                samples[2 * j + k] = samples[2 * j];
            }
            t += tincr;
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
             "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%"PRIx64,
             1, 1, sample_rate, av_get_sample_fmt_name(format), channel_layout);

    const AVFilter* buffersrc = avfilter_get_by_name("abuffer");
    const AVFilter* buffersink = avfilter_get_by_name("abuffersink");

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

//    const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, -1 };
//    if ((ret = av_opt_set_int_list(buffersink_ctx, "sample_fmts", sample_fmts,
//                                   AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0) {
//        printf("Failed to set sample formats: %s\n", av_err2str(ret));
//        goto end;
//    }
//
//    const int64_t channel_layouts[] = { AV_CH_LAYOUT_MONO, -1 };
//    if ((ret = av_opt_set_int_list(buffersink_ctx, "channel_layouts", channel_layouts,
//                                   -1, AV_OPT_SEARCH_CHILDREN)) < 0) {
//        printf("Failed to set channel layouts: %s\n", av_err2str(ret));
//        goto end;
//    }
//
//    const int sample_rates[] = { 8000, -1 };
//    if ((ret = av_opt_set_int_list(buffersink_ctx, "sample_rates", sample_rates,
//                                   -1, AV_OPT_SEARCH_CHILDREN)) < 0) {
//        printf("Failed to set sample rates: %s\n", av_err2str(ret));
//        goto end;
//    }

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
        //av_frame_free(&filtered);
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

    AVFrame* frame = av_frame_alloc();
    frame->format = format;
    frame->channel_layout = channel_layout;
    frame->nb_samples = nb_samples;
    frame->sample_rate = sample_rate;
    frame->channels = av_get_channel_layout_nb_channels(channel_layout);
    if ((ret = av_frame_get_buffer(frame, 0)) < 0) {
        printf("Failed to allocate frame buffer: %s\n", av_err2str(ret));
        goto end;
    }
    fill_samples((uint16_t*)frame->data[0], sample_rate, nb_samples, frame->channels, 440.0);

    char spec[64];
    snprintf(spec, sizeof(spec), "atrim=start_sample=128");
    if ((ret = init_filters(spec)) < 0)
        goto end;

    if ((ret = apply_filters(frame)) < 0)
        goto end;

    printf("Trimmed from %d samples to %d\n", nb_samples, frame->nb_samples);

end:
    //av_frame_free(&frame);
    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);
    avfilter_graph_free(&graph);

    return ret;
}
