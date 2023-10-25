// port.c: ports

#include "minim.h"

mobj open_input_file(const char *f) {
    // TODO: paths
    FILE *stream = fopen(f, "r");
    if (stream == NULL) error1("open_input_file()", "could not open file", Mstring(f));
    return Mport(stream, PORT_FLAG_OPEN | PORT_FLAG_READ);
}

mobj open_output_file(const char *f) {
    // TODO: paths
    FILE *stream = fopen(f, "w");
    if (stream == NULL) error1("open_output_file()", "could not open file", Mstring(f));
    return Mport(stream, PORT_FLAG_OPEN);
}

void close_port(mobj p) {
    if (minim_port_openp(p)) {
        minim_port_unset(p, PORT_FLAG_OPEN);
        fclose(minim_port(p));
    }
}
