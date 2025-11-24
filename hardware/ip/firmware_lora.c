#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <irq.h>
#include <uart.h>
#include <console.h>
#include <generated/csr.h>
#include <generated/soc.h>
#include <system.h>

#include "aht10.h" // Sensor de temperatura e umidade AHT10
#include "lora_rfm95.h" // LoRa RFM95

// -------------------------------------------------------------
// Protótipos locais
// -------------------------------------------------------------
// Rotinas auxiliares usadas apenas neste módulo
static void send_sensor_data(void);
static void busy_wait_ms(unsigned int ms); // pequena espera em ms

// -------------------------------------------------------------
// Temporização (delay)
// -------------------------------------------------------------
// Implementação simples de espera em milissegundos. Mantemos a versão
// local aqui para que o firmware permaneça autocontido.
static void busy_wait_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
    // Fallback quando não há timer hardware: loop de ocupação
    for(volatile int j = 0; j < 2000; j++);
#endif
    }
}


// -------------------------------------------------------------
// Rotina de aplicação: leitura do sensor e envio via rádio LoRa
// -------------------------------------------------------------
static void send_sensor_data(void) {
    dados my_data; // tipo definido em aht10.h (valores em centésimos)
    printf("Lendo dados do sensor AHT10...\n");

    if (aht10_get_data(&my_data)) {
        // Mostra valores no console com duas casas decimais
        printf("  Temperatura: %d.%02d C\n", my_data.temperatura/100, abs(my_data.temperatura) % 100);
        printf("  Umidade: %d.%02d %%\n", my_data.umidade/100, abs(my_data.umidade) % 100);

        // Envia o conteúdo bruto da struct via biblioteca LoRa
        if (!lora_send_bytes((uint8_t*)&my_data, sizeof(dados))) {
            printf("Erro durante o envio LoRa (verifique logs da biblioteca).\n");
        }
    } else {
        printf("Erro ao ler AHT10. Envio LoRa cancelado.\n");
    }
}


// Frequência do LoRa em MHz (uso: 915 MHz - banda US915/BR)
#define LORA_FREQUENCY 915.0f

// -------------------------------------------------------------
// main: inicialização e loop principal
// -------------------------------------------------------------
int main(void) {
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

// Inicializa timer0 se presente
#ifdef CSR_TIMER0_BASE
    timer0_en_write(0);
    timer0_reload_write(0);
    timer0_load_write(CONFIG_CLOCK_FREQUENCY / 1000000); // Configura para contar microssegundos
    timer0_en_write(1);
    timer0_update_value_write(1);
#endif

    busy_wait_ms(500);

    printf("\n---------------- LiteX BIOS --------------\n");
    printf(  "Tarefa 05 – Transmissão de dados via LoRa \n");

    // Inicializa I2C
    i2c_init();

    // Inicializa o sensor AHT10
    aht10_init();

    // Inicializa LoRa usando a biblioteca
    if (!lora_init()) {
        printf("FALHA CRÍTICA: Inicialização do LoRa falhou.\n");
         while(1);
    }

    while (1) {
        send_sensor_data();
        busy_wait_ms(10000);
    }

    return 0;
}
