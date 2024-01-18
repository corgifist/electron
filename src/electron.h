#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#define GLM_FORCE_SWIZZLE
#define GLFW_INCLUDE_NONE

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
#include <unistd.h>
#include <condition_variable>
#include <signal.h>
#include <tuple>
#include <boost/process.hpp>
#include <utility>

#include "gles2.h"
#include "GLFW/glfw3.h"

#include "ImGui/imgui.h"

#include "json.hpp"
#include "dylib.hpp"
#include "IconsFontAwesome5.h"
#include "glm/glm.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"
#include <glm/gtc/type_ptr.hpp>

typedef std::vector<std::string> DylibRegistry;

#define print(expr) std::cout << expr << std::endl
#define JSON_AS_TYPE(x, type) (x).template get<type>()
#define ELECTRON_GET_LOCALIZATION(localization) (((Shared::localizationMap[localization]).template get<std::string>()).c_str())

#define DUMP_VAR(var) print(#var << " = " << (var))

#define SERVER_ERROR_CONST "-xfahgbso"
#define UNDEFINED 4178187

namespace Electron {

    typedef nlohmann::json json_t;

    typedef enum {
        _CloseEditor,
        _CloseWindow,
        _SpawnRenderPreview,
        _SpawnLayerProperties,
        _ReloadSystem,
        _SpawnAssetManager,
        _SpawnProjectConfiguration,
        _SpawnTimeline,
        _SpawnDockspace,
        _SpawnAssetExaminer,
        _TerminateServer,
        _A,
        _B,
        _None
    } Signal;

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

    static bool hasBegining(std::string const &fullString, std::string const &begining) {
        return fullString.rfind(begining, 0) == 0;
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
        float transformedFrame = (float) frame / (float) framerate;
        float minutes = glm::floor(transformedFrame / (float) 60);
        float seconds = glm::floor((int) transformedFrame % 60);
        return string_format("%02i:%02i", (int) minutes, (int) seconds);
    }

    static std::ifstream::pos_type filesize(const char* filename) {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg(); 
    }

    static bool process_is_alive(int pid) {
        #ifdef WIN32
            HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
            DWORD ret = WaitForSingleObject(process, 0);
            CloseHandle(process);
            return ret == WAIT_TIMEOUT;
        #else
            if (0 == kill(pid, 0)) {
                return true;
            }
            return false;
        #endif
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

    static std::vector<std::string> split_string(std::string s, std::string delimiter) {
        size_t pos_start = 0, pos_end, delim_len = delimiter.length();
        std::string token;
        std::vector<std::string> res;

        while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos) {
            token = s.substr (pos_start, pos_end - pos_start);
            pos_start = pos_end + delim_len;
            res.push_back (token);
        }

        res.push_back (s.substr (pos_start));
        return res;
    }

    // trim from start (in place)
    static inline void ltrim(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    }

    // trim from end (in place)
    static inline void rtrim(std::string &s) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), s.end());
    }

    // trim from both ends (in place)
    static inline void trim(std::string &s) {
        rtrim(s);
       ltrim(s);
    }

    // trim from start (copying)
    static inline std::string ltrim_copy(std::string s) {
        ltrim(s);
        return s;
    }

    // trim from end (copying)
    static inline std::string rtrim_copy(std::string s) {
        rtrim(s);
        return s;
    }

    // trim from both ends (copying)
    static inline std::string trim_copy(std::string s) {
        trim(s);
        return s;
    }

    static std::string getEnvVar(std::string key) {
        char * val = getenv( key.c_str() );
        return val == NULL ? std::string("") : std::string(val);
    }

    template<typename Func, typename Tup, std::size_t... index>
    decltype(auto) invoke_helper(Func&& func, Tup&& tup, std::index_sequence<index...>) {
        return func(std::get<index>(std::forward<Tup>(tup))...);
    }

    template<typename Func, typename Tup>
    decltype(auto) invoke(Func&& func, Tup&& tup) {
        constexpr auto Size = std::tuple_size<typename std::decay<Tup>::type>::value;
        return invoke_helper(std::forward<Func>(func),
                            std::forward<Tup>(tup),
                            std::make_index_sequence<Size>{});
    }

    static ImVec2 FitRectInRect(ImVec2 screen, ImVec2 rectangle) {
        ImVec2 dst = screen;
        ImVec2 src = rectangle;
        float scale = glm::min(dst.x / src.x, dst.y / src.y);
        return ImVec2{src.x * scale, src.y * scale};
    }

    static std::string filterFFProbe(std::string ffprobe) {
        std::string transformProbe = "";
        bool appendProbe = false;
        for (auto& line : split_string(ffprobe, "\n")) {
            if (line.find("Input") == 0) appendProbe = true;
            if (appendProbe) transformProbe += line + "\n";
        }
        return transformProbe;
    }

    template <typename T>
    bool IsInBounds(const T& value, const T& low, const T& high) {
        return !(value < low) && (value < high);
    }
}