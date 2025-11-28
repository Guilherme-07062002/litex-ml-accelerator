BUILD_DIR?=../../build/colorlight_i5

include $(BUILD_DIR)/software/include/generated/variables.mak
include $(SOC_DIRECTORY)/software/common.mak

# Arquivos objeto - versão C pura com modelo do final.cc
OBJECTS = crt0.o firmware.o inference.o final.o

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
		-Wl,--no-whole-archive \
		$(LIBS:lib%=-l%) -lgcc
	chmod -x $@

crt0.o: $(CPU_DIRECTORY)/crt0.S
	$(assemble)

%.o: %.c
	$(compile)

%.o: %.cc
	$(compile)

%.o: %.S
	$(assemble)

clean:
	$(RM) $(OBJECTS) firmware.elf firmware.bin *.d .*~ *~

.PHONY: all clean
