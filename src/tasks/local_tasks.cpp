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
                std::string value = details::get_string("local", "mo2_plugin_examples");
                cx().info(context::generic, "Found [local]/mo2_plugin_examples = {}", value);
                
                if (!value.empty()) {
                    cx().info(context::generic, "Adding local MO task: mo2_plugin_examples from {}", value);
                    
                    // Log the absolute path
                    fs::path abs_path = fs::absolute(fs::path(value));
                    cx().info(context::generic, "Absolute path: {}", abs_path.string());
                    
                    // Check if the path exists
                    if (fs::exists(abs_path)) {
                        cx().info(context::generic, "Path exists");
                    } else {
                        cx().warning(context::generic, "Path does not exist: {}", abs_path.string());
                    }
                    
                    // Add the task
                    cx().info(context::generic, "Creating task...");
                    add_task<tasks::local_modorganizer>("mo2_plugin_examples", fs::path(value), false);
                    cx().info(context::generic, "Task created successfully");
                }
            }
            catch (bailed&) {
                // Key doesn't exist, that's fine
                cx().warning(context::generic, "Failed to access [local]/mo2_plugin_examples");
            }
        }
        
        // Try to access any keys in the local_gamebryo section
        if (is_set(flags_, gamebryo)) {
            try {
                // Try a few common game keys
                std::vector<std::string> common_games = {
                    "game_customgame",
                    "game_oblivionremaster",
                    "game_custom"
                };
                
                for (const auto& game : common_games) {
                    try {
                        std::string value = details::get_string("local_gamebryo", game);
                        if (!value.empty()) {
                            cx().info(context::generic, "Adding local MO gamebryo task: {} from {}", game, value);
                            add_task<tasks::local_modorganizer>(game, fs::path(value), true);
                        }
                    }
                    catch (bailed&) {
                        // Key doesn't exist, that's fine
                    }
                }
            }
            catch (std::exception& e) {
                cx().warning(context::generic, "Failed to process local_gamebryo tasks: {}", e.what());
            }
        }
    }

}  // namespace mob::tasks
