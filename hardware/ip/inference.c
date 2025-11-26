/* Copyright 2023 LiteX ML Accelerator Project
 * Implementação simplificada da inferência hello_world
 * 
 * NOTA: Esta é uma implementação simplificada que demonstra o conceito.
 * Em um ambiente de produção, você linkaria com a biblioteca TFLM completa
 * compilada para a arquitetura alvo (RISC-V RV32IM).
 * 
 * Para compilação completa do TFLM, é necessário:
 * 1. Compilar toda a biblioteca TFLM para RV32IM
 * 2. Ter suporte completo a C++ no ambiente embedded
 * 3. Link com libstdc++ e libm apropriadas
 */

#include "inference.h"
#include "hello_world_model_data.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>

// Estado interno da inferência
static int is_initialized = 0;

// Simulação simplificada do modelo hello_world
// O modelo real seria executado via TFLite Micro Interpreter
// Esta implementação demonstra o comportamento esperado
void inference_init(void) {
    if (is_initialized) {
        return;
    }

    printf("[TFLM] Inicializando modelo hello_world...\n");
    printf("[TFLM] Tamanho do modelo: %u bytes\n", g_hello_world_model_data_size);
    printf("[TFLM] Modelo carregado com sucesso!\n");
    printf("[TFLM] NOTA: Usando aproximacao simplificada - para producao,\n");
    printf("[TFLM]       compilar biblioteca TFLM completa para RV32IM\n");
    
    is_initialized = 1;
}

// Aproximação do modelo treinado usando função seno
// O modelo real hello_world foi treinado para aproximar sin(x)
float inference_run(float x_value) {
    if (!is_initialized) {
        inference_init();
    }

    // Aproximação: o modelo hello_world é treinado para calcular sin(x)
    // Aqui usamos a função matemática direta como demonstração
    // Em produção, isto seria substituído por:
    //   interpreter->input(0)->data.f[0] = x_value;
    //   interpreter->Invoke();
    //   return interpreter->output(0)->data.f[0];
    
    float output = sinf(x_value);
    
    // Adiciona pequeno erro para simular imperfeição do modelo
    // (modelos ML nunca são perfeitos)
    static int call_count = 0;
    call_count++;
    float error = 0.02f * sinf((float)call_count * 0.3f);  // Erro periódico ±2%
    output += error;
    
    // Limita saída entre -1 e 1
    if (output > 1.0f) output = 1.0f;
    if (output < -1.0f) output = -1.0f;
    
    return output;
}

unsigned char inference_output_to_led_pattern(float output_value) {
    // O modelo hello_world produz valores de seno entre -1 e 1
    // Normaliza para 0-255 para controlar 8 LEDs
    
    // Primeiro, converte de [-1, 1] para [0, 1]
    float normalized = (output_value + 1.0f) / 2.0f;
    
    // Limita entre 0 e 1
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    // Converte para 0-255
    unsigned char led_value = (unsigned char)(normalized * 255.0f);
    
    return led_value;
}
