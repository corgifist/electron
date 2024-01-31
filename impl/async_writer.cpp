#include "utils/crow_all.h"
#include "editor_core.h"

using namespace Electron;

extern "C" {
    crow::SimpleApp* globalApp = nullptr;
    bool running = false;

    ELECTRON_EXPORT void ServerStart(int port, int pid) {
        crow::SimpleApp app;
        app.loglevel(crow::LogLevel::Warning);
        globalApp = &app;
        running = true;
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

        app.port(port)
            .bindaddr("0.0.0.0")
            .concurrency(2)
            .timeout(5)
            .run();

        thread.join();
    }
}