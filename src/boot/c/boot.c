/*
    A small interpreter for bootstrapping Minim.
    Supports the most basic operations.
*/

#include "../boot.h"

void minim_boot_init() {
    minim_thread *th;

    // initialize globals
    init_minim();

    // initialize thread

    current_tc() = GC_alloc(sizeof(minim_thread));
    th = current_tc();
    th->pid = 0;    // TODO

    global_env(th) = make_env();
    current_continuation(th) = minim_null;
    current_segment(th) = Mstack_segment(NULL, stack_seg_default_size);
    current_sfp(th) = stack_seg_base(current_segment(th));
    current_cp(th) = NULL;
    current_ac(th) = 0;
    current_reentry(th) = GC_alloc(sizeof(jmp_buf));

    input_port(th) = Minput_port(stdin);
    output_port(th) = Moutput_port(stdout);
    error_port(th) = Moutput_port(stderr);
    current_directory(th) = Mstring(get_current_dir());
    command_line(th) = minim_null;
    record_equal_proc(th) = minim_false;
    record_hash_proc(th) = minim_false;
    error_handler(th) = minim_false;
    c_error_handler(th) = minim_false;

    values_buffer(th) = GC_alloc(INIT_VALUES_BUFFER_LEN * sizeof(mobj));
    values_buffer_size(th) = INIT_VALUES_BUFFER_LEN;
    values_buffer_count(th) = 0;
}
