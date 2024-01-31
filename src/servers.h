#pragma once

#include "electron.h"
#include <curl/curl.h>
#include "libraries.h"

namespace Electron {

    struct ServerResponse {
        bool alive;
        std::string response;

        ServerResponse(bool alive, std::string response) {
            this->alive = alive;
            this->response = response;
        }

        json_t ResponseToJSON() {
            return json_t::parse(response);
        }
    };

    typedef void (*ServerState_F)();
    typedef json_t (*ServerDispatch_F)(json_t);
    struct ServerStructure {
        ServerState_F initialization, termination;
        ServerDispatch_F dispatch;

        ServerStructure() {}
    };

    // Provides ability to communicate with electron servers
    class Servers {
    private:
        static ServerResponse PerformSyncedRequest(int port, json_t request);
        static ServerResponse ServerRequest(int port, json_t request);
        static ServerStructure LoadSystem(std::string library);

    public:
        static ServerStructure audioSystem;

        static ServerResponse AsyncWriterRequest(json_t request);
        static json_t AudioServerRequest(json_t request);
        static void Initialize();
        static void Destroy();
    };
}