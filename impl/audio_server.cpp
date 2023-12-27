#include "crow_all.h"
#include "editor_core.h"
#include "OALWrapper/OAL_Funcs.h"
#include "OALWrapper/OAL_Sample.h"

using namespace Electron;

extern "C" {
    crow::SimpleApp* globalApp = nullptr;
    bool running = false;
    std::unordered_map<std::string, cOAL_Sample*> wavRegistry{};


    ELECTRON_EXPORT void ServerStart(int port, int pid) {
        globalApp = new crow::SimpleApp();
        globalApp->loglevel(crow::LogLevel::Warning);
        running = true;
        auto thread = std::thread([](int pid) {
            while (true) {
                if (!process_is_alive(pid)) break;
                if (!running) break;
            }
            globalApp->stop();
            exit(0);
        }, pid);
        
        cOAL_Init_Params params;
        if (!OAL_Init(params)) {
            throw std::runtime_error("(audio-server) cannot initialize OpenAL");
        }
        DUMP_VAR(OAL_Info_GetDeviceName());

        CROW_ROUTE((*globalApp), "/request")
            .methods("POST"_method)([](const crow::request& req) {
                try {
                auto body = crow::json::load(req.body);
                if (!body) {
                    return crow::response(400);
                }
                std::string action = body["action"].s();
                crow::json::wvalue result = {};
                if (action == "kill") {
                    OAL_Close();
                    globalApp->stop();
                    running = false;
                }
                if (action == "load_sample") {
                    if (wavRegistry.find(body["path"].s()) == wavRegistry.end() || body.count("override") != 0) {
                        if (wavRegistry.find(body["path"].s()) != wavRegistry.end()) {
                            OAL_Sample_Unload(wavRegistry[body["path"].s()]);
                        }
                        wavRegistry[body["path"].s()] = OAL_Sample_Load(body["path"].s());
                    }
                }
                if (action == "play_sample") {
                    result["handle"] = OAL_Sample_Play(OAL_FREE, wavRegistry[body["path"].s()], 1.0f, true, 1);
                }
                if (action == "pause_sample") {
                    OAL_Source_SetPaused(body["handle"].i(), body["pause"].b());
                }
                if (action == "stop_sample") {
                    OAL_Source_Stop(body["handle"].i());
                }
                if (action == "seek_sample") {
                    OAL_Source_SetElapsedTime(body["handle"].i(), std::max(body["seek"].d(), 0.0));
                }
                return crow::response(result.dump());
                } catch (std::runtime_error err) {
                    return crow::response(SERVER_ERROR_CONST);
                }
        });


        globalApp->port(port)
            .bindaddr("0.0.0.0")
            .timeout(5)
            .concurrency(2)
            .run();
        thread.join();
    }
}