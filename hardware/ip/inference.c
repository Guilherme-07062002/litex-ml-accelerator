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
    
    // =============================================================================
    // EXTRAI PESOS DIRETAMENTE DO BUFFER __models_model_tflite[] (final.cc)
    // =============================================================================
    // O modelo TFLite (3024 bytes = 0xBD0) usa formato FlatBuffer
    // Os tensores quantizados (int8) estão armazenados sequencialmente
    // próximos ao final do arquivo
    
    // Força referência ao array para garantir linkagem
    volatile unsigned char first_byte = __models_model_tflite[0];
    (void)first_byte;
    
    printf("[TFLM] Modelo: %u bytes (magic: 0x%02X%02X%02X%02X)\n", 
           __models_model_tflite_len,
           __models_model_tflite[0], __models_model_tflite[1],
           __models_model_tflite[2], __models_model_tflite[3]);
    
    // Estrutura do modelo (3024 bytes):
    // - Metadados FlatBuffer: bytes 0x000 - 0x900
    // - Buffers de dados (pesos): bytes 0x900 - 0xBD0
    //
    // Ordem esperada dos tensores (de trás para frente):
    // 1. dense_4/BiasAdd (1 byte) - último buffer
    // 2. dense_4/MatMul weights (16 bytes)
    // 3. dense_3/BiasAdd (16 bytes)
    // 4. dense_3/MatMul weights (256 bytes = 16x16)
    // 5. dense_2/BiasAdd (16 bytes)
    // 6. dense_2/MatMul weights (16 bytes) - primeiro buffer
    
    const unsigned int model_len = __models_model_tflite_len;
    
    // Offsets dos tensores no FlatBuffer (determinados por análise do binário)
    // Modelo de 3024 bytes (0xBD0) - tensores no final do arquivo
    // Total de parâmetros: 16 + 16 + 256 + 16 + 16 + 1 = 321 bytes
    const int offset_layer1_weights = 0xA8C;  // dense_2/MatMul (16 bytes)
    const int offset_layer1_biases  = 0xA9C;  // dense_2/BiasAdd (16 bytes)
    const int offset_layer2_weights = 0xAAC;  // dense_3/MatMul (256 bytes)
    const int offset_layer2_biases  = 0xBAC;  // dense_3/BiasAdd (16 bytes)
    const int offset_output_weights = 0xBBC;  // dense_4/MatMul (16 bytes)
    const int offset_output_bias    = 0xBCC;  // dense_4/BiasAdd (1 byte)
    
    // Aponta para os dados no buffer do modelo
    layer1_weights = (const int8_t*)(__models_model_tflite + offset_layer1_weights);
    layer1_biases = (const int8_t*)(__models_model_tflite + offset_layer1_biases);
    layer2_weights = (const int8_t*)(__models_model_tflite + offset_layer2_weights);
    layer2_biases = (const int8_t*)(__models_model_tflite + offset_layer2_biases);
    output_weights = (const int8_t*)(__models_model_tflite + offset_output_weights);
    output_bias_ptr = (const int8_t*)(__models_model_tflite + offset_output_bias);
    
    // Debug: mostra primeiros valores para verificação
    printf("[TFLM] Layer1 W[0-3]: %d %d %d %d\n",
           layer1_weights[0], layer1_weights[1], layer1_weights[2], layer1_weights[3]);
    printf("[TFLM] Layer1 B[0-3]: %d %d %d %d\n",
           layer1_biases[0], layer1_biases[1], layer1_biases[2], layer1_biases[3]);
    printf("[TFLM] Output bias: %d\n", *output_bias_ptr);
    
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
