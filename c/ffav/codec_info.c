#include <stdio.h>
#include <libavcodec/avcodec.h>

#define PRINT_PROP(p, d) do {   \
    if (desc->props & p)    \
        printf("%s\n", d);\
} while (0)

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("Prints information about a given ffmpeg codec\nUsage: %s [CODEC_NAME]\n", argv[0]);
        return -1;
    }

    const char *codec_name = argv[1];
    const AVCodecDescriptor *desc = avcodec_descriptor_get_by_name(codec_name);
    if (!desc) {
        printf("No codec found\n");
        return -2;
    }

    printf("%s\nId=%d\nType=%s\n\n", desc->long_name, desc->id, av_get_media_type_string(desc->type));

    if (desc->props)
        printf("Properties:\n");
    PRINT_PROP(AV_CODEC_PROP_INTRA_ONLY, "Codec uses only intra compression");
    PRINT_PROP(AV_CODEC_PROP_LOSSY, "Codec supports lossy compression");
    PRINT_PROP(AV_CODEC_PROP_LOSSLESS, "Codec supports lossless compression");
    PRINT_PROP(AV_CODEC_PROP_REORDER, "Codec supports frame reordering");
    PRINT_PROP(AV_CODEC_PROP_BITMAP_SUB, "Subtitle codec is bitmap based (AVSubtitleRect->pict field)");
    PRINT_PROP(AV_CODEC_PROP_TEXT_SUB, "Subtitle codec is text based (AVSubtitleRect->ass)");
    if (desc->props)
        printf("\n");

    if (desc->profiles) {
        printf("Profiles:\n");
        for (int i = 0; desc->profiles[i].profile != FF_PROFILE_UNKNOWN; ++i) {
            const char *profile_name = avcodec_profile_name(desc->id, desc->profiles[i].profile);
            printf("%d\t %s\n", i, profile_name);
        }
        printf("\n");
    }

    return 0;
}
