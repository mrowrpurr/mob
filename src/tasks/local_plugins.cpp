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
#include <fstream>
#include <string>

namespace mob {
    // Global container for local plugins
    static std::map<std::string, fs::path> g_local_plugins;
    
    // Get the path to the local plugins ini file
    fs::path get_local_plugins_ini_path()
    {
        // First check if there's a local_plugins.ini in the current directory
        fs::path local_ini = "mob.local_plugins.ini";
        if (fs::exists(local_ini)) {
            return fs::absolute(local_ini);
        }
        
        // Then check in the same directory as the main ini file
        fs::path main_ini_dir = fs::path(mob_exe_path()).parent_path();
        fs::path main_dir_local_ini = main_ini_dir / "mob.local_plugins.ini";
        if (fs::exists(main_dir_local_ini)) {
            return main_dir_local_ini;
        }
        
        // Return the default path even if it doesn't exist yet
        return fs::absolute(local_ini);
    }
    
    // Parse a line from the local plugins ini file
    // Returns a pair of (plugin_name, plugin_path) if the line is valid
    // Returns an empty pair if the line is a comment or empty
    std::pair<std::string, std::string> parse_local_plugins_ini_line(const std::string& line)
    {
        // Skip empty lines
        if (line.empty()) {
            return {};
        }
        
        // Skip comment lines
        if (line[0] == ';' || line[0] == '#') {
            return {};
        }
        
        // Find the equals sign
        size_t equals_pos = line.find('=');
        if (equals_pos == std::string::npos) {
            return {};
        }
        
        // Extract the plugin name and path
        std::string name = line.substr(0, equals_pos);
        std::string path = line.substr(equals_pos + 1);
        
        // Trim whitespace
        name.erase(0, name.find_first_not_of(" \t"));
        name.erase(name.find_last_not_of(" \t") + 1);
        path.erase(0, path.find_first_not_of(" \t"));
        path.erase(path.find_last_not_of(" \t") + 1);
        
        return {name, path};
    }
    
    // Initialize the global container from the local plugins ini file
    void init_local_plugins()
    {
        // Clear the container first
        g_local_plugins.clear();
        
        // Get the path to the local plugins ini file
        fs::path ini_path = get_local_plugins_ini_path();
        
        // Check if the file exists
        if (!fs::exists(ini_path)) {
            gcx().debug(context::generic, "Local plugins ini file not found at {}", path_to_utf8(ini_path));
            return;
        }
        
        gcx().debug(context::generic, "Reading local plugins from {}", path_to_utf8(ini_path));
        
        // Open the file
        std::ifstream ini_file(ini_path);
        if (!ini_file) {
            gcx().warning(context::generic, "Failed to open local plugins ini file at {}", path_to_utf8(ini_path));
            return;
        }
        
        // Read the file line by line
        std::string line;
        while (std::getline(ini_file, line)) {
            // Parse the line
            auto [name, path_str] = parse_local_plugins_ini_line(line);
            
            // Skip invalid lines
            if (name.empty() || path_str.empty()) {
                continue;
            }
            
            // Store the plugin path
            fs::path plugin_path = fs::path(path_str);
            g_local_plugins[name] = plugin_path;
            
            gcx().debug(context::generic, "Found local plugin: {} at {}", name, path_to_utf8(plugin_path));
        }
        
        // No fallbacks - only use what's in the INI file
        if (g_local_plugins.empty()) {
            gcx().debug(context::generic, "No local plugins found in {}", path_to_utf8(ini_path));
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

    local_plugins::local_plugins() : task("local_plugins", "local_plugin")
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
            if (name.find("game_") == 0 || name.find("modorganizer-game_") == 0) {
                flags = modorganizer::gamebryo;
                cx().debug(context::generic, "detected gamebryo plugin: {}", name);
            }
            
            // Create a modorganizer task for this plugin
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
