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
    std::unordered_map<int, std::unique_ptr<SoLoud::Wav>> handles;
    SoLoud::Soloud instance;

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
                    int index = (int) body["handle"].i();
                    handles[index] = std::make_unique<SoLoud::Wav>(SoLoud::Wav());
                    handles[index]->load(std::string(body["path"].s()).c_str());
                }
                if (action == "play_sample") {
                    instance.play(*handles[body["handle"].i()].get());
                }
                if (action == "stop_sample") {
                    (*handles[body["handle"].i()].get()).stop();
                }
                return crow::response(result.dump());
            });

        running = true;
        app.port(port)
            .bindaddr("0.0.0.0")
            .timeout(5)
            .run();

        thread.join();
        instance.deinit();
    }
}