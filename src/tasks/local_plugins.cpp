#include "pch.h"
#include "tasks.h"
#include "task_manager.h"
#include "../core/conf.h"
#include "../core/ini.h"
#include "../core/op.h"
#include "../core/paths.h"
#include "../core/process.h"
#include "../tools/tools.h"
#include "../utility/fs.h"

namespace mob {
    // Global container for local plugins
    static std::map<std::string, fs::path> g_local_plugins;
    
    // Initialize the global container from the INI file
    void init_local_plugins()
    {
        // Clear the container first
        g_local_plugins.clear();
        
        // Find all INI files
        std::vector<fs::path> ini_files;
        
        // First, try to find the master INI file
        fs::path master_ini = find_in_root(default_ini_filename());
        if (fs::exists(master_ini)) {
            ini_files.push_back(master_ini);
        }
        
        // Then, try to find an INI file in the current directory
        fs::path current_ini = fs::current_path() / default_ini_filename();
        if (fs::exists(current_ini) && !fs::equivalent(current_ini, master_ini)) {
            ini_files.push_back(current_ini);
        }
        
        // Process each INI file
        for (const auto& ini_path : ini_files) {
            // Parse the INI file
            const auto data = parse_ini(ini_path);
            
            // Find the local_plugins section
            for (const auto& [section_name, section_data] : data.sections) {
                if (section_name == "local_plugins") {
                    // Process each entry in the section
                    for (const auto& [name, path_str] : section_data) {
                        // Skip comments
                        if (name.starts_with("#")) {
                            continue;
                        }
                        
                        fs::path plugin_path = fs::path(path_str);
                        
                        // Store the plugin path
                        g_local_plugins[name] = plugin_path;
                    }
                }
            }
        }
        
        // No fallbacks - only use what's in the INI file
        if (g_local_plugins.empty()) {
            gcx().debug(context::generic, "No local plugins found in INI files");
        }
    }
    
    // Get the local plugins
    std::map<std::string, fs::path> get_local_plugins()
    {
        // Initialize if empty
        if (g_local_plugins.empty()) {
            init_local_plugins();
        }
        
        return g_local_plugins;
    }
}

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
        // Get the local plugins from the global container
        auto plugins = get_local_plugins();
        
        // Log the plugins found
        for (const auto& [name, path] : plugins) {
            if (fs::exists(path)) {
                cx().info(context::generic, "found local plugin: {} at {}", name, path_to_utf8(path));
            } else {
                cx().warning(context::generic, "local plugin path does not exist: {}", path_to_utf8(path));
            }
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
