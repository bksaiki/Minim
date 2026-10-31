// port.c: i/o streams

#include "minim.h"

int port_readyp(obj p) {
    if (Mstring_portp(p)) {
        char *s = Mport_buffer(p);
        return Mport_count(p) >= Mstring_length(s);
    } else {
        int c = getc(Mport_file(p));
        ungetc(c, Mport_file(p));
        return c != EOF;
    } 
}

int port_peek(obj p) {
    if (Mstring_portp(p)) {
        char *s = Mport_buffer(p);
        if (Mport_count(p) >= Mstring_length(s)) {
            return EOF;
        } else {
            return Mstring_ref(Mport_buffer(p), Mport_count(p));
        }
    } else {
        int c = getc(Mport_file(p));
        ungetc(c, Mport_file(p));
        return c;
    }
}

int port_read(obj p) {
    if (Mstring_portp(p)) {
        char *s = Mport_buffer(p);
        if (Mport_count(p) >= Mstring_length(s)) {
            return EOF;
        } else {
            int c = Mstring_ref(Mport_buffer(p), Mport_count(p));
            Mport_count(p) += 1;
            return c;
        }
    } else {
        int c = getc(Mport_file(p));
        if (c != EOF) Mport_count(p) += 1;
        return c;
    }
}

void port_write(int c, obj p) {
    putc(c, Mport_file(p));
}
