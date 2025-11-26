BUILD_DIR?=../../build/colorlight_i5

include $(BUILD_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

# Compiladores C++
CXX = $(TRIPLE)-g++
# Remove flags específicas de C que causam warnings em C++
CXXFLAGS_BASE = $(filter-out -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes,$(CFLAGS))
CXXFLAGS = $(CXXFLAGS_BASE) -std=c++17 -fno-rtti -fno-exceptions -fno-threadsafe-statics

# TensorFlow Lite Micro
TFLM_DIR = tflite-micro
TFLM_PORT = tflm
TFLM_INCLUDES = -I$(TFLM_DIR) -I$(TFLM_DIR)/third_party/flatbuffers/include \
                -I$(TFLM_DIR)/third_party/gemmlowp -I$(TFLM_DIR)/third_party/ruy -I.
TFLM_DEFINES = -DTF_LITE_STATIC_MEMORY -DTF_LITE_DISABLE_X86_NEON \
               -DGEMMLOWP_ALLOW_SLOW_SCALAR_FALLBACK \
               -DTF_LITE_USE_GLOBAL_CMATH_FUNCTIONS \
               -DTF_LITE_USE_GLOBAL_MAX -DTF_LITE_USE_GLOBAL_MIN

# Arquivos objeto
OBJECTS   = crt0.o firmware.o hello_world_model_data.o inference.o

all: firmware.bin

# pull in dependency info for *existing* .o files
-include $(OBJECTS:.o=.d)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@
	chmod -x $@

firmware.elf: $(OBJECTS)
	$(CC) $(LDFLAGS) -T linker.ld -N -o $@ \
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
	$(compile)

%.o: %.cpp
	$(compile)

%.o: %.S
	$(assemble)

clean:
	$(RM) $(OBJECTS) firmware.elf firmware.bin *.d .*~ *~

.PHONY: all clean
