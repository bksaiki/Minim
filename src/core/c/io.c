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
//  fprintf
//

void minim_fprintf(FILE *o, const char *form, int v_count, mobj *vs, const char *prim_name) {
    long i;
    int vi, fi;

    // verify arity
    fi = 0;
    for (i = 0; form[i]; ++i) {
        if (form[i] == '~') {
            i++;
            if (!form[i]) {
                fprintf(stderr, "%s: ill-formed formatting escape\n", prim_name);
                fprintf(stderr, " at: ");
                write_object2(stderr, Mstring(form), 1, 0);
                fprintf(stderr, "\n");
                exit(1);
            }

            switch (form[i])
            {
            case 'a':
                // display
                fi++;
                break;
            case 's':
                // write
                fi++;
                break;
            default:
                break;
            }
        }
    }

    if (fi != v_count) {
        fprintf(stderr, "%s: format string requires %d arguments, given %d\n", prim_name, fi, v_count);
        fprintf(stderr, " at: ");
        write_object2(stderr, Mstring(form), 1, 0);
        fprintf(stderr, "\n");
        exit(1);
    }

    // try printing
    vi = 0;
    for (i = 0; form[i]; ++i) {
        if (form[i] == '~') {
            // escape character expected
            i++;
            switch (form[i])
            {
            case '~':
                // tilde
                putc('~', o);
                break;
            case 'a':
                // display
                write_object2(o, vs[vi++], 1, 1);
                break;
            case 's':
                // write
                write_object2(o, vs[vi++], 1, 0);
                break;
            default:
                fprintf(stderr, "%s: unknown formatting escape\n", prim_name);
                fprintf(stderr, " at: %s\n", form);
                exit(1);
                break;
            }
        } else {
            putc(form[i], o);
        }
    }
}

//
//  Primitives
//
mobj input_portp_proc(mobj x) {
    return minim_input_portp(x) ? minim_true : minim_false;
}

mobj output_portp_proc(mobj x) {
    return minim_output_portp(x) ? minim_true : minim_false;
}

mobj current_input_port_proc(int argc, mobj *args) {
    // (-> input-port)
    return input_port(current_thread());
}

mobj current_output_port_proc(int argc, mobj *args) {
    // (-> output-port)
    return output_port(current_thread());
}

mobj open_input_port_proc(int argc, mobj *args) {
    // (-> string input-port)
    FILE *stream;
    mobj port;

    if (!minim_stringp(args[0]))
        bad_type_exn("open-input-port", "string?", args[0]);

    stream = fopen(minim_string(args[0]), "r");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(args[0]));
        minim_shutdown(1);
    }

    port = Minput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN);
    return port;
}

mobj open_output_port_proc(int argc, mobj *args) {
    // (-> string output-port)
    FILE *stream;
    mobj *port;

    if (!minim_stringp(args[0]))
        bad_type_exn("open-output-port", "string?", args[0]);
    
    stream = fopen(minim_string(args[0]), "w");
    if (stream == NULL) {
        fprintf(stderr, "could not open file \"%s\"\n", minim_string(args[0]));
        minim_shutdown(1);
    }

    port = Moutput_port(stream);
    minim_port_set(port, PORT_FLAG_OPEN);
    return port;
}

mobj close_input_port_proc(int argc, mobj *args) {
    // (-> input-port)
    if (!minim_input_portp(args[0]))
        bad_type_exn("close-input-port", "input-port?", args[0]);

    fclose(minim_port(args[0]));
    minim_port_unset(args[0], PORT_FLAG_OPEN);
    return minim_void;
}

mobj close_output_port_proc(int argc, mobj *args) {
    // (-> output-port)
    if (!minim_output_portp(args[0]))
        bad_type_exn("close-output-port", "output-port?", args[0]);

    fclose(minim_port(args[0]));
    minim_port_unset(args[0], PORT_FLAG_OPEN);
    return minim_void;
}

mobj read_proc(int argc, mobj *args) {
    // (-> any)
    // (-> input-port any)
    mobj in_p, o;

    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_input_portp(in_p))
            bad_type_exn("read", "input-port?", in_p);
    }

    o = read_object(minim_port(in_p));
    return (o == NULL) ? minim_eof : o;
}

mobj read_char_proc(int argc, mobj *args) {
    // (-> char)
    // (-> input-port char)
    mobj in_p;
    int ch;
    
    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_input_portp(in_p))
            bad_type_exn("read-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    return (ch == EOF) ? minim_eof : Mchar(ch);
}

mobj peek_char_proc(int argc, mobj *args) {
    // (-> char)
    // (-> input-port char)
    mobj in_p;
    int ch;
    
    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_input_portp(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_eof : Mchar(ch);
}

mobj char_is_ready_proc(int argc, mobj *args) {
    // (-> boolean)
    // (-> input-port boolean)
    mobj in_p;
    int ch;

    if (argc == 0) {
        in_p = input_port(current_thread());
    } else {
        in_p = args[0];
        if (!minim_input_portp(in_p))
            bad_type_exn("peek-char", "input-port?", in_p);
    }

    ch = getc(minim_port(in_p));
    ungetc(ch, minim_port(in_p));
    return (ch == EOF) ? minim_false : minim_true;
}

mobj display_proc(int argc, mobj *args) {
    // (-> any void)
    // (-> any output-port void)
    mobj out_p, o;

    o = args[0];
    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_output_portp(out_p))
            bad_type_exn("display", "output-port?", out_p);
    }

    write_object2(minim_port(out_p), o, 0, 1);
    return minim_void;
}

mobj write_proc(int argc, mobj *args) {
    // (-> any void)
    // (-> any output-port void)
    mobj out_p, o;

    o = args[0];
    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_output_portp(out_p))
            bad_type_exn("write", "output-port?", out_p);
    }

    write_object2(minim_port(out_p), o, 1, 0);
    return minim_void;
}

mobj write_char_proc(int argc, mobj *args) {
    // (-> char void)
    // (-> char output-port void)
    mobj out_p, ch;

    ch = args[0];
    if (!minim_charp(ch))
        bad_type_exn("write-char", "char?", ch);

    if (argc == 1) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[1];
        if (!minim_output_portp(out_p))
            bad_type_exn("write-char", "output-port?", out_p);
    }

    putc(minim_char(ch), minim_port(out_p));
    return minim_void;
}

mobj newline_proc(int argc, mobj *args) {
    // (-> void)
    // (-> output-port void)
    mobj out_p;

    if (argc == 0) {
        out_p = output_port(current_thread());
    } else {
        out_p = args[0];
        if (!minim_output_portp(out_p))
            bad_type_exn("newline", "output-port?", out_p);
    }

    putc('\n', minim_port(out_p));
    return minim_void;
}

mobj fprintf_proc(int argc, mobj *args) {
    // (-> output-port string any ... void)
    if (!minim_output_portp(args[0]))
        bad_type_exn("fprintf", "output-port?", args[0]);
    
    if (!minim_stringp(args[1]))
        bad_type_exn("fprintf", "string?", args[1]);
    
    minim_fprintf(minim_port(args[0]), minim_string(args[1]), argc - 2, &args[2], "fprintf");
    return minim_void;
}

mobj printf_proc(int argc, mobj *args) {
    // (-> string any ... void)
    mobj curr_op;

    if (!minim_stringp(args[0]))
        bad_type_exn("printf", "string?", args[0]);

    curr_op = output_port(current_thread());
    minim_fprintf(minim_port(curr_op), minim_string(args[0]), argc - 1, &args[1], "printf");
    return minim_void;
}
