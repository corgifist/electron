#include "crow.h"
#include "editor_core.h"
#include "soloud/soloud.h"
#include "soloud/soloud_wav.h"

using namespace Electron;

extern "C" {
    crow::SimpleApp* globalApp = nullptr;
    bool running = false;
    std::unordered_map<std::string, SoLoud::Wav> cache;
    SoLoud::Soloud* globalInstance;

    void Dummy() {}

    ELECTRON_EXPORT void ServerStart(int port, int pid) {
        globalApp = new crow::SimpleApp();
        globalApp->loglevel(crow::LogLevel::Warning);
        globalInstance = new SoLoud::Soloud();
        globalInstance->init();
        DUMP_VAR(globalInstance->getBackendString());
        auto thread = std::thread([](int pid) {
            while (true) {
                if (!process_is_alive(pid)) break;
                if (!running) break;
            }
            globalApp->stop();
            exit(0);
        }, pid);

        CROW_ROUTE((*globalApp), "/request")
            .methods("POST"_method)([](const crow::request& req) {
                auto body = crow::json::load(req.body);
                if (!body) {
                    return crow::response(400);
                }
                std::string action = body["action"].s();
                crow::json::wvalue result = {};
                if (action == "kill") {
                    globalApp->stop();
                    running = false;
                }
                if (action == "load_sample") {
                    DUMP_VAR(std::string(body["path"].s()));
                    if (cache.find(body["path"].s()) != cache.end() && body.count("override") == 0) goto cache_exists;
                    cache[body["path"].s()] = SoLoud::Wav();
                    cache[body["path"].s()].load(std::string(body["path"].s()).c_str());
                    cache_exists:
                    Dummy();
                }
                if (action == "play_sample") {
                    result["handle"] = globalInstance->play(cache[body["path"].s()]);
                }
                if (action == "pause_sample") {
                    globalInstance->setPause(body["handle"].i(), body["pause"].b());
                }
                if (action == "stop_sample") {
                    globalInstance->stop(body["handle"].i());
                }
                if (action == "seek_sample") {
                    globalInstance->seek(body["handle"].i(), body["seek"].d());
                }
                return crow::response(result.dump());
            });

        running = true;
        globalApp->port(port)
            .bindaddr("0.0.0.0")
            .timeout(5)
            .concurrency(2)
            .run();
        thread.join();
    }
}