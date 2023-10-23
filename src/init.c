// init.c: initialization of Minim's runtime

#include "minim.h"

struct _M_globals M_glob;

void minim_init() {
    // initialize special values
    minim_null = GC_alloc_atomic(1);
    minim_true = GC_alloc_atomic(1);
    minim_false = GC_alloc_atomic(1);
    minim_eof = GC_alloc_atomic(1);
    minim_void = GC_alloc_atomic(1);

    minim_type(minim_null) = MINIM_OBJ_NULL;
    minim_type(minim_true) = MINIM_OBJ_TRUE;
    minim_type(minim_false) = MINIM_OBJ_FALSE;
    minim_type(minim_eof) = MINIM_OBJ_EOF;
    minim_type(minim_void) = MINIM_OBJ_VOID;
    
    // initialize globals
    M_glob.null_string = Mstring("");
    M_glob.null_vector = Mvector(0, NULL);
    M_glob.thread = GC_alloc(sizeof(mthread));
    intern_table_init();

    // initialize thread
    M_glob.thread->input_port = Mport(stdin, PORT_FLAG_READ | PORT_FLAG_OPEN);
    M_glob.thread->output_port = Mport(stdin, PORT_FLAG_OPEN);
}
