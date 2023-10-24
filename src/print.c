// print.c: minimal Scheme printer

#include "minim.h"

#define op_puts(op, s)      fputs(s, minim_port(op))
#define op_putc(op, c)      fputc(c, minim_port(op))

static void write_char(mobj op, mobj o) {
    mchar c = minim_char(o);

    op_puts(op, "#\\");
    if (c == '\0') op_puts(op, "nul");
    else if (c == BEL_CHAR) op_puts(op, "alarm");
    else if (c == BS_CHAR) op_puts(op, "backspace");
    else if (c == '\t') op_puts(op, "tab");
    else if (c == '\n') op_puts(op, "newline");
    else if (c == VT_CHAR) op_puts(op, "vtab");
    else if (c == FF_CHAR) op_puts(op, "page");
    else if (c == CR_CHAR) op_puts(op, "return");
    else if (c == ESC_CHAR) op_puts(op, "esc");
    else if (c == ' ') op_puts(op, "space");
    else if (c == DEL_CHAR) op_puts(op, "delete");
    else op_putc(op, c);
}

static void write_fixnum(mobj op, mobj o) {
    fprintf(minim_port(op), "%ld", minim_fixnum(o));
}

static void write_symbol(mobj op, mobj o) {
    for (mchar *s = minim_symbol(o); *s; s++) {
        op_putc(op, *s);
    }
}

static void write_string(mobj op, mobj o) {
    op_putc(op, '"');
    for (mchar *s = minim_string(o); *s; s++) {
        if (*s == BEL_CHAR) {
            op_puts(op, "\\a");
        } else if (*s == BS_CHAR) {
            op_puts(op, "\\b");
        } else if (*s == '\t') {
            op_puts(op, "\\t");
        } else if (*s == '\n') {
            op_puts(op, "\\n");
        } else if (*s == VT_CHAR) {
            op_puts(op, "\\v");
        } else if (*s == FF_CHAR) {
            op_puts(op, "\\f");
        } else if (*s == CR_CHAR) {
            op_puts(op, "\\r");
        } else if (*s == '\"') {
            op_puts(op, "\\\"");
        } else if (*s == '\\') {
            op_puts(op, "\\\\");
        } else {
            op_putc(op, *s);
        }
    }
    op_putc(op, '"');
}

static void write_cons(mobj op, mobj o) {
    op_putc(op, '(');

loop:
    write_object(op, minim_car(o));
    if (minim_consp(minim_cdr(o))) {
        // middle of a (proper/improper) list
        op_putc(op, ' ');
        o = minim_cdr(o);
        goto loop;
    } else if (!minim_nullp(minim_cdr(o))) {
        // end of an improper list
        op_puts(op, " . ");
        write_object(op, minim_cdr(o));
    }

    op_putc(op, ')');
}

static void write_vector(mobj op, mobj o) {
    msize len = minim_vector_len(o);
    if (len == 0) {
        op_puts(op, "#()");
    } else {
        op_puts(op, "#(");
        write_object(op, minim_vector_ref(o, 0));
        for (size_t i = 1; i < len; i++) {
            op_putc(op, ' ');
            write_object(op, minim_vector_ref(o, i));
        }
        op_putc(op, ')');
    }
}

static void write_box(mobj op, mobj o) {
    op_puts(op, "#&");
    write_object(op, minim_unbox(o));
}

void write_object(mobj op, mobj o) {
    if (minim_nullp(o)) op_puts(op, "()");
    else if (minim_truep(o)) op_puts(op, "#t");
    else if (minim_falsep(o)) op_puts(op, "#f");
    else if (minim_eofp(o)) op_puts(op, "#<eof>");
    else if (minim_voidp(o)) op_puts(op, "#<void>");
    else if (minim_charp(o)) write_char(op, o);
    else if (minim_fixnump(o)) write_fixnum(op, o);
    else if (minim_symbolp(o)) write_symbol(op, o);
    else if (minim_stringp(o)) write_string(op, o);
    else if (minim_consp(o)) write_cons(op, o);
    else if (minim_vectorp(o)) write_vector(op, o);
    else if (minim_boxp(o)) write_box(op, o);
    else if (minim_portp(o)) op_puts(op, "#<port>");
    else op_puts(op, "#<garbage>");
}
