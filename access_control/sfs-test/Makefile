SANCUS_SUPPORT_DIR = /usr/local/share/sancus-support
DEVICE             = /dev/ttyUSB0
VENDOR_KEY         = 4078d505d82099ba

CC                 = sancus-cc
LD                 = sancus-ld
CRYPTO             = sancus-crypto
LOAD               = sancus-loader
RM                 = rm -f

DEBUG_LEVEL        = -DSFS_WARNING -DCOFFEE_DEBUG -DFLASH_NODEBUG #-DNODEBUG #-DNOCOLOR #-DNODEBUG #-DMY_MALLOC_DEBUG
CFS_BACKEND        = #-DCFS_BACKEND_PROTECTED -DCFS_DUMP #-DNO_CFS_FORMAT

CFLAGS             = -I$(SANCUS_SUPPORT_DIR)/include/ -g --verbose -Wfatal-errors $(DEBUG_LEVEL) $(CFS_BACKEND)
LDFLAGS            = --standalone --verbose --ram-size 24K --rom-size 32K
LIBS               = -L$(SANCUS_SUPPORT_DIR)/lib -ldev-uart -ldev-spi
CRYPTOFLAGS        = --key $(VENDOR_KEY)
LOADFLAGS          = -device $(DEVICE) -baudrate 115200

COFFEE             = ../sfs/cfs/cfs-coffee.o #sfs/cfs/cfs-dummy.o
SHM                = ../sfs/shm/shared-mem.o ../sfs/shm/my_malloc.o
SFS                = ../sfs/sfs-ram.o $(COFFEE)

OBJECTS            = simple-test.o ../../common.o $(SFS)
TARGET             = sfs-test.elf
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
