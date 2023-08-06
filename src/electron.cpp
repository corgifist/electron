#include "app.h"

bool Electron::Rect::contains(Electron::Point p) {
    return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h;
}


std::string Electron::exec(const char* cmd) {
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(cmd, "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    } catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    return result;
}

int main(int argc, char *argv[]) {
    if (std::filesystem::exists("G:/ProgrammingProjects/Electron/corgi.png")) {
        print("LOL EXISTS");
    }
    try {
        Electron::AppInstance instance;
        instance.AddUIContent(new Electron::ProjectConfiguration());
        try {
            instance.Run();
        } catch (Electron::ElectronSignal signal) {
            if (signal == Electron::ElectronSignal_ReloadSystem) {
                instance.Terminate();
                goto system_reloaded;
            }
        }
    } catch (const std::exception& ex) {
        print("\t\tFatal error occured!");
        print("\t" + std::string(ex.what()));
    }
    return 0;

    system_reloaded:
    Electron::exec(argv[0]);
    return 0;
}