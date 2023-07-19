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
#include "json.hpp"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"

#include "GLEW/include/GL/glew.h"
#include "GLFW/glfw3.h"

#define print(expr) std::cout << expr << std::endl
#define JSON_AS_TYPE(x, type) x.template get<type>()

#define DUMP_VAR(var) print(#var << " = " << (var))

namespace Electron {

    typedef nlohmann::json json_t;

    typedef enum {
        ElectronSignal_CloseEditor,
        ElectronSignal_CloseWindow,
        ElectronSignal_SpawnRenderPreview,
        ElectronSignal_A,
        ElectronSignal_B,
        ElectronSignal_None
    } ElectronSignal;

    struct ElectronVector2f {
        float x, y;

        ElectronVector2f(float x, float y) {
            this->x = x;
            this->y = y;
        }

        ElectronVector2f() {}
    };
}