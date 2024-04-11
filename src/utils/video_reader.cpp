#include "video_reader.hpp"

static AVPixelFormat cached_hw_pix_fmt = AV_PIX_FMT_NONE;

static const char* av_make_error(int errnum) {
    static char str[AV_ERROR_MAX_STRING_SIZE];
    memset(str, 0, sizeof(str));
    return av_make_error_string(str, AV_ERROR_MAX_STRING_SIZE, errnum);
}

static AVPixelFormat correct_for_deprecated_pixel_format(AVPixelFormat pix_fmt) {
    switch (pix_fmt) {
        case AV_PIX_FMT_YUVJ420P: return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUVJ422P: return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUVJ444P: return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_YUVJ440P: return AV_PIX_FMT_YUV440P;
        default:                  return pix_fmt;
    }
}

static AVHWDeviceType lastSuccessfullDeviceType = AV_HWDEVICE_TYPE_NONE;

static std::vector<AVHWDeviceType> availableDeviceTypes = {};
static int hw_decoder_init(VideoReaderState* state, AVCodecContext *ctx, const enum AVHWDeviceType type, const char* deviceName)
{
    auto& hw_device_ctx = state->hw_device_ctx;
    auto& hw_pix_fmt = state->hw_pix_fmt;
    auto& device_type = state->device_type;
    int err = 0;

    if (type != AV_HWDEVICE_TYPE_VULKAN) deviceName = nullptr;

    if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
                                      deviceName, NULL, 0)) < 0) {
        return err;
    }
    device_type = type;
    ctx->hw_device_ctx = av_buffer_ref(state->hw_device_ctx);

    return err;
}

static enum AVPixelFormat get_hw_format(AVCodecContext *ctx,
                                        const enum AVPixelFormat *pix_fmts)
{
    const enum AVPixelFormat *p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == cached_hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}



bool video_reader_open(VideoReaderState* state, const char* filename, const char* deviceName, bool forceSoftwareDecoding) {
    printf("force software decoding: %i\n", (int) forceSoftwareDecoding);
    // Unpack members of state
    auto& width = state->width;
    auto& height = state->height;
    auto& time_base = state->time_base;
    auto& av_format_ctx = state->av_format_ctx;
    auto& av_codec_ctx = state->av_codec_ctx;
    auto& video_stream_index = state->video_stream_index;
    auto& av_frame = state->av_frame;
    auto& av_packet = state->av_packet;
    auto& hw_frame = state->hw_frame;
    auto& hw_device_ctx = state->hw_device_ctx;
    auto& hw_pix_fmt = state->hw_pix_fmt;
    auto& device_type = state->device_type;

    state->useHardwareDecoding = !forceSoftwareDecoding;

    // Open the file using libavformat
    av_format_ctx = avformat_alloc_context();
    if (!av_format_ctx) {
        printf("Couldn't created AVFormatContext\n");
        return false;
    }

    if (avformat_open_input(&av_format_ctx, filename, NULL, NULL) != 0) {
        printf("Couldn't open video file\n");
        return false;
    }

    avformat_find_stream_info(av_format_ctx, NULL);

    if (availableDeviceTypes.size() == 0) {
        AVHWDeviceType tmpType = AV_HWDEVICE_TYPE_NONE;
        while ((tmpType = av_hwdevice_iterate_types(tmpType)) != AV_HWDEVICE_TYPE_NONE) 
            availableDeviceTypes.push_back(tmpType);
    }

    // Find the first valid video stream inside the file
    video_stream_index = -1;
    AVCodecParameters* av_codec_params;
    const AVCodec* av_codec;
    for (int i = 0; i < av_format_ctx->nb_streams; ++i) {
        av_codec_params = av_format_ctx->streams[i]->codecpar;
        av_codec = const_cast<AVCodec*>(avcodec_find_decoder(av_codec_params->codec_id));
        if (!av_codec) {
            continue;
        }
        if (av_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            width = av_codec_params->width;
            height = av_codec_params->height;
            time_base = av_format_ctx->streams[i]->time_base;
            break;
        }
    }

    if (video_stream_index == -1) {
        std::cout << "no video stream found!" << std::endl;
        return false;
    }

    // Set up a codec context for the decoder
    av_codec_ctx = avcodec_alloc_context3(av_codec);
    if (!av_codec_ctx) {
        printf("Couldn't create AVCodecContext\n");
        return false;
    }

    if (!hw_device_ctx && state->useHardwareDecoding) {
        for (auto& deviceType : availableDeviceTypes) {
            int err = hw_decoder_init(state, av_codec_ctx, lastSuccessfullDeviceType == AV_HWDEVICE_TYPE_NONE ? deviceType : lastSuccessfullDeviceType, deviceName);
            if (err < 0) continue;
            if (err == 0) {
                lastSuccessfullDeviceType = deviceType;
                break;
            }
        }
        std::cout << "ffmpeg hardware device type: " << av_hwdevice_get_type_name(device_type) << std::endl;
        if (device_type == AV_HWDEVICE_TYPE_NONE) {
            std::cout << "cannot find any available hardware decoders. performing fallback to software decoder" << std::endl;
            state->useHardwareDecoding = false;
            forceSoftwareDecoding = true;
        }
    }

    if (state->useHardwareDecoding) 
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(av_codec, i);
            if (!config) {
                std::cout << "Decoder " << av_codec->name << " does not support device type " << av_hwdevice_get_type_name(device_type) << std::endl;
                state->useHardwareDecoding = false;
                av_buffer_unref(&av_codec_ctx->hw_device_ctx);
                av_codec_ctx->hw_device_ctx = nullptr;
                break;
            }
            if ((config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX) &&
                config->device_type == device_type) {
                    hw_pix_fmt = config->pix_fmt;
                    std::cout << "ffmpeg hardware pixel format: " << av_get_pix_fmt_name(hw_pix_fmt) << std::endl;
                    state->useHardwareDecoding = true;
                    break;
                }
        }

    if (forceSoftwareDecoding) {
        state->useHardwareDecoding = false;
        std::cout << "fallback to software decoding because 'forceSoftwareDecoding' is true " << std::endl;
    }

    cached_hw_pix_fmt = hw_pix_fmt;
    if (state->useHardwareDecoding) av_codec_ctx->get_format = get_hw_format;

    if (avcodec_parameters_to_context(av_codec_ctx, av_codec_params) < 0) {
        printf("Couldn't initialize AVCodecContext\n");
        return false;
    }

    if (!hw_pix_fmt && state->useHardwareDecoding) {
        std::cout << "cannot determine hardware pixel format!" << std::endl;
        return false;
    }

    if (avcodec_open2(av_codec_ctx, av_codec, NULL) < 0) {
        printf("Couldn't open codec\n");
        return false;
    }

    
    av_frame = av_frame_alloc();
    if (!av_frame) {
        printf("Couldn't allocate AVFrame (av_frame)\n");
        return false;
    }

    av_frame->width = width;
    av_frame->height = height;

    hw_frame = av_frame_alloc();
    if (!hw_frame) {
        printf("Couldn't allocate AVFrame (hw_frame)\n");
        return false;
    }

    hw_frame->width = width;
    hw_frame->height = height;

    av_packet = av_packet_alloc();
    if (!av_packet) {
        printf("Couldn't allocate AVPacket\n");
        return false;
    }


    return true;
}

bool video_reader_read_frame(VideoReaderState* state, uint8_t* frame_buffer, int64_t* pts) {

    // Unpack members of state
    auto& width = state->width;
    auto& height = state->height;
    auto& av_format_ctx = state->av_format_ctx;
    auto& av_codec_ctx = state->av_codec_ctx;
    auto& video_stream_index = state->video_stream_index;
    auto& av_frame = state->av_frame;
    auto& hw_frame = state->hw_frame;
    auto& av_packet = state->av_packet;
    auto& sws_scaler_ctx = state->sws_scaler_ctx;    
    auto& hw_device_ctx = state->hw_device_ctx;
    auto& hw_pix_fmt = state->hw_pix_fmt;
    auto& device_type = state->device_type;
    cached_hw_pix_fmt = hw_pix_fmt;

    // Decode one frame
    int response;
    if (state->useHardwareDecoding) {
        while (av_read_frame(av_format_ctx, av_packet) >= 0) {
            if (av_packet->stream_index != video_stream_index) {
                av_packet_unref(av_packet);
                continue;
            }

            response = avcodec_send_packet(av_codec_ctx, av_packet);
            if (response < 0) {
                printf("Failed to decode packet: %s\n", av_make_error(response));
                return false;
            }

            response = avcodec_receive_frame(av_codec_ctx, hw_frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                av_packet_unref(av_packet);
                continue;
            } else if (response < 0) {
                printf("Failed to decode packet: %s\n", av_make_error(response));
                return false;
            }

            if (hw_frame->format == hw_pix_fmt) {
                if (frame_buffer) 
                    av_hwframe_transfer_data(av_frame, hw_frame, 0);
            }

            if (frame_buffer) {
                int size = av_image_get_buffer_size((AVPixelFormat) av_frame->format, av_frame->width, av_frame->height, 1);
                av_image_copy_to_buffer(frame_buffer, size, (const uint8_t* const*) av_frame->data, (const int*) av_frame->linesize, (AVPixelFormat) av_frame->format, av_frame->width, av_frame->height, 1);
            }

            av_packet_unref(av_packet);
            break;
        }
    } else {
        while (av_read_frame(av_format_ctx, av_packet) >= 0) {
            if (av_packet->stream_index != video_stream_index) {
                av_packet_unref(av_packet);
                continue;
            }

            response = avcodec_send_packet(av_codec_ctx, av_packet);
            if (response < 0) {
                printf("Failed to decode packet: %s\n", av_make_error(response));
                return false;
            }

            response = avcodec_receive_frame(av_codec_ctx, av_frame);
            if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                av_packet_unref(av_packet);
                continue;
            } else if (response < 0) {
                printf("Failed to decode packet: %s\n", av_make_error(response));
                return false;
            }

            av_packet_unref(av_packet);
            break;
        }
    }

    if (pts) *pts = av_frame->pts;
    
    if (frame_buffer) {
        if (!sws_scaler_ctx) {
            sws_scaler_ctx = sws_getContext(width, height, correct_for_deprecated_pixel_format(state->useHardwareDecoding ? AV_PIX_FMT_NV12 : (AVPixelFormat) av_frame->format),
                                            width, height, AV_PIX_FMT_RGB0,
                                            SWS_FAST_BILINEAR, NULL, NULL, NULL);
        }
        if (!sws_scaler_ctx) {
            printf("Couldn't initialize sw scaler\n");
            return false;
        }

        uint8_t* dest[4] = { frame_buffer, NULL, NULL, NULL };
        int dest_linesize[4] = { width * 4, 0, 0, 0 };
        sws_scale(sws_scaler_ctx, av_frame->data, av_frame->linesize, 0, av_frame->height, dest, dest_linesize);
    }

    return true;
}

bool video_reader_seek_frame(VideoReaderState* state, int64_t ts) {
    
    // Unpack members of state
    auto& av_format_ctx = state->av_format_ctx;
    auto& av_codec_ctx = state->av_codec_ctx;
    auto& video_stream_index = state->video_stream_index;
    auto& av_packet = state->av_packet;
    auto& av_frame = state->av_frame;
    
    avcodec_flush_buffers(av_codec_ctx);
    av_seek_frame(av_format_ctx, video_stream_index, ts, AVSEEK_FLAG_ANY);
    video_reader_read_frame(state, nullptr, nullptr);

    return true;
}

void video_reader_close(VideoReaderState* state) {
    sws_freeContext(state->sws_scaler_ctx);
    avformat_close_input(&state->av_format_ctx);
    avformat_free_context(state->av_format_ctx);
    av_frame_free(&state->hw_frame);
    av_frame_free(&state->av_frame);
    av_packet_free(&state->av_packet);
    if (state->useHardwareDecoding) av_buffer_unref(&state->hw_device_ctx);
    avcodec_free_context(&state->av_codec_ctx);
}