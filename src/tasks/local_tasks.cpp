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
        // Process the local section
        if (!is_set(flags_, gamebryo)) {
            try {
                // Get the INI file path
                fs::path ini_path = conf().path().prefix() / "mob.ini";
                
                // Process all keys in the local section
                auto ini_data = parse_ini(ini_path);
                auto local_section = ini_data.get_section("local");
                
                for (auto& [key, value] : local_section) {
                    // Skip if it's a comment line or empty value
                    if (key.empty() || key[0] == '#' || key[0] == ';' || value.empty()) {
                        continue;
                    }
                    
                    // Add the task
                    add_task<tasks::local_modorganizer>(key, fs::path(value), false);
                }
            }
            catch (std::exception&) {
                // Failed to process the section, that's fine
            }
        }
        
        // Process the local_gamebryo section
        if (is_set(flags_, gamebryo)) {
            try {
                // Get the INI file path
                fs::path ini_path = conf().path().prefix() / "mob.ini";
                
                // Process all keys in the local_gamebryo section
                auto ini_data = parse_ini(ini_path);
                auto gamebryo_section = ini_data.get_section("local_gamebryo");
                
                for (auto& [key, value] : gamebryo_section) {
                    // Skip if it's a comment line or empty value
                    if (key.empty() || key[0] == '#' || key[0] == ';' || value.empty()) {
                        continue;
                    }
                    
                    // Add the task
                    add_task<tasks::local_modorganizer>(key, fs::path(value), true);
                }
            }
            catch (std::exception&) {
                // Failed to process the section, that's fine
            }
        }
    }

}  // namespace mob::tasks
