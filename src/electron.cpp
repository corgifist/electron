#include "app.h"

int main() {

    try {
        Electron::AppInstance instance;
        instance.AddUIContent(new Electron::ProjectConfiguration());
        instance.Run();
    } catch (const std::exception& ex) {
        MessageBoxA(nullptr, ex.what(), "Electron Fatal Error!", MB_ICONERROR | MB_OK);
        print("GetLastError() = " << GetLastError());
    }
    return 0;
}