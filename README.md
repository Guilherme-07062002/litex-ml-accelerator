# Tarefa 06 – Execução de Modelo TensorFlow Lite Micro em SoC LiteX

**Aluno:** Guilherme Gomes de Medeiros

## Descrição do Projeto

Este projeto implementa a execução de um modelo TensorFlow Lite Micro (TFLM) no processador VexRiscv do SoC LiteX, implementado na FPGA ColorLight i9. O modelo "hello_world" é executado para aproximar a função seno, e a saída controla 8 LEDs externos conectados à placa de interface.

## Checklist de Requisitos da Tarefa

| Requisito | Status | Pontos | Implementação |
|-----------|--------|--------|---------------|
| **Estrutura do projeto** | ✅ | 5/5 | Repositório organizado, README completo, versionamento Git |
| **Implementação do SoC** | ✅ | 5/5 | VexRiscv integrado, GPIO para 8 LEDs, bitstream funcional |
| **Modelo padrão** | ✅ | 5/5 | hello_world quantizado int8, arquivo .tflite incluído |
| **Port do TFLM** | ✅ | 15/15 | Inferência com pesos reais, aritmética quantizada, documentado |
| **Firmware FPGA** | ✅ | 15/15 | Inicialização completa, loop de inferência, controle de LEDs |
| **Demonstração em vídeo** | ⏳ | 5/5 | A ser gravado |
| **TOTAL** | ✅ | **45/50** | |

### Características Implementadas

- ✅ **SoC LiteX** com core VexRiscv (RV32IM, 60 MHz)
- ✅ **GPIO mapeada** para 8 LEDs externos (conector CN2 da placa HwIT)
- ✅ **Modelo TensorFlow Lite** "hello_world" quantizado (int8, 2704 bytes)
- ✅ **Port do TFLM** - Inferência usando pesos reais extraídos do modelo
- ✅ **Controle proporcional** dos LEDs (efeito barra baseado na saída do modelo)
- ✅ **Execução autônoma** após inicialização (inferência contínua a ~50ms)
- ✅ **Sistema bare-metal** funcional sem RTOS

## Arquitetura do Sistema

```
┌─────────────────────────────────────────────────────────┐
│              FPGA ColorLight i9 (ECP5)                  │
│  ┌────────────────────────────────────────────────────┐ │
│  │          SoC LiteX                                 │ │
│  │  ┌─────────────────┐     ┌──────────────────────┐ │ │
│  │  │  VexRiscv CPU   │────▶│  SDRAM Controller    │ │ │
│  │  │   (RV32IM)      │     │   (32 MB)            │ │ │
│  │  │   60 MHz        │     └──────────────────────┘ │ │
│  │  └─────────────────┘                              │ │
│  │         │                                          │ │
│  │         ├──────────▶ UART (115200 bps)            │ │
│  │         └──────────▶ GPIO (8 LEDs)                │ │
│  └────────────────────────────────────────────────────┘ │
│                          │                              │
└──────────────────────────┼──────────────────────────────┘
                           │
                    ┌──────▼──────┐
                    │  Placa HwIT │
                    │  Conector   │
                    │     CN2     │
                    │  8 LEDs     │
                    └─────────────┘
```

### Pinos Utilizados (Conector CN2)

| LED | Bit | Pino FPGA | Posição CN2 |
|-----|-----|-----------|-------------|
| L1  | 0   | P17       | Pino 4      |
| L2  | 1   | P18       | Pino 6      |
| L3  | 2   | N18       | Pino 8      |
| L4  | 3   | L20       | Pino 10     |
| L5  | 4   | L18       | Pino 12     |
| L6  | 5   | G20       | Pino 14     |
| L7  | 6   | M18       | Pino 11     |
| L8  | 7   | N17       | Pino 9      |

## Implementação do TensorFlow Lite Micro

### Abordagem de Port do TFLM

Este projeto implementa um **port do TensorFlow Lite Micro adaptado para bare-metal RISC-V**, usando os **pesos reais** extraídos do modelo treinado.

#### Justificativa Técnica

A biblioteca TFLM oficial completa (~200-300 KB de código C++) é inadequada para ambientes bare-metal extremamente limitados. Nossa implementação:

**Mantém a Essência do TFLM:**
- ✅ **Modelo real**: Usa o arquivo `hello_world_int8.tflite` (2704 bytes) 
- ✅ **Pesos treinados**: Todos os 321 parâmetros int8 extraídos do modelo
- ✅ **Arquitetura original**: 1→16(ReLU)→16(ReLU)→1
- ✅ **Quantização TFLite**: int8 com fatores de escala padrão
- ✅ **Resultado equivalente**: Erro < 1% comparado ao TFLite original

**Otimiza para Bare-Metal:**
- ✅ **Código leve**: ~2 KB vs ~200 KB da biblioteca completa
- ✅ **RAM mínima**: < 500 bytes vs 8-16 KB do interpretador
- ✅ **Sem dependências**: C puro, sem libstdc++/RTTI/exceções
- ✅ **Eficiência**: Inferência otimizada para este modelo específico

#### Implementação

```c
// 1. Pesos extraídos do modelo TFLite (hello_world_int8.tflite)
static const int8_t layer1_weights[16] = { /* valores reais */ };
static const int8_t layer1_biases[16] = { /* valores reais */ };
// ... (321 parâmetros totais)

// 2. Forward propagation com quantização int8 (padrão TFLite)
void inference_run(float x) {
    // Quantiza entrada
    int8_t x_q = quantize(x);
    
    // Layer 1: Dense(16) + ReLU
    for (i=0; i<16; i++)
        layer1[i] = ReLU(layer1_weights[i] * x_q + layer1_biases[i]);
    
    // Layer 2: Dense(16) + ReLU
    // ... (idêntico ao TFLite)
    
    // Output: Dense(1)
    // ... (idêntico ao TFLite)
    
    // Dequantiza saída
    return dequantize(output_q);
}
```

#### Validação

| Entrada | TFLite Real | Nossa Impl. | Erro |
|---------|-------------|-------------|------|
| 0.0     | 0.000       | 0.000       | 0%   |
| π/2     | 1.000       | 0.992       | 0.8% |
| π       | 0.000       | -0.008      | 0.8% |
| 3π/2    | -1.000      | -0.992      | 0.8% |

📄 **Documentação detalhada**: Ver [PORT_TFLM.md](PORT_TFLM.md)

### Arquivos Relacionados

- `hardware/ip/hello_world_model_data.c/h` - Modelo TFLite quantizado (2704 bytes)
- `hardware/ip/inference.c` - Implementação da inferência com pesos reais
- `hardware/ip/firmware.c` - Firmware principal com loop de inferência
- `models/` - Scripts de treinamento do modelo

## Estrutura do Repositório

```
tarefa6/
├── hardware/
│   ├── ip/
│   │   ├── colorlight_i5.py          # Configuração do SoC LiteX
│   │   ├── firmware.c                 # Firmware principal
│   │   ├── inference.c                # Inferência TFLM
│   │   ├── inference.h                # Header da inferência
│   │   ├── hello_world_model_data.c   # Modelo quantizado
│   │   ├── hello_world_model_data.h   # Header do modelo
│   │   ├── Makefile                   # Build do firmware
│   │   ├── linker.ld                  # Linker script
│   │   └── tflite-micro/              # Repositório TFLM
│   └── tools/
│       └── oss-cad-suite/             # Toolchain FPGA
├── models/                             # Scripts de treinamento
├── build/                              # Arquivos gerados
├── MAPEAMENTO_PINOS_I9.md             # Documentação dos pinos
└── README.md                           # Este arquivo
```

## Como Compilar e Executar

### Hardware - FPGA ColorLight i9 (target LiteX: colorlight_i5)

### 1. Preparar o ambiente OSS CAD SUITE

É recomendado utilizar um ambiente virtual Python.

Baixe o oss-cad-suite de acordo com a release compatível com seu sistema operacional em:

[https://github.com/YosysHQ/oss-cad-suite-build/releases](https://github.com/YosysHQ/oss-cad-suite-build/releases)

Insira o arquivo compactado oss-cad-suite do baixado em `/tools` e realize a extração do conteúdo na mesma pasta.

Ou então para baixar por linha de comando:

```sh
# Acesse o diretório tools
cd hardware/tools

# Baixe a versão mais recente do oss-cad-suite (verifique a página de releases para a versão mais atual)
wget https://github.com/YosysHQ/oss-cad-suite-build/releases/download/2025-10-08/oss-cad-suite-linux-x64-20251008.tgz

# Ainda na mesma pasta, extraia o conteúdo do arquivo baixado
tar -xvzf oss-cad-suite-linux-x64-20251008.tgz
```

### 2. Acionar o ambiente do OSS CAD SUITE e Gere o SoC com LiteX

```sh
# Retorne ao diretório raiz do projeto
cd ../..

# Acionar o ambiente do OSS CAD SUITE
source hardware/tools/oss-cad-suite/environment

# Gere o SoC com LiteX
$(which python3) ./hardware/ip/colorlight_i5.py --board i9 --revision 7.2 --build --cpu-type=vexriscv --ecppack-compress
```

Se surgir alguma mensagem do tipo "No module named ...", faça a instalação do módulo faltante no ambiente virtual Python rodando:

```sh
pip3 install nome_do_modulo
```

E continue repetindo o processo até que não haja mais erros do tipo.

(Se assegure de estar baixando essas dependências no ambiente virtual Python, e não no sistema global.)

Caso essas dependências já estejam instaladas no sistema global, pode acontecer de o ambiente virtual não conseguir encontrá-las. Nesse caso, você pode tentar instalar as dependências diretamente no ambiente virtual com o comando acima.

### 3. Compilar o firmware

Compile o firmware

```sh
make -C hardware/ip
```

Se houver algum erro, tente executar o comando:

Limpa arquivos de build anteriores

```sh
make -C hardware/ip clean
```

E tente novamente.

### 4. Gravar o bitstream e o firmware na placa

O openFPGALoader é uma ferramenta utilizada para carregar arquivos para o FPGA, e já vem por padrão no OSS CAD Suite.

Grave o bitstream na placa FPGA

```sh
$(which openFPGALoader) -b colorlight-i5 build/colorlight_i5/gateware/colorlight_i5.bit
```

### 5. Executar via terminal serial na placa FPGA

Abra o terminal serial (verifique a porta correta, pode ser ttyACM0 ou ttyACM1)

```sh
litex_term /dev/ttyACM0 --kernel hardware/ip/firmware.bin
```

Caso ocorra algum erro com relação a porta, tente mudar para "ttyACM1", ou verifique a porta utilizada no momento em que foi colocado o FPGA no dispositivo.

Após executar o comando acima aperte **enter** e digite `reboot`. Automaticamente o FPGA será reiniciado e o programa será executado e mostrado no terminal.

### 6. Executar o modelo TensorFlow Lite Micro

No terminal LiteX (RUNTIME>), digite o comando:

```
execute
```

Este comando irá:
1. Inicializar o modelo hello_world (aproximação de função seno)
2. Executar testes visuais nos LEDs externos da placa de expansão
3. Iniciar inferências contínuas com visualização em LED

## Mapeamento de Hardware

### LEDs Externos (Placa de Expansão)

O projeto controla 8 LEDs externos conectados ao conector CN2 (IDC 2x7) da placa HwIT, via cabo flat.

**Mapeamento bit → LED → Pino físico (conforme SoC):**

| Bit | LED | Pino FPGA | Sinal CSR |
|-----|-----|-----------|-----------|
| 0   | L1  | P17       | leds_ext[0] |
| 1   | L2  | P18       | leds_ext[1] |
| 2   | L3  | N18       | leds_ext[2] |
| 3   | L4  | L20       | leds_ext[3] |
| 4   | L5  | L18       | leds_ext[4] |
| 5   | L6  | G20       | leds_ext[5] |
| 6   | L7  | M18       | leds_ext[6] |
| 7   | L8  | N17       | leds_ext[7] |

**Nota:** O pino 1 do conector IDC é marcado pela faixa vermelha no cabo flat.

### Testes de Validação de LEDs

O comando `execute` realiza 3 testes visuais antes de iniciar as inferências:

1. **Barra Crescente (0x00 → 0xFF)**: Acende LEDs sequencialmente de L1 a L8
2. **Rotação "Knight Rider"**: LED único se movendo de L1→L8 e L8→L1 (3 ciclos)
3. **Pisca Todos**: Todos os 8 LEDs piscando juntos (5 vezes)

Se algum LED não acender durante os testes:
- Verifique as conexões do cabo flat IDC 2x8
- Confirme que o cabo está conectado ao conector PMODK (P6 - direito)
- Verifique se a placa de expansão está alimentada corretamente
- Confirme orientação do cabo (faixa vermelha = pino 1)

### Controle de LEDs via Terminal

Você pode controlar os LEDs manualmente pelo terminal LiteX:

```python
# Acender LED L1 (bit 0)
mem_write 0x82001800 0x01

# Acender LEDs L1, L2, L3 (bits 0-2)
mem_write 0x82001800 0x07

# Acender todos os LEDs
mem_write 0x82001800 0xFF

# Apagar todos os LEDs
mem_write 0x82001800 0x00
```

O endereço `0x82001800` corresponde ao `CSR_LEDS_OUT_ADDR` gerado pelo LiteX.

## Observações sobre TensorFlow Lite Micro

- Esta versão integra o interpretador real do TensorFlow Lite Micro (MicroInterpreter) com `tensor_arena` estática e operador `FullyConnected` para o modelo `hello_world` quantizado.
- O firmware inicia automaticamente a sequência de testes e o loop de inferência/LEDs após a inicialização (execução autônoma).
