#include <setjmp.h>

#include "../gc/gc.h"
#include "arity.h"
#include "builtin.h"
#include "error.h"
#include "eval.h"
#include "lambda.h"
#include "parser.h"

static MinimObject *open_file(MinimEnv *env, const char *fname, uint8_t mode)
{
    MinimObject *port;
    MinimPath *path;
    FILE *file;
    char *clean_path;

    path = (is_absolute_path(fname) ?
            build_path(1, fname) :
            build_path(2, get_working_directory(), fname));
    clean_path = extract_path(path);
    file = fopen(clean_path, ((mode & MINIM_PORT_MODE_READ) ? "r" : "w"));
    if (!file)
    {
        Buffer *bf;

        init_buffer(&bf);
        writef_buffer(bf, "Could not open file \"~s\"", clean_path);
        THROW(env, minim_error(get_buffer(bf), NULL));
    }

    port = minim_file_port(file, mode |
                                 MINIM_PORT_MODE_READY |
                                 MINIM_PORT_MODE_OPEN);
    MINIM_PORT_NAME(port) = clean_path;
    return port;
}

static void close_port(MinimObject *port)
{
    if (MINIM_PORT_TYPE(port) == MINIM_PORT_TYPE_FILE)
    {
        if (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_OPEN)
        {
            fclose(MINIM_PORT_FILE(port));
            MINIM_PORT_MODE(port) &= ~MINIM_PORT_MODE_OPEN;
        }
    }
}

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
    return to_bool(MINIM_INPUT_PORTP(args[0]));
}

MinimObject *minim_builtin_output_portp(MinimEnv *env, size_t argc, MinimObject **args)
{
    return to_bool(MINIM_OUTPUT_PORTP(args[0]));
}

MinimObject *minim_builtin_call_with_input_file(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimArity arity;
    MinimObject *port, *val;

    if (!MINIM_OBJ_STRINGP(args[0]))
        THROW(env, minim_argument_error("string?", "call-with-input-file", 0, args[0]));

    if (!MINIM_OBJ_CLOSUREP(args[1]))
        THROW(env, minim_argument_error("procedure of 1 argument", "call-with-input-file", 0, args[1]));

    minim_get_lambda_arity(MINIM_CLOSURE(args[1]), &arity);
    if (arity.low != 1 || arity.high != 1)
        THROW(env, minim_argument_error("procedure of 1 argument", "call-with-input-file", 0, args[1]));

    port = open_file(env, MINIM_STRING(args[0]), MINIM_PORT_MODE_READ);
    val = eval_lambda(MINIM_CLOSURE(args[1]), NULL, 1, &port);
    close_port(port);
    return val;
}

MinimObject *minim_builtin_call_with_output_file(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimArity arity;
    MinimObject *port, *val;

    if (!MINIM_OBJ_STRINGP(args[0]))
        THROW(env, minim_argument_error("string?", "call-with-output-file", 0, args[0]));

    if (!MINIM_OBJ_CLOSUREP(args[1]))
        THROW(env, minim_argument_error("procedure of 1 argument", "call-with-output-file", 0, args[1]));

    minim_get_lambda_arity(MINIM_CLOSURE(args[1]), &arity);
    if (arity.low != 1 || arity.high != 1)
        THROW(env, minim_argument_error("procedure of 1 argument", "call-with-output-file", 0, args[1]));

    port = open_file(env, MINIM_STRING(args[0]), MINIM_PORT_MODE_WRITE);
    val = eval_lambda(MINIM_CLOSURE(args[1]), NULL, 1, &port);
    close_port(port);
    return val;
}

MinimObject *minim_builtin_with_input_from_file(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimArity arity;
    MinimObject *port, *val, *prev;

    if (!MINIM_OBJ_STRINGP(args[0]))
        THROW(env, minim_argument_error("string?", "with-input-to-file", 0, args[0]));

    if (!MINIM_OBJ_CLOSUREP(args[1]))
        THROW(env, minim_argument_error("procedure of 0 argument", "with-input-to-file", 0, args[1]));

    minim_get_lambda_arity(MINIM_CLOSURE(args[1]), &arity);
    if (arity.low != 0 || arity.high != 0)
        THROW(env, minim_argument_error("procedure of 0 argument", "with-input-to-file", 0, args[1]));

    port = open_file(env, MINIM_STRING(args[0]), MINIM_PORT_MODE_READ);
    prev = minim_input_port;
    minim_input_port = port;
    val = eval_lambda(MINIM_CLOSURE(args[1]), NULL, 1, &port);

    minim_input_port = prev;
    close_port(port);
    return val;
}

MinimObject *minim_builtin_with_output_from_file(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimArity arity;
    MinimObject *port, *val, *prev;

    if (!MINIM_OBJ_STRINGP(args[0]))
        THROW(env, minim_argument_error("string?", "with-output-to-file", 0, args[0]));

    if (!MINIM_OBJ_CLOSUREP(args[1]))
        THROW(env, minim_argument_error("procedure of 0 argument", "with-output-to-file", 0, args[1]));

    minim_get_lambda_arity(MINIM_CLOSURE(args[1]), &arity);
    if (arity.low != 0 || arity.high != 0)
        THROW(env, minim_argument_error("procedure of 0 argument", "with-output-to-file", 0, args[1]));

    port = open_file(env, MINIM_STRING(args[0]), MINIM_PORT_MODE_WRITE);
    prev = minim_output_port;
    minim_output_port = port;
    val = eval_lambda(MINIM_CLOSURE(args[1]), NULL, 0, NULL);

    minim_output_port = prev;
    close_port(port);
    return val;
}

MinimObject *minim_builtin_open_input_file(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_STRINGP(args[0]))
        THROW(env, minim_argument_error("string?", "open-input-file", 0, args[0]));

    return open_file(env, MINIM_STRING(args[0]), MINIM_PORT_MODE_READ);
}

MinimObject *minim_builtin_open_output_file(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OBJ_STRINGP(args[0]))
        THROW(env, minim_argument_error("string?", "open-output-file", 0, args[0]));

    return open_file(env, MINIM_STRING(args[0]), MINIM_PORT_MODE_WRITE);
}

MinimObject *minim_builtin_close_input_port(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_INPUT_PORTP(args[0]))
            THROW(env, minim_argument_error("input port?", "close-input-port", 0, args[0]));

    close_port(args[0]);
    return minim_void;
}

MinimObject *minim_builtin_close_output_port(MinimEnv *env, size_t argc, MinimObject **args)
{
    if (!MINIM_OUTPUT_PORTP(args[0]))
            THROW(env, minim_argument_error("output port?", "close-output-port", 0, args[0]));

    close_port(args[0]);
    return minim_void;
}

MinimObject *minim_builtin_read(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *port, *val;

    if (argc == 0)
    {
        port = minim_input_port;
    }
    else    // argc == 2
    {
        if (!MINIM_INPUT_PORTP(args[0]))
            THROW(env, minim_argument_error("input port?", "read", 0, args[0]));
        port = args[0];
    }

    val = minim_values(0, NULL);
    if (MINIM_PORT_TYPE(port) == MINIM_PORT_TYPE_FILE)
    {
        while (MINIM_PORT_MODE(port) & MINIM_PORT_MODE_READY)
        {
            SyntaxNode *stx, *err;
            
            stx = minim_parse_port(port, &err, 0);
            if (!stx)
            {
                MinimError *e;
                Buffer *bf;

                init_buffer(&bf);
                writef_buffer(bf, "~s:~u:~u", MINIM_PORT_NAME(port),
                                              MINIM_PORT_ROW(port),
                                              MINIM_PORT_COL(port));

                init_minim_error(&e, "bad syntax", err->sym);
                init_minim_error_desc_table(&e->table, 1);
                minim_error_desc_table_set(e->table, 0, "in", get_buffer(bf));
                THROW(env, minim_err(e));
            }

            ++MINIM_VALUES_SIZE(val);
            MINIM_VALUES(val) = GC_realloc(MINIM_VALUES(val), MINIM_VALUES_SIZE(val) * sizeof(MinimObject*));
            MINIM_VALUES_REF(val, MINIM_VALUES_SIZE(val) - 1) = unsyntax_ast_rec(env, stx);
        }
    }

    return (MINIM_VALUES_SIZE(val) == 1 ? MINIM_VALUES_REF(val, 0) : val);
}

MinimObject *minim_builtin_write(MinimEnv *env, size_t argc, MinimObject **args)
{
    PrintParams pp;
    MinimObject *port;

    if (argc == 1)
    {
        port = minim_output_port;
    }
    else    // argc == 2
    {
        if (!MINIM_OUTPUT_PORTP(args[1]))
            THROW(env, minim_argument_error("output port?", "write", 1, args[1]));       
        port = args[1];
    }

    set_default_print_params(&pp);
    if (MINIM_PORT_TYPE(port) == MINIM_PORT_TYPE_FILE)
        print_to_port(args[0], env, &pp, MINIM_PORT_FILE(port));
    
    return minim_void;
}

MinimObject *minim_builtin_display(MinimEnv *env, size_t argc, MinimObject **args)
{
    PrintParams pp;
    MinimObject *port;

    if (argc == 1)
    {
        port = minim_output_port;
    }
    else    // argc == 2
    {
        if (!MINIM_OUTPUT_PORTP(args[1]))
            THROW(env, minim_argument_error("output port?", "write", 1, args[1]));       
        port = args[1];
    }

    set_default_print_params(&pp);
    pp.display = true;
    if (MINIM_PORT_TYPE(port) == MINIM_PORT_TYPE_FILE)
        print_to_port(args[0], env, &pp, MINIM_PORT_FILE(port));
    
    return minim_void;
}

MinimObject *minim_builtin_newline(MinimEnv *env, size_t argc, MinimObject **args)
{
    MinimObject *port;

    if (argc == 0)
    {
        port = minim_output_port;
    }
    else    // argc == 2
    {
        if (!MINIM_OUTPUT_PORTP(args[0]))
            THROW(env, minim_argument_error("output port?", "newline", 0, args[0]));       
        port = args[0];
    }

    if (MINIM_PORT_TYPE(port) == MINIM_PORT_TYPE_FILE)
        fprintf(MINIM_PORT_FILE(port), "\n");

    return minim_void;
}

MinimObject *minim_builtin_write_char(MinimEnv *env, size_t argc, MinimObject **args)
{
    PrintParams pp;
    MinimObject *port;

    if (!MINIM_OBJ_CHARP(args[0]))
        THROW(env, minim_argument_error("char?", "write-char", 0, args[0]));

    if (argc == 1)
    {
        port = minim_output_port;
    }
    else    // argc == 2
    {
        if (!MINIM_OUTPUT_PORTP(args[1]))
            THROW(env, minim_argument_error("output port?", "write-char", 1, args[1]));       
        port = args[1];
    }

    set_default_print_params(&pp);
    pp.display = true;
    if (MINIM_PORT_TYPE(port) == MINIM_PORT_TYPE_FILE)
        print_to_port(args[0], env, &pp, MINIM_PORT_FILE(port));

    return minim_void;
}
