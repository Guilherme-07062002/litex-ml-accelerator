/**
 * Arquivo de configuração privado para a biblioteca SSD1306.
 * Exemplo configurado para Raspberry Pi Pico W usando I2C e incluindo fontes necessárias.
 */

#ifndef __SSD1306_CONF_H__
#define __SSD1306_CONF_H__

// Escolha do barramento
#define SSD1306_USE_I2C
//#define SSD1306_USE_SPI

// Configuração I2C
#define SSD1306_I2C_PORT        i2c1
#define SSD1306_I2C_ADDR        0x3C //(0x3C << 1)

// Espelhar (mirror) a tela, se necessário
// #define SSD1306_MIRROR_VERT
// #define SSD1306_MIRROR_HORIZ

// Inverter cores (opcional)
// # define SSD1306_INVERSE_COLOR

// Incluir apenas as fontes necessárias
#define SSD1306_INCLUDE_FONT_6x8
#define SSD1306_INCLUDE_FONT_7x10
#define SSD1306_INCLUDE_FONT_11x18
#define SSD1306_INCLUDE_FONT_16x26

#define SSD1306_INCLUDE_FONT_16x24

#define SSD1306_INCLUDE_FONT_16x15

// A largura do display pode ser ajustada via esta
// definição. Valor padrão: 128.
 #define SSD1306_WIDTH           128

// Se o eixo horizontal do display não começa na
// coluna 0, habilite a definição abaixo para
// ajustar o deslocamento horizontal.
// #define SSD1306_X_OFFSET

// A altura do display também pode ser ajustada se necessário.
// Valores suportados: 32, 64 ou 128. Valor padrão: 64.
 #define SSD1306_HEIGHT          64

#endif /* __SSD1306_CONF_H__ */
