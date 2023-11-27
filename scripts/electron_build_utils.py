import hash_build_env
import os

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

hash_build_env.functions["filter_list_from_string"] = flfs
hash_build_env.functions["filter_string_equals"] = filter_string_equals
hash_build_env.functions["abspath"] = abspath
hash_build_env.functions["write_build_number"] = write_build_number