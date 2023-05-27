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
 *        ::= {empty vector}
 * 
 *        ::= {symbol} <uptr n> <byte char1> ... <byte charn>
 * 
 *        ::= {string} <uptr n> <byte char1> ... <byte charn>
 * 
 *        ::= {fixnum} <uptr value>
 * 
 *        ::= {char} <byte>
 *
 *        ::= {pair} <uptr n> <fasl elt1> ... <fasl eltn> <fasl cdr>
 * 
 *        ::= {vector} <uptr n> <fasl elt1> ... <fasl eltn>
 *
 *        ::= {hashtable} <uptr n> 
 *                        <keyval kv1> ... <keyval kvn>
 *            <keyval> -> <fasl key> <fasl val>
 * 
 *        ::= {base rtd}
 * 
 *        ::= {record} <uptr n>
 *                     <fasl rtd>
 *                     <field elt1> ... <field eltn>
 * 
 * <uptr n> ::= <ubyte1> ... <ubyte1> <ubyte0>
 * <ubyte1> ::= { k | 1, 0 <= k < 128 }
 * <ubyte0> ::= { k | 0, 0 <= k < 128 }
 *              ubytes encode a mixed-length unsigned integer
 *              each byte stores 7 bits of the uptr and the low
 *              bit signals if the next byte is part of the same uptr
 *
*/

#define FASL_MAGIC  "#@IK0"
#define FASL_END    "@"

//
//  Reading FASL
//

#define read_fasl_type(ip)       ((minim_byte) fgetc(ip))
#define read_fasl_byte(ip)       ((minim_byte) fgetc(ip))

// Unserializes a word that is encoded using a variable-length
// block encoding 7-bits of the word per byte
static minim_uptr read_fasl_uptr(FILE *in) {
    minim_uptr u = 0;
    minim_byte b, c;
    int offset = 0;

    do {
        // read and decompose the block
        b = read_fasl_byte(in);
        c = b & 0x1;
        b >>= 1;

        // shift it into the right place
        u |= (((minim_uptr) b) << offset);
        offset += 7;
    } while (c);

    return u;
}


static minim_object *read_fasl_symbol(FILE *in) {
    char *str;
    long len, i;
    
    len = read_fasl_uptr(in);
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (i = 0; i < len; ++i)
        str[i] = read_fasl_byte(in);
    str[len] = '\0';

    return intern(str);
}

static minim_object *read_fasl_string(FILE *in) {
    char *str;
    long len, i;
    
    len = read_fasl_uptr(in);
    str = GC_alloc_atomic((len + 1) * sizeof(char));
    for (i = 0; i < len; ++i)
        str[i] = read_fasl_byte(in);
    str[len] = '\0';

    return make_string2(str);
}

static minim_object *read_fasl_fixnum(FILE *in) {
    return make_fixnum(read_fasl_uptr(in));
}

static minim_object *read_fasl_char(FILE *in) {
    return make_char(read_fasl_byte(in));
}

static minim_object *read_fasl_pair(FILE *in) {
    minim_object *head, *it;
    long i, len;

    // improper list has at least one element
    // set the head and hold on to it
    len = read_fasl_uptr(in);
    head = make_pair(read_fasl(in), minim_null);
    it = head;

    // iteratively read the rest of the proper elements
    for (i = 1; i < len; i++) {
        minim_cdr(it) = make_pair(read_fasl(in), minim_cdr(it));
        it = minim_cdr(it);
    }

    // read the tail
    minim_cdr(it) = read_fasl(in);

    return head;
}

static minim_object* read_fasl_vector(FILE *in) {
    minim_object *vec;
    long i, len;

    len = read_fasl_uptr(in);
    vec = make_vector(len, NULL);
    for (i = 0; i < len; ++i)
        minim_vector_ref(vec, i) = read_fasl(in);

    return vec;
}

static minim_object* read_fasl_hashtable(FILE *in) {
    minim_object *ht, *h_fn, *e_fn, *k, *v;
    long size, i;

    size = read_fasl_uptr(in);
    ht = make_hashtable(equal_hash_proc_obj, equal_proc_obj);
    for (i = 0; i < size; i++) {
        k = read_fasl(in);
        v = read_fasl(in);
        hashtable_set(ht, k, v);
    }

    return ht;
}

static minim_object* read_fasl_record(FILE *in) {
    minim_object *rec, *rtd;
    long len, i;

    len = read_fasl_uptr(in);
    rtd = read_fasl(in);
    rec = make_record(rtd, len);
    for (i = 0; i < len; i++)
        minim_record_ref(rec, i) = read_fasl(in);

    return rec;
}

minim_object *read_fasl(FILE *in) {
    fasl_type type;
    
    type = read_fasl_type(in);
    switch (type)
    {
    case FASL_NULL:
        return minim_null;
    case FASL_TRUE:
        return minim_true;
    case FASL_FALSE:
        return minim_false;
    case FASL_EOF:
        return minim_eof;
    case FASL_VOID:
        return minim_void;
    case FASL_SYMBOL:
        return read_fasl_symbol(in);
    case FASL_STRING:
        return read_fasl_string(in);
    case FASL_FIXNUM:
        return read_fasl_fixnum(in);
    case FASL_CHAR:
        return read_fasl_char(in);
    case FASL_PAIR:
        return read_fasl_pair(in);
    case FASL_EMPTY_VECTOR:
        return minim_empty_vec;
    case FASL_VECTOR:
        return read_fasl_vector(in);
    case FASL_HASHTABLE:
        return read_fasl_hashtable(in);
    case FASL_BASE_RTD:
        return minim_base_rtd;
    case FASL_RECORD:
        return read_fasl_record(in);
    
    default:
        fprintf(stderr, "read_fasl: malformed FASL data\n");
        exit(1);
        break;
    }
}

//
//  Writing FASL
//

#define write_fasl_type(o, t)       (fputc(((minim_byte) (t)), (o)))
#define write_fasl_byte(o, b)       (fputc(((minim_byte) (b)), (o)))

// Serializes a word using variable-length block
// encoding 7-bits of the word per byte
static void write_fasl_uptr(FILE *out, minim_uptr u) {
    minim_byte b;

    do {
        // extract 7 lowest bits
        b = u & 0x7F;
        u >>= 7;

        // prepare ubyte
        b <<= 1;
        b |= (u != 0) ? 1 : 0;

        // write
        write_fasl_byte(out, b);
    } while (u != 0);
}

// Serializes a symbol
static void write_fasl_symbol(FILE *out, minim_object *sym) {
    write_fasl_uptr(out, strlen(minim_symbol(sym)));
    for (char *s = minim_symbol(sym); *s; s++)
        write_fasl_byte(out, *s);
}

// Serializes a string
static void write_fasl_string(FILE *out, minim_object *str) {
    write_fasl_uptr(out, strlen(minim_string(str)));
    for (char *s = minim_string(str); *s; s++)
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

// Serializes a hashtable
// WARN: internal structure of hashtable may be different
// TODO: equivalence procedure
static void write_fasl_hashtable(FILE *out, minim_object *ht) {
    minim_object *b;
    size_t i;

    write_fasl_uptr(out, minim_hashtable_size(ht));
    for (i = 0; i < minim_hashtable_alloc(ht); ++i) {
        b = minim_hashtable_bucket(ht, i);
        if (b) {
            for (; !minim_is_null(b); b = minim_cdr(b)) {
                write_fasl(out, minim_caar(b));
                write_fasl(out, minim_cdar(b));
            }
        }
    }
}

// Serializes a record
static void write_fasl_record(FILE *out, minim_object *r) {
    int i;

    write_fasl_uptr(out, minim_record_count(r));
    write_fasl(out, minim_record_rtd(r));
    for (i = 0; i < minim_record_count(r); ++i)
        write_fasl(out, minim_record_ref(r, i));
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
        if (minim_is_empty_vec(o)) {
            write_fasl_type(out, FASL_EMPTY_VECTOR);
        } else {
            write_fasl_type(out, FASL_VECTOR);
            write_fasl_vector(out, o);
        }
    } else if (minim_is_hashtable(o)) {
        write_fasl_type(out, FASL_HASHTABLE);
        write_fasl_hashtable(out, o);
    } else if (minim_is_base_rtd(o)) {
        write_fasl_type(out, FASL_BASE_RTD);
    } else if (minim_is_record(o)) {
        write_fasl_type(out, FASL_RECORD);
        write_fasl_record(out, o);
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
    // (-> any)
    // (-> input-port any)
    minim_object *in_p;

    in_p = args[0];
    if (!minim_is_input_port(in_p))
        bad_type_exn("read-fasl", "input-port?", in_p);
    
    return read_fasl(minim_port(in_p));
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
