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
        // Process [local] and [local_gamebryo] sections
        try {
            // Try to directly access the local section
            gcx().info(context::generic, "Trying to directly access [local] section");
            try {
                std::string value = details::get_string("local", "hello_world_plugin");
                gcx().info(context::generic, "Direct access to [local]/hello_world_plugin: '{}'", value);
                
                if (!value.empty()) {
                    gcx().info(context::generic, "Adding local MO task: hello_world_plugin from {}", value);
                    add_task<tasks::local_modorganizer>("hello_world_plugin", fs::path(value), false);
                }
                else {
                    gcx().info(context::generic, "Value for hello_world_plugin is empty");
                }
            }
            catch (bailed&) {
                gcx().info(context::generic, "Bailed when trying to access [local]/hello_world_plugin");
            }
            catch (std::exception& e) {
                gcx().info(context::generic, "Exception when accessing [local]/hello_world_plugin: {}", e.what());
            }
            
            // Try to directly access the local_gamebryo section
            gcx().info(context::generic, "Trying to directly access [local_gamebryo] section");
            try {
                std::string value = details::get_string("local_gamebryo", "game_customgame");
                gcx().info(context::generic, "Direct access to [local_gamebryo]/game_customgame: '{}'", value);
            }
            catch (bailed&) {
                gcx().info(context::generic, "Bailed when trying to access [local_gamebryo]/game_customgame");
            }
            catch (std::exception& e) {
                gcx().info(context::generic, "Exception when accessing [local_gamebryo]/game_customgame: {}", e.what());
            }
            
            // Debug the format_options function
            auto options = format_options();
            gcx().info(context::generic, "Formatted options count: {}", options.size());
            
            // Only print the first 10 lines to avoid flooding the log
            for (size_t i = 0; i < std::min(options.size(), size_t(10)); ++i) {
                gcx().info(context::generic, "Option line {}: {}", i, options[i]);
            }
            
            // Check if there are more lines
            if (options.size() > 10) {
                gcx().info(context::generic, "... and {} more lines", options.size() - 10);
            }
            
            // Try to find any lines containing "local" or "hello"
            for (const auto& line : options) {
                if (line.find("local") != std::string::npos || line.find("hello") != std::string::npos) {
                    gcx().info(context::generic, "Found relevant line: {}", line);
                }
            }
            
            // Try to manually add the task
            gcx().info(context::generic, "Manually adding hello_world_plugin task");
            try {
                add_task<tasks::local_modorganizer>("hello_world_plugin", fs::path("..\\cpp_examples\\hello_world_plugin"), false);
                gcx().info(context::generic, "Successfully added hello_world_plugin task manually");
            }
            catch (std::exception& e) {
                gcx().info(context::generic, "Exception when manually adding hello_world_plugin task: {}", e.what());
            }
        }
        catch (std::exception& e) {
            gcx().warning(context::generic, "Failed to process local MO tasks: {}", e.what());
        }
    }

}  // namespace mob
