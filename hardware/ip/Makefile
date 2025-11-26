BUILD_DIR?=../../build/colorlight_i5

include $(BUILD_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

# Compiladores C++
CXX = $(TRIPLE)-g++
# Remove flags específicas de C que causam warnings em C++
CXXFLAGS_BASE = $(filter-out -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes,$(CFLAGS))
CXXFLAGS = $(CXXFLAGS_BASE) -std=c++17 -fno-rtti -fno-exceptions -fno-threadsafe-statics

# Macro para compilar C++
define compilexx
	$(CXX) -c $(CXXFLAGS) $(DEPFLAGS) $(INCLUDES) $<
endef

# TensorFlow Lite Micro
TFLM_DIR = tflite-micro
TFLM_PORT = tflm
TFLM_INCLUDES = -I$(TFLM_DIR) -I$(TFLM_DIR)/third_party/flatbuffers/include \
				-I$(TFLM_DIR)/third_party/gemmlowp -I$(TFLM_DIR)/third_party/ruy -I.
TFLM_DEFINES = -DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON \
			   -DGEMMLOWP_ALLOW_SLOW_SCALAR_FALLBACK \
			   -DTF_LITE_USE_GLOBAL_CMATH_FUNCTIONS \
			   -DTF_LITE_USE_GLOBAL_MAX -DTF_LITE_USE_GLOBAL_MIN

# Fontes essenciais do TFLM para o modelo hello_world (FullyConnected)
TFLM_SRCS = \
	$(TFLM_DIR)/tensorflow/lite/core/api/flatbuffer_conversions.cc \
	$(TFLM_DIR)/tensorflow/lite/core/api/tensor_utils.cc \
	$(TFLM_DIR)/tensorflow/lite/core/c/common.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/flatbuffer_utils.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/memory_helpers.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/micro_allocator.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/micro_context.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/micro_interpreter.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/micro_log.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/micro_op_resolver.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/kernels/kernel_util.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/kernels/fully_connected_common.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/kernels/fully_connected.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/kernels/activations_common.cc \
	$(TFLM_DIR)/tensorflow/lite/micro/kernels/activations.cc \
	$(TFLM_PORT)/debug_log.cc \
	$(TFLM_PORT)/micro_time.cc \
	$(TFLM_PORT)/system_setup.cc

TFLM_OBJECTS = $(TFLM_SRCS:.cc=.o)

# Arquivos objeto
# Usa a versão com MicroInterpreter (inference.cc). Mantemos o nome distinto para evitar conflito com inference.c
OBJECTS   = crt0.o firmware.o hello_world_model_data.o inference_cc.o $(TFLM_OBJECTS)

all: firmware.bin

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
	chmod -x $@


firmware.elf: $(OBJECTS)
	$(CXX) $(LDFLAGS) -T linker.ld -N -o $@ \
		$(OBJECTS) \
		$(PACKAGES:%=-L$(BUILD_DIR)/software/%) \
		-Wl,--whole-archive \
		-Wl,--gc-sections \
		-Wl,-Map,$@.map \
		$(LIBS:lib%=-l%)
	chmod -x $@

crt0.o: $(CPU_DIRECTORY)/crt0.S
	$(assemble)

%.o: %.c
	$(compile)

%.o: %.cc
	$(compilexx) -c $(TFLM_INCLUDES) $(TFLM_DEFINES) $< -o $@

%.o: %.cpp
	$(compilexx) -c $(TFLM_INCLUDES) $(TFLM_DEFINES) $< -o $@

%.o: %.S
	$(assemble)

clean:
	$(RM) $(OBJECTS) firmware.elf firmware.bin *.d .*~ *~
	$(RM) $(TFLM_DIR)/tensorflow/lite/**/*.o $(TFLM_DIR)/tensorflow/lite/**/*.d
	$(RM) $(TFLM_PORT)/*.o $(TFLM_PORT)/*.d

.PHONY: all clean

# Regra explícita para compilar a versão C++ da inferência
inference_cc.o: inference.cc
	$(compilexx) -c $(TFLM_INCLUDES) $(TFLM_DEFINES) $< -o $@
