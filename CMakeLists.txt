cmake_minimum_required(VERSION 3.4)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

project(Electron)
file (GLOB_RECURSE ElectronSrc 
        "src/*.cpp"
        "src/*.c")

add_compile_options(-fPIC)
add_executable(Electron ${ElectronSrc})
target_link_options(Electron PUBLIC "-Wl,--export-all-symbols")

target_link_libraries(Electron -lglfw3)
target_link_libraries(Electron -lopengl32)
target_compile_definitions(Electron PUBLIC GLEW_STATIC=1)
add_compile_options(-m64)

add_link_options(-rdynamic)