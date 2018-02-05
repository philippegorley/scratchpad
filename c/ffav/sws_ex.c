#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/frame.h>
#include <stdio.h>

// try to recreate crash from http://trac.ffmpeg.org/ticket/5356
int main(int argc, char **argv)
{
    struct SwsContext*  ctx;
    int   srcH = 800;
    int   srcW = 1280;
    enum AVPixelFormat   srcFMT = AV_PIX_FMT_NV12;
    enum AVPixelFormat   dstFMT = AV_PIX_FMT_YUV420P;
    int padding = 0;
    //int padding = 1;

    uint8_t* rgbSrc = (uint8_t*)malloc(srcH*srcW*3 + padding);
    memset(rgbSrc, 255, srcW*srcH*3);

    printf("Addr=0x%p size=%d\n", rgbSrc, srcW*srcH*3);
    ctx = sws_getContext(srcW,
                        srcH,
                        srcFMT,
                        srcW,
                        srcH,
                        dstFMT,
                        SWS_FAST_BILINEAR,
                        NULL,
                        NULL,
                        NULL);

    AVFrame* srcFrame = av_frame_alloc();
    AVFrame* dstFrame = av_frame_alloc();

    int outputBufferSize = av_image_get_buffer_size(dstFMT,
                               srcW,
                               srcH,
                               1);
    uint8_t* buffer = (uint8_t*)av_malloc(outputBufferSize);

    av_image_fill_arrays(dstFrame->data,
                   dstFrame->linesize,
                   buffer,
                   dstFMT,
                   srcW,
                   srcH,
                   1);

    av_image_fill_arrays(srcFrame->data,
                   srcFrame->linesize,
                   rgbSrc,
                   srcFMT,
                   srcW,
                   srcH,
                   1 );

    srcFrame->width = srcW;
    srcFrame->height = srcH;
    srcFrame->format = srcFMT;

    sws_scale(ctx,
              (const uint8_t* const*)srcFrame,
              srcFrame->linesize,
              0,
              srcH,
              dstFrame->data,
              dstFrame->linesize);

    return 0;
}
