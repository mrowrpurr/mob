#include "pch.h"
#include "task_manager.h"
#include "../core/context.h"
#include "../core/conf.h"
#include "task.h"
#include "tasks.h"

namespace mob {

    task_manager::task_manager() : interrupt_(false) {}

    task_manager& task_manager::instance()
    {
        static task_manager m;
        return m;
    }

    void task_manager::add(std::unique_ptr<task> t)
    {
        top_level_.push_back(std::move(t));
    }

    void task_manager::register_task(task* t)
    {
        all_.push_back(t);
    }

    std::vector<task*> task_manager::find(std::string_view pattern)
    {
        auto tasks = find_by_pattern(pattern);

        if (tasks.empty())
            tasks = find_by_alias(pattern);

        return tasks;
    }

    task* task_manager::find_one(std::string_view pattern, bool verbose)
    {
        const auto tasks = find(pattern);

        // okay, only one match
        if (tasks.size() == 1)
            return tasks[0];

        // bad

        if (tasks.empty()) {
            if (verbose)
                u8cerr << "no task matches '" << pattern << "'\n";
        }
        else if (tasks.size() > 1) {
            if (verbose) {
                u8cerr << "found " << tasks.size() << " matches for pattern "
                       << "'" << pattern << "'\n"
                       << "the pattern must only match one task\n";
            }
        }

        return nullptr;
    }

    std::vector<task*> task_manager::all()
    {
        return all_;
    }

    std::vector<task*> task_manager::top_level()
    {
        std::vector<task*> v;

        for (auto&& t : top_level_)
            v.push_back(t.get());

        return v;
    }

    void task_manager::add_alias(std::string name, std::vector<std::string> names)
    {
        auto itor = aliases_.find(name);
        if (itor != aliases_.end()) {
            gcx().warning(context::generic, "alias {} already exists", name);
            return;
        }

        aliases_.emplace(std::move(name), std::move(names));
    }

    const task_manager::alias_map& task_manager::aliases()
    {
        return aliases_;
    }

    void task_manager::run_all()
    {
        try {
            for (auto&& t : top_level_) {
                t->run();

                if (interrupt_)
                    break;
            }
        }
        catch (interrupted&) {
        }

        for (auto&& t : top_level_) {
            t->check_bailed();
        }
    }

    void task_manager::interrupt_all()
    {
        // handles multiple tasks failing simultaneously
        std::scoped_lock lock(interrupt_mutex_);

        if (!interrupt_) {
            interrupt_ = true;
            for (auto&& t : top_level_)
                t->interrupt();
        }
    }

    std::vector<task*> task_manager::find_by_pattern(std::string_view pattern)
    {
        std::vector<task*> tasks;

        for (auto&& t : all_) {
            if (t->name_matches(pattern))
                tasks.push_back(t);
        }

        return tasks;
    }

    std::vector<task*> task_manager::find_by_alias(std::string_view alias_name)
    {
        std::vector<task*> v;

        auto itor = aliases_.find(alias_name);
        if (itor == aliases_.end())
            return v;

        for (auto&& a : itor->second) {
            const auto temp = find_by_pattern(a);
            v.insert(v.end(), temp.begin(), temp.end());
        }

        return v;
    }

    bool task_manager::valid_task_name(std::string_view pattern)
    {
        if (!find(pattern).empty())
            return true;

        if (pattern == "_override")
            return true;

        return false;
    }
    
    void task_manager::add_local_mo_tasks()
    {
        // Process [mo:local] and [mo:local:gamebryo] sections
        try {
            // Get all options from the configuration
            auto options = format_options();
            
            // We need to extract all keys from the mo:local and mo:local:gamebryo sections
            // This is a bit tricky since we don't have direct access to the section keys
            // We'll use the format_options() function to get all options and extract the keys
            
            std::vector<std::string> local_keys;
            std::vector<std::string> gamebryo_keys;
            
            // Extract keys from the formatted options
            for (const auto& line : options) {
                if (line.find("mo:local  ") == 0 && line.find(" = ") != std::string::npos) {
                    auto key_start = line.find("  ") + 2;
                    auto key_end = line.find(" = ");
                    auto key = line.substr(key_start, key_end - key_start);
                    
                    // Skip if it's a comment line
                    if (key.empty() || key[0] == '#' || key[0] == ';') {
                        continue;
                    }
                    
                    local_keys.push_back(key);
                }
                else if (line.find("mo:local:gamebryo  ") == 0 && line.find(" = ") != std::string::npos) {
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
            
            // Now process the keys using the proper configuration access methods
            
            // Process entries from the mo:local section (non-gamebryo plugins)
            try {
                for (const auto& key : local_keys) {
                    try {
                        std::string value = mob::details::get_string("mo:local", key);
                        if (!value.empty()) {
                            gcx().debug(context::generic, "Adding local MO task: {} from {}", key, value);
                            mob::add_task<mob::tasks::local_modorganizer>(key, fs::path(value), false);
                        }
                    }
                    catch (bailed&) {
                        // Key doesn't exist, that's fine
                    }
                }
            }
            catch (bailed&) {
                // Section doesn't exist or is empty, that's fine
            }
            
            // Process entries from the mo:local:gamebryo section (gamebryo plugins)
            try {
                for (const auto& key : gamebryo_keys) {
                    try {
                        std::string value = mob::details::get_string("mo:local:gamebryo", key);
                        if (!value.empty()) {
                            gcx().debug(context::generic, "Adding local MO gamebryo task: {} from {}", key, value);
                            mob::add_task<mob::tasks::local_modorganizer>(key, fs::path(value), true);
                        }
                    }
                    catch (bailed&) {
                        // Key doesn't exist, that's fine
                    }
                }
            }
            catch (bailed&) {
                // Section doesn't exist or is empty, that's fine
            }
        }
        catch (std::exception& e) {
            gcx().warning(context::generic, "Failed to process local MO tasks: {}", e.what());
        }
    }

}  // namespace mob
