#include "minimpriv.h"

struct MinimGlobal global;

void init_global_state(uint8_t flags)
{
    // state
    init_minim_module_cache(&global.cache);
    init_minim_symbol_table(&global.builtins);
    global.symbols = init_intern_table();
    global.current_dir = get_current_dir();
    global.pid = get_current_pid();

    // Pointers in static data
    GC_register_root(global.cache);
    GC_register_root(global.builtins);
    GC_register_root(global.symbols);
    GC_register_root(global.current_dir);

    // statistics
    global.stat_exprs = 0;
    global.stat_procs = 0;
    global.stat_objs = 0;

    // flags
    global.flags = flags;
}
