# TODO

The task is:

- We PARTIALLY added support for `local_plugin` as a task based on plugins define in the .ini file.

Only two things don't work:
- 1. `mob list` doesn't show the tasks added by the local plugin (from the .ini file)
- 2. `mob build` doesn't build the tasks added by the local plugin (from the .ini file)

`mob build` by itself works, but you can't do `mob build local_plugin` (I want this to build all local plugins) and you also can't do `mob build plugin_name` (I want this to build a specific local plugin).

If you read `main.cpp`, you'll find an order-of-operations kinda problem.

Main FIRST loads tasks (which include our local plugins) and THEN it loads the .ini file.

This kinda has to be the case for how things work. oof.

I tried a lot of different approaches and now I'm falling back to this....

please look at how the ini is read from places like ini.cpp and conf.ini ... you can pass -i/--ini to customize the .ini file and ALSO you can say --no-default-inis which prevents the root mob.ini from being loaded.

We don't HAVE to support this.

To be honest, we could even have our own SEPARATE mob.local.ini which just has local tasks and we wouldn't have to get into this rats nest of complexity, honestly.

But we could also try to DUPLICATE the way that the .ini files are loaded in the current app into local_tasks.cpp so, when it's loaded, it could parse the .ini file and JUST look for the `[local_plugins]` section? Ignore the rest.

I do NOT want to try to rearrange the order of operations in main.cpp because that would be a nightmare and I don't want to deal with it. I tried, trust me. It's not worth it.

Right now we read from `[local_plugins]` but it ONLY works during a full `mob build` for some reason.

What do you think about... rip that out and just replace with reading a `mob.local_plugins.ini`? or `local_plugins.ini`? Let's do `mob.local_plugins.ini`.

I think this codebase does some gnarly manual .ini parsing instead of using a library, which is... ugh. So... just read the .ini file and we super lazy.

Ignore lines starting with ; or #

Honestly you can even ignore sections, cause there can only be one section in mob.local_plugins.ini lol.

We just care about x = y lines.

The format is <plugin_name> = <plugin_path>

Sound cool?

Any questions?

Oh... so, here's an annoying thing.

I wasn't able to get this project setup and working with my IDE yet, so all C++ files are FULL of errors. They're NOT real errors. Ignore them please.

So, how do you get feedback?

Compile often!

To compile, run `./bootstrap.ps1` please in the terminal. That compiles it.

If it shows something like "run `.\mob -d prefix/path build` to start building" then it worked.

Else it'll show C++ compiler errors.

Note: we're on Windows and only care about Windows. The compile is MSVC. Ignore the editor's clangd errors please.

Ok, sound good?

I guess, before you start, could you please do one things for me?

1. Write the task, as you understand it, into TODO.md
2. Then read ABOUT_TASKS.md and ABOUT_TASKS_AND_DEPENDENCIES.md to understand how the tasks and dependencies work. I really hope these are correct. Don't trust them too much.
3. Poke around the code, starting with main.cpp and then local_plugins.cpp please.
4. After that... you wrote the TODO ... you read docs ... you read code ... then UPDATE the TODO.md with some more specific details and steps you plan to take.
5. Then summarize to me. Before you start coding.
