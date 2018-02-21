#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <stdarg.h>
#include <stdio.h>

// Can't scale unless format is software
// Can't hardware encode unless format is hardware

static AVFormatContext *ifmt_ctx = NULL;
static AVFormatContext *ofmt_ctx = NULL;
static AVCodecContext *dec_ctx = NULL;
static AVCodecContext *enc_ctx = NULL;
static AVBufferRef *hw_device_ctx = NULL;
static struct SwsContext *sws_ctx = NULL;

static int setup_hw()
{
    AVBufferRef *hw_frames_ref;
    AVHWFramesContext *frames_ctx = NULL;
    int ret = 0;

    if ((ret = av_hwdevice_ctx_create(&hw_device_ctx, AV_HWDEVICE_TYPE_VAAPI, NULL, NULL, 0)) < 0) {
        fprintf(stderr, "Failed to create VAAPI device\n");
        return ret;
    }

    if (!(hw_frames_ref = av_hwframe_ctx_alloc(hw_device_ctx))) {
        fprintf(stderr, "Failed to create VAAPI frame context\n");
        return -1;
    }
    frames_ctx = (AVHWFramesContext*)(hw_frames_ref->data);
    frames_ctx->format = AV_PIX_FMT_VAAPI;
    frames_ctx->sw_format = AV_PIX_FMT_NV12;
    frames_ctx->width = enc_ctx->width;
    frames_ctx->height = enc_ctx->height;
    frames_ctx->initial_pool_size = 20;
    if ((ret = av_hwframe_ctx_init(hw_frames_ref)) < 0) {
        fprintf(stderr, "Failed to initialize VAAPI frame context\n");
        av_buffer_unref(&hw_frames_ref);
        return ret;
    }

    enc_ctx->hw_frames_ctx = av_buffer_ref(hw_frames_ref);
    if (!enc_ctx->hw_frames_ctx)
        ret = AVERROR(ENOMEM);

    av_buffer_unref(&hw_frames_ref);
    return ret;
}

static int open_input_file(const char *filename)
{
    int ret = 0;

    if ((ret = avformat_open_input(&ifmt_ctx, filename, NULL, NULL)) < 0) {
        fprintf(stderr, "Failed to open input\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Could not find stream info\n");
        return ret;
    }

    AVCodec *dec = NULL;
    int idx = av_find_best_stream(ifmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (idx < 0) {
        fprintf(stderr, "No video stream found\n");
        return idx;
    }

    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx) {
        fprintf(stderr, "Failed to allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    if ((ret = avcodec_parameters_to_context(dec_ctx, ifmt_ctx->streams[idx]->codecpar)) < 0) {
        fprintf(stderr, "Could not copy parameters to decoder context\n");
        return ret;
    }

    dec_ctx->framerate = av_guess_frame_rate(ifmt_ctx, ifmt_ctx->streams[idx], NULL);
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        fprintf(stderr, "Failed to open decoder\n");
        return ret;
    }

    av_dump_format(ifmt_ctx, idx, filename, 0);
    return 0;
}

static int open_output_file(const char *filename)
{
    int ret = 0;

    // Force mp4 to use h264_vaapi encoder
    if ((ret = avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", filename)) < 0) {
        fprintf(stderr, "Failed to allocate output context\n");
        return ret;
    }

    AVCodec *enc = avcodec_find_encoder_by_name("h264_vaapi");
    if (!enc) {
        fprintf(stderr, "H264 (VAAPI) encoder not found\n");
        return AVERROR_INVALIDDATA;
    }

    AVStream *out_st = avformat_new_stream(ofmt_ctx, enc);
    if (!out_st) {
        fprintf(stderr, "Failed to allocate output stream\n");
        return AVERROR(ENOMEM);
    }

    enc_ctx = avcodec_alloc_context3(enc);
    if (!enc_ctx) {
        fprintf(stderr, "Failed to allocate encoder context\n");
        return AVERROR(ENOMEM);
    }

    enc_ctx->height = dec_ctx->height;
    enc_ctx->width = dec_ctx->width;
    enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
    enc_ctx->pix_fmt = AV_PIX_FMT_VAAPI;
    //enc_ctx->pix_fmt = AV_PIX_FMT_NV12;
    setup_hw();

    if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if ((ret = avcodec_open2(enc_ctx, enc, NULL)) < 0) {
        fprintf(stderr, "Failed to open encoder\n");
        return ret;
    }

    if ((ret = avcodec_parameters_from_context(out_st->codecpar, enc_ctx)) < 0) {
        fprintf(stderr, "Could not copy parameters to output stream\n");
        return ret;
    }

    out_st->time_base = enc_ctx->time_base;

    av_dump_format(ofmt_ctx, 0, filename, 1);

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE)) < 0) {
            fprintf(stderr, "Could not open output file '%s'", filename);
            return ret;
        }
    }

    if ((ret = avformat_write_header(ofmt_ctx, NULL)) < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        return ret;
    }

    return 0;
}

static int scale_and_encode(AVFrame *input, AVFrame *output)
{
    int ret = 0;

    sws_ctx = sws_getCachedContext(sws_ctx,
                                   dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
                                   enc_ctx->width, enc_ctx->height, enc_ctx->pix_fmt,
                                   SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws_ctx) {
        fprintf(stderr, "Can't create scaler context for fmt:%s s:%dx%d -> fmt:%s s:%dx%d\n",
                av_get_pix_fmt_name(dec_ctx->pix_fmt), dec_ctx->width, dec_ctx->height,
                av_get_pix_fmt_name(enc_ctx->pix_fmt), enc_ctx->width, enc_ctx->height);
        return AVERROR(EINVAL);
    }

    sws_scale(sws_ctx, (const uint8_t *const)input->data, (const int)input->linesize, 0,
              input->height, output->data, output->linesize);

    return 0;
}

/**
 * Grab frame from input file
 * Software convert to NV12 format
 * Hardware encode the scaled frame
 */
int main(int argc, char *argv[])
{
    int ret = 0;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n"
                "Example to show how to convert formats in software and hardware encode\n", argv[0]);
        exit(0);
    }

    const char *input_file = argv[1];
    const char *output_file = argv[2];

    av_register_all();
    avcodec_register_all();
    avdevice_register_all();
    av_log_set_level(AV_LOG_VERBOSE);

    if (open_input_file(input_file) < 0)
        goto end;
    if (open_output_file(output_file) < 0)
        goto end;

    AVPacket pkt = { .data = NULL, .size = 0 };
    AVFrame *in_frame, *out_frame;
    while ((ret = av_read_frame(ifmt_ctx, &pkt)) >= 0) {
        int idx = pkt.stream_index;
        int got_frame;
        in_frame = av_frame_alloc();
        out_frame = av_frame_alloc();
        if (!in_frame || !out_frame) {
            fprintf(stderr, "Failed to allocate frame\n");
            ret = AVERROR(ENOMEM);
            goto end;
        }
        av_packet_rescale_ts(&pkt, ifmt_ctx->streams[idx]->time_base, dec_ctx->time_base);
        if ((ret = avcodec_decode_video2(dec_ctx, in_frame, &got_frame, &pkt)) < 0) {
            goto end;
        }

        if (got_frame) {
            if ((ret = scale_and_encode(in_frame, out_frame)) < 0) {
                goto end;
            }
        } else {
            av_frame_free(&in_frame);
        }
    }

end:
    avcodec_free_context(&enc_ctx);
    avcodec_free_context(&dec_ctx);
    av_buffer_unref(&hw_device_ctx);
    avformat_close_input(&ifmt_ctx);
    avformat_free_context(ifmt_ctx);
    if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);
    sws_freeContext(sws_ctx);

    if (ret < 0)
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}
