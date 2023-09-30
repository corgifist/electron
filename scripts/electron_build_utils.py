import hash_build_env

def flfs(l:list[str], s: str):
    return list(filter(lambda x: not s in x, l))

def filter_string_equals(l, s):
    return list(filter(lambda x: s in x, l))

hash_build_env.functions["filter_list_from_string"] = flfs
hash_build_env.functions["filter_string_equals"] = filter_string_equals