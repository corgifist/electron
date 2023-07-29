# HashBuild
HashBuild is a minimalistic build system written in Python that can provide full control of compiling process
It uses Hashing algorithms to decide which files to compile and which don't

## Brief Documentation
HashBuild loads `hash_build` script and executes it's `default_scenario`.
Scenario is simply a sequence of actions.

Here is the example `hash_build` script:
```
# this is a comment

# defining some variables
build_arch = "64"
executable_name = "MathTest.exe"

scenario build {
    print("HashBuild test") # print some text
    print(cat("Build arch: ", $build_arch)) # cat(a, b) is used for concatenating strings or lists

    # glob_files(folder, extension) returns all files from directory with selected extension
    # object_compile(objects, build_arch) compiles all source code files and returns list of compiled .o files
    objects = object_compile(glob_files("src", "cpp"), $build_arch) 
    print($objects)

    deps = list() # create list
    list_append($deps, glfw3) # include glfw3 library in build (just for example)

    # link_executable(objects, output_exec_name, deps) links all object files together to create a working executable file
    link_executable($objects, $executable_name, $deps)
}

# Another scenario for cleaning purposes
scenario clean {
    print("Cleaning...")

    # removes all object files
    object_clean()
}

# 'build' is now a default scenario
default_scenario = build
```

It is recommended to clean object files when changing linking deps.

Use this command to run specific scenario without chaning `default_scenario` variable
```
python hash_build.py -dscenario_name
```

Work In Progress...
