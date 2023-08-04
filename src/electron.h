#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <vector>
#include <fstream>
#include <functional>
#include <any>
#include <cmath>
#include <filesystem>
#include <regex>
#include <sys/stat.h>
#include <variant>
#include "json.hpp"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"

#include "GLEW/include/GL/glew.h"
#include "GLFW/glfw3.h"

#define print(expr) std::cout << expr << std::endl
#define JSON_AS_TYPE(x, type) (x).template get<type>()
#define ELECTRON_GET_LOCALIZATION(instance, localization) (((instance->localizationMap[localization]).template get<std::string>()).c_str())

#define DUMP_VAR(var) print(#var << " = " << (var))

namespace Electron {

    typedef nlohmann::json json_t;

    typedef enum {
        ElectronSignal_CloseEditor,
        ElectronSignal_CloseWindow,
        ElectronSignal_SpawnRenderPreview,
        ElectronSignal_SpawnLayerProperties,
        ElectronSignal_ReloadSystem,
        ElectronSignal_SpawnAssetManager,
        ElectronSignal_A,
        ElectronSignal_B,
        ElectronSignal_None
    } ElectronSignal;


    std::string exec(const char* cmd);

    inline bool file_exists(const std::string& name) {
        struct stat buffer;   
        return (stat (name.c_str(), &buffer) == 0); 
    }

    static bool hasEnding (std::string const &fullString, std::string const &ending) {
        if (fullString.length() >= ending.length()) {
            return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
        } else {
            return false;
        }
    }

    static std::ifstream::pos_type filesize(const char* filename) {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg(); 
    }

    struct ElectronVector2f {
        float x, y;

        ElectronVector2f(float x, float y) {
            this->x = x;
            this->y = y;
        }

        ElectronVector2f() {}
    };

    struct Point {
        float x, y;

        Point(float x, float y) {
            this->x = x;
            this->y = y;
        }

        Point() {}
    };

    struct Rect {
        float x, y, w, h;
    
        Rect(float x, float y, float w, float h) {
            this->x = x;
            this->y = y;
            this->w = w;
            this->h = h;
        }

        Rect() {}

        bool contains(Point p);
    };
}