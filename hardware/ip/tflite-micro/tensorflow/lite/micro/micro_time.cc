// Port do TensorFlow Lite Micro para VexRiscv/LiteX
// Implementação de micro_time para timing

#include "tensorflow/lite/micro/micro_time.h"

#include <stdint.h>
#include <generated/csr.h>

#ifdef CSR_TIMER0_BASE
extern "C" {
#include <system.h>
}
#endif

namespace tflite {

int32_t ticks_per_second() {
#ifdef CONFIG_CLOCK_FREQUENCY
  return CONFIG_CLOCK_FREQUENCY / 1000000; // Retorna ticks por microssegundo
#else
  return 1000000; // Default: 1 MHz
#endif
}

int32_t GetCurrentTimeTicks() {
#ifdef CSR_TIMER0_BASE
  timer0_update_value_write(1);
  return timer0_value_read();
#else
  // Fallback: retorna 0 se não há timer
  return 0;
#endif
}

}  // namespace tflite
