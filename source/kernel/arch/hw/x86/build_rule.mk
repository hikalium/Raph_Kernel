include ../../../../../common.mk

MOUNT_DIR = /mnt/Raph_Kernel
IMAGE = /tmp/$(IMAGEFILE)
BUILD_DIR = ../../../../../build

OVMF_DIR = $(HOME)/edk2-UDK2017/

MAKE_SUBDIR := $(MAKE) -C
MAKE := $(MAKE) ARCH=$(ARCH) -f $(BUILD_RULE_FILE)

ifdef INIT
	REMOTE_COMMAND += export INIT=$(INIT); 
	INIT_FILE = $(INIT)
else
	BRANCH_INIT_FILE = $(shell git rev-parse --abbrev-ref HEAD)
	INIT_FILE = $(if $(shell ls init_script | grep -w $(BRANCH_INIT_FILE)),$(BRANCH_INIT_FILE),default)
endif

ifdef KERNEL_DEBUG
	QEMU_OPTIONS += -s -S 
endif

doc: export PROJECT_NUMBER:=$(shell git rev-parse HEAD ; git diff-index --quiet HEAD || echo "(with uncommitted changes)")

.PHONY: clean all disk run image mount umount debugqemu showerror numerror doc debug

default: image

###################################
# for remote host (Vagrant)
###################################

all: image

run:
	$(MAKE) qemuend
	$(MAKE) qemurun
	-telnet 127.0.0.1 1235
	$(MAKE) qemuend

qemurun: image
	sudo qemu-system-x86_64 \
		-drive if=pflash,readonly,file=$(OVMF_DIR)OVMF_CODE.fd,format=raw \
		-drive if=pflash,file=$(OVMF_DIR)OVMF_VARS.fd,format=raw \
		-cpu qemu64 -smp 8 -machine q35 -clock hpet \
		-monitor telnet:127.0.0.1:1235,server,nowait \
		-vnc 0.0.0.0:0,password \
		-net nic \
		-net bridge,br=br0 \
		$(QEMU_OPTIONS) \
		$(IMAGE)&
#    -usb -usbdevice keyboard \
# -net dump,file=./packet.pcap \
#	-drive id=disk,file=$(IMAGE),if=none -device ahci,id=ahci -device ide-drive,drive=disk,bus=ahci.0
	sleep 0.2s
	echo "set_password vnc a" | netcat 127.0.0.1 1235

debugqemu:
	sudo gdb -x ./.gdbinit -p `ps aux | grep qemu | sed -n 2P | awk '{ print $$2 }'`

qemuend:
	-sudo pkill -KILL qemu

$(BUILD_DIR)/script:
	cp script $@

$(BUILD_DIR)/rump.bin:
	cp rump.bin $(BUILD_DIR)/rump.bin

$(BUILD_DIR)/init_script/$(INIT_FILE): init_script/$(INIT_FILE)
	mkdir -p $(BUILD_DIR)/init_script/
	cp $^ $@

../../../../../source/tool/mkfs:
	$(MAKE_SUBDIR) ../../../../tool build

$(BUILD_DIR)/fs.img: ../../../../../source/tool/mkfs
	-rm $@
	cp ../../../../../README.md readme.md
	../../../../../source/tool/mkfs $@ readme.md
	rm readme.md

bin_sub: $(BUILD_DIR)/script $(BUILD_DIR)/init_script/$(INIT_FILE) $(BUILD_DIR)/fs.img $(BUILD_DIR)/rump.bin
	$(MAKE_SUBDIR) ../../../../kernel build
	$(MAKE_SUBDIR) ../../../../testmodule build

bin:
	mkdir -p $(BUILD_DIR)
	$(MAKE) bin_sub

image:
	$(MAKE) mount
	$(MAKE) bin
	sudo cp memtest86+.bin $(MOUNT_DIR)/boot/memtest86+.bin
	sudo sh -c 'sed -e "s/\/core\/init_script\/default/\/core\/init_script\/$(INIT_FILE)/g" grub.cfg > $(MOUNT_DIR)/boot/grub/grub.cfg'
	-sudo rm -rf $(MOUNT_DIR)/core
	sudo cp -r $(BUILD_DIR) $(MOUNT_DIR)/core
	$(MAKE) umount

cpimage: image
	cp $(IMAGE) /vagrant/

$(IMAGE):
	$(MAKE) umount
	dd if=/dev/zero of=$(IMAGE) bs=1M count=200
	./disk.sh disk-setup
	./disk.sh grub-install
	$(MAKE) mount
	sudo cp memtest86+.bin $(MOUNT_DIR)/boot/memtest86+.bin
	$(MAKE) umount

hd: image
	@if [ ! -e /dev/sdb ]; then echo "error: insert usb memory!"; exit -1; fi
	sudo dd if=$(IMAGE) of=/dev/sdb bs=1M

disk:
	$(MAKE) diskclean
	$(MAKE) $(IMAGE)
	$(MAKE) image

mount: $(IMAGE)
	./disk.sh mount

umount:
	./disk.sh umount

deldisk: umount
	-rm -f $(IMAGE)

clean: deldisk
	-rm -rf $(BUILD_DIR)
	$(MAKE_SUBDIR) ../../../../kernel clean
	$(MAKE_SUBDIR) ../../../../testmodule clean
	$(MAKE_SUBDIR) ../../../../tool clean

diskclean: deldisk clean

showerror:
	$(MAKE) image 2>&1 | egrep "In function|error:"

numerror:
	@echo -n "number of error: "
	@$(MAKE) image 2>&1 | egrep "error:" | wc -l

doc:
	doxygen

debug:
	gdb -x .gdbinit_for_kernel
