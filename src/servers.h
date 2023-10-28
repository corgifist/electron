#pragma once

#include "electron.h"
#include <curl/curl.h>

namespace Electron {
    class Servers {
    private:
        static bool PerformSyncedRequest(int port, json_t request);
        static bool ServerRequest(int port, json_t request);

    public:
        static bool AsyncWriterRequest(json_t request);
        static bool AudioServerRequest(json_t request);
        static void InitializeCurl();
        static void DestroyCurl();
    };
}