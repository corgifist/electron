#ifndef video_reader_hpp
#define video_reader_hpp

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/pixdesc.h>
#include <libavutil/hwcontext.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libavutil/hwcontext_vulkan.h>
#include <inttypes.h>
}

#include <vector>
#include <iostream>
#include <string>

#include "../shared.h"

struct VideoReaderState {
    // Public things for other parts of the program to read from
    int width, height;
    AVRational time_base;

    // Private internal state
    AVFormatContext* av_format_ctx;
    AVCodecContext* av_codec_ctx;
    int video_stream_index;
    AVFrame* av_frame;
    AVFrame* hw_frame;
    AVPacket* av_packet;
    SwsContext* sws_scaler_ctx;

    AVBufferRef* hw_device_ctx;
    AVPixelFormat hw_pix_fmt;
    AVHWDeviceType device_type;

    bool useHardwareDecoding;

    VideoReaderState() {
        this->hw_device_ctx = nullptr;
        this->hw_pix_fmt = (AVPixelFormat) 0;
        this->device_type = AV_HWDEVICE_TYPE_NONE;
        this->sws_scaler_ctx = nullptr;
    }
};

bool video_reader_open(VideoReaderState* state, const char* filename, const char* deviceName, bool forceSoftwareDecoding = false);
bool video_reader_read_frame(VideoReaderState* state, uint8_t* image, int64_t* pts);
bool video_reader_seek_frame(VideoReaderState* state, int64_t ts);
void video_reader_close(VideoReaderState* state);

#endif
