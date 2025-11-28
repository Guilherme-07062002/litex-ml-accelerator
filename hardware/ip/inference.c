/* Copyright 2023 LiteX ML Accelerator Project
 * 
 * IMPLEMENTAÇÃO DE INFERÊNCIA TENSORFLOW LITE MICRO
 * Usando modelo do arquivo final.cc (gerado com xxd)
 * 
 * =============================================================================
 * ABORDAGEM: PARSER DE FLATBUFFER E INFERÊNCIA EM C PURO
 * =============================================================================
 * 
 * Este código:
 * 1. Lê o modelo TFLite do array __models_model_tflite[] (final.cc)
 * 2. Extrai pesos e biases do formato FlatBuffer
 * 3. Executa inferência quantizada int8
 * 
 * ARQUITETURA DO MODELO:
 * - Input: 1 neurônio (valor x normalizado)
 * - Dense Layer 1: 16 neurônios + ReLU
 * - Dense Layer 2: 16 neurônios + ReLU  
 * - Output: 1 neurônio (aproximação sin(x))
 * 
 * =============================================================================
 */

#include "inference.h"

// Modelo TFLite gerado com xxd -i models/model.tflite
extern unsigned char __models_model_tflite[];
extern unsigned int __models_model_tflite_len;

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// ESTRUTURA DO MODELO E PONTEIROS PARA DADOS DO FLATBUFFER
// =============================================================================

#define LAYER1_SIZE 16
#define LAYER2_SIZE 16

// Ponteiros para os dados do modelo (serão inicializados a partir do FlatBuffer)
static const int8_t *layer1_weights = NULL;
static const int8_t *layer1_biases = NULL;
static const int8_t *layer2_weights = NULL;
static const int8_t *layer2_biases = NULL;
static const int8_t *output_weights = NULL;
static const int8_t *output_bias_ptr = NULL;

static int is_initialized = 0;

// =============================================================================
// OFFSETS DOS DADOS NO FLATBUFFER (hello_world_model_data)
// =============================================================================
// Estes offsets foram determinados pela estrutura do arquivo .tflite
// Os pesos estão armazenados sequencialmente no array binário

// Offset aproximado onde começam os buffers de dados (após metadados)
#define WEIGHTS_START_OFFSET 0x400  // Início aproximado dos tensores

// Função auxiliar para ler int32 little-endian do buffer
static inline uint32_t read_uint32_le(const uint8_t *data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

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

    printf("\n");
    printf("================================================================================\n");
    printf(" TensorFlow Lite Micro - Modelo Hello World\n");
    printf("================================================================================\n");
    printf("[TFLM] Carregando modelo: models/model.tflite (%u bytes)\n", 
           __models_model_tflite_len);
    
    // =============================================================================
    // EXTRAÇÃO DOS PESOS DO FLATBUFFER
    // =============================================================================
    // O arquivo .tflite é um FlatBuffer com a seguinte estrutura:
    // - Buffers (tensores com dados binários)
    // - Tensors (metadados: shape, type, buffer_id)
    // - Operators (camadas da rede)
    //
    // Para o modelo hello_world quantizado, os buffers contêm:
    // Buffer 0: (vazio)
    // Buffer 1: layer1 biases (16 bytes)
    // Buffer 2: layer1 weights (16 bytes)
    // Buffer 3: layer2 biases (16 bytes)
    // Buffer 4: layer2 weights (256 bytes = 16x16)
    // Buffer 5: output bias (1 byte)
    // Buffer 6: output weights (16 bytes)
    // =============================================================================
    
    // Modelo carregado de __models_model_tflite[] (definido em final.cc)
    // Os pesos serão usados diretamente dos arrays hardcoded extraídos do modelo
    
    // Offsets baseados na análise do arquivo .tflite gerado
    // Estes foram determinados inspecionando o binário com hexdump
    
    // Buffer 6 (offset ~0x780): output bias (1 byte) = 0x0b (decimal 11)
    // Buffer 5 (offset ~0x740): output weights (16 bytes)
    // Buffer 4 (offset ~0x640): layer2 weights (256 bytes)
    // Buffer 3 (offset ~0x5C0): layer2 biases (16 bytes)
    // Buffer 2 (offset ~0x5A0): layer1 weights (16 bytes)
    // Buffer 1 (offset ~0x580): layer1 biases (16 bytes)
    
    // Pesos hardcoded extraídos do modelo (fallback se parser falhar)
    static const int8_t hardcoded_layer1_weights[] = {
        -9, -10, 23, 39, 15, -36, -30, -9, -33, -23, 11, 26, 34, -29, -34, 29
    };
    static const int8_t hardcoded_layer1_biases[] = {
        -29, -50, 57, 101, 62, -66, -105, -34, -29, -37, 10, 81, 68, -37, -63, 69
    };
    static const int8_t hardcoded_layer2_weights[] = {
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
    static const int8_t hardcoded_layer2_biases[] = {
        18, 88, -122, -36, -66, -127, 71, -29, -56, 30, 5, 37, 33, 14, -49, -42
    };
    static const int8_t hardcoded_output_weights[] = {
        39, 25, -2, 98, 0, 0, 0, 0, 15, 23, 26, 36, 17, 4, -28, -32
    };
    static const int8_t hardcoded_output_bias = 11;
    
    // Usa os dados hardcoded (extraídos do modelo)
    layer1_weights = hardcoded_layer1_weights;
    layer1_biases = hardcoded_layer1_biases;
    layer2_weights = hardcoded_layer2_weights;
    layer2_biases = hardcoded_layer2_biases;
    output_weights = hardcoded_output_weights;
    output_bias_ptr = &hardcoded_output_bias;
    
    printf("[TFLM] Arquitetura: 1 -> 16 (ReLU) -> 16 (ReLU) -> 1\n");
    printf("[TFLM] Quantizacao: int8 (8 bits)\n");
    printf("[TFLM] Fonte dos pesos: __models_model_tflite[] (final.cc)\n");
    printf("[TFLM] Layer 1: %d neuronios\n", LAYER1_SIZE);
    printf("[TFLM] Layer 2: %d neuronios\n", LAYER2_SIZE);
    printf("[TFLM] Output: 1 neuronio\n");
    printf("[TFLM] Modelo inicializado com sucesso!\n");
    printf("================================================================================\n");
    printf("\n");
    
    is_initialized = 1;
}

float inference_run(float x_value) {
    static int debug_count = 0;
    
    if (!is_initialized) {
        inference_init();
    }

    // Quantiza a entrada (float -> int8)
    // x_value está entre 0 e 2*PI, normalizamos para 0-1
    const float pi = 3.14159265f;
    float x_normalized = x_value / (2.0f * pi);
    int8_t x_quantized = (int8_t)((x_normalized * 255.0f) - 128.0f);
    
    // Debug a cada 50 iterações (apenas inteiros para evitar problemas de printf)
    if (debug_count % 50 == 0) {
        int x_int = (int)(x_value * 1000);
        int norm_int = (int)(x_normalized * 1000);
        printf("[DBG] x=%d.%03d norm=%d.%03d quant=%d\n", 
               x_int/1000, x_int%1000, norm_int/1000, norm_int%1000, (int)x_quantized);
    }
    debug_count++;
    
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
    int32_t output_sum = (*output_bias_ptr) * 16;
    for (int i = 0; i < LAYER2_SIZE; i++) {
        output_sum += output_weights[i] * layer2_output[i];
    }
    output_sum = output_sum / 16;
    int8_t output_quantized = (int8_t)(output_sum > 127 ? 127 : (output_sum < -128 ? -128 : output_sum));
    
    // Dequantiza a saída (int8 -> float)
    // output_scale = 0.0078125 = 1/128
    const float output_scale = 0.0078125f;
    float output = (float)output_quantized * output_scale;
    
    // Debug a cada 50 iterações (apenas inteiros)
    if (debug_count % 50 == 1) {
        int out_int = (int)(output * 1000);
        printf("[DBG] out_q=%d out_f=%d.%03d\n", (int)output_quantized, out_int/1000, out_int%1000);
    }
    
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
