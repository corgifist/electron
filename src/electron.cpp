#include "utils/backward.hpp"
#include "electron.h"
#include "libraries.h"
#include "app.h"
#include "build_number.h"
#include <chrono>
#include <thread>

#define start_server(type, port) system((std::string("./") + argv[0] + " --type " + type + " --port " + std::to_string(port) + " --pid " + std::to_string(getpid()) + " &").c_str())

using namespace Electron;

int main(int argc, char *argv[]) {
    bool disableBackwards = false;
    setenv("ANGLE_DEFAULT_PLATFORM", "vulkan", 0); 
    backward::SignalHandling* sh = nullptr;
    if (argc > 1) {
        std::string serverType = "";
        int port = 80;
        int pid = 0;
        bool useServer = false;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--type") {
                serverType = argv[++i];
                useServer = true;
            } else if (arg == "--port") {
                std::string ps = argv[++i];
                port = std::stoi(ps);
                useServer = true;
            } else if (arg == "--pid") {
                pid = std::stoi(argv[++i]);
                useServer = true;
            } else if (arg == "--disable-backwards") {
                disableBackwards = true;
            }
        }
        if (useServer) {
            print("starting " << serverType << " server at port " << std::to_string(port));
            Libraries::LoadLibrary("libs", serverType);
            Libraries::GetFunction<void(int, int)>(serverType, "ServerStart")(port, pid);
            exit(0);
        }
    }
    if (!disableBackwards) {
        sh = new backward::SignalHandling();
    }
    print("electron " + std::to_string(BUILD_NUMBER) + " is starting soon...");

    if (!std::filesystem::exists("cache/")) {
        print("cache folder not found, so creating one");
        std::filesystem::create_directories("cache/");
    }

    if (start_server("async_writer", 4040)) {}

    AppCore::Initialize();
    AppCore::ExecuteSignal(Signal::_SpawnDockspace);
    AppCore::ExecuteSignal(Signal::_SpawnProjectConfiguration);
    AppCore::ExecuteSignal(Signal::_SpawnRenderPreview);
    AppCore::ExecuteSignal(Signal::_SpawnLayerProperties);
    AppCore::ExecuteSignal(Signal::_SpawnAssetExaminer);
    AppCore::ExecuteSignal(Signal::_SpawnAssetManager);
    AppCore::ExecuteSignal(Signal::_SpawnTimeline);
    try {
        AppCore::Run();
    } catch (Signal signal) {
        if (signal == Signal::_ReloadSystem) {
            AppCore::Terminate();
            goto system_reloaded;
        }
    }

    return 0;

system_reloaded:
    if (system(string_format("%s &", argv[0]).c_str())) {}
    return 0;
}