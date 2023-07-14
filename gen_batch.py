import os
import sys
import platform

CPP_COMPILER = "g++" # change if you want
LIBRARY_EXTENSION = 'so' if 'Linux' in platform.platform() else 'dll'
target_cpp_file = sys.argv[1:][0]

def get_all_files_with_ext(path, ext):
    files = list()
    for file in os.listdir(path):
        if file.endswith("." + ext):
            files.append(os.path.join(path, file))
    return files

def compile_to_object_file(cpp_file):
    acc = CPP_COMPILER
    acc += f" -c impl/{cpp_file}.cpp -I src/ -g -fPIC -m64 -DELECTRON_IMPLEMENTATION_MODE -std=c++20"
    return acc

def compile_to_shared_library(cpp_file, compile_task):
    acc = CPP_COMPILER
    acc += f" -shared -o {'lib' if LIBRARY_EXTENSION == 'so' else ''}{cpp_file}.{LIBRARY_EXTENSION} {cpp_file}.o {'-lopengl32' if LIBRARY_EXTENSION == '.dll' else ''} -lstdc++ -ldl -fPIC -g -L. -m64"
    return acc


compile_task_list = get_all_files_with_ext("CMakeFiles/Electron.dir/src", "o")
compile_task_list = list(filter(lambda x: 'ui_core' not in x, compile_task_list))
compile_task_list = list(filter(lambda x: 'imgui' not in x, compile_task_list))
compile_task_list = list(filter(lambda x: 'ImGui' not in x, compile_task_list))

print(compile_task_list)

batch_result = ""
batch_result += compile_to_object_file(target_cpp_file) + "\n"
batch_result += compile_to_shared_library(target_cpp_file, compile_task_list) + "\n"
batch_result += f"rm {target_cpp_file}.o\n"

with open("compile_impl.sh", "w+") as f:
    f.write(batch_result)