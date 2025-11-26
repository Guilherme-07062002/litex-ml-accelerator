/* Copyright 2023 LiteX ML Accelerator Project
 * Implementação REAL da inferência usando TensorFlow Lite Micro
 */

#include "inference.h"
#include "hello_world_model_data.h"

// TensorFlow Lite Micro headers
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include <cstdio>

namespace {
// Constantes para o interpretador
constexpr int kTensorArenaSize = 4 * 1024;  // 4KB para a arena de tensores
alignas(16) uint8_t tensor_arena[kTensorArenaSize];

// Ponteiros globais do interpretador
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input = nullptr;
TfLiteTensor* output = nullptr;

// Flag de inicialização
bool is_initialized = false;

}  // namespace

extern "C" {

void inference_init(void) {
    if (is_initialized) {
        return;
    }

    printf("[TFLM] Inicializando TensorFlow Lite Micro...\n");
    
    // Inicializa o sistema TFLM
    tflite::InitializeTarget();
    
    // Carrega o modelo
    model = tflite::GetModel(g_hello_world_model_data);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("[TFLM] ERRO: Versao do modelo (%lu) != TFLITE_SCHEMA_VERSION (%d)\n",
               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    printf("[TFLM] Modelo carregado! Tamanho: %u bytes\n", g_hello_world_model_data_size);
    
    // Configura os operadores necessários para o modelo hello_world
    // O modelo usa apenas: FullyConnected (dense layers)
    static tflite::MicroMutableOpResolver<1> micro_op_resolver;
    micro_op_resolver.AddFullyConnected();
    
    // Cria o interpretador
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;
    
    // Aloca tensores
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        printf("[TFLM] ERRO: AllocateTensors() falhou!\n");
        return;
    }
    
    printf("[TFLM] Tensores alocados com sucesso!\n");
    printf("[TFLM] Arena usada: %zu / %d bytes\n", 
           interpreter->arena_used_bytes(), kTensorArenaSize);
    
    // Obtém ponteiros para tensores de entrada e saída
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    printf("[TFLM] Input shape: [%d]\n", input->dims->data[1]);
    printf("[TFLM] Output shape: [%d]\n", output->dims->data[1]);
    printf("[TFLM] Modelo pronto para inferencia!\n");
    
    is_initialized = true;
}

float inference_run(float x_value) {
    if (!is_initialized) {
        inference_init();
        if (!is_initialized) {
            // Se falhou a inicialização, usa fallback
            printf("[TFLM] Usando fallback sin(x)\n");
            return sinf(x_value);
        }
    }
    
    // Define o valor de entrada
    // O modelo hello_world espera valores normalizados entre 0 e 1
    // correspondendo a x normalizado (x / (2*PI))
    input->data.f[0] = x_value;
    
    // Executa a inferência
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        printf("[TFLM] ERRO: Invoke() falhou!\n");
        return 0.0f;
    }
    
    // Obtém o resultado
    float y_pred = output->data.f[0];
    
    return y_pred;
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

}  // extern "C"
