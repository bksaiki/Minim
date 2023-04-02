/*
    Fast-Load Serialization (FASL)

    Serialization library for Scheme objects
*/

#include "../minim.h"

/* FASL representation
 *
 * <fasl-file> ::= <fasl-prefix> <fasl-group>* <fasl-suffix>
 * 
 * <fasl-prefix> ::= #@IK0
 * 
 * <fasl-suffix> ::= @ {EOF}
 * 
 * <fasl-group> ::= <fasl-header> <fasl-object>*
 * 
 * <fasl-header> ::= \0\0\0 minim <word version>
 * 
 * <fasl-object> 
 * 
 * <fasl> ::= {null}
 *        
 *        ::= {true}
 * 
 *        ::= {false}
 * 
 *        ::= {eof}
 * 
 *        ::= {void}
 * 
 *        ::= {symbol} <word n> <byte char1> ... <byte charn>
 * 
 *        ::= {string} <word n> <byte char1> ... <byte charn>
 * 
 *        ::= {fixnum} <word value>
 * 
 *        ::= {char} <byte>
 *
 *        ::= {pair} <word n> <fasl elt1> ... <fasl eltn> <fasl cdr>
 * 
 *        ::= {vector} <word n> <fasl elt1> ... <fasl eltn>
 *
 *        ::= {hashtable} <word n> 
 *                        <keyval kv1> ... <keyval kvn>
 *            <keyval> -> <fasl key> <fasl val>
 * 
 *        ::= {base rtd}
 * 
 *        ::= {record} <word size>
 *                     <word n>
 *                     <fasl rtd>
 *                     <field elt1> ... <field eltn>
 *
*/

//
//  Reading FASL
//

minim_object *read_fasl(FILE *out) {

}

//
//  Writing FASL
//

#define write_fasl_type(o, t)       (fputc(((minim_byte) (t)), (o)))
#define write_fasl_byte(o, b)       (fputc(((minim_byte) (b)), (o)))

// Serializes a word (64-bits) in LSB first
static void write_fasl_uptr(FILE *out, minim_uptr u) {
    int i;

    for (i = 0; i < ptr_size; ++i) {
        write_fasl_byte(out, i & 0xFF);
        u >>= 8;
    }
}

// Serializes a symbol
static void write_fasl_symbol(FILE *out, minim_object *sym) {
    char *s;

    write_fasl_uptr(out, strlen(minim_symbol(sym)));
    for (s = minim_symbol(sym); *s; s++)
        write_fasl_byte(out, *s);
}

// Serializes a string
static void write_fasl_string(FILE *out, minim_object *str) {
    char *s;

    write_fasl_uptr(out, strlen(minim_string(str)));
    for (s = minim_string(str); *s; s++)
        fputc(*s, out);
}

// Serializes a pair, improper list, or list
static void write_fasl_pair(FILE *out, minim_object *p) {
    minim_object *it;
    long len;
    
    len = improper_list_length(p);
    write_fasl_uptr(out, len);
    for (it = p; minim_is_pair(it); it = minim_cdr(it))
        write_fasl(out, minim_car(it));
    
    write_fasl(out, it);
}

// Serializes a vector
static void write_fasl_vector(FILE *out, minim_object *v) {
    long i, len;
    
    len = minim_vector_len(v);
    write_fasl_uptr(out, len);
    for (i = 0; i < len; ++i)
        write_fasl(out, minim_vector_ref(v, i));
}

void write_fasl(FILE *out, minim_object *o) {
    if (minim_is_null(o)) {
        write_fasl_type(out, FASL_NULL);
    } else if (minim_is_true(o)) {
        write_fasl_type(out, FASL_TRUE);
    } else if (minim_is_false(o)) {
        write_fasl_type(out, FASL_FALSE);
    } else if (minim_is_eof(o)) {
        write_fasl_type(out, FASL_EOF);
    } else if (minim_is_void(o)) {
        write_fasl_type(out, FASL_VOID);
    } else if (minim_is_symbol(o)) {
        // TODO: interned vs. uninterned
        write_fasl_type(out, FASL_SYMBOL);
        write_fasl_symbol(out, o);
    } else if (minim_is_string(o)) {
        write_fasl_type(out, FASL_STRING);
        write_fasl_string(out, o);
    } else if (minim_is_fixnum(o)) {
        write_fasl_type(out, FASL_FIXNUM);
        write_fasl_uptr(out, minim_fixnum(o));
    } else if (minim_is_char(o)) {
        write_fasl_type(out, FASL_CHAR);
        write_fasl_byte(out, minim_char(o));
    } else if (minim_is_pair(o)) {
        write_fasl_type(out, FASL_PAIR);
        write_fasl_pair(out, o);
    } else if (minim_is_vector(o)) {
        write_fasl_type(out, FASL_VECTOR);
        write_fasl_vector(out, o);
    } else {
        fprintf(stderr, "write_fasl: object not serializable\n");
        fprintf(stderr, " object: ");
        write_object(stderr, o);
        fprintf(stderr, "\n");
        exit(1);
    }
}

//
//  Primitive
//

minim_object *read_fasl_proc(int argc, minim_object **args) {
    fprintf(stderr, "read_fasl_proc: unimplemented\n");
    exit(1);
}

minim_object *write_fasl_proc(int argc, minim_object **args) {
    // (-> any void)
    // (-> any output-port void)
    minim_object *out_p, *o;

    o = args[0];
    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_is_output_port(out_p))
            bad_type_exn("write-fasl", "output-port?", out_p);
    }

    write_fasl(minim_port(out_p), o);
    return minim_void;
}
