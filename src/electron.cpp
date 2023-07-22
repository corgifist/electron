#include "app.h"

bool Electron::Rect::contains(Electron::Point p) {
    return p.x >= x && p.y >= y && p.x <= x + w && p.y <= y + h;
}

int main() {

    try {
        Electron::AppInstance instance;
        instance.AddUIContent(new Electron::ProjectConfiguration());
        instance.Run();
    } catch (const std::exception& ex) {
        print("\t\tFatal error occured!");
        print("\t" + std::string(ex.what()));
    }
    return 0;
}