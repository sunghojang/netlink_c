CC =  arm-poky-linux-gnueabi-gcc
INC += /root/Network/libnl/libnl-3.2.25/build_root/include/libnl3
LIBS = /root/Network/libnl/libnl-3.2.25/build_root/lib/ -lnl-genl-3 -lnl-idiag-3 -lnl-3



all: $(TARGET)
	$(CC) -I$(INC) -L$(LIBS) netlink.c
	cp a.out /tftp
