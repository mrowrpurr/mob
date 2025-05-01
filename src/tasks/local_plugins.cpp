#include "pch.h"
#include "tasks.h"
#include "task_manager.h"
#include "../core/conf.h"
#include "../core/ini.h"
#include "../core/op.h"
#include "../core/process.h"
#include "../tools/tools.h"

namespace mob::tasks {

    local_plugins::local_plugins() : task("local_plugins")
    {
        // Register each local plugin as a separate task
        register_plugin_tasks();
    }
    
    void local_plugins::register_plugin_tasks()
    {
        // Get the list of local plugins
        auto plugins = read_local_plugins();
        
        // Log the number of plugins found
        cx().info(context::generic, "found {} local plugins to register", plugins.size());
        
        // For each plugin, create a modorganizer task and register it
        for (const auto& [name, path] : plugins) {
            // Determine if this is a gamebryo plugin based on the name
            modorganizer::flags flags = modorganizer::noflags;
            if (name.find("gamebryo") == 0) {
                flags = modorganizer::gamebryo;
            }
            
            add_task<modorganizer>(name, path, flags);
        }
    }

    fs::path local_plugins::source_path()
    {
        return {};
    }

    void local_plugins::do_clean(clean c)
    {
        // Get the list of local plugins
        auto plugins = read_local_plugins();

        // For each plugin, remove the junction point in modorganizer_super
        for (const auto& [name, path] : plugins) {
            const auto junction_path = modorganizer::super_path() / name;
            
            if (fs::exists(junction_path)) {
                cx().info(context::generic, "removing junction point for {}", name);
                op::delete_directory(cx(), junction_path, op::optional);
            }
        }
    }

    void local_plugins::do_fetch()
    {
        // Get the list of local plugins
        auto plugins = read_local_plugins();

        // Create junction points for all plugins
        create_junction_points(plugins);
    }

    void local_plugins::do_build_and_install()
    {
        // Get the list of local plugins
        auto plugins = read_local_plugins();

        // Build each plugin
        build_plugins(plugins);
    }

    std::map<std::string, fs::path> local_plugins::read_local_plugins()
    {
        std::map<std::string, fs::path> plugins;

        // Get the local_plugins section from the already loaded INI files
        auto section = conf().get_section("local_plugins");
        
        // Process each entry in the section
        for (const auto& [name, path_str] : section) {
            // Skip comments
            if (name.starts_with("#")) {
                continue;
            }
            
            fs::path plugin_path = fs::path(path_str);
            
            // Check if the path exists
            if (fs::exists(plugin_path)) {
                plugins[name] = plugin_path;
                cx().info(context::generic, "found local plugin: {} at {}", name, path_to_utf8(plugin_path));
            } else {
                cx().warning(context::generic, "local plugin path does not exist: {}", path_to_utf8(plugin_path));
            }
        }
        
        if (plugins.empty()) {
            cx().warning(context::generic, "no local plugins found in INI files");
        }

        return plugins;
    }

    void local_plugins::create_junction_points(const std::map<std::string, fs::path>& plugins)
    {
        // Make sure the super directory is initialized
        initialize_super(cx(), modorganizer::super_path());

        // For each plugin, create a junction point in modorganizer_super
        for (const auto& [name, path] : plugins) {
            const auto junction_path = modorganizer::super_path() / name;
            
            // Skip if the junction already exists
            if (fs::exists(junction_path)) {
                cx().debug(context::generic, "junction point already exists for {}", name);
                continue;
            }
            
            cx().info(context::generic, "creating junction point for {} -> {}", name, path_to_utf8(path));
            
            // Create the junction point using mklink /J
            process p;
            p.binary("cmd")
                .arg("/c")
                .arg("mklink")
                .arg("/J")
                .arg(junction_path.wstring())
                .arg(path.wstring());
            
            p.set_context(&cx());
            int exit_code = p.run_and_join();
            
            if (exit_code != 0) {
                cx().error(context::generic, "failed to create junction point for {}", name);
            }
        }
    }

    void local_plugins::build_plugins(const std::map<std::string, fs::path>& plugins)
    {
        // For each plugin, create and run a modorganizer task with the local flag
        for (const auto& [name, path] : plugins) {
            cx().info(context::generic, "building local plugin {}", name);
            
            // Check if the plugin has a CMakeLists.txt
            if (!fs::exists(path / "CMakeLists.txt")) {
                cx().warning(context::generic, "plugin {} has no CMakeLists.txt, skipping", name);
                continue;
            }
            
            // Determine if this is a gamebryo plugin based on the name
            modorganizer::flags flags = modorganizer::noflags;
            if (name.find("game_") == 0 || name.find("modorganizer-game_") == 0) {
                flags = modorganizer::gamebryo;
                cx().debug(context::generic, "detected gamebryo plugin: {}", name);
            }
            
            // Create a modorganizer task for this plugin
            modorganizer mo_task(name, path, flags);
            
            // Run the task
            mo_task.run();
        }
    }

}  // namespace mob::tasks
