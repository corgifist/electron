import sys

target_file = sys.argv[1]

UI_CORE_HEADER_BEGIN = '''
#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS

#include "electron.h"
#include "ImGui/imgui.h"
#include "ImGui/imgui_stdlib.h"
#include "editor_core.h"
#include "graphics_core.h"
#include "ImGuiFileDialog.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/implot.h"

#if defined(WIN32) || defined(WIN64)
    #define UI_EXPORT __declspec(dllexport) __stdcall
#else
    #define UI_EXPORT
#endif
#define SHORTCUT(shortcut) ((Electron::AppInstance*) instance)->shortcuts. shortcut

extern "C" {
'''

UI_CORE_HEADER_END = '''
}
#undef UI_EXPORT
'''

with open("src/ui_core.cpp", "r+") as ui_core_file:
    ui_core_string = ui_core_file.read()
    ui_core_lines = ui_core_string.split("\n")
    ui_core_acc = UI_CORE_HEADER_BEGIN + "\n\n"

    brace_indentation = 0
    for line in ui_core_lines:
        if (line.split(" ")[0] == "HEADER_TARGET"):
            line = line.replace("{", ";").replace("HEADER_TARGET", "")
            ui_core_acc += "UI_EXPORT " + line + "\n"

    ui_core_acc += UI_CORE_HEADER_END
    print(ui_core_acc)
    
    with open(target_file, "w+") as result_file:
        result_file.write(ui_core_acc)