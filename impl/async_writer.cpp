#include "editor_core.h"
#include "crow.h"

using namespace Electron;

extern "C" {
    crow::SimpleApp* globalApp = nullptr;
    bool running = false;

    ELECTRON_EXPORT void ServerStart(int port, int pid) {
        crow::SimpleApp app;
        app.loglevel(crow::LogLevel::Warning);
        globalApp = &app;
        auto thread = std::thread([](int pid) {
            while (true) {
                if (!process_is_alive(pid)) break;
                if (!running) break;
            }
            globalApp->stop();
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
                if (action == "write") {
                    std::ofstream file(body["path"].s());
                    file << std::string(body["content"].s());
                    file.close();
                    result = {
                        "ok"
                    };
                }
                return crow::response(result.dump());
            });

        running = true;
        app.port(port)
            .bindaddr("0.0.0.0")
            .multithreaded()
            .timeout(5)
            .run();

        thread.join();
    }
}