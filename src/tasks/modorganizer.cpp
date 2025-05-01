#include "pch.h"
#include "tasks.h"
#include "../core/process.h"

namespace mob::tasks {

    // given a vector of names (some projects have more than one, see add_tasks() in
    // main.cpp), this prepends the simplified name to the vector and returns it
    //
    // most MO project names are something like "modorganizer-uibase" on github and
    // the simplified name is used for two main reasons:
    //
    //  1) individual directories in modorganizer_super have historically used the
    //     simplified name only
    //
    //  2) it's useful to have an simplified name for use on the command line
    //
    std::vector<std::string> make_names(std::vector<std::string> names)
    {
        // first name in the list might be a "modorganizer-something"
        const auto main_name = names[0];

        const auto dash = main_name.find("-");
        if (dash != std::string::npos) {
            // remove the part before the dash and the dash
            names.insert(names.begin(), main_name.substr(dash + 1));
        }

        return names;
    }

    // creates the repo in modorganizer_super, used to add submodules
    //
    // only one task will end up past the mutex and the flag, so it's only done
    // once
    //
    void initialize_super(context& cx, const fs::path& super_root)
    {
        static std::mutex mutex;
        static bool initialized = false;

        std::scoped_lock lock(mutex);
        if (initialized)
            return;

        initialized = true;

        cx.trace(context::generic, "checking super");

        git_wrap g(super_root);

        // happens when running mob again in the same build tree
        if (g.is_git_repo()) {
            cx.debug(context::generic, "super already initialized");
            return;
        }

        // create empty repo
        cx.trace(context::generic, "initializing super");
        g.init_repo();
    }

    modorganizer::modorganizer(std::string long_name, flags f)
        : modorganizer(std::vector<std::string>{long_name}, f)
    {
    }

    modorganizer::modorganizer(std::vector<const char*> names, flags f)
        : modorganizer(std::vector<std::string>(names.begin(), names.end()), f)
    {
    }

    modorganizer::modorganizer(std::vector<std::string> names, flags f)
        : task(make_names(names)), repo_(names[0]), flags_(f)
    {
        if (names.size() > 1) {
            project_ = names[1];
        }
        else {
            project_ = make_names(names)[0];
        }
    }
    
    modorganizer::modorganizer(std::string name, const fs::path& local_path, flags f)
        : task({name}), repo_(name), flags_(static_cast<flags>(f | local)), local_path_(local_path)
    {
        project_ = name;
    }

    bool modorganizer::is_gamebryo_plugin() const
    {
        return is_set(flags_, gamebryo);
    }

    bool modorganizer::is_nuget_plugin() const
    {
        return is_set(flags_, nuget);
    }
    
    bool modorganizer::is_local_plugin() const
    {
        return is_set(flags_, local);
    }

    fs::path modorganizer::source_path() const
    {
        // something like build/modorganizer_super/uibase
        return super_path() / name();
    }

    fs::path modorganizer::project_file_path() const
    {
        // ask cmake for the build path it would use
        const auto build_path = create_cmake_tool(source_path()).build_path();

        // use the INSTALL project
        return build_path / (project_ + ".sln");
    }

    fs::path modorganizer::super_path()
    {
        return conf().path().build() / "modorganizer_super";
    }

    url modorganizer::git_url() const
    {
        return make_git_url(task_conf().mo_org(), repo_);
    }

    std::string modorganizer::org() const
    {
        return task_conf().mo_org();
    }

    std::string modorganizer::repo() const
    {
        return repo_;
    }

    void modorganizer::do_clean(clean c)
    {
        // delete the whole directory
        if (is_set(c, clean::reclone)) {
            git_wrap::delete_directory(cx(), source_path());

            // no need to do anything else
            return;
        }

        // cmake clean
        if (is_set(c, clean::reconfigure))
            run_tool(create_cmake_tool(cmake::clean));

        // msbuild clean
        if (is_set(c, clean::rebuild))
            run_tool(create_msbuild_tool(msbuild::clean));
    }

    void modorganizer::do_fetch()
    {
        // make sure the super directory is initialized, only done once
        initialize_super(cx(), super_path());

        if (is_local_plugin()) {
            // For local plugins, we don't need to fetch anything from git
            // The junction point is already created by the local_plugins task
            cx().debug(context::generic, "skipping fetch for local plugin {}", name());
            return;
        }

        // find the best suitable branch
        const auto fallback = task_conf().mo_fallback_branch();
        auto branch         = task_conf().mo_branch();
        if (!fallback.empty() && !git_wrap::remote_branch_exists(git_url(), branch)) {
            cx().warning(context::generic,
                         "{} has no remote {} branch, switching to {}", repo_, branch,
                         fallback);
            branch = fallback;
        }

        // clone/pull
        run_tool(make_git().url(git_url()).branch(branch).root(source_path()));
    }

    void modorganizer::do_build_and_install()
    {
        // Only add git submodule for non-local plugins
        if (!is_local_plugin()) {
            // adds a git submodule in modorganizer_super for this project; note that
            // git_submodule_adder runs a thread because adding submodules is slow, but
            // can happen while stuff is building
            git_submodule_adder::instance().queue(
                std::move(git_submodule()
                              .url(git_url())
                              .branch(task_conf().mo_branch())
                              .submodule(name())
                              .root(super_path())));
        }

        // not all modorganizer projects need to actually be built, such as
        // cmake_common, so don't try if there's no cmake file
        if (!fs::exists(source_path() / "CMakeLists.txt")) {
            cx().trace(context::generic, "{} has no CMakeLists.txt, not building",
                       repo_);

            return;
        }

        // run cmake
        run_tool(create_cmake_tool());

        // run restore for nuget
        //
        // until https://gitlab.kitware.com/cmake/cmake/-/issues/20646 is resolved,
        // we need a manual way of running the msbuild -t:restore
        if (is_nuget_plugin())
            run_tool(create_msbuild_tool().targets({"restore"}));

        // run msbuild
        run_tool(create_msbuild_tool());
    }

    cmake modorganizer::create_cmake_tool(cmake::ops o)
    {
        return create_cmake_tool(source_path(), o, task_conf().configuration());
    }

    cmake modorganizer::create_cmake_tool(const fs::path& root, cmake::ops o, config c)
    {
        return std::move(
            cmake(o)
                .generator(cmake::vs)
                .def("CMAKE_INSTALL_PREFIX:PATH", conf().path().install())
                .def("DEPENDENCIES_DIR", conf().path().build())
                .def("BOOST_ROOT", boost::source_path())
                .def("BOOST_LIBRARYDIR", boost::lib_path(arch::x64))
                .def("SPDLOG_ROOT", spdlog::source_path())
                .def("LOOT_PATH", libloot::source_path())
                .def("LZ4_ROOT", lz4::source_path())
                .def("QT_ROOT", qt::installation_path())
                .def("ZLIB_ROOT", zlib::source_path())
                .def("PYTHON_ROOT", python::source_path())
                .def("SEVENZ_ROOT", sevenz::source_path())
                .def("LIBBSARCH_ROOT", libbsarch::source_path())
                .def("BOOST_DI_ROOT", boost_di::source_path())
                // gtest has no RelWithDebInfo, so simply use Debug/Release
                .def("GTEST_ROOT",
                     gtest::build_path(arch::x64, c == config::debug ? config::debug
                                                                     : config::release))
                .def("OPENSSL_ROOT_DIR", openssl::source_path())
                .def("DIRECTXTEX_ROOT", directxtex::source_path())
                .root(root));
    }

    msbuild modorganizer::create_msbuild_tool(msbuild::ops o)
    {
        return std::move(msbuild(o)
                             .solution(project_file_path())
                             .configuration(task_conf().configuration())
                             .architecture(arch::x64));
    }
    
    void modorganizer::build_with_custom_generator(cmake::generators generator)
    {
        // Create a custom cmake tool with the specified generator
        auto custom_cmake = create_cmake_tool(cmake::generate);
        
        // Override the generator
        custom_cmake.generator(generator);
        
        // Enable compile_commands.json generation if using Ninja
        if (generator == cmake::ninja) {
            custom_cmake.def("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")
                       .def("ENABLE_TRANSLATIONS", "OFF")
                       .def("TRANSLATIONS_DISABLED", "ON")  // Additional flag to completely disable translations
                       .def("CMAKE_C_COMPILER", "cl.exe")   // Use MSVC compiler
                       .def("CMAKE_CXX_COMPILER", "cl.exe"); // Use MSVC compiler
            cx().info(context::generic, "using Ninja generator with compile_commands.json for {}", name());
        }
        
        // Log the build path
        cx().info(context::generic, "build path: {}", path_to_utf8(custom_cmake.build_path()));
        
        // Run the custom cmake tool
        try {
            cx().info(context::generic, "running CMake configuration...");
            run_tool(custom_cmake);
            cx().info(context::generic, "CMake configuration completed successfully");
        }
        catch (const std::exception& e) {
            cx().error(context::generic, "CMake configuration failed: {}", e.what());
            throw;
        }
        
        // Run the appropriate build tool based on the generator
        if (generator == cmake::ninja) {
            // Get the ninja path from the configuration
            auto ninja_path = conf().tool().get("ninja");
            if (ninja_path.empty()) {
                ninja_path = "ninja"; // Assume ninja is in PATH if not configured
            }
            
            // Check if build.ninja exists
            auto build_ninja_path = custom_cmake.build_path() / "build.ninja";
            if (!fs::exists(build_ninja_path)) {
                cx().error(context::generic, "build.ninja not found at {}", path_to_utf8(build_ninja_path));
                cx().bail_out(context::generic, "CMake did not generate build.ninja file");
            }
            
            cx().info(context::generic, "running Ninja in {}", path_to_utf8(custom_cmake.build_path()));
            
            // Create and run the ninja process
            auto p = process()
                .binary(ninja_path)
                .arg("install")
                .cwd(custom_cmake.build_path());
            
            p.set_context(&cx());
            
            try {
                cx().info(context::generic, "executing: {} install", path_to_utf8(ninja_path));
                int result = p.run_and_join();
                
                if (result == 0) {
                    cx().info(context::generic, "ninja build completed successfully for {}", name());
                } else {
                    cx().error(context::generic, "ninja build failed with exit code {}", result);
                    cx().bail_out(context::generic, "ninja build failed");
                }
            }
            catch (const std::exception& e) {
                cx().error(context::generic, "ninja build failed: {}", e.what());
                throw;
            }
        } else if (generator == cmake::jom) {
            // For jom, we would run jom here
            // This is just a placeholder for future expansion
            cx().bail_out(context::generic, "jom generator not yet supported for custom builds");
        } else {
            // For Visual Studio, we would run msbuild here
            // This is just a placeholder for future expansion
            cx().bail_out(context::generic, "Visual Studio generator not yet supported for custom builds");
        }
    }
    
    void modorganizer::generate_compile_commands()
    {
        // Check if we should generate compile_commands.json
        bool generate_compile_commands = false;
        auto cmake_section = conf().get_section("cmake");
        if (cmake_section.find("local_plugins_compile_commands") != cmake_section.end()) {
            generate_compile_commands = cmake_section["local_plugins_compile_commands"] == "true";
        }
        
        if (!generate_compile_commands) {
            return;
        }
        
        cx().info(context::generic, "generating compile_commands.json for {}", name());
        
        // Create a custom cmake tool with Ninja generator specifically for compile_commands.json
        auto ninja_cmake = create_cmake_tool(cmake::generate);
        
        // Create ninja_build directory for the compile_commands.json file
        fs::path source_dir = is_local_plugin() ? local_path_ : source_path();
        auto ninja_build_path = source_dir / "ninja_build";
        
        // Make sure the path is within the allowed prefix
        if (is_local_plugin()) {
            // For local plugins, use a path within the prefix
            ninja_build_path = conf().path().build() / "modorganizer_super" / name() / "ninja_build";
        }
        
        // Create the directory if it doesn't exist
        if (!fs::exists(ninja_build_path)) {
            fs::create_directories(ninja_build_path);
        }
        
        // Configure for Ninja with compile_commands.json
        ninja_cmake.generator(cmake::ninja)
                  .def("CMAKE_EXPORT_COMPILE_COMMANDS", "ON")
                  .def("ENABLE_TRANSLATIONS", "OFF")
                  .def("TRANSLATIONS_DISABLED", "ON")
                  .def("CMAKE_C_COMPILER", "cl.exe")
                  .def("CMAKE_CXX_COMPILER", "cl.exe")
                  .output(ninja_build_path);  // Set the output directory explicitly
        
        // Run CMake configuration only (no build)
        try {
            cx().info(context::generic, "running CMake with Ninja for compile_commands.json...");
            
            // Get the ninja path from the configuration
            auto ninja_path = conf().tool().get("ninja");
            if (ninja_path.empty()) {
                ninja_path = "ninja"; // Assume ninja is in PATH if not configured
            }
            
            // Run the CMake configuration
            run_tool(ninja_cmake);
            
            // Check if compile_commands.json was generated
            auto compile_commands_path = ninja_build_path / "compile_commands.json";
            if (fs::exists(compile_commands_path)) {
                cx().info(context::generic, "compile_commands.json generated at {}", path_to_utf8(compile_commands_path));
            } else {
                cx().warning(context::generic, "compile_commands.json not found at {}", path_to_utf8(compile_commands_path));
            }
        }
        catch (const std::exception& e) {
            cx().warning(context::generic, "failed to generate compile_commands.json: {}", e.what());
        }
    }

}  // namespace mob::tasks
