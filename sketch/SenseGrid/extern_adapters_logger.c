#include "sg_adapters_logger.h"

// Ponte C -> C++: só chama o init do logger
void extern_adapters_logger_bootstrap(void) {
    sg_adapters_logger_init();
}
