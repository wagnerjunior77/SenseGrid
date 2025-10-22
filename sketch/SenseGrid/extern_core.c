#include "sg_core.h"

// Ponte C -> C++: só chama o init do core
void extern_core_bootstrap(void) {
    sg_core_init();
}
