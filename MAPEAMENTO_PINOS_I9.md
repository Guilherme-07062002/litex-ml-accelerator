# Mapeamento de Pinos - ColorLight i9 FPGA

## Placa de Expansão Externa (8 LEDs)

### Conector CN2 (Conector IDC 14 pinos)

Este projeto utiliza o conector **CN2** da placa HwIT para controlar 8 LEDs externos através de uma placa de expansão conectada via cabo flat IDC 14 pinos (2x7).

### Tabela de Mapeamento Completo

| Bit CSR | LED Placa | Pino FPGA | Posição CN2 | Função       |
|---------|-----------|-----------|-------------|--------------||
| bit 0   | L1        | P17       | Pino 4      | leds_ext[0]  |
| bit 1   | L2        | P18       | Pino 6      | leds_ext[1]  |
| bit 2   | L3        | N18       | Pino 8      | leds_ext[2]  |
| bit 3   | L4        | L20       | Pino 10     | leds_ext[3]  |
| bit 4   | L5        | L18       | Pino 12     | leds_ext[4]  |
| bit 5   | L6        | G20       | Pino 14     | leds_ext[5]  |
| bit 6   | L7        | M18       | Pino 11     | leds_ext[6]  |
| bit 7   | L8        | N17       | Pino 9      | leds_ext[7]  |

### Especificações Elétricas

- **Padrão de I/O:** LVCMOS33 (3.3V)
- **Conector:** CN2 - IDC 14 pinos (2x7)
- **Cabo:** Flat cable com marcação vermelha no pino 1
- **Lógica:** Ativa-alta (1 = LED aceso, 0 = LED apagado)

### Pinagem do Conector CN2 (IDC 14 pinos)

Conforme documentação da placa HwIT:

| Pino | Sinal | Pino | Sinal |
|------|-------|------|-------|
| 1    | GND   | 2    | 5V    |
| 3    | 3V3   | 4    | P17   |
| 5    | T17   | 6    | P18   |
| 7    | GND   | 8    | N18   |
| 9    | N17   | 10   | L20   |
| 11   | M18   | 12   | L18   |
| 13   | GND   | 14   | G20   |

**Referência:** https://github.com/dvcirilo/colorlight-i9-examples/tree/main/doc

## Uso no Firmware (C)

### Incluir Header Gerado

```c
#include <generated/csr.h>
```

### Funções Disponíveis

```c
// Ler estado atual dos LEDs
uint32_t estado = leds_out_read();

// Escrever padrão nos LEDs (0x00 a 0xFF)
leds_out_write(0xFF);  // Todos ligados
leds_out_write(0x01);  // Apenas L1 ligado
leds_out_write(0x00);  // Todos desligados
```

### Exemplos de Padrões

```c
// Barra crescente
for(int i = 0; i <= 8; i++) {
    unsigned char pattern = 0;
    for(int j = 0; j < i; j++) {
        pattern |= (1 << j);
    }
    leds_out_write(pattern);
    delay();
}

// Rotação (Knight Rider)
for(int pos = 0; pos < 8; pos++) {
    leds_out_write(1 << pos);
    delay();
}

// Piscar todos
for(int blink = 0; blink < 5; blink++) {
    leds_out_write(0xFF);
    delay();
    leds_out_write(0x00);
    delay();
}
```

## Uso no Terminal LiteX

### Acesso Direto ao Registrador CSR

O registrador `CSR_LEDS_OUT` está mapeado no endereço **0x82001800**.

```bash
# Acender LED L1
mem_write 0x82001800 0x01

# Acender LEDs L1 a L4 (barra de 4 LEDs)
mem_write 0x82001800 0x0F

# Padrão alternado (L1, L3, L5, L7)
mem_write 0x82001800 0x55

# Padrão alternado (L2, L4, L6, L8)
mem_write 0x82001800 0xAA

# Todos ligados
mem_write 0x82001800 0xFF

# Todos desligados
mem_write 0x82001800 0x00
```

## Definições no SoC (Python/LiteX)

### Arquivo: `hardware/ip/colorlight_i5.py`

```python
# Adicionar extensão de pinos (Conector CN2)
leds_pads = [
    ("leds_ext", 0, 
        Pins("P17 P18 N18 L20 L18 G20 M18 N17"),
        IOStandard("LVCMOS33")
    )
]
platform.add_extension(leds_pads)

# Criar GPIOOut e registrar CSR
self.submodules.leds = GPIOOut(platform.request("leds_ext"))
self.add_csr("leds")
```

## Constraints Geradas (.lpf)

O LiteX gera automaticamente as constraints no arquivo `build/colorlight_i5/gateware/colorlight_i5.lpf`:

```
LOCATE COMP "leds_ext[0]" SITE "P17";
IOBUF PORT "leds_ext[0]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[1]" SITE "P18";
IOBUF PORT "leds_ext[1]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[2]" SITE "N18";
IOBUF PORT "leds_ext[2]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[3]" SITE "L20";
IOBUF PORT "leds_ext[3]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[4]" SITE "L18";
IOBUF PORT "leds_ext[4]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[5]" SITE "G20";
IOBUF PORT "leds_ext[5]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[6]" SITE "M18";
IOBUF PORT "leds_ext[6]" IO_TYPE=LVCMOS33;
LOCATE COMP "leds_ext[7]" SITE "N17";
IOBUF PORT "leds_ext[7]" IO_TYPE=LVCMOS33;
```

## Troubleshooting

### Problema: LEDs não acendem

**Possíveis causas:**

1. **Cabo mal conectado**
   - Verifique se o cabo flat está firmemente conectado em ambas as pontas
   - Confirme orientação: faixa vermelha = pino 1

2. **Conector errado**
   - Certifique-se de estar usando o conector **CN2** (IDC 14 pinos)
   - Não confundir com CN3, CN4 ou CN5

3. **Lógica invertida**
   - Se LEDs acendem ao contrário (1=apaga, 0=acende), modifique:
   ```c
   leds_out_write(~pattern);  // Inverte todos os bits
   ```

4. **Ordem de bits invertida**
   - Se LEDs acendem na ordem reversa (L8→L1 em vez de L1→L8):
   ```python
   # Em colorlight_i5.py, inverta a ordem dos pinos:
   Pins("N17 M18 G20 L18 L20 N18 P18 P17")
   ```

### Problema: Apenas alguns LEDs funcionam

1. Verifique soldas da placa de expansão
2. Teste cada LED individualmente:
   ```bash
   mem_write 0x82001800 0x01  # Testa L1
   mem_write 0x82001800 0x02  # Testa L2
   mem_write 0x82001800 0x04  # Testa L3
   # ... e assim por diante
   ```

### Validação de Hardware

Execute o comando `execute` no terminal LiteX. O sistema realizará 3 testes automáticos:

1. ✅ Barra crescente (todos os LEDs devem acender sequencialmente)
2. ✅ Rotação Knight Rider (um LED por vez se movendo)
3. ✅ Pisca todos (todos os 8 LEDs piscando juntos)

Se todos os testes passarem, o hardware está funcionando corretamente.

## Referências

- **Plataforma:** ColorLight i5/i9 (Lattice ECP5 FPGA)
- **Framework:** LiteX SoC Builder
- **CPU:** PicoRV32 (RISC-V RV32IM)
- **Toolchain:** OSS CAD Suite (Yosys, nextpnr, ecppack)
- **Documentação LiteX:** https://github.com/enjoy-digital/litex
- **ColorLight Platform:** https://github.com/litex-hub/litex-boards
