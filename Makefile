TARGET = rgb_pir_light
MCU = attiny84
F_CPU = 8000000

FUSE_H = 0xdf
FUSE_L = 0xe2

PORT = ttyACM0

AVRDUDE = avrdude -c arduino -b 19200 -P /dev/$(PORT) -p $(MCU)

COMPILE = avr-gcc -std=gnu99 -Wall -Os -I. -mmcu=$(MCU) -DF_CPU=$(F_CPU) -DDEBUG_LEVEL=0

OBJECTS = $(TARGET).o led_stuff.o

all: $(TARGET).hex

.c.o:
	$(COMPILE) -c $< -o $@

.S.o:
	$(COMPILE) -x assembler-with-cpp -c $< -o $@

.c.s:
	$(COMPILE) -S $< -o $@

flash:  all
	$(AVRDUDE) -U flash:w:$(TARGET).hex:i

program: flash

fuse:
	$(AVRDUDE) -U hfuse:w:$(FUSE_H):m -U lfuse:w:$(FUSE_L):m


readfuse: getfuse
getfuse:
	$(AVRDUDE) -U hfuse:r:-:h -U lfuse:r:-:h

clean:
	rm -f $(TARGET).hex $(TARGET).lst $(TARGET).obj $(TARGET).cof $(TARGET).list $(TARGET).map $(TARGET).eep.hex $(TARGET).bin *.o $(TARGET).s

$(TARGET).bin:  $(OBJECTS)
	$(COMPILE) -o $(TARGET).bin $(OBJECTS)

$(TARGET).hex:  $(TARGET).bin
	rm -f $(TARGET).hex
	avr-objcopy -j .text -j .data -O ihex $(TARGET).bin $(TARGET).hex
	avr-size --mcu=$(MCU) --format=avr $(TARGET).bin

disasm: $(TARGET).bin
	avr-objdump -d $(TARGET).bin

cpp:
	$(COMPILE) -E $(TARGET).c