# Understanding Tasks in MOB (Mod Organizer Build System)

## Overview

MOB (Mod Organizer Build System) is a build tool designed specifically for building Mod Organizer 2 and its various components. It manages the process of downloading, extracting, and building all the dependencies and components required for Mod Organizer 2.

## Task System

The core of MOB is its task system. Tasks represent individual components that need to be built, such as libraries, plugins, or tools. Each task is responsible for:

1. Fetching (downloading or cloning from git)
2. Building
3. Installing

### Task Types

There are several types of tasks in MOB:

1. **Basic Tasks**: These are the fundamental building blocks, like `boost`, `python`, `zlib`, etc.
2. **ModOrganizer Tasks**: These are specific to Mod Organizer components, like `uibase`, `game_gamebryo`, etc.
3. **Parallel Tasks**: These are groups of tasks that can be run in parallel.
4. **Local Plugins**: These are plugins that are developed locally and not fetched from git.

### Task Lifecycle

Each task goes through a specific lifecycle:

1. **Clean**: Removes previous build artifacts if needed.
2. **Fetch**: Downloads or clones the source code.
3. **Build and Install**: Compiles the code and installs it to the destination directory.

### Task Implementation

Tasks are implemented as C++ classes that inherit from the `task` class or `basic_task<T>` template. Each task must implement:

- `do_clean(clean c)`: Cleans the task based on the given flags.
- `do_fetch()`: Fetches the required files.
- `do_build_and_install()`: Builds and installs the task.

## CMake Integration

MOB uses CMake as its primary build system. It provides a `cmake` tool class that wraps CMake functionality:

```cpp
class cmake : public basic_process_runner {
public:
    // Generators supported by MOB
    enum class generators {
        vs = 0x01,    // Visual Studio
        jom = 0x02,   // JOM/NMake
        ninja = 0x04  // Ninja (supports compile_commands.json)
    };
    
    // Operations
    enum class ops {
        generate = 1, // Generate build files
        clean         // Clean build files
    };
    
    // ... other methods ...
};
```

The `cmake` tool supports different generators:
- **Visual Studio**: Generates `.sln` and `.vcxproj` files.
- **JOM/NMake**: Generates makefiles for JOM or NMake.
- **Ninja**: Generates Ninja build files, which also support generating `compile_commands.json` for better IDE integration.

## Building Local Plugins

MOB supports building local plugins that are not part of the main Mod Organizer repository. This is done through the `local_plugins` task:

1. Local plugins are defined in the `[local_plugins]` section of the `mob.ini` file.
2. The `local_plugins` task creates junction points in the `modorganizer_super` directory.
3. It then builds each plugin using the appropriate build system.

However, there are some limitations:
- MOB has safety checks that prevent operations outside the prefix directory.
- This can cause issues when trying to build plugins that are located outside the prefix.

## Command Line Interface

MOB provides a command-line interface for building tasks:

```
mob -d <prefix> build [task1 task2 ...]
```

Options:
- `-d <prefix>`: Sets the destination directory.
- `--redownload`: Re-downloads files.
- `--reextract`: Re-extracts archives.
- `--reconfigure`: Reconfigures the task.
- `--rebuild`: Cleans and rebuilds projects.
- `--new`: Implies all the four flags above.

## Conclusion

MOB is a powerful build system designed specifically for Mod Organizer 2. It manages the complex process of building all the dependencies and components required for Mod Organizer 2. The task system is the core of MOB, providing a flexible and extensible way to define and build components.
