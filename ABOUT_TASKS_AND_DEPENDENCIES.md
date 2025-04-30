# Understanding Task Dependencies in MOB Build System

This document provides a detailed explanation of how task dependencies are managed in the MOB build tool for Mod Organizer 2.

## Task Dependency Management

Unlike traditional build systems that use explicit dependency declarations, the MOB build system uses a sequential ordering approach to manage dependencies between tasks. This document explains how this works and how to understand the dependency structure.

## Sequential Ordering vs. Explicit Dependencies

In many build systems, dependencies are explicitly declared. For example, in a Makefile, you might see:

```makefile
target: dependency1 dependency2
```

The MOB build system takes a different approach. Instead of explicitly declaring dependencies, it relies on the order in which tasks are added to the build queue. Tasks that depend on other tasks must be placed later in the build order.

## How Dependencies are Defined

Dependencies are defined in the `add_tasks()` function in `main.cpp`. This function creates a sequence of tasks to be executed. Tasks that depend on other tasks are added later in the sequence.

For example, in `main.cpp`, we see:

```cpp
add_task<parallel_tasks>()
    .add_task<sevenz>()
    .add_task<zlib>()
    .add_task<gtest>()
    .add_task<libbsarch>()
    .add_task<libloot>()
    .add_task<openssl>()
    .add_task<bzip2>()
    .add_task<directxtex>();

add_task<parallel_tasks>()
    .add_task<tasks::python>()
    .add_task<lz4>()
    .add_task<spdlog>();
```

This means that `python`, `lz4`, and `spdlog` will be built after `sevenz`, `zlib`, `gtest`, etc. If `python` depends on `zlib`, this ordering ensures that `zlib` is built before `python`.

## Parallel Tasks

The MOB build system uses the `parallel_tasks` class to run multiple tasks concurrently. Tasks within a `parallel_tasks` instance are run in parallel, but each `parallel_tasks` instance is run sequentially.

```cpp
add_task<parallel_tasks>()
    .add_task<task1>()
    .add_task<task2>()
    .add_task<task3>();

add_task<parallel_tasks>()
    .add_task<task4>()
    .add_task<task5>();
```

In this example, `task1`, `task2`, and `task3` are run in parallel. Once all of them are complete, `task4` and `task5` are run in parallel.

## Dependency Examples

Let's look at some specific examples of dependencies in the MOB build system:

1. **Python Dependencies**: The `python` task depends on `zlib` and `bzip2`. These dependencies are managed by ensuring that `zlib` and `bzip2` are built before `python`.

2. **PyQt Dependencies**: The `pyqt` task depends on `python` and `sip`. These dependencies are managed by ensuring that `python` and `sip` are built before `pyqt`.

3. **Mod Organizer Dependencies**: The various Mod Organizer tasks depend on third-party libraries like `boost`, `lz4`, `spdlog`, etc. These dependencies are managed by ensuring that the third-party libraries are built before the Mod Organizer tasks.

## Manual Dependency Specification

When building specific tasks manually, dependencies are not automatically built. For example, if you run `mob build pyqt`, the `python` and `sip` dependencies will not be built automatically. You would need to run `mob build python sip pyqt` to build all the required components.

## Dependency Resolution in the Build Process

During the build process, the MOB build system follows these steps to resolve dependencies:

1. It starts with the first task in the sequence.
2. It checks if the task is enabled.
3. If the task is enabled, it runs the task's `clean`, `fetch`, and `build_and_install` methods as needed.
4. It moves to the next task in the sequence.

This sequential approach ensures that dependencies are built before the tasks that depend on them, as long as the tasks are ordered correctly in the `add_tasks()` function.

## Dependency Visualization

To visualize the dependencies between tasks, you can use the `mob list --all` command. This command shows a task tree that illustrates which tasks are built in parallel and which are built sequentially.

## Adding New Dependencies

When adding a new task to the MOB build system, you need to consider its dependencies and place it appropriately in the build order. If your new task depends on existing tasks, it should be added after those tasks in the `add_tasks()` function.

## Conclusion

The MOB build system's approach to dependency management is simple but effective. By ordering tasks correctly in the build sequence, it ensures that dependencies are built before the tasks that depend on them. This approach avoids the complexity of explicit dependency declarations while still providing a reliable build process.
