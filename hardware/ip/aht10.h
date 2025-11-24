#ifndef AHT10_H_
#define AHT10_H_

#include <stdint.h>
#include <stdbool.h>

// -------------------------------------------------------------
// Estruturas de dados
// -------------------------------------------------------------
/**
 * @brief Container para valores de temperatura e umidade coletados.
 *
 * Observação: os valores são representados como inteiros escalados por 100
 * para evitar uso de ponto-flutuante no firmware (ex.: 2534 -> 25.34).
 */
typedef struct {
    int16_t temperatura; // temperatura em centésimos de grau (°C * 100)
    int16_t umidade;     // umidade relativa em centésimos (%% * 100)
} dados;


// -------------------------------------------------------------
// Interface pública (protótipos)
// -------------------------------------------------------------
/**
 * @brief Configura o driver I2C em modo bit-bang.
 *
 * Deve ser chamada antes de usar quaisquer funções que dependam do barramento.
 */
void i2c_init(void);

/**
 * @brief Faz um escaneamento simples do barramento I2C e loga endereços detectados.
 */
void i2c_scan(void);

/**
 * @brief Executa a sequência de inicialização do AHT10.
 *
 * @return 0 em caso de sucesso, -1 se ocorreu algum erro na comunicação.
 */
int aht10_init(void);

/**
 * @brief Leitura de depuração: lê o sensor e imprime os valores formatados.
 */
void aht10_read(void);

/**
 * @brief Solicita leitura ao AHT10 e preenche a struct fornecida.
 *
 * @param d Ponteiro para a struct onde temperatura e umidade serão gravadas.
 * @return true se a leitura foi bem sucedida, false caso contrário.
 */
bool aht10_get_data(dados *d);

#endif // AHT10_H_