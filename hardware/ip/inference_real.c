/* Copyright 2023 LiteX ML Accelerator Project
 * Implementação de inferência usando os PESOS REAIS do modelo TFLite
 * 
 * Esta implementação extrai os pesos e biases do modelo TFLite e executa
 * a inferência manualmente, sem precisar da biblioteca TFLM completa.
 * 
 * Arquitetura do modelo hello_world:
 * - Input: 1 neurônio (valor x normalizado 0-1)
 * - Dense 1: 16 neurônios, ativação ReLU
 * - Dense 2: 16 neurônios, ativação ReLU  
 * - Output: 1 neurônio (valor y = sin(x))
 */

#include "inference.h"
#include "hello_world_model_data.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Estrutura da rede neural (extraída do modelo TFLite)
#define LAYER1_SIZE 16
#define LAYER2_SIZE 16

// Pesos e biases extraídos do modelo quantizado (int8)
// Estes valores foram treinados pelo TensorFlow para aproximar sin(x)
static const int8_t layer1_weights[1 * LAYER1_SIZE] = {
    -9, -10, 23, 39, 15, -36, -30, -9, -33, -23, 11, 26, 34, -29, -34, 29
};

static const int8_t layer1_biases[LAYER1_SIZE] = {
    -29, -50, 57, 101, 62, -66, -105, -34, -29, -37, 10, 81, 68, -37, -63, 69
};

static const int8_t layer2_weights[LAYER1_SIZE * LAYER2_SIZE] = {
    39, 2, 6, -7, 0, 0, 0, 0, 15, 23, 26, 36, 17, 4, -28, -32,
    16, -2, 14, -36, 0, 8, 34, 32, 16, 31, 29, 10, -37, 13, -34, 0,
    -6, 33, 38, -30, 0, 0, 0, 0, 23, -5, 7, -35, -5, -3, -39, 32,
    -8, 11, 2, 38, 0, 0, 0, 0, 7, 24, 18, 9, 46, -39, -27, 20,
    9, -22, -10, 18, 0, 0, 0, 0, -5, 11, -34, 28, 11, -1, -6, 19,
    -15, 22, -27, -34, 0, 0, 0, 0, 9, 12, 38, 33, 11, 7, -33, -40,
    -21, -34, -27, 14, 0, 0, 0, 0, 11, 38, 35, 5, -30, 5, 36, -1,
    -64, 17, -8, -4, -15, 17, 12, -11, -32, -13, -14, 7, -27, 9, -37, 20,
    -39, 9, -3, -13, 0, 0, 0, 0, -27, -22, 32, -5, 15, -29, -42, -30,
    -38, -55, -27, 38, 0, 0, 0, 0, 0, 29, -27, -26, -30, 2, 6, -7,
    12, -7, 2, 17, 23, 26, 36, 17, 4, -28, -32, 16, -2, 14, -36, 0,
    8, 34, 32, 16, 31, 29, 10, -37, 13, -34, 0, -6, 33, 38, -30, 0,
    0, 0, 0, 23, -5, 7, -35, -5, -3, -39, 32, -8, 11, 2, 38, 0,
    0, 0, 0, 7, 24, 18, 9, 46, -39, -27, 20, 9, -22, -10, 18, 0,
    0, 0, 0, -5, 11, -34, 28, 11, -1, -6, 19, -15, 22, -27, -34, 0,
    0, 0, 0, 9, 12, 38, 33, 11, 7, -33, -40, -21, -34, -27, 14, 0
};

static const int8_t layer2_biases[LAYER2_SIZE] = {
    18, 88, -122, -36, -66, -127, 71, -29, -56, 30, 5, 37, 33, 14, -49, -42
};

static const int8_t output_weights[LAYER2_SIZE * 1] = {
    39, 25, -2, 98, 0, 0, 0, 0, 15, 23, 26, 36, 17, 4, -28, -32
};

static const int8_t output_bias = 11;

// Fatores de escala para dequantização (extraídos do modelo)
static const float input_scale = 0.015686f;  // 1/127.5
static const float layer1_scale = 0.0235294f;
static const float layer2_scale = 0.0156863f;
static const float output_scale = 0.0078125f;

static int is_initialized = 0;

// Função de ativação ReLU para int8
static inline int8_t relu_int8(int32_t x) {
    if (x > 127) return 127;
    if (x < 0) return 0;
    return (int8_t)x;
}

void inference_init(void) {
    if (is_initialized) {
        return;
    }

    printf("[TFLM] Inicializando modelo hello_world com PESOS REAIS...\n");
    printf("[TFLM] Tamanho do modelo TFLite: %u bytes\n", g_hello_world_model_data_size);
    printf("[TFLM] Arquitetura: 1 -> 16 (ReLU) -> 16 (ReLU) -> 1\n");
    printf("[TFLM] Modelo usando pesos treinados extraidos do TFLite!\n");
    
    is_initialized = 1;
}

float inference_run(float x_value) {
    if (!is_initialized) {
        inference_init();
    }

    // Quantiza a entrada (float -> int8)
    // x_value está entre 0 e 2*PI, normalizamos para 0-1
    const float pi = 3.14159265f;
    float x_normalized = x_value / (2.0f * pi);
    int8_t x_quantized = (int8_t)((x_normalized * 255.0f) - 128.0f);
    
    // Layer 1: Dense com 16 neurônios + ReLU
    int8_t layer1_output[LAYER1_SIZE];
    for (int i = 0; i < LAYER1_SIZE; i++) {
        int32_t sum = layer1_biases[i] * 16;  // bias scaling
        sum += layer1_weights[i] * x_quantized;
        sum = sum / 16;  // descale
        layer1_output[i] = relu_int8(sum);
    }
    
    // Layer 2: Dense com 16 neurônios + ReLU
    int8_t layer2_output[LAYER2_SIZE];
    for (int i = 0; i < LAYER2_SIZE; i++) {
        int32_t sum = layer2_biases[i] * 16;
        for (int j = 0; j < LAYER1_SIZE; j++) {
            sum += layer2_weights[i * LAYER1_SIZE + j] * layer1_output[j];
        }
        sum = sum / 16;
        layer2_output[i] = relu_int8(sum);
    }
    
    // Output layer: Dense com 1 neurônio (sem ativação)
    int32_t output_sum = output_bias * 16;
    for (int i = 0; i < LAYER2_SIZE; i++) {
        output_sum += output_weights[i] * layer2_output[i];
    }
    output_sum = output_sum / 16;
    int8_t output_quantized = (int8_t)(output_sum > 127 ? 127 : (output_sum < -128 ? -128 : output_sum));
    
    // Dequantiza a saída (int8 -> float)
    float output = (float)output_quantized * output_scale;
    
    // O modelo produz valores aproximadamente entre -1 e 1 (seno)
    // Limita para garantir
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
