SANCUS_SUPPORT_DIR = /usr/local/share/sancus-support
DEVICE             = /dev/ttyUSB0
VENDOR_KEY         = 4078d505d82099ba

CC                 = sancus-cc
LD                 = sancus-ld
CRYPTO             = sancus-crypto
LOAD               = sancus-loader
RM                 = rm -f

CFLAGS             = -I$(SANCUS_SUPPORT_DIR)/include/ -g --verbose #-DNODEBUG
LDFLAGS            = --standalone --verbose --ram-size 24K --rom-size 32K
LIBS               = -L$(SANCUS_SUPPORT_DIR)/lib -ldev-uart -ldev-spi
CRYPTOFLAGS        = --key $(VENDOR_KEY)
LOADFLAGS          = -device $(DEVICE) -baudrate 115200

OBJECTS            = common.o flash-disk.o basic-file-sys.o file-sys.o simple-test.o#nb_disk_accesses_test.o#shared_memory_test.o
TARGET             = file-sys.elf
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

PCCC        = gcc
PCCFLAGS    = -g -Wfatal-errors -DNSANCUS_COMPILE

.PHONY: pc
pc:
	$(PCCC) $(PCCFLAGS) -c common.c -o common.o
	$(PCCC) $(PCCFLAGS) -c ram-disk.c -o ram-disk.o
	$(PCCC) $(PCCFLAGS) -c basic-file-sys.c -o basic-file-sys.o
	$(PCCC) $(PCCFLAGS) -c file-sys.c -o file-sys.o
	$(PCCC) $(PCCFLAGS) -c main.c -o main.o
	$(PCCC) $(PCCFLAGS) common.o main.o ram-disk.o basic-file-sys.o file-sys.o -o file-sys

