#include "servers.h"

namespace Electron {

    ServerStructure Servers::audioSystem{};


    ServerStructure Servers::LoadSystem(std::string library) {
        ServerStructure structure;
        Libraries::LoadLibrary("libs", library);
        structure.initialization = Libraries::GetFunction<void()>(library, "ServerInit");
        structure.termination = Libraries::GetFunction<void()>(library, "ServerTerminate");
        structure.dispatch = Libraries::GetFunction<json_t(json_t)>(library, "ServerDispatch");
        return structure;
    }

    json_t Servers::AudioServerRequest(json_t request) {
        return audioSystem.dispatch(request);
    }

    void Servers::Initialize() {
        audioSystem = LoadSystem("audio_server");
        audioSystem.initialization();
    }

    void Servers::Destroy() {
        audioSystem.termination();
    }
}