#include "crow_all.h"
#include "editor_core.h"
#include <AL/al.h>
#include <AL/alc.h>
#include "AudioFile.h"

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

    crow::SimpleApp* globalApp = nullptr;
    bool running = false;
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

    ELECTRON_EXPORT void ServerStart(int port, int pid) {
        globalApp = new crow::SimpleApp();
        globalApp->loglevel(crow::LogLevel::Warning);
        running = true;
        auto thread = std::thread([](int pid) {
            while (true) {
                if (!process_is_alive(pid)) break;
                if (!running) break;
            }
            globalApp->stop();
            JoinThreads();
            exit(0);
        }, pid);
        

        auto devices = ListALDevices();
        for (auto& device : devices) {
            print("OpenAL Device: " << device);
        }

        alDevice = alcOpenDevice(nullptr);
        if (!alDevice) 
            throw std::runtime_error("(audio-server) cannot open audio device!");

        ALCint attrList[] = {
            ALC_FREQUENCY, 44100,
            0
        };
        alContext = alcCreateContext(alDevice, attrList);
        if (!alContext) 
            throw std::runtime_error("(audio-server) cannot create audio context!");
        if (!alcMakeContextCurrent(alContext)) 
            throw std::runtime_error("(audio-server) cannot set current audio context!");

        DUMP_VAR(alGetString(AL_VERSION));

        CROW_ROUTE((*globalApp), "/request")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                if (!body) {
                    return crow::response(400);
                }
                std::string action = body["action"].s();
                crow::json::wvalue result = {};
                if (action == "kill") {
                    JoinThreads();
                    globalApp->stop();
                    running = false;
                }
                if (action == "load_sample") {
                    std::string path = body["path"].s();
                    if (loadedRegistry.find(path) == loadedRegistry.end() || body.count("override") != 0) {
                        loadedRegistry.insert(path);
                        if (wavRegistry.find(path) != wavRegistry.end()) {
                            wavRegistry[path].Destroy();
                        }
                        loadInfo.push_back(false);
                        int infoIndex = loadInfo.size() - 1;
                        threadRegistry.push_back(
                            std::thread([](std::string path, int infoIndex) {
                                AudioBundle bundle;
                                alGenBuffers(1, &bundle.buffer);
                                std::vector<uint8_t> audioBytes;
                                std::string audioFile = read_file(path);
                                for (int i = 0; i < audioFile.size(); i++) {
                                    audioBytes.push_back((uint8_t) audioFile[i]);
                                }
                                bundle.data.loadFromMemory(audioBytes);
                                alBufferData(bundle.buffer, GetALFormat(bundle.data.getNumChannels(), bundle.data.getBitDepth()), 
                                                    audioBytes.data(), audioBytes.size(), bundle.data.getSampleRate());
                                wavRegistry[path] = bundle;
                                loadInfo[infoIndex] = true; 
                            }, path, infoIndex)
                        );
                        result["id"] = infoIndex;
                    }
                }
                if (action == "play_sample") {
                    ALuint source;
                    AudioBundle& bundle = wavRegistry[body["path"].s()];
                    playRegistry[source] = body["path"].s();
                    alGenSources(1, &source);
                    alSourcef(source, AL_PITCH, 1.0f);
                    alSourcef(source, AL_GAIN, 1.0f);
                    alSourcei(source, AL_LOOPING, AL_FALSE);
                    alSourcei(source, AL_BUFFER, bundle.buffer);
                    result["handle"] = source;
                }
                if (action == "pause_sample") {
                    bool playing = !body["pause"].b();

                    ALenum state;
                    alGetSourcei(body["handle"].i(), AL_SOURCE_STATE, &state);

                    if (playing && state != AL_PLAYING) alSourcePlay(body["handle"].i());
                    else if (!playing && state != AL_PAUSED) alSourcePause(body["handle"].i());
                }
                if (action == "stop_sample") {
                    int source = body["handle"].i();
                    playRegistry.erase(source);
                    alSourceStop(body["handle"].i());
                    alDeleteSources(1, (ALuint*) &source);
                }
                if (action == "seek_sample") {
                    float seek = body["seek"].d();
                    alSourcef(body["handle"].i(), AL_SEC_OFFSET, seek);
                }
                if (action == "is_loaded") {
                    result["loaded"] = (wavRegistry.find(body["path"].s()) != wavRegistry.end());
                }
                if (action == "load_status") {
                    result["status"] = loadInfo.at(body["id"].i());
                }
                return crow::response(result.dump());
        });


        globalApp->port(port)
            .bindaddr("0.0.0.0")
            .timeout(5)
            .concurrency(2)
            .run();
        JoinThreads();
        thread.join();
    }
}