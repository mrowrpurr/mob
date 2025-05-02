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
    
    // Get all paths to local plugins ini files, starting from the current directory
    // and walking up to the filesystem root
    std::vector<fs::path> get_local_plugins_ini_paths()
    {
        std::vector<fs::path> ini_paths;
        
        // Start with the current directory
        fs::path current_dir = fs::current_path();
        
        // Walk up the directory tree to the root
        while (!current_dir.empty()) {
            // Check for mob.local_plugins.ini in this directory
            fs::path local_ini = current_dir / "mob.local_plugins.ini";
            if (fs::exists(local_ini)) {
                ini_paths.push_back(fs::absolute(local_ini));
                gcx().debug(context::generic, "Found local plugins ini at {}", path_to_utf8(local_ini));
            }
            
            // Move to the parent directory
            fs::path parent_dir = current_dir.parent_path();
            
            // Break if we've reached the root (parent is the same as current)
            if (parent_dir == current_dir) {
                break;
            }
            
            current_dir = parent_dir;
        }
        
        // Also check in the same directory as the main exe
        fs::path main_exe_dir = fs::path(mob_exe_path()).parent_path();
        fs::path main_dir_local_ini = main_exe_dir / "mob.local_plugins.ini";
        
        // Only add if it's not already in the list
        if (fs::exists(main_dir_local_ini)) {
            bool already_added = false;
            for (const auto& path : ini_paths) {
                if (fs::equivalent(path, main_dir_local_ini)) {
                    already_added = true;
                    break;
                }
            }
            
            if (!already_added) {
                ini_paths.push_back(main_dir_local_ini);
                gcx().debug(context::generic, "Found local plugins ini at {}", path_to_utf8(main_dir_local_ini));
            }
        }
        
        // If no ini files were found, return the default path in the current directory
        if (ini_paths.empty()) {
            fs::path default_ini = fs::absolute("mob.local_plugins.ini");
            ini_paths.push_back(default_ini);
            gcx().debug(context::generic, "No local plugins ini files found, using default path {}", path_to_utf8(default_ini));
        }
        
        return ini_paths;
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
    
    // Initialize the global container from all local plugins ini files
    void init_local_plugins()
    {
        // Clear the container first
        g_local_plugins.clear();
        
        // Get all paths to local plugins ini files
        std::vector<fs::path> ini_paths = get_local_plugins_ini_paths();
        
        // Process each ini file in reverse order (from root to current directory)
        // This way, plugins defined in directories closer to the current directory
        // will override those defined in directories closer to the root
        for (auto it = ini_paths.rbegin(); it != ini_paths.rend(); ++it) {
            const fs::path& ini_path = *it;
            
            // Skip if the file doesn't exist
            if (!fs::exists(ini_path)) {
                continue;
            }
            
            gcx().debug(context::generic, "Reading local plugins from {}", path_to_utf8(ini_path));
            
            // Open the file
            std::ifstream ini_file(ini_path);
            if (!ini_file) {
                gcx().warning(context::generic, "Failed to open local plugins ini file at {}", path_to_utf8(ini_path));
                continue;
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
                
                // Store the plugin path (overriding any existing entry with the same name)
                fs::path plugin_path = fs::path(path_str);
                g_local_plugins[name] = plugin_path;
                
                gcx().debug(context::generic, "Found local plugin: {} at {}", name, path_to_utf8(plugin_path));
            }
        }
        
        // No fallbacks - only use what's in the INI files
        if (g_local_plugins.empty()) {
            gcx().debug(context::generic, "No local plugins found in any ini files");
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
