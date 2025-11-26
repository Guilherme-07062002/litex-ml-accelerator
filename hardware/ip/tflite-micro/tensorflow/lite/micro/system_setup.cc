// Port do TensorFlow Lite Micro para VexRiscv/LiteX
// Implementação de system_setup

#include "tensorflow/lite/micro/system_setup.h"

namespace tflite {

void InitializeTarget() {
  // Inicialização específica da plataforma, se necessário
  // Para o LiteX/VexRiscv, a inicialização é feita no main()
}

}  // namespace tflite
