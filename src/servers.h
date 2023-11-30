#pragma once

#include "electron.h"
#include <curl/curl.h>

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

    // Provides ability to communicate with electron servers
    class Servers {
    private:
        static ServerResponse PerformSyncedRequest(int port, json_t request);
        static ServerResponse ServerRequest(int port, json_t request);

    public:
        static ServerResponse AsyncWriterRequest(json_t request);
        static ServerResponse AudioServerRequest(json_t request);
        static void InitializeCurl();
        static void DestroyCurl();
    };
}