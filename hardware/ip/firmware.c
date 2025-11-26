#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <uart.h>
#include <console.h>
#include <generated/csr.h>
#include <irq.h>

#include "inference.h"

static char *readstr(void)
{
    char c[2];
    static char s[64];
    static int ptr = 0;

    if(readchar_nonblock()) {
        c[0] = readchar();
        c[1] = 0;
        switch(c[0]) {
            case 0x7f:
            case 0x08:
                if(ptr > 0) {
                    ptr--;
                    putsnonl("\x08 \x08");
                }
                break;
            case 0x07:
                break;
            case '\r':
            case '\n':
                s[ptr] = 0x00;
                putsnonl("\n");
                ptr = 0;
                return s;
            default:
                if(ptr >= (sizeof(s) - 1))
                    break;
                putsnonl(c);
                s[ptr] = c[0];
                ptr++;
                break;
        }
    }
    return NULL;
}

static char *get_token(char **str)
{
    char *c, *d;

    c = (char *)strchr(*str, ' ');
    if(c == NULL) {
        d = *str;
        *str = *str+strlen(*str);
        return d;
    }
    *c = 0;
    d = *str;
    *str = c+1;
    return d;
}

static void prompt(void)
{
    printf("RUNTIME>");
}

static void help(void)
{
    puts("Available commands:");
    puts("help                            - this command");
    puts("reboot                          - reboot CPU");
    puts("led                             - led test");
    puts("execute                            - execute project");
}

static void reboot(void)
{
    ctrl_reset_write(1);
}

static void toggle_led(void)
{
    int i;
    printf("invertendo led...\n");
    i = leds_out_read();
    leds_out_write(!i);
}


static void execute(void)
{
    printf("Inicializando modelo TensorFlow Lite Micro...\n");
    
    // Inicializa o modelo
    inference_init();
    
    printf("Executando inferencias continuas (pressione Ctrl+C para parar)...\n");
    printf("Modelo: hello_world - aproximacao de funcao seno\n\n");
    
    // Variável de entrada - varia de 0 a 2*PI
    float x = 0.0f;
    float x_increment = 0.1f;  // Incremento a cada iteração
    const float pi = 3.14159265f;
    
    int iteration = 0;
    
    // Loop contínuo de inferência
    while(1) {
        // Executa inferência
        float y_pred = inference_run(x);
        
        // Converte saída para padrão de LED (0-255)
        unsigned char led_pattern = inference_output_to_led_pattern(y_pred);
        
        // Atualiza LEDs - cria efeito de barra proporcional ao valor
        // Os 8 LEDs acendem progressivamente conforme o valor aumenta
        unsigned char led_output = 0;
        int num_leds_on = (led_pattern * 8) / 256;  // Quantos LEDs acender (0-8)
        
        for(int i = 0; i < num_leds_on; i++) {
            led_output |= (1 << i);
        }
        
        leds_out_write(led_output);
        
        // Exibe informações a cada 10 iterações (~2 segundos)
        if (iteration % 10 == 0) {
            // Converte floats para inteiros para evitar dependência de softfloat
            int x_int = (int)(x * 1000);  // x em miliradians
            int y_pred_int = (int)(y_pred * 10000);  // y_pred com 4 casas decimais
            
            printf("Iter %4d | x=%d.%03d | y_pred=%s%d.%04d | LEDs=0x%02X (%d/8)\n",
                   iteration, 
                   x_int / 1000, x_int % 1000,
                   (y_pred >= 0) ? "+" : "-",
                   (y_pred_int < 0 ? -y_pred_int : y_pred_int) / 10000,
                   (y_pred_int < 0 ? -y_pred_int : y_pred_int) % 10000,
                   led_output, num_leds_on);
        }
        
        // Incrementa x (cicla de 0 a 2*PI)
        x += x_increment;
        if (x >= 2.0f * pi) {
            x = 0.0f;
            printf("\n--- Ciclo completo (0 a 2*PI) ---\n\n");
        }
        
        iteration++;
        
        // Delay simples (~200ms)
        for(volatile int i = 0; i < 1000000; i++);
        
        // Verifica se há comando do usuário
        if(readchar_nonblock()) {
            char c = readchar();
            if(c == 0x03) {  // Ctrl+C
                printf("\n\nExecucao interrompida pelo usuario.\n");
                break;
            }
        }
    }
    
    printf("Modelo executado com sucesso!\n");
}

static void console_service(void) {
    char *str;
    char *token;

    str = readstr();
    if(str == NULL) return;
    token = get_token(&str);
    if(strcmp(token, "help") == 0)
        help();
    else if(strcmp(token, "reboot") == 0)
        reboot();
    else if(strcmp(token, "led") == 0)
        toggle_led();
    else if(strcmp(token, "execute") == 0)
        execute();
    prompt();
}

int main(void) {
#ifdef CONFIG_CPU_HAS_INTERRUPT
    irq_setmask(0);
    irq_setie(1);
#endif
    uart_init();

    printf("Hellorld!\n");
    help();
    prompt();

    while(1) {
        console_service();
    }

    return 0;
}
