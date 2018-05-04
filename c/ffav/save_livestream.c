/*
 * Copyright (c) 2013 Stefano Sabatini
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/**
 * @file
 * libavformat/libavcodec demuxing and muxing API example.
 *
 * Remux streams from one container format to another.
 * @example remuxing.c
 */

#include <libavutil/time.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <signal.h>

static AVFormatContext *ifmt1_ctx = NULL, *ifmt2_ctx = NULL, *ofmt_ctx = NULL;
static int *stream_mapping = NULL;
static int stream_mapping_size = 0;
static int stream_index = 0;
static int nb_pkts = 0;

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("%s (%d): pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
           tag, nb_pkts,
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index);
}

static int open_input(AVFormatContext *ifmt_ctx, const char *in_filename)
{
    int ret;
    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        fprintf(stderr, "Could not open input file '%s'", in_filename);
        return ret;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        fprintf(stderr, "Failed to retrieve input stream information");
        return ret;
    }

    av_dump_format(ifmt_ctx, 0, in_filename, 0);
}

static int create_streams(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx)
{
    int i, ret = 0;
    for (i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *out_stream;
        AVStream *in_stream = ifmt_ctx->streams[i];
        AVCodecParameters *in_codecpar = in_stream->codecpar;

        if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
            in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
            stream_mapping[i] = -1;
            continue;
        }

        stream_mapping[i] = stream_index++;

        out_stream = avformat_new_stream(ofmt_ctx, NULL);
        if (!out_stream) {
            fprintf(stderr, "Failed allocating output stream\n");
            ret = AVERROR_UNKNOWN;
            return ret;
        }

        ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
        if (ret < 0) {
            fprintf(stderr, "Failed to copy codec parameters\n");
            return ret;
        }
        out_stream->codecpar->codec_tag = 0;
    }

    return ret;
}

static int remux_pkt(AVFormatContext *ifmt_ctx, AVFormatContext *ofmt_ctx)
{
    int ret;
    AVStream *in_stream, *out_stream;
    AVPacket pkt;

    ret = av_read_frame(ifmt_ctx, &pkt);
    if (ret < 0)
        return ret;

    in_stream  = ifmt_ctx->streams[pkt.stream_index];
    if (pkt.stream_index >= stream_mapping_size ||
        stream_mapping[pkt.stream_index] < 0) {
        av_packet_unref(&pkt);
        return 0;
    }

    pkt.stream_index = stream_mapping[pkt.stream_index];
    out_stream = ofmt_ctx->streams[pkt.stream_index];
    log_packet(ifmt_ctx, &pkt, "in");

    /* copy packet */
    pkt.pts = av_rescale_q_rnd(av_gettime(), in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt.dts = av_rescale_q_rnd(av_gettime(), in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
    pkt.pos = -1;
    log_packet(ofmt_ctx, &pkt, "out");

    ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
    if (ret < 0) {
        fprintf(stderr, "Error muxing packet\n");
        return ret;
    }
    av_packet_unref(&pkt);
}

int main(int argc, char **argv)
{
    AVOutputFormat *ofmt = NULL;
    const char *in1_filename, *in2_filename, *out_filename;
    int ret, i;

    if (argc < 4) {
        printf("usage: %s input input output\n"
               "API example program to remux 2 RTP streams with libavformat and libavcodec.\n"
               "The output format is guessed according to the file extension.\n"
               , argv[0]);
        return 1;
    }

    in1_filename = argv[1];
    in2_filename = argv[2];
    out_filename = argv[3];

    if ((ret = open_input(ifmt1_ctx, in1_filename)) < 0)
        goto end;

    if ((ret = open_input(ifmt2_ctx, in2_filename)) < 0)
        goto end;

    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
        fprintf(stderr, "Could not create output context\n");
        ret = AVERROR_UNKNOWN;
        goto end;
    }

    stream_mapping_size = ifmt1_ctx->nb_streams + ifmt2_ctx->nb_streams;
    stream_mapping = av_mallocz_array(stream_mapping_size, sizeof(*stream_mapping));
    if (!stream_mapping) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    ofmt = ofmt_ctx->oformat;

    if ((ret = create_streams(ifmt1_ctx, ofmt_ctx)) < 0)
        goto end;

    if ((ret = create_streams(ifmt2_ctx, ofmt_ctx)) < 0)
        goto end;

    av_dump_format(ofmt_ctx, 0, out_filename, 1);

    if (!(ofmt->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            fprintf(stderr, "Could not open output file '%s'", out_filename);
            goto end;
        }
    }

    ret = avformat_write_header(ofmt_ctx, NULL);
    if (ret < 0) {
        fprintf(stderr, "Error occurred when opening output file\n");
        goto end;
    }

    while (1) {
        if ((ret = remux_pkt(ifmt1_ctx, ofmt_ctx)) < 0)
            break;
        if ((ret = remux_pkt(ifmt2_ctx, ofmt_ctx)) < 0)
            break;
        if (nb_pkts++ > 300)
            break;
    }

    av_write_trailer(ofmt_ctx);

end:
    /* close inputs */
    avformat_close_input(&ifmt1_ctx);
    avformat_close_input(&ifmt2_ctx);

    /* close output */
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
        avio_closep(&ofmt_ctx->pb);
    avformat_free_context(ofmt_ctx);

    av_freep(&stream_mapping);

    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        return 1;
    }

    return 0;
}
