// init.c: initialization of Minim's runtime

#include "minim.h"

struct _M_globals M_glob;

void init_minim() {
    // initialize globals
    M_glob.null_string = Mstring("");
    M_glob.null_vector = Mvector(0, NULL);
    M_glob.thread = GC_alloc(sizeof(mthread));
    // initialize thread
    M_glob.thread->input_port = Mport(stdin, PORT_FLAG_READ | PORT_FLAG_OPEN);
    M_glob.thread->output_port = Mport(stdin, PORT_FLAG_OPEN);
}
