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
        // This is a static function, but we need to access instance data
        // Since this is just a placeholder for basic_task, it's not actually used
        static fs::path dummy;
        return dummy;
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
        // Create a symlink from the modorganizer_super directory to the local directory
        fs::path super_path = modorganizer::super_path();
        fs::path link_path = super_path / name();
        
        cx().trace(context::generic, "creating symlink from {} to {}", link_path, local_path_);
        
        if (conf().global().dry())
            return;
            
        if (fs::exists(link_path)) {
            if (fs::is_symlink(link_path)) {
                cx().trace(context::generic, "symlink already exists, removing first");
                fs::remove(link_path);
            } else {
                cx().bail_out(context::generic, "cannot create symlink, path exists and is not a symlink: {}", link_path);
            }
        }
        
        std::error_code ec;
        
        // Try normal symlink creation first
        fs::create_directory_symlink(local_path_, link_path, ec);
        
        if (!ec)
            return;
            
        cx().trace(context::generic, "failed to create symlink: {}", ec.message());
        
        // If normal attempt failed, try with elevated privileges
        cx().trace(context::generic, "attempting to create symlink with elevated privileges");
        
        // Create a temporary batch file to create the symlink
        fs::path batch_path = make_temp_file();
        batch_path.replace_extension(".bat");
        
        std::string batch_content = 
            "@echo off\n"
            "echo Creating symlink with elevated privileges...\n"
            "mklink /D \"" + path_to_utf8(link_path) + "\" \"" + path_to_utf8(local_path_) + "\"\n";
            
        op::write_text_file(cx(), encodings::utf8, batch_path, batch_content);
        
        // Execute the batch file with elevated privileges
        SHELLEXECUTEINFOW sei = {0};
        sei.cbSize = sizeof(sei);
        sei.lpVerb = L"runas";  // Request elevation
        sei.lpFile = L"cmd.exe";
        std::wstring params = L"/c \"" + batch_path.wstring() + L"\"";
        sei.lpParameters = params.c_str();
        sei.nShow = SW_SHOW;
        
        if (!ShellExecuteExW(&sei)) {
            const auto e = GetLastError();
            cx().bail_out(context::generic, "failed to create symlink with elevated privileges: {}", error_message(e));
        }
        
        // Wait for the symlink to be created
        int attempts = 0;
        while (!fs::exists(link_path) && attempts < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            attempts++;
        }
        
        // Clean up the batch file
        op::delete_file(cx(), batch_path, op::optional);
        
        if (!fs::exists(link_path)) {
            cx().bail_out(context::generic, "failed to create symlink after elevation attempt");
        }
    }

    void local_modorganizer::do_build_and_install()
    {
        // Check if the directory has a CMakeLists.txt
        if (!fs::exists(source_path() / "CMakeLists.txt")) {
            cx().trace(context::generic, "{} has no CMakeLists.txt, not building", name());
            return;
        }

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
        return modorganizer::create_cmake_tool(source_path(), o, task_conf().configuration());
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
