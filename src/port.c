/**
 * @file
 * Ports: textual input and output over files and strings, binary over files
 * and bytevectors, and their primitives.
 */

#include "port.h"

#include <stdlib.h>
#include <string.h>

#include "char.h"
#include "gc.h"
#include "heap.h"
#include "prims.h"
#include "system.h"

static minim_obj standard[3];      // over stdin, stdout, and stderr
static minim_obj string_name;      // every string port's name
static minim_obj bytevector_name;  // every bytevector port's name

// Lookahead sentinels; any other value is a char or byte.
enum { NO_LOOKAHEAD = -1, EOF_LOOKAHEAD = -2 };

// The fields of the port `p`, its header first.
static minim_obj *fields(minim_obj p) {
    return (minim_obj *)(p - MINIM_TAG_TYPED);
}

// A new port with `flags`, named `name`, over the string or bytevector `buffer`,
// or `file`.
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

// A new string of length `cap`, starting with the first `len` chars of `s`.
static minim_obj copy_prefix(minim_obj s, size_t len, size_t cap) {
    MINIM_FRAME(&s);
    minim_obj t = minim_make_string(cap, 0);
    memcpy(minim_string_chars(t), minim_string_chars(s), len * sizeof(uint32_t));
    MINIM_RETURN(t);
}

// A new bytevector of length `cap`, starting with the first `len` bytes of `bv`.
static minim_obj copy_bytes(minim_obj bv, size_t len, size_t cap) {
    MINIM_FRAME(&bv);
    minim_obj copy = minim_make_bytevector(cap, 0);
    memcpy(minim_bytevector_data(copy), minim_bytevector_data(bv), len);
    MINIM_RETURN(copy);
}

void minim_port_init(void) {
    static const char *const names[] = {"stdin", "stdout", "stderr"};
    FILE *const files[] = {stdin, stdout, stderr};
    string_name = minim_string_from_cstr("string");
    minim_gc_add_global(&string_name);
    bytevector_name = minim_string_from_cstr("bytevector");
    minim_gc_add_global(&bytevector_name);
    for (int i = 0; i < 3; i++) {
        minim_obj name = minim_string_from_cstr(names[i]);
        unsigned dir = i == 0 ? MINIM_PORT_INPUT : MINIM_PORT_OUTPUT;
        standard[i] = make_port(dir | MINIM_PORT_FILE, MINIM_FALSE, name, files[i]);
        minim_gc_add_global(&standard[i]);
    }
}

minim_obj minim_standard_port(int i) {
    return standard[i];
}

minim_obj minim_open_input_string(minim_obj s) {
    s = copy_prefix(s, minim_string_length(s), minim_string_length(s));
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

minim_obj minim_port_open_file(minim_obj path, unsigned flags, const char *msg) {
    static const char *const modes[2][2] = {{"w", "wb"}, {"r", "rb"}};
    bool input = flags & MINIM_PORT_INPUT;
    bool binary = flags & MINIM_PORT_BINARY;
    char *name = minim_string_to_utf8(path);
    FILE *f = minim_fopen(name, modes[input][binary]);
    free(name);
    if (f == NULL)
        minim_error_of_kind(MINIM_ERROR_FILE, msg, path);
    minim_obj s = copy_prefix(path, minim_string_length(path), minim_string_length(path));
    minim_obj p = make_port(flags | MINIM_PORT_FILE, MINIM_FALSE, s, f);
    minim_gc_add_destructor(p, close_dead);
    return p;
}

minim_obj minim_open_input_file(minim_obj path) {
    return minim_port_open_file(path, MINIM_PORT_INPUT, "open-input-file: cannot open file");
}

minim_obj minim_open_output_file(minim_obj path) {
    return minim_port_open_file(path, MINIM_PORT_OUTPUT, "open-output-file: cannot open file");
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

// Takes the lookahead of the port `p`: a char or byte, -1 for the end, or
// NO_LOOKAHEAD if there is none.
static int32_t take_lookahead(minim_obj p) {
    minim_obj *f = fields(p);
    intptr_t ahead = minim_fixnum_value(f[4]);
    if (ahead == NO_LOOKAHEAD)
        return NO_LOOKAHEAD;
    f[4] = minim_fixnum(NO_LOOKAHEAD);
    return ahead == EOF_LOOKAHEAD ? -1 : (int32_t)ahead;
}

int32_t minim_port_read_char(minim_obj p) {
    int32_t ahead = take_lookahead(p);
    if (ahead != NO_LOOKAHEAD)
        return ahead;
    minim_obj *f = fields(p);
    if (minim_port_flags(p) & MINIM_PORT_FILE)
        return minim_utf8_getc(minim_port_file(p));
    size_t i = (size_t)minim_fixnum_value(f[3]);
    if (i == minim_string_length(f[1]))
        return -1;
    f[3] = minim_fixnum((intptr_t)i + 1);
    return (int32_t)minim_string_ref(f[1], i);
}

// Reads a byte from the open binary input port `p`, or returns -1 at its end.
static int32_t read_byte(minim_obj p) {
    int32_t ahead = take_lookahead(p);
    if (ahead != NO_LOOKAHEAD)
        return ahead;
    if (minim_port_flags(p) & MINIM_PORT_FILE) {
        int b = getc(minim_port_file(p));
        return b == EOF ? -1 : b;
    }
    minim_obj *f = fields(p);
    size_t i = (size_t)minim_fixnum_value(f[3]);
    if (i == minim_bytevector_length(f[1]))
        return -1;
    f[3] = minim_fixnum((intptr_t)i + 1);
    return minim_bytevector_data(f[1])[i];
}

// Makes room for `n` more bytes in the bytevector output port `*p`, which the
// caller roots, doubling its buffer as needed; returns where they go.
static size_t reserve_bytes(const minim_obj *p, size_t n) {
    size_t len = (size_t)minim_fixnum_value(fields(*p)[3]);
    size_t cap = minim_bytevector_length(fields(*p)[1]);
    if (len + n > cap) {
        while (len + n > cap)
            cap *= 2;
        minim_obj bv = copy_bytes(fields(*p)[1], len, cap);  // may move `*p`
        fields(*p)[1] = bv;
        minim_gc_write(*p, bv);
    }
    fields(*p)[3] = minim_fixnum((intptr_t)(len + n));
    return len;
}

// Writes the byte `b` to the open binary output port `*p`, which the caller
// roots.
static void write_byte(const minim_obj *p, uint8_t b) {
    if (minim_port_flags(*p) & MINIM_PORT_FILE) {
        putc(b, minim_port_file(*p));
        return;
    }
    size_t i = reserve_bytes(p, 1);
    minim_bytevector_data(fields(*p)[1])[i] = b;
}

// The end is kept too, so a terminal's end of input is read once.
void minim_port_unread_char(minim_obj p, int32_t c) {
    fields(p)[4] = minim_fixnum(c < 0 ? EOF_LOOKAHEAD : c);
}

minim_obj minim_get_output_string(minim_obj p) {
    size_t len = (size_t)minim_fixnum_value(fields(p)[3]);
    return copy_prefix(fields(p)[1], len, len);
}

int32_t minim_port_peek_char(minim_obj p) {
    int32_t c = minim_port_read_char(p);
    minim_port_unread_char(p, c);
    return c;
}

void minim_port_write_char(minim_obj p, uint32_t c) {
    minim_port_write_chars(p, &c, 1);
}

void minim_port_write_chars(minim_obj p, const uint32_t *chars, size_t n) {
    if (n == 0)  // `chars` may be NULL
        return;
    if (minim_port_flags(p) & MINIM_PORT_FILE) {
        for (size_t i = 0; i < n; i++) {
            unsigned char bytes[4];
            fwrite(bytes, 1, minim_utf8_encode(chars[i], bytes), minim_port_file(p));
        }
        return;
    }
    MINIM_FRAME(&p);
    size_t len = (size_t)minim_fixnum_value(fields(p)[3]);
    size_t cap = minim_string_length(fields(p)[1]);
    if (len + n > cap) {
        while (len + n > cap)
            cap *= 2;
        minim_obj s = copy_prefix(fields(p)[1], len, cap);
        fields(p)[1] = s;
        minim_gc_write(p, s);
    }
    memcpy(minim_string_chars(fields(p)[1]) + len, chars, n * sizeof(uint32_t));
    fields(p)[3] = minim_fixnum((intptr_t)(len + n));
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

static minim_obj prim_is_textual_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(minim_is_port(args[0]) && !(minim_port_flags(args[0]) & MINIM_PORT_BINARY));
}

static minim_obj prim_is_binary_port(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    return boolean(has_flags(args[0], MINIM_PORT_BINARY));
}

// Whether `x` is a port open for `dir`.
static bool is_open(minim_obj x, unsigned dir) {
    return minim_is_port(x) && (minim_port_flags(x) & (dir | MINIM_PORT_CLOSED)) == dir;
}

// Raises `msg` unless `x` is a port open for `dir`, binary if `binary`, else
// textual.
static void check_open(minim_obj x, unsigned dir, bool binary, const char *msg) {
    check(is_open(x, dir) && (bool)(minim_port_flags(x) & MINIM_PORT_BINARY) == binary, msg, x);
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

// A char read, or the eof object.
static minim_obj char_or_eof(int32_t c) {
    return c < 0 ? MINIM_EOF : minim_char((uint32_t)c);
}

static minim_obj prim_read_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, false, "read-char: not an open textual input port");
    return char_or_eof(minim_port_read_char(args[0]));
}

static minim_obj prim_peek_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, false, "peek-char: not an open textual input port");
    return char_or_eof(minim_port_peek_char(args[0]));
}

// The next line, without its \n, \r, or \r\n; the eof object at the end.
static minim_obj prim_read_line(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, false, "read-line: not an open textual input port");
    minim_obj line = minim_open_output_string();
    MINIM_FRAME(&line);
    size_t len = 0;
    int32_t c;
    for (; (c = minim_port_read_char(args[0])) >= 0 && c != '\n' && c != '\r'; len++)
        minim_port_write_char(line, (uint32_t)c);
    if (c == '\r' && minim_port_peek_char(args[0]) == '\n')
        minim_port_read_char(args[0]);
    MINIM_RETURN(c < 0 && len == 0 ? MINIM_EOF : minim_get_output_string(line));
}

// Up to `args[0]` chars, or the eof object if there are none before the end.
static minim_obj prim_read_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t k = fixnum_arg(args[0], "read-string: not a non-negative fixnum");
    check(k >= 0, "read-string: not a non-negative fixnum", args[0]);
    check_open(args[1], MINIM_PORT_INPUT, false, "read-string: not an open textual input port");
    minim_obj chars = minim_open_output_string();
    MINIM_FRAME(&chars);
    size_t len = 0;
    int32_t c = 0;
    for (; len < (size_t)k && (c = minim_port_read_char(args[1])) >= 0; len++)
        minim_port_write_char(chars, (uint32_t)c);
    MINIM_RETURN(c < 0 && len == 0 ? MINIM_EOF : minim_get_output_string(chars));
}

static minim_obj prim_read(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, false, "read: not an open textual input port");
    return minim_read(args[0]);
}

// Always true: stdio can't tell whether input is waiting (ports.md).
static minim_obj prim_is_char_ready(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, false, "char-ready?: not an open textual input port");
    return MINIM_TRUE;
}

static minim_obj prim_write_char(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    uint32_t c = (uint32_t)char_arg(args[0], "write-char: not a char");
    check_open(args[1], MINIM_PORT_OUTPUT, false, "write-char: not an open textual output port");
    minim_port_write_char(args[1], c);
    return MINIM_VOID;
}

// Writes the string `args[0]` from `args[2]` to `args[3]`, #f for its end.
static minim_obj prim_write_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "write-string: not a string", args[0]);
    check_open(args[1], MINIM_PORT_OUTPUT, false, "write-string: not an open textual output port");
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

// Whether `x` is a string or bytevector output port, binary if `binary`.
static bool is_buffer_output(minim_obj x, bool binary) {
    unsigned want = MINIM_PORT_OUTPUT | (binary ? MINIM_PORT_BINARY : 0);
    unsigned mask = MINIM_PORT_OUTPUT | MINIM_PORT_FILE | MINIM_PORT_BINARY;
    return minim_is_port(x) && (minim_port_flags(x) & mask) == want;
}

static minim_obj prim_get_output_string(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(is_buffer_output(args[0], false), "get-output-string: not a string output port",
          args[0]);
    return minim_get_output_string(args[0]);
}

static minim_obj prim_write(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[1], MINIM_PORT_OUTPUT, false, "write: not an open textual output port");
    minim_port_write(args[1], args[0]);
    return MINIM_VOID;
}

static minim_obj prim_display(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[1], MINIM_PORT_OUTPUT, false, "display: not an open textual output port");
    minim_port_display(args[1], args[0]);
    return MINIM_VOID;
}

static minim_obj open_output_bytevector(void) {
    minim_obj bv = minim_make_bytevector(16, 0);
    return make_port(MINIM_PORT_OUTPUT | MINIM_PORT_BINARY, bv, bytevector_name, NULL);
}

static minim_obj prim_open_input_bytevector(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_bytevector(args[0]), "open-input-bytevector: not a bytevector", args[0]);
    size_t len = minim_bytevector_length(args[0]);
    minim_obj bv = copy_bytes(args[0], len, len);
    return make_port(MINIM_PORT_INPUT | MINIM_PORT_BINARY, bv, bytevector_name, NULL);
}

static minim_obj prim_open_binary_input_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "open-binary-input-file: not a string", args[0]);
    return minim_port_open_file(args[0], MINIM_PORT_INPUT | MINIM_PORT_BINARY,
                                "open-binary-input-file: cannot open file");
}

static minim_obj prim_open_binary_output_file(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(minim_is_string(args[0]), "open-binary-output-file: not a string", args[0]);
    return minim_port_open_file(args[0], MINIM_PORT_OUTPUT | MINIM_PORT_BINARY,
                                "open-binary-output-file: cannot open file");
}

static minim_obj prim_open_output_bytevector(minim_obj *args, size_t n) {
    MINIM_UNUSED(args, n);
    return open_output_bytevector();
}

static minim_obj get_output_bytevector(minim_obj p) {
    size_t len = (size_t)minim_fixnum_value(fields(p)[3]);
    return copy_bytes(fields(p)[1], len, len);
}

static minim_obj prim_get_output_bytevector(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check(is_buffer_output(args[0], true), "get-output-bytevector: not a bytevector output port",
          args[0]);
    return get_output_bytevector(args[0]);
}

// A byte read, or the eof object.
static minim_obj byte_or_eof(int32_t b) {
    return b < 0 ? MINIM_EOF : minim_fixnum(b);
}

static minim_obj prim_read_u8(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, true, "read-u8: not an open binary input port");
    return byte_or_eof(read_byte(args[0]));
}

static minim_obj prim_peek_u8(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, true, "peek-u8: not an open binary input port");
    int32_t b = read_byte(args[0]);
    minim_port_unread_char(args[0], b);
    return byte_or_eof(b);
}

// Always true, as char-ready? is.
static minim_obj prim_is_u8_ready(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    check_open(args[0], MINIM_PORT_INPUT, true, "u8-ready?: not an open binary input port");
    return MINIM_TRUE;
}

// Up to `args[0]` bytes, or the eof object if there are none before the end.
static minim_obj prim_read_bytevector(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    intptr_t k = fixnum_arg(args[0], "read-bytevector: not a non-negative fixnum");
    check(k >= 0, "read-bytevector: not a non-negative fixnum", args[0]);
    check_open(args[1], MINIM_PORT_INPUT, true, "read-bytevector: not an open binary input port");
    minim_obj bytes = open_output_bytevector();
    MINIM_FRAME(&bytes);
    size_t len = 0;
    int32_t b = 0;
    for (; len < (size_t)k && (b = read_byte(args[1])) >= 0; len++)
        write_byte(&bytes, (uint8_t)b);
    MINIM_RETURN(b < 0 && len == 0 ? MINIM_EOF : get_output_bytevector(bytes));
}

// Reads into the bytevector args[0], in the range args[2] and args[3], from
// args[1]; returns how many bytes, or the eof object if there are none before
// the end.
static minim_obj prim_read_bytevector_into(minim_obj *args, size_t n) {
    check(minim_is_bytevector(args[0]), "read-bytevector!: not a bytevector", args[0]);
    check_open(args[1], MINIM_PORT_INPUT, true,
               "read-bytevector!: not an open binary input port");
    range r = range_args(args, n, 2, minim_bytevector_length(args[0]),
                         "read-bytevector!: bad index");
    size_t i = r.start;
    int32_t b = 0;
    for (; i < r.end && (b = read_byte(args[1])) >= 0; i++)
        minim_bytevector_data(args[0])[i] = (uint8_t)b;
    return b < 0 && i == r.start ? MINIM_EOF : minim_fixnum((intptr_t)(i - r.start));
}

static minim_obj prim_write_u8(minim_obj *args, size_t n) {
    MINIM_UNUSED(n);
    uint8_t b = byte_arg(args[0], "write-u8: not a byte");
    check_open(args[1], MINIM_PORT_OUTPUT, true, "write-u8: not an open binary output port");
    write_byte(&args[1], b);
    return MINIM_VOID;
}

// Writes the bytes of the bytevector args[0] in the range args[2] and args[3]
// to args[1].
static minim_obj prim_write_bytevector(minim_obj *args, size_t n) {
    check(minim_is_bytevector(args[0]), "write-bytevector: not a bytevector", args[0]);
    check_open(args[1], MINIM_PORT_OUTPUT, true,
               "write-bytevector: not an open binary output port");
    range r = range_args(args, n, 2, minim_bytevector_length(args[0]),
                         "write-bytevector: bad index");
    size_t count = r.end - r.start;
    if (minim_port_flags(args[1]) & MINIM_PORT_FILE) {
        fwrite(minim_bytevector_data(args[0]) + r.start, 1, count, minim_port_file(args[1]));
        return MINIM_VOID;
    }
    size_t at = reserve_bytes(&args[1], count);
    memcpy(minim_bytevector_data(fields(args[1])[1]) + at, minim_bytevector_data(args[0]) + r.start,
           count);
    return MINIM_VOID;
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
    {"textual-port?", prim_is_textual_port, 1, 1},
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
    {"%read", prim_read, 1, 1},
    {"%char-ready?", prim_is_char_ready, 1, 1},
    {"%write-char", prim_write_char, 2, 2},
    {"%write-string", prim_write_string, 4, 4},
    {"%flush-output-port", prim_flush_output_port, 1, 1},
    {"get-output-string", prim_get_output_string, 1, 1},
    {"%write", prim_write, 2, 2},
    {"%display", prim_display, 2, 2},
    {"open-input-bytevector", prim_open_input_bytevector, 1, 1},
    {"open-output-bytevector", prim_open_output_bytevector, 0, 0},
    {"get-output-bytevector", prim_get_output_bytevector, 1, 1},
    {"open-binary-input-file", prim_open_binary_input_file, 1, 1},
    {"open-binary-output-file", prim_open_binary_output_file, 1, 1},
    {"%read-u8", prim_read_u8, 1, 1},
    {"%peek-u8", prim_peek_u8, 1, 1},
    {"%u8-ready?", prim_is_u8_ready, 1, 1},
    {"%read-bytevector", prim_read_bytevector, 2, 2},
    {"%read-bytevector!", prim_read_bytevector_into, 2, 4},
    {"%write-u8", prim_write_u8, 2, 2},
    {"%write-bytevector", prim_write_bytevector, 2, 4},
    {NULL, NULL, 0, 0},
};
