/* ChromascopeIP M12.5: actual target libavcodec inventory (no media input).
 * Compile on each supported distribution with its installed FFmpeg headers.
 * Does not establish exploitability or substitute for patched-OpenCV tests.
 */
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <stdio.h>
#include <string.h>

static int has_hardware_decoder(void) {
    void *opaque = NULL;
    const AVCodec *codec;
    while ((codec = av_codec_iterate(&opaque)) != NULL) {
        if (av_codec_is_decoder(codec) && codec->name &&
            (strstr(codec->name, "_cuvid") || strstr(codec->name, "_nvdec")))
            return 1;
    }
    return 0;
}

static int jpegxl_parser_present(void) {
    void *opaque = NULL;
    const AVCodecParser *parser;
    while ((parser = av_parser_iterate(&opaque)) != NULL) {
        for (int i = 0; i < 7; ++i) {
            if (parser->codec_ids[i] == AV_CODEC_ID_JPEGXL)
                return 1;
        }
    }
    return 0;
}

/* Newer FFmpeg releases distinguish animated JPEG XL with a separate
 * AV_CODEC_ID_JPEGXL_ANIM; older distribution headers do not define it.
 * Keep the stable still-image codec ID test and also reject the registered
 * animated decoder by name so one source builds across supported versions.
 */
static int jpegxl_decoder_present(void) {
    void *opaque = NULL;
    const AVCodec *codec;
    while ((codec = av_codec_iterate(&opaque)) != NULL) {
        if (av_codec_is_decoder(codec) &&
            (codec->id == AV_CODEC_ID_JPEGXL ||
             (codec->name &&
              (strcmp(codec->name, "libjxl_anim") == 0 ||
               strcmp(codec->name, "jpegxl_anim") == 0))))
            return 1;
    }
    return 0;
}

int main(void) {
    const char *config = avcodec_configuration();
    const int lcevc = config && strstr(config, "--enable-liblcevc-dec") != NULL;
    printf("libavcodec_version=%u.%u.%u\n",
           avcodec_version() >> 16, (avcodec_version() >> 8) & 255,
           avcodec_version() & 255);
    printf("libavformat_version=%u.%u.%u\n",
           avformat_version() >> 16, (avformat_version() >> 8) & 255,
           avformat_version() & 255);
    printf("jpegxl_parser_compiled=%s\n", jpegxl_parser_present() ? "yes" : "no");
    printf("jpegxl_decoder_compiled=%s\n", jpegxl_decoder_present() ? "yes" : "no");
    printf("lcevc_external_decoder_enabled=%s\n", lcevc ? "yes" : "no");
    printf("nvdec_named_decoders_present=%s\n", has_hardware_decoder() ? "yes" : "no");
    if (!config || lcevc) {
        fputs("UNAPPROVED: FFmpeg runtime lacks usable configuration or enables LCEVC\n", stderr);
        return 1;
    }
    return 0;
}
