vpath application/%.cpp ../src/firmware
vpath modules/%.cpp ../
vpath modules/%.c ../

SOURCES_COMMON := 

# Common for all targets
ifeq (,$(findstring PROJECT_TARGET_USB_OVER_SERIAL_HOST,$(DEFINES)))
    SOURCES_COMMON += \
    modules/dbms/src/LESSDB.cpp \
    modules/midi/src/MIDI.cpp \
    modules/midi/src/transport/usb/USB.cpp \
    modules/midi/src/transport/serial/Serial.cpp \
    modules/midi/src/transport/ble/BLE.cpp \
    modules/u8g2/csrc/u8x8_string.c \
    modules/u8g2/csrc/u8x8_setup.c \
    modules/u8g2/csrc/u8x8_u8toa.c \
    modules/u8g2/csrc/u8x8_8x8.c \
    modules/u8g2/csrc/u8x8_u16toa.c \
    modules/u8g2/csrc/u8x8_display.c \
    modules/u8g2/csrc/u8x8_fonts.c \
    modules/u8g2/csrc/u8x8_byte.c \
    modules/u8g2/csrc/u8x8_cad.c \
    modules/u8g2/csrc/u8x8_gpio.c \
    modules/u8g2/csrc/u8x8_d_ssd1306_128x64_noname.c \
    modules/u8g2/csrc/u8x8_d_ssd1306_128x32.c \
    application/util/configurable/Configurable.cpp \
    application/util/scheduler/Scheduler.cpp
endif

ifeq ($(CORE_MCU_ARCH),arm)
    SOURCES_COMMON += modules/EmuEEPROM/src/EmuEEPROM.cpp
endif

# Common include dirs
INCLUDE_DIRS_COMMON := \
-I"./include" \
-I"../modules/" \
-I"../modules/core/include" \
-I"../modules/EmuEEPROM/include" \
-I"../src/firmware" \
-I"../src/firmware/bootloader/" \
-I"../src/firmware/application/" \
-I"../src/firmware/board/include" \
-I"../src/firmware/board/src/arch/$(CORE_MCU_ARCH)/$(CORE_MCU_VENDOR)/variants/$(CORE_MCU_FAMILY)" \
-I"$(MCU_GEN_DIR)" \
-I"$(TARGET_GEN_DIR)"
