import hash_build_env
from hash_build_interpreter import *
from hash_build_common import *

def interpret_master(block):
    for node in block:
        interpret(node)

def interpret(node):
    if (len(node)) == 0:
        return node

    if node[0] == 'assign':
        # [assign, name, value]
        hash_build_env.var_env[node[1]] = interpret(node[2])
    elif node[0] == 'scenario':
        # [scenario, name, ast]
        hash_build_env.scenarios[node[1]] = node[2]
    elif node[0] == 'call':
        # [call, callee, args]
        calle_key = node[1]
        interpreted_args = list()
        for arg in node[2]:
            interpreted_args.append(interpret(arg))
        return hash_build_env.arg_wrapper(hash_build_env.functions[calle_key], interpreted_args)
    elif node[0] == 'var':
        # [var, var_name]
        try:
            return hash_build_env.var_env[node[1]]
        except KeyError:
            fatal(f"hash_build_interpreter: no variable named {node[1]}")
            exit(1)
    elif node[0] == 'if':
        # [if, expr, block, else_block]
        if interpret(node[1]):
            interpret_master(node[2])
        elif node[3] != None:
            interpret_master(node[3])
    
    return node