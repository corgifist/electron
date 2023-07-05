import sys

target_api_file = sys.argv[1]

BEGIN_UI_API = '''
#include "electron.h"
#include "ImGui/imgui_internal.h"

#define CSTR(str) (str).c_str()

#define IMPLEMENT_UI_WRAPPER(ui_proc_name, args_signature, call_signature, typedef_args, fn_type) \
    typedef fn_type (__stdcall *ui_proc_name ## _T) typedef_args; \
    fn_type ui_proc_name args_signature { \
        HINSTANCE implLib = IMPL_LIBRARY; \
        if (!implLib) throw std::runtime_error(std::string("failed to load impl library in ui wrapper ") + #ui_proc_name); \
        ui_proc_name ## _T proc = (ui_proc_name ## _T) GetProcAddress(implLib, #ui_proc_name); \
        if (!proc) throw std::runtime_error(std::string("failed to load proc from impl library in ui wrapper ") + #ui_proc_name); \
        fn_type ret_val = proc call_signature; \
        return ret_val; \
    }

#define IMPLEMENT_UI_VOID_WRAPPER(ui_proc_name, args_signature, call_signature, typedef_args) \
    typedef void (__stdcall *ui_proc_name ## _T) typedef_args; \
    void ui_proc_name args_signature { \
        HINSTANCE implLib = IMPL_LIBRARY; \
        if (!implLib) throw std::runtime_error(std::string("failed to load impl library in ui wrapper ") + #ui_proc_name); \
        ui_proc_name ## _T proc = (ui_proc_name ## _T) GetProcAddress(implLib, #ui_proc_name); \
        if (!proc) throw std::runtime_error(std::string("failed to load proc from impl library in ui wrapper ") + #ui_proc_name); \
        proc call_signature; \
    }

static HINSTANCE IMPL_LIBRARY = GetModuleHandle(NULL);

static std::string ElectronImplTag(const char* name, void* ptr) {
    std::string temp = name;
    temp += "##";
    temp += std::to_string((long long) ptr);
    return temp;
}

'''

class ArgSignature:
    def __init__(self, name, type):
        self.name = name
        self.type = type

    def __repr__(self):
        return f'ArgSignature {self.name} {self.type}'

class WrapperSignature:
    def __init__(self, return_type, name, args):
        self.return_type = return_type
        self.name = name
        self.args = args

    def __repr__(self):
        return f"WrapperSignature {self.return_type} {self.name} {self.args}"

with open("src/ui_core.h", "r+") as header_file:
    header_string = header_file.read().replace("const char*", "string_typedef")
    header_parts = header_string.split("\n")
    header_parts = list(map(lambda x: x.strip(), header_parts))
    header_parts = list(filter(lambda x: x.split(" ")[0] == "UI_EXPORT", header_parts))

    methods_register = list()
    
    for line in header_parts:
        line = line \
            .replace("(", " ") \
            .replace(")", " ") \
            .replace(", ", " ") \
            .replace("UI_EXPORT", " ") \
            .replace(";", "") \
            .replace("const ", "const").strip()
        line_parts = line.split(" ")

        args_signature = []
        args_pos = 2
        while args_pos < len(line_parts):
            args_signature.append(ArgSignature(line_parts[args_pos + 1], line_parts[args_pos]))
            args_pos += 2

        signature = WrapperSignature(line_parts[0], line_parts[1], args_signature)
        methods_register.append(signature)
    
    ui_api_string = BEGIN_UI_API + "\n\n"

    for method in methods_register:
        named_args_string = "("
        arg_index = 0
        for arg in method.args:
            last = arg_index + 1 == len(method.args)
            named_args_string += arg.type + " " + arg.name
            if (not last):
                named_args_string += "," 
            arg_index += 1
        named_args_string += ")"
        named_args_string = named_args_string.replace("const", "const ")

        argumented_args_string = "("
        arg_index = 0
        for arg in method.args:
            last = arg_index + 1 == len(method.args)
            argumented_args_string += arg.name
            if (not last):
                argumented_args_string += " ,"
            arg_index += 1
        argumented_args_string += ")"

        typed_args_string = "("
        arg_index = 0
        for arg in method.args:
            last = arg_index + 1 == len(method.args)
            typed_args_string += arg.type
            if (not last):
                typed_args_string += " ,"
            arg_index += 1
        typed_args_string += ")"
        typed_args_string = typed_args_string.replace("const", "const ")

        if (method.return_type == "void"):
            ui_api_string += f'IMPLEMENT_UI_VOID_WRAPPER({method.name}, {named_args_string}, {argumented_args_string}, {typed_args_string})\n'
        else:
            ui_api_string += f'IMPLEMENT_UI_WRAPPER({method.name}, {named_args_string}, {argumented_args_string}, {typed_args_string}, {method.return_type})\n'


    ui_api_string = ui_api_string.replace("string_typedef", "const char*")
    print(ui_api_string)
    with open(target_api_file, "w+") as ui_api_file:
        ui_api_file.write(ui_api_string)