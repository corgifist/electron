#include "editor_core.h"
#include <AL/al.h>
#include <AL/alc.h>
#include "AudioFile.h"
#include <set>

using namespace Electron;

extern "C" {
    struct AudioBundle {
        AudioFile<float> data;
        ALuint buffer;

        AudioBundle() {}

        void Destroy() {
            alDeleteBuffers(1, &buffer);
        }
    };  

    ALenum GetALFormat(int channels, int bitsPerSample) {
        ALenum format;
        if(channels == 1 && bitsPerSample == 8)
            format = AL_FORMAT_MONO8;
        else if(channels == 1 && bitsPerSample == 16)
            format = AL_FORMAT_MONO16;
        else if(channels == 2 && bitsPerSample == 8)
            format = AL_FORMAT_STEREO8;
        else if(channels == 2 && bitsPerSample == 16)
            format = AL_FORMAT_STEREO16;
        return format;
    }

    std::vector<std::string> ListALDevices() {
        const ALCchar* devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
        const ALCchar *device = devices, *next = devices + 1;
        size_t len = 0;
        std::vector<std::string> result;

        while (device && *device != '\0' && next && *next != '\0') {
                result.push_back(device);
                len = strlen(device);
                device += (len + 1);
                next += (len + 2);
        }
        return result;
    }


    int loadIndex = 0;
    ALCdevice* alDevice = nullptr;
    ALCcontext* alContext;
    std::unordered_map<std::string, AudioBundle> wavRegistry;
    std::unordered_map<int, std::string> playRegistry;
    std::vector<bool> loadInfo;
    std::vector<std::thread> threadRegistry;
    std::set<std::string> loadedRegistry;

    void JoinThreads() {
        for (auto& thread : threadRegistry) {
            thread.join();
        }
        threadRegistry.clear();
    }


    ELECTRON_EXPORT void ServerInit() {
        auto devices = ListALDevices();
        for (auto& device : devices) {
            print("OpenAL Device: " << device);
        }

        alDevice = alcOpenDevice(devices[0].c_str());
        if (!alDevice) 
            throw std::runtime_error("(audio-server) cannot open audio device!");

        ALCint attrList[] = {
            ALC_FREQUENCY, 44100,
            ALC_MONO_SOURCES, 512,
            ALC_STEREO_SOURCES, 512,
            0
        };
        alContext = alcCreateContext(alDevice, attrList);
        if (!alContext) 
            throw std::runtime_error("(audio-server) cannot create audio context!");
        if (!alcMakeContextCurrent(alContext)) 
            throw std::runtime_error("(audio-server) cannot set current audio context!");

        DUMP_VAR(alGetString(AL_VERSION));
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
            if (!hasEnding(path, ".wav")) {
                return {{"id", -1}};
            }
            if (!file_exists(path)) return {{"id", -1}};
            if (loadedRegistry.find(path) == loadedRegistry.end() || body.count("override") != 0) {
                loadedRegistry.insert(path);
                if (wavRegistry.find(path) != wavRegistry.end()) {
                    wavRegistry[path].Destroy();
                }
                loadInfo.push_back(false);
                int infoIndex = loadInfo.size() - 1;
                threadRegistry.push_back(
                    std::thread([](std::string path, int infoIndex) {
                        alcMakeContextCurrent(alContext);
                        AudioBundle bundle;
                        alGenBuffers(1, &bundle.buffer);
                        std::vector<uint8_t> audioBytes;
                        std::string audioFile = read_file(path);
                        for (int i = 0; i < audioFile.size(); i++) {
                            audioBytes.push_back((uint8_t) audioFile[i]);
                        }
                        bundle.data.loadFromMemory(audioBytes);
                        alBufferData(bundle.buffer, GetALFormat(bundle.data.getNumChannels(), bundle.data.getBitDepth()), 
                                                    audioBytes.data() + bundle.data.samplesStartIndex, audioBytes.size() - bundle.data.samplesStartIndex, bundle.data.getSampleRate());
                        wavRegistry[path] = bundle;
                        loadInfo[infoIndex] = true; 
                    }, path, infoIndex)
                );
                result["id"] = infoIndex;
            }
        }
        if (action == "play_sample") {
            ALuint source;
            AudioBundle& bundle = wavRegistry[JSON_AS_TYPE(body["path"], std::string)];
            playRegistry[source] = JSON_AS_TYPE(body["path"], std::string);
            alGenSources(1, &source);
            alSourcef(source, AL_PITCH, 1.0f);
            alSourcef(source, AL_GAIN, 1.0f);
            alSourcei(source, AL_LOOPING, AL_FALSE);
            alSourcei(source, AL_BUFFER, bundle.buffer);
            result["handle"] = source;
        }
        if (action == "pause_sample") {
            bool playing = !JSON_AS_TYPE(body["pause"], bool);

            ALenum state;
            alGetSourcei(JSON_AS_TYPE(body["handle"], int), AL_SOURCE_STATE, &state);
            if (playing && state != AL_PLAYING) alSourcePlay(JSON_AS_TYPE(body["handle"], int));
            else if (!playing && state != AL_PAUSED) alSourcePause(JSON_AS_TYPE(body["handle"], int));
        }
        if (action == "stop_sample") {
            int source = JSON_AS_TYPE(body["handle"], int);
            playRegistry.erase(source);
            alSourceStop(JSON_AS_TYPE(body["handle"], int));
            alDeleteSources(1, (ALuint*) &source);
        }
        if (action == "seek_sample") {
            float seek = JSON_AS_TYPE(body["seek"], double);
            alSourcef(JSON_AS_TYPE(body["handle"], int), AL_SEC_OFFSET, seek);
        }
        if (action == "is_loaded") {
            result["loaded"] = (wavRegistry.find(JSON_AS_TYPE(body["path"], std::string)) != wavRegistry.end());
        }
        if (action == "load_status") {
            if (JSON_AS_TYPE(body["id"], int) == -1) return {{"result", false}};
            result["status"] = loadInfo.at(JSON_AS_TYPE(body["id"], int));
        }
        if (action == "get_audio_duration") {
            result["duration"] = wavRegistry[JSON_AS_TYPE(body["path"], std::string)].data.getLengthInSeconds();
        }
        return result;
    }
}