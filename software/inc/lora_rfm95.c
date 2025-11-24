#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/irq.h"
#include "lora_rfm95.h"

// ============================
// DEFINIÇÕES E REGISTRADORES INTERNOS
// ============================
// Mapa de registradores do módulo RFM95 (modo LoRa)

#define REG_FIFO                 0x00 // FIFO para leitura/escrita de dados (256 bytes)
#define REG_OP_MODE              0x01 // Modo de operação do rádio (Sleep, Standby, TX, RX) e seleção LoRa/FSK
#define REG_FRF_MSB              0x06 // Byte MSB da frequência de portadora
#define REG_FRF_MID              0x07 // Byte intermediário da frequência de portadora
#define REG_FRF_LSB              0x08 // Byte LSB da frequência de portadora (escrever aqui altera a frequência)
#define REG_PA_CONFIG            0x09 // Configuração do amplificador de potência (PA)
#define REG_LNA                  0x0C // Configuração do amplificador de baixo ruído (LNA)
#define REG_FIFO_ADDR_PTR        0x0D // Ponteiro de endereço para operações no FIFO
#define REG_FIFO_TX_BASE_ADDR    0x0E // Endereço base do FIFO para transmissões
#define REG_FIFO_RX_BASE_ADDR    0x0F // Endereço base do FIFO para recepção
#define REG_FIFO_RX_CURRENT_ADDR 0x10 // Endereço do último pacote recebido no FIFO
#define REG_IRQ_FLAGS_MASK       0x11 // Máscara para habilitar/desabilitar IRQs específicas
#define REG_IRQ_FLAGS            0x12 // Flags de IRQ (TxDone, RxDone, CrcError, etc.)
#define REG_RX_NB_BYTES          0x13 // Número de bytes do payload recebido
#define REG_MODEM_CONFIG_1       0x1D // Parâmetros do modem: BW, CR, modo de cabeçalho
#define REG_MODEM_CONFIG_2       0x1E // Parâmetros do modem: Spreading Factor e CRC
#define REG_PREAMBLE_MSB         0x20 // MSB do comprimento do preâmbulo
#define REG_PREAMBLE_LSB         0x21 // LSB do comprimento do preâmbulo
#define REG_PAYLOAD_LENGTH       0x22 // Comprimento do payload (em bytes)
#define REG_MODEM_CONFIG_3       0x26 // Configurações adicionais: LDO, AGC
#define REG_DIO_MAPPING_1        0x40 // Mapeamento das funções dos pinos DIO0..DIO3
#define REG_VERSION              0x42 // Versão do chip (útil para verificação de comunicação)
#define REG_PA_DAC               0x4D // Configurações do DAC do PA (inclui modo +20dBm)
// MODOS
#define MODE_SLEEP               0x00
#define MODE_STDBY               0x01
#define MODE_TX                  0x03
#define MODE_RX_CONTINUOUS       0x05

// IRQ FLAGS
#define IRQ_TX_DONE_MASK         0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK 0x20
#define IRQ_RX_DONE_MASK         0x40

#define REG_PKT_RSSI_VALUE       0x1A // Contém o valor do RSSI do pacote mais recente.


// ============================
// VARIÁVEIS PRIVADAS (STATIC)
// ============================
static lora_config_t lora;
volatile static bool tx_done = false;
volatile static bool rx_done = false;
volatile static bool dio0_event = false;

// ============================
// PROTÓTIPOS DE FUNÇÕES PRIVADAS
// ============================
static void lora_reset();
static void lora_write_reg(uint8_t reg, uint8_t value);
static uint8_t lora_read_reg(uint8_t reg);
static void lora_write_fifo(const uint8_t *data, uint8_t len);
static void lora_read_fifo(uint8_t *data, uint8_t len);
static void lora_set_mode(uint8_t mode);
static void cs_select();
static void cs_deselect();
static void dio0_irq_handler(uint gpio, uint32_t events);
static void handle_dio0_events();

// ============================
// IMPLEMENTAÇÃO DAS FUNÇÕES
// ============================

// --- Funções públicas ---

bool lora_init(lora_config_t config) {
    lora = config; // Copia a configuração para a variável estática

    // --- Inicialização do hardware ---
    spi_init(lora.spi_instance, 5E6);
    gpio_set_function(lora.pin_miso, GPIO_FUNC_SPI);
    gpio_set_function(lora.pin_mosi, GPIO_FUNC_SPI);
    gpio_set_function(lora.pin_sck, GPIO_FUNC_SPI);
    gpio_init(lora.pin_cs); gpio_set_dir(lora.pin_cs, GPIO_OUT); gpio_put(lora.pin_cs, 1);
    gpio_init(lora.pin_rst); gpio_set_dir(lora.pin_rst, GPIO_OUT);
    
    gpio_init(lora.pin_dio0); gpio_set_dir(lora.pin_dio0, GPIO_IN);
    gpio_pull_down(lora.pin_dio0);
    gpio_set_irq_enabled_with_callback(lora.pin_dio0, GPIO_IRQ_EDGE_RISE, true, &dio0_irq_handler);

    lora_reset();
    
    lora_set_mode(MODE_SLEEP);
    lora_set_mode(MODE_STDBY);

    lora_write_reg(REG_IRQ_FLAGS, 0xFF); // Limpa todas as flags de IRQ
    
    uint64_t frf = ((uint64_t)lora.frequency << 19) / 32000000;
    lora_write_reg(REG_FRF_MSB, (uint8_t)(frf >> 16));
    lora_write_reg(REG_FRF_MID, (uint8_t)(frf >> 8));
    lora_write_reg(REG_FRF_LSB, (uint8_t)(frf >> 0));

    // Configurações para maior alcance e robustez
    lora_write_reg(REG_PA_CONFIG, 0xFF); // PaConfig: potência máxima (PA_BOOST)
    lora_write_reg(REG_PA_DAC, 0x87); // PaDac: ativa modo +20dBm
    lora_write_reg(REG_MODEM_CONFIG_1, 0x78); // ModemConfig1: BW ~62.5kHz, CR 4/8
    lora_write_reg(REG_MODEM_CONFIG_2, 0xC4); // ModemConfig2: SF12, CRC habilitado
    lora_write_reg(REG_MODEM_CONFIG_3, 0x0C); // ModemConfig3: LDO e AGC habilitados
    lora_write_reg(REG_PREAMBLE_MSB, 0x00);
    lora_write_reg(REG_PREAMBLE_LSB, 0x0C);

    lora_write_reg(0x0B, 0x37); // OCP default
    lora_write_reg(0x39, 0x12);

    // Outras configurações básicas
    lora_write_reg(REG_FIFO_TX_BASE_ADDR, 0x00);
    lora_write_reg(REG_FIFO_RX_BASE_ADDR, 0x00);
    lora_write_reg(REG_LNA, 0x23); // LNA boost para RX
    lora_write_reg(REG_IRQ_FLAGS_MASK, 0x00); // Libera todas as IRQs
    lora_write_reg(REG_IRQ_FLAGS, 0xFF); // Limpa IRQs
    
    // lora_set_mode(MODE_STDBY);
    
    uint8_t version = lora_read_reg(REG_VERSION);
    return (version == 0x12);
}

bool lora_send(const char *msg) {
    if (strlen(msg) > 255) return false;

    lora_set_mode(MODE_STDBY); 
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    lora_write_fifo((const uint8_t*)msg, strlen(msg));
    lora_write_reg(REG_PAYLOAD_LENGTH, strlen(msg));

    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_write_reg(REG_DIO_MAPPING_1, 0x40); // DIO0 -> TxDone

    tx_done = false;
    lora_set_mode(MODE_TX);

    absolute_time_t start_time = get_absolute_time();
    while (!tx_done) {
        handle_dio0_events();
        if (absolute_time_diff_us(start_time, get_absolute_time()) > (TX_TIMEOUT_MS * 1000)) {
            lora_set_mode(MODE_STDBY); // Aborta TX
            return false; // Timeout
        }
        tight_loop_contents();
    }

    lora_set_mode(MODE_STDBY);
    return true;
}

int lora_receive(char *buf, size_t maxlen) {
    handle_dio0_events();
    if (!rx_done) return 0;
    rx_done = false;

    uint8_t len = lora_read_reg(REG_RX_NB_BYTES);
    if (len > maxlen - 1) {
        printf("[AVISO] Pacote de %u bytes truncado para %u.\n", len, (unsigned)(maxlen - 1));
        len = (uint8_t)(maxlen - 1);
    }

    uint8_t fifo_addr = lora_read_reg(REG_FIFO_RX_CURRENT_ADDR);
    lora_write_reg(REG_FIFO_ADDR_PTR, fifo_addr);

    lora_read_fifo((uint8_t*)buf, len);
    buf[len] = '\0';

    return len;
}

// Implementação de funções auxiliares (envio/recebimento de buffers)
bool lora_send_bytes(const uint8_t *data, size_t len) {
    if (len > 255) return false;

    lora_set_mode(MODE_STDBY);
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    lora_write_fifo(data, len); // Usa a função existente de escrita no FIFO
    lora_write_reg(REG_PAYLOAD_LENGTH, len);

    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_write_reg(REG_DIO_MAPPING_1, 0x40); // DIO0 -> TxDone

    tx_done = false;
    lora_set_mode(MODE_TX);

    absolute_time_t start_time = get_absolute_time();
    while (!tx_done) {
        handle_dio0_events();
        if (absolute_time_diff_us(start_time, get_absolute_time()) > (TX_TIMEOUT_MS * 1000)) {
            lora_set_mode(MODE_STDBY);
            return false;
        }
        tight_loop_contents();
    }

    lora_set_mode(MODE_STDBY);
    return true;
}

int lora_receive_bytes(uint8_t *buf, size_t maxlen) {
    handle_dio0_events();
    if (!rx_done) return 0;
    rx_done = false;

    uint8_t len = lora_read_reg(REG_RX_NB_BYTES);
    if (len > maxlen) {
        printf("[AVISO] Pacote de %u bytes truncado para %u.\n", len, (unsigned)maxlen);
        len = (uint8_t)maxlen;
    }

    uint8_t fifo_addr = lora_read_reg(REG_FIFO_RX_CURRENT_ADDR);
    lora_write_reg(REG_FIFO_ADDR_PTR, fifo_addr);

    lora_read_fifo(buf, len); // Lê os bytes brutos

    return len;
}


void lora_start_rx_continuous(void) {
    lora_write_reg(REG_IRQ_FLAGS, 0xFF);
    lora_write_reg(REG_DIO_MAPPING_1, 0x00); // DIO0 -> RxDone
    lora_write_reg(REG_FIFO_ADDR_PTR, 0x00);
    lora_set_mode(MODE_RX_CONTINUOUS);
}

// --- Funções privadas ---

static void cs_select() { gpio_put(lora.pin_cs, 0); }
static void cs_deselect() { gpio_put(lora.pin_cs, 1); }

static void lora_reset() {
    gpio_put(lora.pin_rst, 0); sleep_ms(10);
    gpio_put(lora.pin_rst, 1); sleep_ms(10);
}

static void lora_write_reg(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { (uint8_t)(reg | 0x80), value };
    cs_select();
    spi_write_blocking(lora.spi_instance, buf, 2);
    cs_deselect();
}

static uint8_t lora_read_reg(uint8_t reg) {
    uint8_t buf[2] = { reg & 0x7F, 0x00 };
    uint8_t rx[2];
    cs_select();
    spi_write_read_blocking(lora.spi_instance, buf, rx, 2);
    cs_deselect();
    return rx[1];
}

static void lora_write_fifo(const uint8_t *data, uint8_t len) {
    cs_select();
    uint8_t addr = REG_FIFO | 0x80;
    spi_write_blocking(lora.spi_instance, &addr, 1);
    spi_write_blocking(lora.spi_instance, data, len);
    cs_deselect();
}

static void lora_read_fifo(uint8_t *data, uint8_t len) {
    cs_select();
    uint8_t addr = REG_FIFO & 0x7F;
    spi_write_blocking(lora.spi_instance, &addr, 1);
    spi_read_blocking(lora.spi_instance, 0x00, data, len);
    cs_deselect();
}

static void lora_set_mode(uint8_t mode) {
    lora_write_reg(REG_OP_MODE, (0x80 | mode)); // Bit 7 (LongRangeMode) sempre deve ser 1
}

static void dio0_irq_handler(uint gpio, uint32_t events) {
    (void)gpio; (void)events;
    dio0_event = true;
}

static void handle_dio0_events() {
    if (!dio0_event) return;
    dio0_event = false;

    uint8_t irq_flags = lora_read_reg(REG_IRQ_FLAGS);
    lora_write_reg(REG_IRQ_FLAGS, 0xFF); // Limpa todas as flags escrevendo 1s

    if ((irq_flags & IRQ_RX_DONE_MASK) && !(irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK)) {
        rx_done = true;
    } else if (irq_flags & IRQ_TX_DONE_MASK) {
        tx_done = true;
    } else if (irq_flags & IRQ_PAYLOAD_CRC_ERROR_MASK) {
        printf("[LORA_LIB] Erro de CRC no pacote!\n");
    }
}



// Função para obter RSSI do pacote recebido
int lora_get_rssi(void) {
    uint8_t rssi_raw = lora_read_reg(REG_PKT_RSSI_VALUE);
    // A fórmula para calcular o RSSI em dBm é RSSI = -157 + Rssi (para o frontend de HF)
    // Veja a seção 5.5.5 do datasheet do SX1276/7/8/9.
    return rssi_raw - 157;
}
