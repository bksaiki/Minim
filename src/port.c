/**
 * @file
 * Ports: textual input and output over files and strings, and their
 * primitives.
 */

#include "port.h"

#include <stdlib.h>
#include <string.h>

#include "char.h"
#include "gc.h"
#include "heap.h"
#include "prims.h"
#include "system.h"

static minim_obj standard[3];  // over stdin, stdout, and stderr
static minim_obj string_name;  // every string port's name

// A new port with `flags`, named `name`, over the string `buffer` or `file`.
static minim_obj make_port(unsigned flags, minim_obj buffer, minim_obj name, FILE *file) {
    MINIM_FRAME(&buffer, &name);
    minim_obj *p = minim_heap_alloc(MINIM_PORT_SIZE);
    p[0] = minim_make_header(flags, MINIM_TYPE_PORT);
    p[1] = buffer;
    p[2] = name;
    p[3] = minim_fixnum(0);
    p[4] = minim_fixnum(-1);  // no lookahead
    p[5] = (minim_obj)file;
    MINIM_RETURN((minim_obj)p | MINIM_TAG_TYPED);
}

// A mutable copy of the string `s`.
static minim_obj copy_string(minim_obj s) {
    MINIM_FRAME(&s);
    size_t n = minim_string_length(s);
    minim_obj t = minim_make_string(n, 0);
    memcpy(minim_string_chars(t), minim_string_chars(s), n * sizeof(uint32_t));
    MINIM_RETURN(t);
}

void minim_port_init(void) {
    static const char *const names[] = {"stdin", "stdout", "stderr"};
    FILE *const files[] = {stdin, stdout, stderr};
    minim_gc_add_global(&string_name);
    string_name = minim_string_from_cstr("string");
    for (int i = 0; i < 3; i++) {
        minim_gc_add_global(&standard[i]);
        minim_obj name = minim_string_from_cstr(names[i]);
        unsigned dir = i == 0 ? MINIM_PORT_INPUT : MINIM_PORT_OUTPUT;
        standard[i] = make_port(dir | MINIM_PORT_FILE, MINIM_FALSE, name, files[i]);
    }
}

minim_obj minim_standard_port(int i) {
    return standard[i];
}

// Copies the string, so changing it doesn't change the port.
minim_obj minim_open_input_string(minim_obj s) {
    s = copy_string(s);
    return make_port(MINIM_PORT_INPUT, s, string_name, NULL);
}

minim_obj minim_open_output_string(void) {
    minim_obj s = minim_make_string(16, 0);
    return make_port(MINIM_PORT_OUTPUT, s, string_name, NULL);
}

// A port of direction `dir` over the file `path`; raises a file error, `msg`,
// if it can't be opened.
static minim_obj open_file(minim_obj path, unsigned dir, const char *msg) {
    char *name = minim_string_to_utf8(path);
    FILE *f = minim_fopen(name, dir == MINIM_PORT_INPUT ? "r" : "w");
    free(name);
    if (f == NULL)
        minim_error_of_kind(MINIM_ERROR_FILE, msg, path);
    minim_obj s = copy_string(path);
    return make_port(dir | MINIM_PORT_FILE, MINIM_FALSE, s, f);
}

minim_obj minim_open_input_file(minim_obj path) {
    return open_file(path, MINIM_PORT_INPUT, "open-input-file: cannot open file");
}

minim_obj minim_open_output_file(minim_obj path) {
    return open_file(path, MINIM_PORT_OUTPUT, "open-output-file: cannot open file");
}

void minim_port_close(minim_obj p) {
    unsigned flags = minim_port_flags(p);
    if (flags & MINIM_PORT_CLOSED)
        return;
    FILE *file = minim_port_file(p);
    if (file == stdin || file == stdout || file == stderr) {
        if (flags & MINIM_PORT_OUTPUT)
            fflush(file);  // left open: C still uses it
    } else if (file != NULL) {
        fclose(file);
    }
    minim_obj *fields = (minim_obj *)(p - MINIM_TAG_TYPED);
    fields[0] = minim_make_header(flags | MINIM_PORT_CLOSED, MINIM_TYPE_PORT);
    fields[5] = (minim_obj)NULL;
}

// Whether `x` is a port with every flag of `flags`.
static bool has_flags(minim_obj x, unsigned flags) {
    return minim_is_port(x) && (minim_port_flags(x) & flags) == flags;
}

static minim_obj prim_is_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_port(args[0]));
}

static minim_obj prim_is_input_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(has_flags(args[0], MINIM_PORT_INPUT));
}

static minim_obj prim_is_output_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(has_flags(args[0], MINIM_PORT_OUTPUT));
}

// Every port is textual.
static minim_obj prim_is_binary_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return MINIM_FALSE;
}

// Whether the port `x` is open for `dir`; raises `msg` unless `x` is a port.
static minim_obj is_open_for(minim_obj x, unsigned dir, const char *msg) {
    check(minim_is_port(x), msg, x);
    return boolean((minim_port_flags(x) & (dir | MINIM_PORT_CLOSED)) == dir);
}

static minim_obj prim_is_input_port_open(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return is_open_for(args[0], MINIM_PORT_INPUT, "input-port-open?: not a port");
}

static minim_obj prim_is_output_port_open(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return is_open_for(args[0], MINIM_PORT_OUTPUT, "output-port-open?: not a port");
}

static minim_obj prim_close_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_port(args[0]), "close-port: not a port", args[0]);
    minim_port_close(args[0]);
    return MINIM_VOID;
}

static minim_obj prim_close_input_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(has_flags(args[0], MINIM_PORT_INPUT), "close-input-port: not an input port", args[0]);
    minim_port_close(args[0]);
    return MINIM_VOID;
}

static minim_obj prim_close_output_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(has_flags(args[0], MINIM_PORT_OUTPUT), "close-output-port: not an output port",
          args[0]);
    minim_port_close(args[0]);
    return MINIM_VOID;
}

static minim_obj prim_open_input_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "open-input-string: not a string", args[0]);
    return minim_open_input_string(args[0]);
}

static minim_obj prim_open_output_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return minim_open_output_string();
}

static minim_obj prim_open_input_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "open-input-file: not a string", args[0]);
    return minim_open_input_file(args[0]);
}

static minim_obj prim_open_output_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "open-output-file: not a string", args[0]);
    return minim_open_output_file(args[0]);
}

static minim_obj prim_eof_object(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return MINIM_EOF;
}

static minim_obj prim_is_eof_object(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(args[0] == MINIM_EOF);
}

// The standard port `args[0]`: 0 for input, 1 for output, 2 for errors.
static minim_obj prim_standard_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t i = fixnum_arg(args[0], "%standard-port: not 0, 1, or 2");
    check(i >= 0 && i <= 2, "%standard-port: not 0, 1, or 2", args[0]);
    return minim_standard_port((int)i);
}

const minim_prim_def minim_port_prims[] = {
    {"port?", prim_is_port, 1, 1},
    {"input-port?", prim_is_input_port, 1, 1},
    {"output-port?", prim_is_output_port, 1, 1},
    {"textual-port?", prim_is_port, 1, 1},
    {"binary-port?", prim_is_binary_port, 1, 1},
    {"input-port-open?", prim_is_input_port_open, 1, 1},
    {"output-port-open?", prim_is_output_port_open, 1, 1},
    {"close-port", prim_close_port, 1, 1},
    {"close-input-port", prim_close_input_port, 1, 1},
    {"close-output-port", prim_close_output_port, 1, 1},
    {"open-input-string", prim_open_input_string, 1, 1},
    {"open-output-string", prim_open_output_string, 0, 0},
    {"open-input-file", prim_open_input_file, 1, 1},
    {"open-output-file", prim_open_output_file, 1, 1},
    {"eof-object", prim_eof_object, 0, 0},
    {"eof-object?", prim_is_eof_object, 1, 1},
    {"%standard-port", prim_standard_port, 1, 1},
    {NULL, NULL, 0, 0},
};
