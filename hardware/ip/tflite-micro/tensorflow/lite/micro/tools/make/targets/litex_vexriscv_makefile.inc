# Makefile para target LiteX VexRiscv
TARGET := litex_vexriscv
TARGET_ARCH := riscv32

# Toolchain
TARGET_TOOLCHAIN_PREFIX := riscv64-unknown-elf-
CXX_TOOL := g++
CC_TOOL := gcc
AR_TOOL := ar

# Flags do compilador
CXXFLAGS += -march=rv32im -mabi=ilp32
CXXFLAGS += -Os
CXXFLAGS += -fno-exceptions
CXXFLAGS += -fno-rtti
CXXFLAGS += -fno-threadsafe-statics
CXXFLAGS += -fmessage-length=0
CXXFLAGS += -fno-builtin
CXXFLAGS += -ffunction-sections
CXXFLAGS += -fdata-sections
CXXFLAGS += -funsigned-char
CXXFLAGS += -Wall
CXXFLAGS += -Wno-unused-parameter

CCFLAGS += -march=rv32im -mabi=ilp32
CCFLAGS += -Os
CCFLAGS += -fno-builtin
CCFLAGS += -ffunction-sections
CCFLAGS += -fdata-sections
CCFLAGS += -Wall

# Defines
CXXFLAGS += -DTF_LITE_STATIC_MEMORY
CXXFLAGS += -DTF_LITE_MCU_DEBUG_LOG

CCFLAGS += -DTF_LITE_STATIC_MEMORY
CCFLAGS += -DTF_LITE_MCU_DEBUG_LOG

# Includes específicos do LiteX
# Path relativo ao diretório tflite-micro (hardware/ip/tflite-micro)
LITEX_BUILD_DIR ?= ../../build/colorlight_i5

# Headers stub C++ para bare-metal
INCLUDES += -I../tflm_compat

# Includes do sistema (usar variável de ambiente ou path absoluto)
# O usuário deve ter oss-cad-suite instalado
TOOLCHAIN_ROOT ?= /home/user/embarcatech/tarefa5/hardware/tools/oss-cad-suite
ifneq ($(wildcard $(TOOLCHAIN_ROOT)),)
  INCLUDES += \
    -I$(TOOLCHAIN_ROOT)/lib/python3.11/site-packages/pythondata_software_picolibc/data/newlib/libc/include \
    -I$(TOOLCHAIN_ROOT)/lib/python3.11/site-packages/litex/soc/software/include
endif

INCLUDES += \
  -I$(LITEX_BUILD_DIR)/software/include \
  -I$(LITEX_BUILD_DIR)/software/include/generated

# Arquivo de output
MICROLITE_LIB_NAME := libtensorflow-microlite.a
