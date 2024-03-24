import hash_build_env
import glob, os, shutil, hashlib

def flfs(l:list[str], s: str):
    return list(filter(lambda x: not s in x, l))

def filter_string_equals(l, s):
    return list(filter(lambda x: s in x, l))

def abspath(x):
    return str(os.path.abspath(x))

def write_build_number():
    bn_content = open("src/build_number.h", "r").read()
    with open("src/build_number.h", "w+") as bn_file:
        print(bn_content)
        bn_lines = bn_content.split("\n")
        bn_second_parts = bn_lines[1].split(" ")
        incremented_number = int(bn_second_parts[2]) + 1
        bn_file.write("#pragma once\n#define BUILD_NUMBER " + str(incremented_number))

def move_shared_libs_to_folder():
    libs = []
    for file in glob.glob("*.*"):
        libs.append(str(file))
    libs = list(filter(lambda x: ".so" in x, libs))
    for file in libs:
        shutil.move(file, f"libs/{file}")

def compile_shader_blobs():
    for pipeline in glob.glob("compute/spirv/*.pipeline"):
        print(f"compiling pipeline {pipeline}")
        with open(pipeline, "r+") as pipeline_file:
            pipeline_code = pipeline_file.read()
            pipeline_lines = pipeline_code.split("\n")
            vertex_code = ""
            fragment_code = ""
            append_state = "#vertex"
            for pipeline_line in pipeline_lines:
                if "#vertex" in pipeline_line:
                    append_state = "#vertex"
                elif "#fragment" in pipeline_line:
                    append_state = "#fragment"
                elif append_state == "#vertex":
                    vertex_code += pipeline_line + "\n"
                elif append_state == "#fragment":
                    fragment_code += pipeline_line + "\n"

            if vertex_code != "":
                print(vertex_code)
                vertex_temp_path = "compute/spirv/" + str(pathlib.Path(pipeline).stem) + ".vert"
                with open(vertex_temp_path, "w+") as vertex_spirv_file:
                    vertex_spirv_file.write(vertex_code)
                if os.system(f"glslc {vertex_temp_path} -o {vertex_temp_path + '.spv'}") != 0:
                    print("vertex shader compilation failure! exiting...")
                    os.remove(vertex_temp_path)
                    exit(1)
                os.remove(vertex_temp_path)
            if fragment_code != "":
                print(fragment_code)
                fragment_temp_path = "compute/spirv/" + str(pathlib.Path(pipeline).stem) + ".frag"
                with open(fragment_temp_path, "w+") as fragment_spirv_file:
                    fragment_spirv_file.write(fragment_code)
                if os.system(f"glslc {fragment_temp_path} -o {fragment_temp_path + '.spv'}") != 0:
                    print("fragment shader compilation failure! exiting...")
                    os.remove(fragment_temp_path)
                    exit(1)
                os.remove(fragment_temp_path)

def read_file(path):
    with open(path, "r+") as file:
        return file.read()
    
def write_file(path, str):
    with open(path, "w+") as file:
        file.write(str)
    
def hash_string(str):
    return hashlib.blake2s(str.encode()).hexdigest()

hash_build_env.functions["filter_list_from_string"] = flfs
hash_build_env.functions["filter_string_equals"] = filter_string_equals
hash_build_env.functions["abspath"] = abspath
hash_build_env.functions["write_build_number"] = write_build_number
hash_build_env.functions["move_shared_libs"] = move_shared_libs_to_folder
hash_build_env.functions["compile_shader_blobs"] = compile_shader_blobs
hash_build_env.functions["read_file"] = read_file
hash_build_env.functions["hash_string"] = hash_string
hash_build_env.functions["write_file"] = write_file