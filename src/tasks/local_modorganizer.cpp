#include "pch.h"
#include "tasks.h"

namespace mob::tasks {


    local_modorganizer::local_modorganizer(std::string name, fs::path local_path, bool is_gamebryo)
        : basic_task(std::move(name)), local_path_(std::move(local_path)), is_gamebryo_(is_gamebryo)
    {
    }

    // Static functions required by basic_task
    fs::path local_modorganizer::source_path()
    {
        // In a static method, we can't access instance data
        // Return the super path, which will be used as the base for the symlink
        return modorganizer::super_path();
    }

    bool local_modorganizer::prebuilt()
    {
        return false;
    }

    void local_modorganizer::do_clean(clean c)
    {
        // delete the whole directory
        if (is_set(c, clean::reclone)) {
            // Just remove the symlink, not the actual source directory
            if (fs::exists(source_path()) && fs::is_symlink(source_path())) {
                cx().trace(context::generic, "removing symlink {}", source_path());
                fs::remove(source_path());
            }
            return;
        }

        // cmake clean
        if (is_set(c, clean::reconfigure))
            run_tool(create_cmake_tool(cmake::clean));

        // msbuild clean
        if (is_set(c, clean::rebuild))
            run_tool(create_msbuild_tool(msbuild::clean));
    }

    void local_modorganizer::do_fetch()
    {
        // Skip symlink creation and just use the local path directly
        cx().info(context::generic, "Using local path directly: {}", local_path_.string());
        
        // Make sure the local path exists
        if (!fs::exists(local_path_)) {
            cx().warning(context::generic, "Local path does not exist: {}", local_path_.string());
            return;
        }
        
        // Check if the local path has a CMakeLists.txt
        if (!fs::exists(local_path_ / "CMakeLists.txt")) {
            cx().warning(context::generic, "Local path does not have a CMakeLists.txt: {}", local_path_.string());
        } else {
            cx().info(context::generic, "Found CMakeLists.txt in local path");
        }
    }

    void local_modorganizer::do_build_and_install()
    {
        // Check if the directory has a CMakeLists.txt
        if (!fs::exists(local_path_ / "CMakeLists.txt")) {
            cx().info(context::generic, "{} has no CMakeLists.txt, not building", name());
            return;
        }

        cx().info(context::generic, "Building {} from {}", name(), local_path_.string());

        // Run cmake
        run_tool(create_cmake_tool());

        // Run restore for nuget if needed
        if (is_gamebryo_)
            run_tool(create_msbuild_tool().targets({"restore"}));

        // Run msbuild
        run_tool(create_msbuild_tool());
    }

    cmake local_modorganizer::create_cmake_tool(cmake::ops o)
    {
        return modorganizer::create_cmake_tool(local_path_, o, task_conf().configuration());
    }

    msbuild local_modorganizer::create_msbuild_tool(msbuild::ops o)
    {
        return std::move(msbuild(o)
                        .solution(project_file_path())
                        .configuration(task_conf().configuration())
                        .architecture(arch::x64));
    }

    fs::path local_modorganizer::project_file_path()
    {
        // Ask cmake for the build path it would use
        const auto build_path = create_cmake_tool().build_path();
        
        // Use the project name as the solution name
        return build_path / (name() + ".sln");
    }

} // namespace mob::tasks
