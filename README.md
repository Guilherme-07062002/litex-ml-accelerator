# Tarefa 06 – Execução de Modelo TensorFlow Lite Micro em SoC LiteX

**Aluno:** Guilherme Gomes de Medeiros

## Descrição do Projeto

Este projeto implementa a execução de um modelo TensorFlow Lite Micro (TFLM) no processador VexRiscv do SoC LiteX, implementado na FPGA ColorLight i9. O modelo "hello_world" é executado para aproximar a função seno, e a saída controla 8 LEDs externos conectados à placa de interface.

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
│  │  ┌─────────────────┐     ┌──────────────────────┐  │ │
│  │  │  VexRiscv CPU   │────▶│  SDRAM Controller    │   │ │
│  │  │   (RV32IM)      │     │   (32 MB)            │  │ │
│  │  │   60 MHz        │     └──────────────────────┘  │ │
│  │  └─────────────────┘                               │ │
│  │         │                                          │ │
│  │         ├──────────▶ UART (115200 bps)             │ │
│  │         └──────────▶ GPIO (8 LEDs)                 │ │
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

E tente novamente o comando de compilação do firmware.

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

Após executar o comando acima aperte **enter** e digite `reboot`. Automaticamente o FPGA será reiniciado e o programa será executado.

## Mapeamento de Hardware

### LEDs Externos (Placa de Expansão)

O projeto controla 8 LEDs externos conectados ao conector CN2 (IDC 2x7) da placa HwIT, via cabo flat.

**Mapeamento bit → LED → Pino físico (conforme SoC):**

| Bit | LED | Pino FPGA | Sinal CSR   |
|-----|-----|-----------|-------------|
| 0   | L1  | P17       | leds_ext[0] |
| 1   | L2  | P18       | leds_ext[1] |
| 2   | L3  | N18       | leds_ext[2] |
| 3   | L4  | L20       | leds_ext[3] |
| 4   | L5  | L18       | leds_ext[4] |
| 5   | L6  | G20       | leds_ext[5] |
| 6   | L7  | M18       | leds_ext[6] |
| 7   | L8  | N17       | leds_ext[7] |

### Testes de Validação de LEDs

Antes da execução das inferências no modelo, a FPGA realiza 3 testes visuais:

1. **Barra Crescente (0x00 → 0xFF)**: Acende LEDs sequencialmente de L1 a L8
2. **Rotação "Knight Rider"**: LED único se movendo de L1→L8 e L8→L1 (3 ciclos)
3. **Pisca Todos**: Todos os 8 LEDs piscando juntos (5 vezes)

Se algum LED não acender durante os testes:
- Verifique as conexões do cabo flat IDC 2x8
- Confirme que o cabo está conectado ao conector PMODK (P6 - direito)
- Verifique se a placa de expansão está alimentada corretamente
- Confirme orientação do cabo (faixa vermelha = pino 1)
