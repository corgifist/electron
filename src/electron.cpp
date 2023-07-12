#include "app.h"

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