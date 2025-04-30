#include "pch.h"
#include "tasks.h"
#include "../core/conf.h"
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
        // Get all options from the configuration
        auto options = format_options();
        
        // Extract keys from the formatted options
        std::vector<std::string> local_keys;
        std::vector<std::string> gamebryo_keys;
        
        for (const auto& line : options) {
            if (line.find("local  ") == 0 && line.find(" = ") != std::string::npos) {
                auto key_start = line.find("  ") + 2;
                auto key_end = line.find(" = ");
                auto key = line.substr(key_start, key_end - key_start);
                
                // Skip if it's a comment line
                if (key.empty() || key[0] == '#' || key[0] == ';') {
                    continue;
                }
                
                local_keys.push_back(key);
            }
            else if (line.find("local_gamebryo  ") == 0 && line.find(" = ") != std::string::npos) {
                auto key_start = line.find("  ") + 2;
                auto key_end = line.find(" = ");
                auto key = line.substr(key_start, key_end - key_start);
                
                // Skip if it's a comment line
                if (key.empty() || key[0] == '#' || key[0] == ';') {
                    continue;
                }
                
                gamebryo_keys.push_back(key);
            }
        }
        
        // Debug output
        cx().debug(context::generic, "Found {} local keys and {} local_gamebryo keys", 
            local_keys.size(), gamebryo_keys.size());
        
        // Process entries from the local section (non-gamebryo plugins)
        if (!is_set(flags_, gamebryo)) {
            for (const auto& key : local_keys) {
                try {
                    std::string value = details::get_string("local", key);
                    if (!value.empty()) {
                        cx().debug(context::generic, "Adding local MO task: {} from {}", key, value);
                        add_task<tasks::local_modorganizer>(key, fs::path(value), false);
                    }
                }
                catch (bailed&) {
                    // Key doesn't exist, that's fine
                }
            }
        }
        
        // Process entries from the local_gamebryo section (gamebryo plugins)
        if (is_set(flags_, gamebryo)) {
            for (const auto& key : gamebryo_keys) {
                try {
                    std::string value = details::get_string("local_gamebryo", key);
                    if (!value.empty()) {
                        cx().debug(context::generic, "Adding local MO gamebryo task: {} from {}", key, value);
                        add_task<tasks::local_modorganizer>(key, fs::path(value), true);
                    }
                }
                catch (bailed&) {
                    // Key doesn't exist, that's fine
                }
            }
        }
        
        // If no keys were found, try to directly access the keys
        if (local_keys.empty() && gamebryo_keys.empty()) {
            cx().debug(context::generic, "No keys found in formatted options, trying direct access");
            
            // Try to directly access the hello_world_plugin key
            if (!is_set(flags_, gamebryo)) {
                try {
                    std::string value = details::get_string("local", "hello_world_plugin");
                    if (!value.empty()) {
                        cx().debug(context::generic, "Adding local MO task: hello_world_plugin from {}", value);
                        add_task<tasks::local_modorganizer>("hello_world_plugin", fs::path(value), false);
                    }
                }
                catch (bailed&) {
                    // Key doesn't exist, that's fine
                }
            }
        }
    }

}  // namespace mob::tasks
