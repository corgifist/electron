#define WITH_MINIAUDIO
#include "crow.h"
#include "editor_core.h"
#include "soloud/soloud.h"
#include "soloud/soloud_wav.h"
#include "soloud/soloud_thread.h"

using namespace Electron;

extern "C" {
    crow::SimpleApp* globalApp = nullptr;
    bool running = false;
    std::unordered_map<std::string, std::unique_ptr<SoLoud::Wav>> cache;
    SoLoud::Soloud instance;

    void Dummy() {}

    ELECTRON_EXPORT void ServerStart(int port, int pid) {
        crow::SimpleApp app;
        app.loglevel(crow::LogLevel::Warning);
        globalApp = &app;
        instance.init();
        auto thread = std::thread([](int pid) {
            while (true) {
                if (!process_is_alive(pid)) break;
                if (!running) break;
            }
            globalApp->stop();
            exit(0);
        }, pid);

        CROW_ROUTE(app, "/request")
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
                    if (cache.find(body["path"].s()) != cache.end()) goto cache_exists;
                    cache[body["path"].s()] = std::make_unique<SoLoud::Wav>(SoLoud::Wav());
                    cache[body["path"].s()]->load(std::string(body["path"].s()).c_str());
                    cache_exists:
                    Dummy();
                }
                if (action == "play_sample") {
                    result["handle"] = instance.play(*cache[body["path"].s()].get());
                }
                if (action == "stop_sample") {
                    instance.stop(body["handle"].i());
                }
                return crow::response(result.dump());
            });

        running = true;
        app.port(port)
            .bindaddr("0.0.0.0")
            .timeout(5)
            .concurrency(2)
            .run();

        thread.join();
        instance.deinit();
    }
}