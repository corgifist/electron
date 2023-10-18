#pragma once

#include "electron.h"
#include <curl/curl.h>

namespace Electron {
    class Servers {
    private:
        static void PerformSyncedRequest(int port, json_t request);
        static void ServerRequest(int port, json_t request);

    public:
        static void AsyncWriterRequest(json_t request);
        static void InitializeCurl();
        static void DestroyCurl();
    };
}