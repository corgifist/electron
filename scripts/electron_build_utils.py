import hash_build_env

def flfs(l:list[str], s: str):
    return list(filter(lambda x: not s in x, l))

hash_build_env.functions["filter_list_from_string"] = flfs