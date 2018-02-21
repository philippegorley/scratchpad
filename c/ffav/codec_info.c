#include <libavcodec/avcodec.h>
#include <stdio.h>

void show_caps(const AVCodec *codec)
{
    /**
     * AV_CODEC_CAP_FRAME_THREADS
     * AV_CODEC_CAP_SLICE_THREADS
     * AV_CODEC_CAP_PARAM_CHANGE?
     * AV_CODEC_CAP_LOSSLESS
     * AV_CODEC_CAP_HARDWARE
     * AV_CODEC_CAP_HYBRID
     */
    if (codec->capabilities & AV_CODEC_CAP_HARDWARE || codec->capabilities & AV_CODEC_CAP_HYBRID) {
        // May be HWAccel, check avcodec_get_hw_config
    }
}

void show_profiles(enum AVCodecID id, const AVProfile *profiles)
{
    if (profiles) {
        printf("Available profiles:\n");
        for (int i = 0; profiles[i].profile != FF_PROFILE_UNKNOWN; ++i) {
            const char *profile_name = avcodec_profile_name(id, profiles[i].profile);
            printf("%d\t %s\n", i, profile_name);
        }
    }
}

void show_codec(AVCodec *codec, int is_encoder)
{
    printf("Found %s: %s (%s)\n", (is_encoder ? "encoder" : "decoder"), codec->long_name, codec->name);
    if (codec->wrapper_name) {
        printf("Codec is backed by the %s library\n", codec->wrapper_name);
    }
    /**
     * More things to print:
     * Is it a wrapper?
     * Supported pixel/sample formats
     */
    show_profiles(codec->id, codec->profiles);
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Usage: %s [CODEC_NAME]\n", argv[0]);
        return -1;
    }

    avcodec_register_all();

    const char *codec_name = argv[1];
    AVCodec *enc = avcodec_find_encoder_by_name(codec_name);
    if (enc)
        show_codec(enc, 1);
    AVCodec *dec = avcodec_find_decoder_by_name(codec_name);
    if (dec)
        show_codec(dec, 0);

    return 0;
}
