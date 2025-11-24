#include "aht10.h"
#include <stdio.h>
#include <generated/csr.h>
#include <system.h>

// ------------------------------------------------------------------
// Temporizações locais
// ------------------------------------------------------------------
// Implementação simples de espera em milissegundos usada pelo driver
// (mantida aqui para evitar dependências externas durante build).
static void busy_wait_ms(unsigned int ms) {
    for (unsigned int i = 0; i < ms; ++i) {
#ifdef CSR_TIMER0_BASE
        busy_wait_us(1000);
#else
        for(volatile int j = 0; j < 1000; j++);
#endif
    }
}

// ------------------------------------------------------------------
// Driver I2C por bit-banging usando CSRs
// ------------------------------------------------------------------
// Controle manual das linhas SCL/SDA via registradores gerados pelo QoR
static uint32_t i2c_w_reg = 0;
static void i2c_delay(void) { busy_wait_us(5); }

static void i2c_set_scl(int val) {
    if (val) i2c_w_reg |= (1 << CSR_I2C_W_SCL_OFFSET);
    else     i2c_w_reg &= ~(1 << CSR_I2C_W_SCL_OFFSET);
    i2c_w_write(i2c_w_reg);
}

static void i2c_set_sda(int val) {
    if (val) i2c_w_reg |= (1 << CSR_I2C_W_SDA_OFFSET);
    else     i2c_w_reg &= ~(1 << CSR_I2C_W_SDA_OFFSET);
    i2c_w_write(i2c_w_reg);
}

static void i2c_set_oe(int val) {
    if (val) i2c_w_reg |= (1 << CSR_I2C_W_OE_OFFSET);
    else     i2c_w_reg &= ~(1 << CSR_I2C_W_OE_OFFSET);
    i2c_w_write(i2c_w_reg);
}

static int i2c_read_sda(void) {
    return (i2c_r_read() & (1 << CSR_I2C_R_SDA_OFFSET)) != 0;
}

// API pública: inicialização do driver I2C e utilitários
void i2c_init(void) {
    // Coloca as linhas em estado de alta e habilita saída
    i2c_set_oe(1); i2c_set_scl(1); i2c_set_sda(1);
    busy_wait_ms(1);
}

// Rotinas internas para controle do barramento I2C (start/stop/byte R/W)
static void i2c_start(void) {
    // Sequência de start: SDA sobe, SCL sobe, depois SDA cai
    i2c_set_sda(1); i2c_set_oe(1); i2c_set_scl(1); i2c_delay();
    i2c_set_sda(0); i2c_delay();
    i2c_set_scl(0); i2c_delay();
}

static void i2c_stop(void) {
    // Sequência de stop: SDA baixa, SCL sobe, SDA sobe
    i2c_set_sda(0); i2c_set_oe(1); i2c_set_scl(0); i2c_delay();
    i2c_set_scl(1); i2c_delay();
    i2c_set_sda(1); i2c_delay();
}

static bool i2c_write_byte(uint8_t byte) {
    int i; bool ack;
    // Força direção de saída e envia 8 bits, MSB primeiro
    i2c_set_oe(1);
    for (i = 0; i < 8; i++) {
        i2c_set_sda((byte & 0x80) != 0); i2c_delay();
        i2c_set_scl(1); i2c_delay();
        i2c_set_scl(0); i2c_delay();
        byte <<= 1;
    }
    // Libera linha SDA para receber ACK do escravo
    i2c_set_oe(0); i2c_set_sda(1); i2c_delay();
    i2c_set_scl(1); i2c_delay();
    ack = !i2c_read_sda();
    i2c_set_scl(0); i2c_delay();
    return ack;
}

static uint8_t i2c_read_byte(bool send_ack) {
    int i; uint8_t byte = 0;
    // Prepara para ler: direção de entrada
    i2c_set_oe(0); i2c_set_sda(1); i2c_delay();
    for (i = 0; i < 8; i++) {
        byte <<= 1;
        i2c_set_scl(1); i2c_delay();
        if (i2c_read_sda()) byte |= 1;
        i2c_set_scl(0); i2c_delay();
    }
    // Envia ACK/NACK conforme solicitado
    i2c_set_oe(1); i2c_set_sda(!send_ack); i2c_delay();
    i2c_set_scl(1); i2c_delay();
    i2c_set_scl(0); i2c_delay();
    return byte;
}

// Função pública de debug: varre endereços I2C e imprime os encontrados
void i2c_scan(void) {
    printf("Escaneando barramento I2C...\n");
    for (uint8_t addr = 1; addr < 128; addr++) {
        i2c_start();
        if (i2c_write_byte(addr << 1 | 0)) {
            printf("  Dispositivo encontrado em 0x%02X\n", addr);
        }
        i2c_stop();
        busy_wait_us(100);
    }
    printf("Scan completo.\n");
}


// ------------------------------------------------------------------
// AHT10: comandos e leitura do sensor (usa as rotinas I2C acima)
// ------------------------------------------------------------------
#define AHT10_I2C_ADDR 0x38

// Inicializa o sensor enviando a sequência de setup definida pelo fabricante
int aht10_init(void) {
    i2c_start();
    if (!i2c_write_byte(AHT10_I2C_ADDR << 1 | 0)) { i2c_stop(); return -1; }
    if (!i2c_write_byte(0xE1)) { i2c_stop(); return -1; }
    if (!i2c_write_byte(0x08)) { i2c_stop(); return -1; }
    if (!i2c_write_byte(0x00)) { i2c_stop(); return -1; }
    i2c_stop();
    busy_wait_ms(100);
    return 0;
}

// Solicita leitura ao sensor e converte os 6 bytes retornados em valores
// inteiros com escala *100 (ex.: 2534 => 25.34)
bool aht10_get_data(dados *d) {
    uint8_t data[6];
    uint32_t raw_hum, raw_temp;
    
    // Envia comando de medição conforme o protocolo AHT10
    i2c_start();
    if (!i2c_write_byte(AHT10_I2C_ADDR << 1 | 0)) { i2c_stop(); return false; } // request write
    if (!i2c_write_byte(0xAC)) { i2c_stop(); return false; }
    if (!i2c_write_byte(0x33)) { i2c_stop(); return false; }
    if (!i2c_write_byte(0x00)) { i2c_stop(); return false; }
    i2c_stop();

    // Tempo de espera para conclusão da medição
    busy_wait_ms(80);

    // Lê os 6 bytes de resposta do sensor
    i2c_start();
    if (!i2c_write_byte(AHT10_I2C_ADDR << 1 | 1)) { i2c_stop(); return false; } // request read
    data[0] = i2c_read_byte(true);
    data[1] = i2c_read_byte(true);
    data[2] = i2c_read_byte(true);
    data[3] = i2c_read_byte(true);
    data[4] = i2c_read_byte(true);
    data[5] = i2c_read_byte(false); // último byte com NACK
    i2c_stop();

    // Se o bit de busy estiver ativo, a leitura não está pronta
    if (data[0] & 0x80) {
        printf("Erro: AHT10 ainda ocupado.\n");
        return false;
    }

    // Reconstrói os valores brutos conforme layout do sensor
    raw_hum = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | (data[3] >> 4);
    raw_temp = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | data[5];

    // Converte para escala *100 (umidade: 0..10000 => 0.00..100.00)
    d->umidade = (int16_t)(((uint64_t)raw_hum * 10000) / 0x100000);

    // Temperatura em centésimos de grau (ex.: 2534 => 25.34°C)
    d->temperatura = (int16_t)((((uint64_t)raw_temp * 20000) / 0x100000) - 5000);
    
    return true;
}

// Função de depuração: lê o sensor e imprime valores formatados
void aht10_read(void) {
    dados my_data;
    printf("Lendo AHT10 (modo debug)...\n");
    if (aht10_get_data(&my_data)) {
        // Exibe com duas casas decimais (valores armazenados como centésimos)
        printf("Umidade: %d.%02d %%\n",
            my_data.umidade / 100, my_data.umidade % 100);
        printf("Temperatura: %d.%02d C\n",
            my_data.temperatura / 100, (my_data.temperatura > 0 ? my_data.temperatura : -my_data.temperatura) % 100);
    } else {
        printf("Falha ao ler AHT10.\n");
    }
}