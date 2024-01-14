#include "servers.h"

static size_t servers_write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)buffer, size * nmemb);
    return size * nmemb;
}

namespace Electron {

    static CURL* hnd = nullptr;
    ServerStructure Servers::audioSystem{};

    ServerResponse Servers::PerformSyncedRequest(int port, json_t request) {
        CURLcode ret;
        std::string json_dump = request.dump();
        std::string response;
        curl_easy_setopt(hnd, CURLOPT_URL, string_format("0.0.0.0:%i/request", port).c_str());
        curl_easy_setopt(hnd, CURLOPT_NOPROGRESS, 1);
        curl_easy_setopt(hnd, CURLOPT_POSTFIELDS, json_dump.c_str());
        curl_easy_setopt(hnd, CURLOPT_USERAGENT, "curl/7.38.0");
        curl_easy_setopt(hnd, CURLOPT_TCP_KEEPALIVE, 1L);
        curl_easy_setopt(hnd, CURLOPT_MAXREDIRS, 50L);
        curl_easy_setopt(hnd, CURLOPT_WRITEFUNCTION, servers_write_data);
        curl_easy_setopt(hnd, CURLOPT_WRITEDATA, &response);
        ret = curl_easy_perform(hnd);
        if (response == SERVER_ERROR_CONST) {
            throw std::runtime_error("uncaught exception occured while performing server request");
        }
        return ServerResponse(ret == CURLE_OK, response);
    }

    ServerResponse Servers::ServerRequest(int port, json_t request) {
        return PerformSyncedRequest(port, request);
    }

    ServerStructure Servers::LoadSystem(std::string library) {
        ServerStructure structure;
        Libraries::LoadLibrary("libs", library);
        structure.initialization = Libraries::GetFunction<void()>(library, "ServerInit");
        structure.termination = Libraries::GetFunction<void()>(library, "ServerTerminate");
        structure.dispatch = Libraries::GetFunction<json_t(json_t)>(library, "ServerDispatch");
        return structure;
    }

    ServerResponse Servers::AsyncWriterRequest(json_t request) {
        return ServerRequest(4040, request);
    }

    json_t Servers::AudioServerRequest(json_t request) {
        return audioSystem.dispatch(request);
    }

    void Servers::Initialize() {
        hnd = curl_easy_init();
        audioSystem = LoadSystem("audio_server");
        audioSystem.initialization();
    }

    void Servers::Destroy() {
        if (hnd) {
            curl_easy_cleanup(hnd);
            hnd = nullptr;
        }
        audioSystem.termination();
    }
}