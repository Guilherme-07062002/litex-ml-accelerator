// Declarações da função de inferência
#ifndef INFERENCE_H
#define INFERENCE_H

#include <stdint.h>

// Inicializa o modelo de inferência
void inference_init(void);

// Executa inferência com entrada x (0 a 2*PI)
// Retorna valor aproximado de sin(x) entre -1 e 1
float inference_run(float x);

// Converte a saída do modelo para padrão de LED (0-255)
unsigned char inference_output_to_led_pattern(float output_value);

#endif // INFERENCE_H
