#include "pch.h"
#include "tasks.h"
#include "../core/conf.h"
#include "../core/ini.h"
#include "../tasks/task_manager.h"

namespace mob::tasks {

    local_tasks::local_tasks(flags f)
        : task("local_tasks"), flags_(f)
    {
    }

    void local_tasks::do_clean(clean)
    {
        // Nothing to clean
    }

    void local_tasks::do_fetch()
    {
        // Nothing to fetch
    }

    void local_tasks::do_build_and_install()
    {
        // Try to directly access the mo2_plugin_examples key
        if (!is_set(flags_, gamebryo)) {
            try {
                // Process all keys in the local section
                auto ini_data = parse_ini(conf().path().prefix() / "mob.ini");
                auto local_section = ini_data.get_section("local");
                
                for (auto& [key, value] : local_section) {
                    // Skip if it's a comment line or empty value
                    if (key.empty() || key[0] == '#' || key[0] == ';' || value.empty()) {
                        continue;
                    }
                    
                    cx().info(context::generic, "Found [local]/{} = {}", key, value);
                    cx().info(context::generic, "Adding local MO task: {} from {}", key, value);
                    
                    // Get the absolute path and super path
                    fs::path abs_path = fs::absolute(fs::path(value));
                    cx().info(context::generic, "Absolute path: {}", abs_path.string());
                    
                    // Check if the path exists
                    if (fs::exists(abs_path)) {
                        cx().info(context::generic, "Path exists");
                    } else {
                        cx().warning(context::generic, "Path does not exist: {}", abs_path.string());
                        continue;
                    }
                    
                    // Get the super path
                    fs::path super_path = modorganizer::super_path();
                    fs::path link_path = super_path / key; // Use the key from the INI file
                    
                    cx().info(context::generic, "Checking for symlink from {} to {}", link_path, abs_path);
                    
                    // Check if the symlink already exists
                    if (fs::exists(link_path)) {
                        if (fs::is_symlink(link_path)) {
                            cx().info(context::generic, "Symlink already exists");
                        } else {
                            cx().error(context::generic, "Path exists but is not a symlink: {}", link_path);
                            cx().error(context::generic, "Please remove the directory and create a symlink instead:");
                            cx().error(context::generic, "1. Remove: {}", link_path);
                            cx().error(context::generic, "2. Run: mklink /D \"{}\" \"{}\"", link_path, abs_path);
                            cx().error(context::generic, "3. Then run mob build again");
                            return;
                        }
                    } else {
                        // Symlink doesn't exist, tell the user to create it
                        cx().error(context::generic, "Symlink does not exist: {}", link_path);
                        cx().error(context::generic, "Please create the symlink manually:");
                        cx().error(context::generic, "1. Run: mklink /D \"{}\" \"{}\"", link_path, abs_path);
                        cx().error(context::generic, "2. Then run mob build again");
                        return;
                    }
                    
                    // Add the task
                    cx().info(context::generic, "Creating task...");
                    add_task<tasks::local_modorganizer>(key, fs::path(value), false);
                    cx().info(context::generic, "Task created successfully");
                }
            }
            catch (bailed&) {
                // Key doesn't exist, that's fine
                cx().warning(context::generic, "Failed to process [local] section");
            }
        }
        
        // Process all keys in the local_gamebryo section
        if (is_set(flags_, gamebryo)) {
            try {
                // Process all keys in the local_gamebryo section
                auto ini_data = parse_ini(conf().path().prefix() / "mob.ini");
                auto gamebryo_section = ini_data.get_section("local_gamebryo");
                
                for (auto& [key, value] : gamebryo_section) {
                    // Skip if it's a comment line or empty value
                    if (key.empty() || key[0] == '#' || key[0] == ';' || value.empty()) {
                        continue;
                    }
                    
                    cx().info(context::generic, "Found [local_gamebryo]/{} = {}", key, value);
                    cx().info(context::generic, "Adding local MO gamebryo task: {} from {}", key, value);
                    
                    // Get the absolute path and super path
                    fs::path abs_path = fs::absolute(fs::path(value));
                    cx().info(context::generic, "Absolute path: {}", abs_path.string());
                    
                    // Check if the path exists
                    if (fs::exists(abs_path)) {
                        cx().info(context::generic, "Path exists");
                    } else {
                        cx().warning(context::generic, "Path does not exist: {}", abs_path.string());
                        continue;
                    }
                    
                    // Get the super path
                    fs::path super_path = modorganizer::super_path();
                    fs::path link_path = super_path / key; // Use the key from the INI file
                    
                    cx().info(context::generic, "Checking for symlink from {} to {}", link_path, abs_path);
                    
                    // Check if the symlink already exists
                    if (fs::exists(link_path)) {
                        if (fs::is_symlink(link_path)) {
                            cx().info(context::generic, "Symlink already exists");
                        } else {
                            cx().error(context::generic, "Path exists but is not a symlink: {}", link_path);
                            cx().error(context::generic, "Please remove the directory and create a symlink instead:");
                            cx().error(context::generic, "1. Remove: {}", link_path);
                            cx().error(context::generic, "2. Run: mklink /D \"{}\" \"{}\"", link_path, abs_path);
                            cx().error(context::generic, "3. Then run mob build again");
                            return;
                        }
                    } else {
                        // Symlink doesn't exist, tell the user to create it
                        cx().error(context::generic, "Symlink does not exist: {}", link_path);
                        cx().error(context::generic, "Please create the symlink manually:");
                        cx().error(context::generic, "1. Run: mklink /D \"{}\" \"{}\"", link_path, abs_path);
                        cx().error(context::generic, "2. Then run mob build again");
                        return;
                    }
                    
                    // Add the task
                    cx().info(context::generic, "Creating task...");
                    add_task<tasks::local_modorganizer>(key, fs::path(value), true);
                    cx().info(context::generic, "Task created successfully");
                }
            }
            catch (std::exception& e) {
                cx().warning(context::generic, "Failed to process local_gamebryo tasks: {}", e.what());
            }
        }
    }

}  // namespace mob::tasks
