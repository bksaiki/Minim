/**
 * @file
 * Evaluation of code already expanded into core forms.
 */

#ifndef MINIM_EVAL_H
#define MINIM_EVAL_H

#include "minim.h"

/** Value of the top-level form `expr`, in core forms: minim_eval without `%expand`. */
minim_obj minim_eval_expanded(minim_obj expr);

#endif
