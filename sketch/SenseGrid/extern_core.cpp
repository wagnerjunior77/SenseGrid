#include "sg_core.h"

// Ponte C -> C++: apenas inicializa o core (usando defaults/NVS)
extern "C" void extern_core_bootstrap(void) {
    sg_core_init(nullptr);
}
