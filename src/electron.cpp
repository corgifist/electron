#include "backward.hpp"
#include "electron.h"
#include "app.h"

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
    std::ofstream tempStream("electron.log");
    tempStream << "[ELECTRON LOG BEGIN]";

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