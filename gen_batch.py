import os
import sys

CPP_COMPILER = "g++" # change if you want
target_cpp_file = sys.argv[1:][0]

def get_all_files_with_ext(path, ext):
    files = list()
    for file in os.listdir(path):
        if file.endswith("." + ext):
            files.append(os.path.join(path, file))
    return files

def compile_to_object_file(cpp_file):
    acc = CPP_COMPILER
    acc += f" -c impl/{cpp_file}.cpp -I src/ -g -fPIC -m64 -DELECTRON_IMPLEMENTATION_MODE"
    return acc

def compile_to_shared_library(cpp_file, compile_task):
    acc = CPP_COMPILER
    acc += f" -shared -o {cpp_file}.dll {cpp_file}.o -lglfw3 -lopengl32 -fPIC -g -mwindows -L. -m64"
    return acc


compile_task_list = get_all_files_with_ext("CMakeFiles/Electron.dir/src", "o")
compile_task_list = list(filter(lambda x: 'ui_core' not in x, compile_task_list))
compile_task_list = list(filter(lambda x: 'imgui' not in x, compile_task_list))
compile_task_list = list(filter(lambda x: 'graphics' not in x, compile_task_list))
compile_task_list = list(filter(lambda x: 'ImGui' not in x, compile_task_list))

print(compile_task_list)

batch_result = "@echo off\n"
batch_result += compile_to_object_file(target_cpp_file) + "\n"
batch_result += compile_to_shared_library(target_cpp_file, compile_task_list) + "\n"

with open("compile_impl.bat", "w+") as f:
    f.write(batch_result)