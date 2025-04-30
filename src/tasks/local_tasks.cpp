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
        // Directly access the INI file
        try {
            // Get the INI file path
            fs::path ini_path = conf().path().prefix() / "mob.ini";
            
            // Parse the INI file
            ini_data ini = parse_ini(ini_path);
            
            // Process entries from the local section (non-gamebryo plugins)
            if (!is_set(flags_, gamebryo)) {
                // Get the local section
                auto local_section = ini.get_section("local");
                
                // Log the number of entries
                cx().info(context::generic, "Found {} entries in [local] section", local_section.size());
                
                // Process each entry
                for (auto& [key, value] : local_section) {
                    // Skip if it's a comment line or empty value
                    if (key.empty() || key[0] == '#' || key[0] == ';' || value.empty()) {
                        continue;
                    }
                    
                    cx().info(context::generic, "Adding local MO task: {} from {}", key, value);
                    add_task<tasks::local_modorganizer>(key, fs::path(value), false);
                }
            }
            
            // Process entries from the local_gamebryo section (gamebryo plugins)
            if (is_set(flags_, gamebryo)) {
                // Get the local_gamebryo section
                auto gamebryo_section = ini.get_section("local_gamebryo");
                
                // Log the number of entries
                cx().info(context::generic, "Found {} entries in [local_gamebryo] section", gamebryo_section.size());
                
                // Process each entry
                for (auto& [key, value] : gamebryo_section) {
                    // Skip if it's a comment line or empty value
                    if (key.empty() || key[0] == '#' || key[0] == ';' || value.empty()) {
                        continue;
                    }
                    
                    cx().info(context::generic, "Adding local MO gamebryo task: {} from {}", key, value);
                    add_task<tasks::local_modorganizer>(key, fs::path(value), true);
                }
            }
        }
        catch (std::exception& e) {
            cx().warning(context::generic, "Failed to process local MO tasks: {}", e.what());
            
            // Fallback to direct access
            cx().info(context::generic, "Falling back to direct access");
            
            // Try to directly access the mo2_plugin_examples key
            if (!is_set(flags_, gamebryo)) {
                try {
                    std::string value = details::get_string("local", "mo2_plugin_examples");
                    if (!value.empty()) {
                        cx().info(context::generic, "Adding local MO task: mo2_plugin_examples from {}", value);
                        add_task<tasks::local_modorganizer>("mo2_plugin_examples", fs::path(value), false);
                    }
                }
                catch (bailed&) {
                    // Key doesn't exist, that's fine
                    cx().warning(context::generic, "Failed to access [local]/mo2_plugin_examples");
                }
            }
        }
    }

}  // namespace mob::tasks
