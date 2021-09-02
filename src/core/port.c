#include "builtin.h"

MinimObject *minim_builtin_current_input_port(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_input_port;
}

MinimObject *minim_builtin_current_output_port(MinimEnv *env, size_t argc, MinimObject **args)
{
    return minim_output_port;
}

MinimObject *minim_builtin_portp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_PORTP(args[0]));
}

MinimObject *minim_builtin_input_portp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_PORTP(args[0]) && (MINIM_PORT_MODE(args[0]) & MINIM_PORT_MODE_READ));
}

MinimObject *minim_builtin_output_portp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OBJ_PORTP(args[0]) && (MINIM_PORT_MODE(args[0]) & MINIM_PORT_MODE_WRITE));
}
