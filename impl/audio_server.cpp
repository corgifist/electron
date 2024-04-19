#include <set>
#define MINIAUDIO_IMPLEMENTATION
#include "utils/miniaudio.h"

#include "editor_core.h"

#define THREAD_POOL_SIZE 50

#define SAMPLE_RATE 48000
#define CHANNEL_COUNT 2
#define SAMPLE_FORMAT ma_format_f32

using namespace Electron;

extern "C" {

    typedef void (*Electron_AudioDataT)(float*, int, void*);

    struct AudioBundle {
        std::string path;

        AudioBundle() {}
    };

    struct AudioDecoder {
        ma_decoder decoder;
        Electron_AudioDataT audioDataProc;
        void* pUserData;

        bool pause;
        bool ready;
        AudioDecoder() {
            this->ready = false;
        }
    };

    int loadIndex = 0;

    ma_device device;
    ma_decoder_config decoderConfig;

    std::vector<AudioDecoder> decoders;
    std::unordered_map<std::string, AudioBundle> wavRegistry;
    std::unordered_map<int, std::string> playRegistry;
    std::vector<bool> loadInfo;
    std::vector<std::thread> threadRegistry;
    std::set<std::string> loadedRegistry;

    std::mutex wavRegistryMutex;

    bool currentlyMixing = false;

    void JoinThreads() {
        for (auto& thread : threadRegistry) {
            thread.join();
        }
        threadRegistry.clear();
        loadInfo.clear();
    }

    ma_uint32 read_and_mix_pcm_frames_f32(ma_decoder* pDecoder, float* pOutputF32, ma_uint32 frameCount, Electron_AudioDataT audioProc, void* pUserData) {
        ma_result result;
        float temp[4096];
        ma_uint32 tempCapInFrames = ma_countof(temp) / CHANNEL_COUNT;
        ma_uint32 totalFramesRead = 0;

        while (totalFramesRead < frameCount) {
            ma_uint64 iSample;
            ma_uint64 framesReadThisIteration;
            ma_uint32 totalFramesRemaining = frameCount - totalFramesRead;
            ma_uint32 framesToReadThisIteration = tempCapInFrames;
            if (framesToReadThisIteration > totalFramesRemaining) {
                framesToReadThisIteration = totalFramesRemaining;
            }

            result = ma_decoder_read_pcm_frames(pDecoder, temp, framesToReadThisIteration, &framesReadThisIteration);
            if (result != MA_SUCCESS || framesReadThisIteration == 0) {
                break;
            }

            if (audioProc) {
                audioProc(temp, framesReadThisIteration, pUserData);
            }

            /* Mix the frames together. */
            for (iSample = 0; iSample < framesReadThisIteration*CHANNEL_COUNT; ++iSample) {
                pOutputF32[totalFramesRead*CHANNEL_COUNT + iSample] += temp[iSample];
            }

            totalFramesRead += (ma_uint32)framesReadThisIteration;

            if (framesReadThisIteration < (ma_uint32)framesToReadThisIteration) {
                break;  /* Reached EOF. */
            }
        }
        
        return totalFramesRead;
    }


    void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 framesCount) {
        currentlyMixing = true;
        float* pOutputF32 = (float*) pOutput;
        for (auto& decoderBundle : decoders) {
            ma_decoder* decoder = &decoderBundle.decoder;
            if (!decoderBundle.ready) continue;
            if (decoderBundle.pause) continue;
            read_and_mix_pcm_frames_f32(decoder, pOutputF32, framesCount, decoderBundle.audioDataProc, decoderBundle.pUserData);
        }
        currentlyMixing = false;
    }

    ELECTRON_EXPORT void ServerInit() {
        ma_device_config deviceConfig = ma_device_config_init(ma_device_type_playback);
        deviceConfig.playback.format = SAMPLE_FORMAT;
        deviceConfig.playback.channels = CHANNEL_COUNT;
        deviceConfig.sampleRate = SAMPLE_RATE;
        deviceConfig.pUserData = nullptr;
        deviceConfig.dataCallback = (ma_device_data_proc) data_callback;

        if (ma_device_init(nullptr, &deviceConfig, &device) != MA_SUCCESS) {
            throw std::runtime_error("cannot initialize miniaudio device!");
        }

        decoderConfig = ma_decoder_config_init(SAMPLE_FORMAT, CHANNEL_COUNT, SAMPLE_RATE);

        ma_device_start(&device);

        loadInfo.push_back(true); // always true
        decoders.push_back(AudioDecoder());
    }

    ELECTRON_EXPORT void ServerTerminate() {
        JoinThreads();
    }

    ELECTRON_EXPORT json_t ServerDispatch(json_t body) {
        std::string action = JSON_AS_TYPE(body["action"], std::string);
        json_t result = {};
        if (action == "kill") {
            throw Electron::Signal::_TerminateServer;
        }
        if (action == "load_sample") {
            std::string path = JSON_AS_TYPE(body["path"], std::string);
            if (threadRegistry.size() > THREAD_POOL_SIZE) {
                JoinThreads();
            }
            if (loadedRegistry.find(path) == loadedRegistry.end() || body.count("override") != 0) {
                loadedRegistry.insert(path);
                loadInfo.push_back(false);
                int infoIndex = loadInfo.size() - 1;
                threadRegistry.push_back(
                    std::thread([](std::string path, int infoIndex) {
                        AudioBundle bundle;
                        bundle.path = path;
                        wavRegistry[path] = bundle;
                        loadInfo[infoIndex] = true; 
                    }, path, infoIndex)
                );
                result["id"] = infoIndex;
            }
        }
        if (action == "play_sample") {
            decoders.push_back(AudioDecoder());
            AudioDecoder& decoder = decoders.at(decoders.size() - 1);
            int decoderIndex = decoders.size() - 1;
            Electron_AudioDataT audioProc = body.contains("audio_proc") ? (Electron_AudioDataT) (uint64_t) body["audio_proc"] : nullptr;
            void* userData = body.contains("user_data") ? (void*) (uint64_t) body["user_data"] : nullptr;
            decoder.audioDataProc = audioProc;
            decoder.pUserData = userData;
            decoder.pause = false;

            ma_decoder_init_file(JSON_AS_TYPE(body["path"], std::string).c_str(), &decoderConfig, &decoder.decoder);
            decoder.ready = true;

            playRegistry[decoderIndex] = JSON_AS_TYPE(body["path"], std::string);

            result["handle"] = decoderIndex;
        }
        if (action == "create_decoder") {
            ma_decoder* decoder = new ma_decoder();
            ma_decoder_init_file(JSON_AS_TYPE(body["path"], std::string).c_str(), &decoderConfig, decoder);
            result["decoder"] = (uint64_t) decoder;
        }
        if (action == "read_decoder") {
            float* pOutputF32 = (float*) (uint64_t) body["dest"];
            ma_decoder* decoder = (ma_decoder*) (uint64_t) body["decoder"];
            ma_uint64 framesToRead = body["frames_to_read"];
            ma_uint64 framesRead;
            ma_decoder_read_pcm_frames(decoder, pOutputF32, framesToRead, &framesRead);
            result["frames_read"] = framesRead;
        }
        if (action == "destroy_decoder") {
            delete (ma_decoder*) (uint64_t) body["decoder"];
        }
        if (action == "pause_sample") {
            bool pause = JSON_AS_TYPE(body["pause"], bool);
            AudioDecoder& decoder = decoders.at(body["handle"]);
            decoder.pause = pause;
        }
        if (action == "stop_sample") {
            int source = body["handle"];
            if (source >= decoders.size()) return {};
            playRegistry.erase(source);
            AudioDecoder& decoder = decoders.at(source);
            while (currentlyMixing) {}
            if (decoder.ready) {
                ma_decoder_uninit(&decoder.decoder);
            }
            decoder.ready = false;
        }
        if (action == "destroy_sample") {
            AudioBundle& bundle = wavRegistry[JSON_AS_TYPE(body["path"], std::string)];
            wavRegistry.erase(JSON_AS_TYPE(body["path"], std::string));
            loadedRegistry.erase(JSON_AS_TYPE(body["path"], std::string));
        }
        if (action == "seek_sample") {
            double seek = JSON_AS_TYPE(body["seek"], double);
            AudioDecoder& decoder = decoders.at(body["handle"]);
            ma_decoder_seek_to_pcm_frame(&decoder.decoder, SAMPLE_RATE * seek);
        }
        if (action == "is_loaded") {
            result["loaded"] = (wavRegistry.find(JSON_AS_TYPE(body["path"], std::string)) != wavRegistry.end());
        }
        if (action == "load_status") {
            if (JSON_AS_TYPE(body["id"], int) == -1) return {{"result", false}};
            if (JSON_AS_TYPE(body["id"], int) >= loadInfo.size()) return {{"result", true}};
            result["status"] = loadInfo.at(JSON_AS_TYPE(body["id"], int));
        }
        if (action == "audio_buffer_ptr") {
        }
        return result;
    }
}