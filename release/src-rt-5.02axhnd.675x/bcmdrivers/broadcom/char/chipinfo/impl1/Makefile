# Makefile for the bcm963xx board drivers
#


obj-$(CONFIG_BCM_CHIPINFO) := chipinfo.o 
ifneq ($(PREBUILT_EXTRAMOD),1)
bcmchipinfo-objs += chipinfo.o
else
chipinfo-objs := chipinfo$(PRBM_EXT).o
endif

EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)  -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD)  -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)  -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD) 
EXTRA_CFLAGS += -Werror -Wfatal-errors
EXTRA_CFLAGS += -g

-include $(TOPDIR)/Rules.make


clean:
	rm -f core *.ko *.o *.a *.s

