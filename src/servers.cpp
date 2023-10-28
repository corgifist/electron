#include "servers.h"

static size_t servers_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}

namespace Electron {

    static CURL* hnd = nullptr;


    bool Servers::PerformSyncedRequest(int port, json_t request) {
        CURLcode ret;
        std::string json_dump = request.dump();
        curl_easy_setopt(hnd, CURLOPT_URL, string_format("0.0.0.0:%i/request", port).c_str());
        curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, json_dump.c_str());
        curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
        curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
        curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, servers_write_data);
        ret = curl_easy_perform(hnd);
        return ret == CURLE_OK;
    }

    bool Servers::ServerRequest(int port, json_t request) {
        return PerformSyncedRequest(port, request);
    }

    bool Servers::AsyncWriterRequest(json_t request) {
        return ServerRequest(4040, request);
    }

    bool Servers::AudioServerRequest(json_t request) {
        return ServerRequest(4045, request);
    }

    void Servers::InitializeCurl() {
        hnd = curl_easy_init();
    }

    void Servers::DestroyCurl() {
        if (hnd) {
            curl_easy_cleanup(hnd);
            hnd = nullptr;
        }
    }
}