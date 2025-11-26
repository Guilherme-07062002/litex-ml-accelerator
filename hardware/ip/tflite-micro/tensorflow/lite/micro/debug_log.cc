// Port do TensorFlow Lite Micro para VexRiscv/LiteX
// Implementação do debug_log para saída via UART

#include "tensorflow/lite/micro/debug_log.h"

#include <stdio.h>

extern "C" void DebugLog(const char* s) {
  printf("%s", s);
}
