// thread.c: threads

#include "../minim.h"

mobj Mthread_context() {
    mobj tc, env;
    
    tc = GC_alloc(tc_size);
    env = make_base_env();

    tc_ac(tc) = 0;
    tc_cp(tc) = NULL;
    tc_sfp(tc) = NULL;
    tc_esp(tc) = NULL;
    tc_ccont(tc) = minim_null;
    tc_env(tc) = NULL;
    tc_vc(tc) = 0;
    tc_values(tc) = NULL;
    tc_stack_base(tc) = NULL;
    tc_stack_size(tc) = 0;
    tc_stack_link(tc) = minim_null;
    tc_reentry(tc) = GC_alloc(sizeof(jmp_buf));
    tc_input_port(tc) = Minput_port(stdin);
    tc_output_port(tc) = Moutput_port(stdout);
    tc_error_port(tc) = Moutput_port(stderr);
    tc_directory(tc) = Mstring(get_current_dir());
    tc_command_line(tc) = minim_null;
    tc_record_equal(tc) = minim_false;
    tc_record_hash(tc) = minim_false;
    tc_error_handler(tc) = minim_false;
    tc_c_error_handler(tc) = minim_false;
    tc_tenv(tc) = env;
    return tc;
}
