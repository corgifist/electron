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

if (MSVC OR MSYS OR MINGW) 
  target_link_libraries(Electron opengl32 glfw3)
else()
  target_link_libraries(Electron glut glfw GL GLU m Xrandr Xi X11 pthread stdc++ dl)
endif()

target_compile_definitions(Electron PUBLIC GLEW_STATIC=1)
target_compile_features(Electron PRIVATE cxx_std_20)
add_compile_options(-m64)

if (NOT (MSVC OR MSYS OR MINGW))
  target_link_options(Electron PUBLIC -rdynamic)
endif()
