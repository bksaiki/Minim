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

// A port's lookahead: none, or the end of input, peeked; else a char.
enum { NO_LOOKAHEAD = -1, EOF_LOOKAHEAD = -2 };

// The fields of the port `p`, its header first.
static minim_obj *fields(minim_obj p) {
    return (minim_obj *)(p - MINIM_TAG_TYPED);
}

// A new port with `flags`, named `name`, over the string `buffer` or `file`.
static minim_obj make_port(unsigned flags, minim_obj buffer, minim_obj name, FILE *file) {
    MINIM_FRAME(&buffer, &name);
    minim_obj *p = minim_heap_alloc(MINIM_PORT_SIZE);
    p[0] = minim_make_header(flags, MINIM_TYPE_PORT);
    p[1] = buffer;
    p[2] = name;
    p[3] = minim_fixnum(0);
    p[4] = minim_fixnum(NO_LOOKAHEAD);
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

// Closes the file of a file port found dead, unless closed already.
static void close_dead(minim_obj p) {
    FILE *f = minim_port_file(p);
    if (f != NULL)
        fclose(f);
}

// A port of direction `dir` over the file `path`; raises a file error, `msg`,
// if it can't be opened.  Its file is closed when it is collected.
static minim_obj open_file(minim_obj path, unsigned dir, const char *msg) {
    char *name = minim_string_to_utf8(path);
    FILE *f = minim_fopen(name, dir == MINIM_PORT_INPUT ? "r" : "w");
    free(name);
    if (f == NULL)
        minim_error_of_kind(MINIM_ERROR_FILE, msg, path);
    minim_obj s = copy_string(path);
    minim_obj p = make_port(dir | MINIM_PORT_FILE, MINIM_FALSE, s, f);
    minim_gc_add_destructor(p, close_dead);
    return p;
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
    minim_obj *f = fields(p);
    f[0] = minim_make_header(flags | MINIM_PORT_CLOSED, MINIM_TYPE_PORT);
    f[5] = (minim_obj)NULL;
}

int32_t minim_port_read_char(minim_obj p) {
    minim_obj *f = fields(p);
    intptr_t ahead = minim_fixnum_value(f[4]);
    if (ahead != NO_LOOKAHEAD) {
        f[4] = minim_fixnum(NO_LOOKAHEAD);
        return ahead == EOF_LOOKAHEAD ? -1 : (int32_t)ahead;
    }
    if (minim_port_flags(p) & MINIM_PORT_FILE)
        return minim_utf8_getc(minim_port_file(p));
    size_t i = (size_t)minim_fixnum_value(f[3]);
    if (i == minim_string_length(f[1]))
        return -1;
    f[3] = minim_fixnum((intptr_t)i + 1);
    return (int32_t)minim_string_ref(f[1], i);
}

// Peeking the end keeps it too, so a terminal's end of input is read once.
int32_t minim_port_peek_char(minim_obj p) {
    int32_t c = minim_port_read_char(p);
    fields(p)[4] = minim_fixnum(c < 0 ? EOF_LOOKAHEAD : c);
    return c;
}

// A string port's buffer doubles when full.
void minim_port_write_char(minim_obj p, uint32_t c) {
    if (minim_port_flags(p) & MINIM_PORT_FILE) {
        unsigned char bytes[4];
        fwrite(bytes, 1, minim_utf8_encode(c, bytes), minim_port_file(p));
        return;
    }
    MINIM_FRAME(&p);
    size_t i = (size_t)minim_fixnum_value(fields(p)[3]);
    size_t cap = minim_string_length(fields(p)[1]);
    if (i == cap) {
        minim_obj s = minim_make_string(2 * cap, 0);
        memcpy(minim_string_chars(s), minim_string_chars(fields(p)[1]), cap * sizeof(uint32_t));
        fields(p)[1] = s;
        minim_gc_write(p, s);
    }
    minim_string_set(fields(p)[1], i, c);
    fields(p)[3] = minim_fixnum((intptr_t)i + 1);
    MINIM_RETURN_VOID;
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

// Whether `x` is a port open for `dir`.
static bool is_open(minim_obj x, unsigned dir) {
    return minim_is_port(x) && (minim_port_flags(x) & (dir | MINIM_PORT_CLOSED)) == dir;
}

// Whether the port `x` is open for `dir`; raises `msg` unless `x` is a port.
static minim_obj is_open_for(minim_obj x, unsigned dir, const char *msg) {
    check(minim_is_port(x), msg, x);
    return boolean(is_open(x, dir));
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

// The chars `chars[0..n)` as a new string; frees `chars`.
static minim_obj string_of(uint32_t *chars, size_t n) {
    minim_obj s = minim_make_string(n, 0);
    if (n > 0)  // `chars` is NULL when empty
        memcpy(minim_string_chars(s), chars, n * sizeof(uint32_t));
    free(chars);
    return s;
}

// Appends `c` to the malloc'd `*chars` of length `*n`.
static void push_char(uint32_t **chars, size_t *n, uint32_t c) {
    if ((*n & (*n - 1)) == 0) {  // full at 0, 1, 2, 4, ...
        *chars = realloc(*chars, (*n ? 2 * *n : 1) * sizeof(uint32_t));
        if (*chars == NULL)
            minim_out_of_memory();
    }
    (*chars)[(*n)++] = c;
}

// A char read, or the eof object.
static minim_obj char_or_eof(int32_t c) {
    return c < 0 ? MINIM_EOF : minim_char((uint32_t)c);
}

static minim_obj prim_read_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(is_open(args[0], MINIM_PORT_INPUT), "read-char: not an open input port", args[0]);
    return char_or_eof(minim_port_read_char(args[0]));
}

static minim_obj prim_peek_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(is_open(args[0], MINIM_PORT_INPUT), "peek-char: not an open input port", args[0]);
    return char_or_eof(minim_port_peek_char(args[0]));
}

// The chars up to a line's end, a linefeed, a carriage return, or both, or the
// end of input; the eof object if there are none before the end.
static minim_obj prim_read_line(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_obj p = args[0];
    check(is_open(p, MINIM_PORT_INPUT), "read-line: not an open input port", p);
    uint32_t *chars = NULL;
    size_t len = 0;
    int32_t c;
    while ((c = minim_port_read_char(p)) >= 0 && c != '\n' && c != '\r')
        push_char(&chars, &len, (uint32_t)c);
    if (c == '\r' && minim_port_peek_char(p) == '\n')
        minim_port_read_char(p);
    if (c < 0 && len == 0)
        return MINIM_EOF;
    return string_of(chars, len);
}

// Up to `args[0]` chars, or the eof object if there are none before the end.
static minim_obj prim_read_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t k = fixnum_arg(args[0], "read-string: not a non-negative fixnum");
    check(k >= 0, "read-string: not a non-negative fixnum", args[0]);
    check(is_open(args[1], MINIM_PORT_INPUT), "read-string: not an open input port", args[1]);
    uint32_t *chars = NULL;
    size_t len = 0;
    int32_t c = 0;
    while (len < (size_t)k && (c = minim_port_read_char(args[1])) >= 0)
        push_char(&chars, &len, (uint32_t)c);
    if (c < 0 && len == 0)
        return MINIM_EOF;
    return string_of(chars, len);
}

// Always true: stdio can't tell whether input is waiting (ports.md).
static minim_obj prim_is_char_ready(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(is_open(args[0], MINIM_PORT_INPUT), "char-ready?: not an open input port", args[0]);
    return MINIM_TRUE;
}

static minim_obj prim_write_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    uint32_t c = (uint32_t)char_arg(args[0], "write-char: not a char");
    check(is_open(args[1], MINIM_PORT_OUTPUT), "write-char: not an open output port", args[1]);
    minim_port_write_char(args[1], c);
    return MINIM_VOID;
}

// Writes the string `args[0]` from `args[2]` to `args[3]`, #f for its end.
static minim_obj prim_write_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "write-string: not a string", args[0]);
    check(is_open(args[1], MINIM_PORT_OUTPUT), "write-string: not an open output port", args[1]);
    size_t len = minim_string_length(args[0]);
    size_t end = args[3] == MINIM_FALSE ? len : bound_arg(args[3], len, "write-string: bad end");
    size_t start = bound_arg(args[2], end, "write-string: bad start");
    for (size_t i = start; i < end; i++)
        minim_port_write_char(args[1], minim_string_ref(args[0], i));
    return MINIM_VOID;
}

static minim_obj prim_flush_output_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(is_open(args[0], MINIM_PORT_OUTPUT), "flush-output-port: not an open output port",
          args[0]);
    if (minim_port_flags(args[0]) & MINIM_PORT_FILE)
        fflush(minim_port_file(args[0]));
    return MINIM_VOID;
}

// A copy of what was written to the string port `args[0]` so far; unlike
// Chez's, it doesn't reset the port.
static minim_obj prim_get_output_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    minim_obj p = args[0];
    check(minim_is_port(p) && (minim_port_flags(p) & (MINIM_PORT_OUTPUT | MINIM_PORT_FILE))
              == MINIM_PORT_OUTPUT,
          "get-output-string: not a string output port", p);
    MINIM_FRAME(&p);
    size_t len = (size_t)minim_fixnum_value(fields(p)[3]);
    minim_obj s = minim_make_string(len, 0);
    memcpy(minim_string_chars(s), minim_string_chars(fields(p)[1]), len * sizeof(uint32_t));
    MINIM_RETURN(s);
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
    {"%read-char", prim_read_char, 1, 1},
    {"%peek-char", prim_peek_char, 1, 1},
    {"%read-line", prim_read_line, 1, 1},
    {"%read-string", prim_read_string, 2, 2},
    {"%char-ready?", prim_is_char_ready, 1, 1},
    {"%write-char", prim_write_char, 2, 2},
    {"%write-string", prim_write_string, 4, 4},
    {"%flush-output-port", prim_flush_output_port, 1, 1},
    {"get-output-string", prim_get_output_string, 1, 1},
    {NULL, NULL, 0, 0},
};
