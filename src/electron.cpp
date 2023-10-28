#include "backward.hpp"
#include "electron.h"
#include "libraries.h"
#include "app.h"

#define start_server(type, port) system((std::string("./") + argv[0] + " --type " + type + " --port " + std::to_string(port) + " --pid " + std::to_string(getpid()) + " &").c_str())

bool Electron::Rect::contains(Electron::Point p) {
    return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h;
}

std::string Electron::exec(const char *cmd) {
    char buffer[128];
    std::string result = "";
    FILE *pipe = popen(cmd, "r");
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
    }
    pclose(pipe);
    return result;
}



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
        Electron::Libraries::GetFunction<void(int, int)>(serverType, "ServerStart")(port, pid);
        exit(0);
    }


    std::ofstream tempStream("electron.log");
    tempStream << "[ELECTRON LOG BEGIN]";
    start_server("async_writer", 4040);
    start_server("audio_server", 4045);

    /* std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    std::cout.rdbuf(tempStream.rdbuf()); */

    Electron::AppInstance instance;
    instance.AddUIContent(new Electron::Dockspace());
    instance.AddUIContent(new Electron::ProjectConfiguration());
    instance.AddUIContent(new Electron::RenderPreview());
    instance.AddUIContent(new Electron::LayerProperties());
    instance.AddUIContent(new Electron::AssetManager());
    instance.AddUIContent(new Electron::Timeline());
    try {
        instance.Run();
    } catch (Electron::ElectronSignal signal) {
        if (signal == Electron::ElectronSignal_ReloadSystem) {
            instance.Terminate();
            goto system_reloaded;
        }
    }

    return 0;

system_reloaded:
    Electron::exec(argv[0]);
    return 0;
}