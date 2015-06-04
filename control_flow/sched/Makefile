SANCUS_SUPPORT_DIR = /usr/local/share/sancus-support
DEVICE             = /dev/ttyUSB0
VENDOR_KEY         = 4078d505d82099ba

CC                 = sancus-cc
LD                 = sancus-ld
CRYPTO             = sancus-crypto
LOAD               = sancus-loader
RM                 = rm -f

SCHED_ASM_PATH     = /media/DATA/Documenten/KUL/CW/2eFase/thesis/git/code/control-flow/spm-compiler/asm_scheduler/sched

CFLAGS             = -I$(SANCUS_SUPPORT_DIR)/include/ -g --verbose -Wfatal-errors
LDFLAGS            = --standalone --verbose --ram-size 24K --rom-size 32K --sched-asm-path=$(SCHED_ASM_PATH)
LIBS               = -L$(SANCUS_SUPPORT_DIR)/lib -ldev-uart
CRYPTOFLAGS        = --key $(VENDOR_KEY)
LOADFLAGS          = -device $(DEVICE) -baudrate 115200

OBJECTS            = main.o common.o foobar.o scheduler.o
TARGET             = main.elf
TARGET_NO_MACS     = $(TARGET)-no-macs

$(TARGET): $(TARGET_NO_MACS)
	$(CRYPTO) $(CRYPTOFLAGS) -o $@ $<

$(TARGET_NO_MACS): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

.PHONY: load
load: $(TARGET)
	$(LOAD) $(LOADFLAGS) $<

.PHONY: clean
clean:
	$(RM) $(TARGET) $(TARGET_NO_MACS) $(OBJECTS)
