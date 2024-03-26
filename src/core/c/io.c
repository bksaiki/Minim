/*
    Basic I/O
*/

#include "../minim.h"

//
//  Objects
//

static void gc_port_dtor(void *ptr, void *data) {
    mobj o = ((mobj) ptr);
    if (minim_port_openp(o) &&
        minim_port(o) != stdout &&
        minim_port(o) != stderr &&
        minim_port(o) != stdin)
        fclose(minim_port(o));
}

mobj Minput_port(FILE *stream) {
    mobj o = GC_alloc(minim_port_size);
    minim_type(o) = MINIM_OBJ_PORT;
    minim_port_flags(o) = PORT_FLAG_READ;
    minim_port(o) = stream;
    GC_register_dtor(o, gc_port_dtor);
    return o;
}

mobj Moutput_port(FILE *stream) {
    mobj o = GC_alloc(minim_port_size);
    minim_type(o) = MINIM_OBJ_PORT;
    minim_port_flags(o) = 0x0;
    minim_port(o) = stream;
    GC_register_dtor(o, gc_port_dtor);
    return o;
}

//
//  Primitives
//

mobj input_portp_proc(mobj x) {
    // (-> any/c boolean)
    return minim_input_portp(x) ? minim_true : minim_false;
}

mobj output_portp_proc(mobj x) {
    // (-> any/c boolean)
    return minim_output_portp(x) ? minim_true : minim_false;
}

mobj string_portp_proc(mobj x) {
    // (-> any/c boolean)
    return minim_string_portp(x) ? minim_true : minim_false;
}

mobj current_input_port() {
    // (-> input-port)
    return tc_input_port(current_tc());
}

mobj current_output_port() {
    // (-> output-port)
    return tc_output_port(current_tc());
}

mobj current_error_port() {
    // (-> output-port)
    return tc_error_port(current_tc());
}

mobj open_input_file(mobj name) {
    // (-> str input-port)
    FILE *stream;
    mobj port;

    stream = fopen(minim_string(name), "r");
    if (stream == NULL) {
        minim_error1("open-input-file", "cannot open input file", name);
    }

    port = Minput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN | PORT_FLAG_READ);
    return port;
}

mobj open_output_file(mobj name) {
    // (-> str output-port)
    FILE *stream;
    mobj port;

    stream = fopen(minim_string(name), "w");
    if (stream == NULL) {
        minim_error1("open-output-file", "cannot open output file", name);
    }

    port = Moutput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN);
    return port;
}

mobj open_input_string(mobj str) {
    // (-> str input-port)
    FILE *stream;
    mobj port;

    // port backed by a temporary file
    stream = tmpfile();
    if (stream == NULL) {
        minim_error("open-input-string", "cannot create buffer");
    }

    fputs(minim_string(str), stream);
    fseek(stream, 0, SEEK_SET);

    port = Minput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN | PORT_FLAG_STR);
    return port;
}

mobj open_output_string() {
    // (-> str output-port)
    FILE *stream;
    mobj port;

    // port backed by a temporary file
    stream = tmpfile();
    if (stream == NULL) {
        minim_error("open-output-string", "cannot create buffer");
    }

    port = Moutput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN | PORT_FLAG_STR);
    return port;
}

mobj close_input_port(mobj port) {
    // (-> input-port? void)
    fclose(minim_port(port));
    minim_port_unset(port, PORT_FLAG_OPEN);
    return minim_void;
}

mobj close_output_port(mobj port) {
    // (-> output-port? void)
    fclose(minim_port(port));
    minim_port_unset(port, PORT_FLAG_OPEN);
    return minim_void;
}

mobj get_output_string(mobj port) {
    // (-> (and output-port? string-port?) string?)
    mobj str;
    size_t len, read;
    
    // read from file
    len = ftell(minim_port(port));
    if (len != 0) {
        str = Mstring2(len, 0);
        fseek(minim_port(port), 0, SEEK_SET);
        read = fread(minim_string(str), len, 1, minim_port(port));
        if (read != 1) {
            minim_error("get-output-string", "failed to read from");
        }

        // reset port, set null-terminator and return
        rewind(minim_port(port));
        minim_string(str)[len] = '\0';
        return str;
    } else {
        return Mstring("");
    }
}

mobj read_proc(mobj port) {
    // (-> input-port? any/c)
    return read_object(minim_port(port));
}

mobj read_char_proc(mobj port) {
    // (-> input-port? char?)
    mchar ch = getc(minim_port(port));
    return (ch == EOF) ? minim_eof : Mchar(ch);
}

mobj peek_char_proc(mobj port) {
    // (-> input-port? char?)
    mchar ch = getc(minim_port(port));
    ungetc(ch, minim_port(port));
    return (ch == EOF) ? minim_eof : Mchar(ch);
}

mobj char_readyp_proc(mobj port) {
    // (-> input-port? char?)
    mchar ch = getc(minim_port(port));
    ungetc(ch, minim_port(port));
    return (ch == EOF) ? minim_false : minim_true;
}

mobj put_char_proc(mobj port, mobj ch) {
    // (-> output-port? char? void)
    putc(minim_char(ch), minim_port(port));
    return minim_void;
}

mobj flush_output_proc(mobj port) {
    // (-> output-port? void)
    fflush(minim_port(port));
    return minim_void;
}

mobj put_string_proc(mobj port, mobj str, mobj start, mobj end) {
    // (-> output-port? string? integer? integer?)
    for (long i = minim_fixnum(start); i < minim_fixnum(end); i++)
        putc(minim_string_ref(str, i), minim_port(port));
    return minim_void;
}

mobj newline_proc(mobj port) {
    // (-> output-port? void)
    putc('\n', minim_port(port));
    return minim_void;
}
