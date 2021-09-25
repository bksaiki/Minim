#include "../common/path.h"

#include "env.h"
#include "global.h"
#include "intern.h"
#include "module.h"

struct MinimGlobal global;

void init_global_state()
{
    init_minim_module_cache(&global.cache);
    init_minim_symbol_table(&global.builtins);
    global.symbols = init_intern_table();
    global.current_dir = get_current_dir();
    global.pid = get_current_pid();
}
