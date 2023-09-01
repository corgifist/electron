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
#include <locale>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <thread>
#include <future>
#include <cstdlib>
#include <time.h>
#include <chrono>
#include "json.hpp"
#include "clip.h"
#include "dylib.hpp"
#include "IconsFontAwesome5.h"
#define GLM_FORCE_SWIZZLE
#include "glm/glm.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"

#include "GLEW/include/GL/glew.h"
#include "GLFW/glfw3.h"


typedef std::unordered_map<std::string, dylib> DylibRegistry;

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
        ElectronSignal_SpawnTimeline,
        ElectronSignal_A,
        ElectronSignal_B,
        ElectronSignal_None
    } ElectronSignal;


    std::string exec(const char* cmd);

    inline bool file_exists(const std::string& name) {
        return std::filesystem::exists(name); 
    }

    static bool hasEnding (std::string const &fullString, std::string const &ending) {
        if (fullString.length() >= ending.length()) {
            return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));
        } else {
            return false;
        }
    }

    template<typename T>
    inline bool future_is_ready(std::future<T>& t){
        return t.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    template<class T>
    static T base_name(T const & path, T const & delims = "/\\") {
        return path.substr(path.find_last_of(delims) + 1);
    }

    
    template<typename ... Args>
    static std::string string_format( const std::string& format, Args ... args ) {
        int size_s = std::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
        if( size_s <= 0 ){ throw std::runtime_error( "Error during formatting." ); }
        auto size = static_cast<size_t>( size_s );
        std::unique_ptr<char[]> buf( new char[ size ] );
        std::snprintf( buf.get(), size, format.c_str(), args ... );
        return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
    }

    static std::string read_file(const std::string& filename) {
        std::string buffer;
        std::ifstream in(filename.c_str(), std::ios_base::binary | std::ios_base::ate);
        in.exceptions(std::ios_base::badbit | std::ios_base::failbit | std::ios_base::eofbit);
        buffer.resize(in.tellg());
        in.seekg(0, std::ios_base::beg);
        in.read(&buffer[0], buffer.size());
        return buffer;
    }
    
    template <typename T>
    std::basic_string<T> lowercase(const std::basic_string<T>& s) {
        std::basic_string<T> s2 = s;
        std::transform(s2.begin(), s2.end(), s2.begin(),
            [](const T v){ return static_cast<T>(std::tolower(v)); });
        return s2;
    }

    template <typename T>
    std::basic_string<T> uppercase(const std::basic_string<T>& s) {
        std::basic_string<T> s2 = s;
        std::transform(s2.begin(), s2.end(), s2.begin(),
            [](const T v){ return static_cast<T>(std::toupper(v)); });
        return s2;
    }

    static std::string formatToTimestamp(int frame, int framerate) {
        float transformedFrame = (float) frame / 60.0f;
        float minutes = glm::floor(transformedFrame / (float) framerate);
        float seconds = glm::floor((int) transformedFrame % 60);
        return string_format("%02i:%02i", (int) minutes, (int) seconds);
    }

    static std::ifstream::pos_type filesize(const char* filename) {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg(); 
    }

    static std::string intToHex(int x) {
        std::stringstream stream;
        stream << std::hex << x;
        return stream.str();
    }

    static float precision( float f, int places ) {
        float n = std::pow(10.0f, places ) ;
        return std::round(f * n) / n ;
    }

    static int hexToInt(std::string hex) {
        int x;   
        std::stringstream ss;
        ss << std::hex << hex;
        ss >> x;
        return x;
    }

    static int seedrand() {
        srand(time(NULL));
        return rand();
    }

    static void update_log() {
        std::stringstream sstream;
        std::cout.rdbuf(sstream.rdbuf());
        
        std::ofstream file("electron.log");
        file << sstream.str();
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