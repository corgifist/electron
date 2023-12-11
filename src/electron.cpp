#include "backward.hpp"
#include "electron.h"
#include "libraries.h"
#include "app.h"
#include <chrono>
#include <thread>

#define start_server(type, port) system((std::string("./") + argv[0] + " --type " + type + " --port " + std::to_string(port) + " --pid " + std::to_string(getpid()) + " &").c_str())

using namespace Electron;

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::string serverType = "";
        int port = 80;
        int pid = 0;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--type") {
                serverType = argv[++i];
            } else if (arg == "--port") {
                std::string ps = argv[++i];
                port = std::stoi(ps);
            } else if (arg == "--pid") {
                pid = std::stoi(argv[++i]);
            }
        }
        print("starting " << serverType << " server at port " << std::to_string(port));
        Libraries::GetFunction<void(int, int)>(serverType, "ServerStart")(port, pid);
        exit(0);
    }

    if (!std::filesystem::exists("cache/")) {
        print("cache folder not found, so creating one");
        std::filesystem::create_directories("cache/");
    }

    std::ofstream tempStream("electron.log");
    tempStream << "[ELECTRON LOG BEGIN]";
    start_server("async_writer", 4040);
    start_server("audio_server", 4045);
    print("waiting for servers to startup");
    while (Servers::AsyncWriterRequest({{"action", "alive"}}).alive) {}
    print("async writer server is running");
    while (Servers::AudioServerRequest({{"action", "alive"}}).alive) {}
    print("audio server is running");

    /* std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    std::cout.rdbuf(tempStream.rdbuf()); */

    AppInstance instance;
    instance.ExecuteSignal(Signal::_SpawnDockspace);
    instance.ExecuteSignal(Signal::_SpawnProjectConfiguration);
    instance.ExecuteSignal(Signal::_SpawnRenderPreview);
    instance.ExecuteSignal(Signal::_SpawnLayerProperties);
    instance.ExecuteSignal(Signal::_SpawnAssetExaminer);
    instance.ExecuteSignal(Signal::_SpawnAssetManager);
    instance.ExecuteSignal(Signal::_SpawnTimeline);
    try {
        instance.Run();
    } catch (Signal signal) {
        if (signal == Signal::_ReloadSystem) {
            instance.Terminate();
            goto system_reloaded;
        }
    }

    return 0;

system_reloaded:
    system(string_format("%s &", argv[0]).c_str());
    return 0;
}