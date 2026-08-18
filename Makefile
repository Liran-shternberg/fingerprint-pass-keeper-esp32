PORT ?= $(shell ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -1)
FQBN ?= esp32:esp32:makergo_c3_supermini
SKETCH ?= fingerprint_keyboard
BAUD ?= 115200

.PHONY: all build flash upload monitor clean

all: build

build:
	sudo env HOME=$(HOME) python3 typer/gen_cipher.py fingerprint_keyboard/secrets.h typer/public_key.pem fingerprint_keyboard/cipher.h
	sudo env HOME=$(HOME) arduino-cli compile -b $(FQBN) $(SKETCH)

flash: build upload

upload:
	sudo env HOME=$(HOME) arduino-cli upload -b $(FQBN) -p $(PORT) $(SKETCH)

monitor:
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)

clean:
	sudo rm -rf ~/.cache/arduino/sketches
