# TODO: Local Plugin Support

## Current Understanding of the Task

The task is to fix the support for `local_plugin` tasks that are defined in the .ini file. Currently, there are two issues:

1. `mob list` doesn't show the tasks added by the local plugin (from the .ini file)
2. `mob build` doesn't build the tasks added by the local plugin (from the .ini file)

The problem is related to the order of operations in main.cpp, where tasks are loaded first and then the .ini file is loaded afterward. This means that the local plugin tasks defined in the .ini file aren't properly registered when the tasks are initially loaded.

## Detailed Analysis

After examining the codebase, I've identified the following key points:

1. In `main.cpp`, the `add_tasks()` function is called first to register all tasks, including the `local_plugins` task.
2. The `local_plugins` task is responsible for reading plugin information from the .ini file and registering each plugin as a separate task.
3. However, the .ini file is only loaded after the tasks are registered, which means the `local_plugins` task doesn't have access to the plugin information when it's first initialized.
4. The current implementation in `local_plugins.cpp` attempts to read from the `[local_plugins]` section of the main .ini file, but this doesn't work properly for `mob list` and `mob build local_plugin` commands.

## Proposed Solution

Create a separate .ini file specifically for local plugins:

1. Create a new file `mob.local_plugins.ini` that will be read directly by the `local_plugins` task.
2. Implement a simple parser in `local_plugins.cpp` that:
   - Reads the `mob.local_plugins.ini` file
   - Ignores comments (lines starting with ';' or '#')
   - Parses lines in the format `<plugin_name> = <plugin_path>`
   - Registers each plugin as a separate task

3. Update the `local_plugins` task to:
   - Read from this new .ini file during initialization
   - Register each plugin as a separate task with the task manager
   - Support the `mob build local_plugin` command to build all local plugins
   - Support the `mob build plugin_name` command to build a specific local plugin

This approach avoids having to restructure the order of operations in main.cpp, which would be complex and potentially introduce new issues.

## Implementation Steps

- [x] 1. Modify `local_plugins.cpp` to read from a separate `mob.local_plugins.ini` file instead of the main .ini file.
- [x] 2. Implement a simple parser for this file that ignores comments and parses plugin definitions.
- [x] 3. Update the `register_plugin_tasks()` method to register each plugin as a separate task with the task manager.
- [x] 4. Ensure that the `local_plugins` task properly handles the `mob build local_plugin` command.
- [ ] 5. Test the implementation with `mob list` and `mob build` commands.
