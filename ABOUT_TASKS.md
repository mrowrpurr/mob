# Understanding Tasks in MOB Build System

This document provides an overview of the task system in the MOB build tool for Mod Organizer 2.

## What are Tasks?

Tasks are the fundamental building blocks of the MOB build system. Each task represents a specific component or project that needs to be built as part of the Mod Organizer 2 ecosystem. Tasks handle downloading, extracting, configuring, building, and installing their respective components.

## Task Hierarchy

The MOB build system organizes tasks in a hierarchical structure:

1. **Base Task Class**: The `task` class is the ultimate base class for all tasks.
2. **Basic Task Template**: Most tasks inherit from `basic_task<T>`, which provides common functionality.
3. **Parallel Tasks**: The `parallel_tasks` class allows multiple tasks to run concurrently.
4. **Specific Task Implementations**: Individual tasks like `boost`, `python`, `zlib`, etc.

## Task Types

Tasks in the MOB build system can be categorized into several types:

1. **Third-party Tasks**: These build external dependencies like `boost`, `python`, `zlib`, etc.
2. **Super Tasks**: These build the core Mod Organizer 2 components, using the `modorganizer` task class.
3. **Other Tasks**: Special tasks like `translations`, `installer`, and `local_plugins`.

## Task Lifecycle

Each task goes through several phases during execution:

1. **Clean**: Removes previous build artifacts if needed (controlled by flags like `redownload`, `reextract`, etc.)
2. **Fetch**: Downloads or clones the required source code.
3. **Build and Install**: Compiles and installs the component.

## Task Implementation

Tasks are implemented by overriding three key methods:

```cpp
void do_clean(clean c);       // Clean previous build artifacts
void do_fetch();              // Download/clone source code
void do_build_and_install();  // Build and install
```

## Task Configuration

Tasks are configured through the MOB INI system. Each task can have its own section in the INI file, with options like:

- `enabled`: Whether the task is enabled.
- `configuration`: Build configuration (Debug, Release, RelWithDebInfo).
- Git-related options: `mo_org`, `mo_branch`, `no_pull`, etc.

## Task Dependencies

The MOB build system doesn't have a formal concept of task dependencies. Instead, it relies on task ordering. Tasks that depend on other tasks must be placed later in the build order. This ordering is defined in the `add_tasks()` function in `main.cpp`.

## Parallel Execution

The MOB build system uses the `parallel_tasks` class to run multiple tasks concurrently. Tasks that don't depend on each other can be grouped into a `parallel_tasks` instance to improve build times.

## Custom Tasks

The MOB build system allows for the creation of custom tasks. For example, the `local_plugins` task enables building local plugin projects alongside the main Mod Organizer 2 codebase.

## CMake Integration

Many tasks use CMake as their build system. The MOB build system provides a `cmake` tool class that wraps CMake functionality. Tasks can create and configure a `cmake` tool instance to build their components.

Key CMake-related functionality:

1. The `cmake` command in MOB can run CMake with the correct variables for MO2 projects.
2. The `modorganizer` task uses CMake to build MO2 components.
3. The `cmake_common` project provides common CMake functionality for MO2 projects.

## Task Manager

The `task_manager` class manages all tasks in the system. It provides functionality to:

1. Register tasks
2. Find tasks by name
3. Get a list of all tasks
4. Interrupt all tasks

## Example: Adding a New Task

To add a new task to the MOB build system:

1. Create a new task class that inherits from `basic_task<YourTask>`.
2. Implement the required methods: `do_clean()`, `do_fetch()`, and `do_build_and_install()`.
3. Add the task to the build order in the `add_tasks()` function in `main.cpp`.
4. Configure the task in the INI file.

## Example: Local Plugins Task

The `local_plugins` task demonstrates how to extend the MOB build system with custom functionality:

1. It reads a list of local plugin paths from the INI file.
2. It creates junction points in the `modorganizer_super` directory.
3. It builds each plugin using the task manager.

This allows developers to build their own plugins alongside the main Mod Organizer 2 codebase.
