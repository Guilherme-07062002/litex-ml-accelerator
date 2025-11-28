/* Copyright 2023 LiteX ML Accelerator Project
 * 
 * IMPLEMENTAÇÃO SIMPLIFICADA - Bare-Metal Compatibility
 * Extrai pesos do modelo TFLite mas não usa interpretador completo
 * (TFLite Micro completo requer libstdc++ que não está disponível)
 */

#include "inference.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Modelo TFLite gerado com xxd -i models/model.tflite
extern "C" {
    extern unsigned char __models_model_tflite[];
    extern unsigned int __models_model_tflite_len;
}

// =============================================================================
// CONFIGURAÇÃO DO TFLITE MICRO
// =============================================================================

namespace {
    const tflite::Model* model = nullptr;
    tflite::MicroInterpreter* interpreter = nullptr;
    TfLiteTensor* input = nullptr;
    TfLiteTensor* output = nullptr;
    
    // Arena para alocação de tensores (ajustar se necessário)
    constexpr int kTensorArenaSize = 10 * 1024; // 10KB
    alignas(16) uint8_t tensor_arena[kTensorArenaSize];
    
    bool is_initialized = false;
}

// =============================================================================
// INICIALIZAÇÃO
// =============================================================================

extern "C" void inference_init(void) {
    if (is_initialized) {
        return;
    }
    
    printf("\n");
    printf("================================================================================\n");
    printf(" TensorFlow Lite Micro - Interpretador Completo\n");
    printf("================================================================================\n");
    printf("[TFLM] Carregando modelo: models/model.tflite (%u bytes)\n", 
           __models_model_tflite_len);
    
    // Inicializa TFLite Micro
    tflite::InitializeTarget();
    
    // Carrega o modelo
    model = tflite::GetModel(__models_model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[ERRO] Versao do modelo (%d) != versao do schema (%d)\n",
               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    printf("[TFLM] Modelo carregado (schema version: %d)\n", model->version());
    
    // Configura operadores necessários para o modelo hello_world
    static tflite::MicroMutableOpResolver<5> resolver;
    
    // Adiciona operadores usados pelo modelo
    resolver.AddQuantize();
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddDequantize();
    resolver.AddLogistic(); // Para ativações, se necessário
    
    printf("[TFLM] Operadores registrados: Quantize, FullyConnected, ReLU, Dequantize\n");
    
    // Cria o interpretador
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;
    
    // Aloca tensores
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        printf("[ERRO] Falha ao alocar tensores\n");
        return;
    }
    printf("[TFLM] Tensores alocados (arena: %d bytes)\n", kTensorArenaSize);
    
    // Obtém ponteiros para tensores de entrada e saída
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    // Mostra informações dos tensores
    printf("[TFLM] Input tensor:\n");
    printf("       - Dims: %d", input->dims->size);
    for (int i = 0; i < input->dims->size; i++) {
        printf(" x %d", input->dims->data[i]);
    }
    printf("\n");
    printf("       - Type: %d (1=float32, 9=int8)\n", input->type);
    
    printf("[TFLM] Output tensor:\n");
    printf("       - Dims: %d", output->dims->size);
    for (int i = 0; i < output->dims->size; i++) {
        printf(" x %d", output->dims->data[i]);
    }
    printf("\n");
    printf("       - Type: %d\n", output->type);
    
    printf("[TFLM] Modelo inicializado com sucesso!\n");
    printf("================================================================================\n");
    printf("\n");
    
    is_initialized = true;
}

// =============================================================================
// INFERÊNCIA
// =============================================================================

extern "C" float inference_run(float x_value) {
    static int debug_count = 0;
    
    if (!is_initialized) {
        inference_init();
        if (!is_initialized) {
            return 0.0f; // Falha na inicialização
        }
    }
    
    // Normaliza entrada para [0, 1]
    const float pi = 3.14159265f;
    float x_normalized = x_value / (2.0f * pi);
    
    // Debug periódico
    if (debug_count % 50 == 0) {
        int x_int = (int)(x_value * 1000);
        int norm_int = (int)(x_normalized * 1000);
        printf("[DBG] x=%d.%03d norm=%d.%03d\n", 
               x_int/1000, x_int%1000, norm_int/1000, norm_int%1000);
    }
    debug_count++;
    
    // Define entrada do modelo
    // O modelo pode esperar float32 ou int8 quantizado
    if (input->type == kTfLiteFloat32) {
        input->data.f[0] = x_normalized;
    } else if (input->type == kTfLiteInt8) {
        // Quantiza para int8
        int8_t x_quantized = (int8_t)((x_normalized * 255.0f) - 128.0f);
        input->data.int8[0] = x_quantized;
    }
    
    // Executa inferência
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        printf("[ERRO] Falha na inferencia\n");
        return 0.0f;
    }
    
    // Lê saída do modelo
    float y_pred = 0.0f;
    if (output->type == kTfLiteFloat32) {
        y_pred = output->data.f[0];
    } else if (output->type == kTfLiteInt8) {
        // Dequantiza de int8 para float
        int8_t y_quantized = output->data.int8[0];
        const float output_scale = 0.0078125f; // 1/128
        y_pred = (float)y_quantized * output_scale;
    }
    
    // Debug periódico
    if (debug_count % 50 == 1) {
        int out_int = (int)(y_pred * 1000);
        printf("[DBG] out_f=%d.%03d\n", out_int/1000, 
               (out_int < 0 ? -out_int : out_int) % 1000);
    }
    
    // Limita entre -1 e 1
    if (y_pred > 1.0f) y_pred = 1.0f;
    if (y_pred < -1.0f) y_pred = -1.0f;
    
    return y_pred;
}

// =============================================================================
// CONVERSÃO PARA LEDS
// =============================================================================

extern "C" unsigned char inference_output_to_led_pattern(float output_value) {
    // Converte de [-1, 1] para [0, 1]
    float normalized = (output_value + 1.0f) / 2.0f;
    
    // Limita
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    
    // Converte para 0-255
    unsigned char led_value = (unsigned char)(normalized * 255.0f);
    
    return led_value;
}
