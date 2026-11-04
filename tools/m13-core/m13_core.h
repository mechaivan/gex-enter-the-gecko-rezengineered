/* M-13 core - compatibility shim.
 *
 * Since I-3 the single implementation lives in the port:
 *   port/include/gex/core/loop_core.h
 *   port/src/core/loop_core.c
 *
 * This header keeps the prototype's names so tools/m13-core/test_m13_core.c
 * (the 120 checks written on 2026-10-10) still compiles and still tests the
 * real code instead of a copy. New code should include gex/core/loop_core.h
 * and use the gex_loop_core_ names.
 */
#ifndef M13_CORE_H
#define M13_CORE_H

#include "gex/core/loop_core.h"

typedef gex_loop_config m13_config_t;
typedef gex_loop_core m13_state_t;

#define m13_init gex_loop_core_init
#define m13_iterate gex_loop_core_iterate

#endif /* M13_CORE_H */
