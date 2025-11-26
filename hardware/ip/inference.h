/* Copyright 2023 LiteX ML Accelerator Project
 * Implementação da inferência TensorFlow Lite Micro para hello_world
 */

#ifndef INFERENCE_H_
#define INFERENCE_H_

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa o modelo TFLite Micro
void inference_init(void);

// Executa uma inferência com o valor de entrada x (radianos)
// Retorna o valor de saída (seno de x), normalizado entre 0 e 1
float inference_run(float x_value);

// Converte valor de saída do modelo (0.0 a 1.0) para padrão de LEDs (0-255)
unsigned char inference_output_to_led_pattern(float output_value);

#ifdef __cplusplus
}
#endif

#endif  // INFERENCE_H_
