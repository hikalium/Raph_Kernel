include common.mk

VNC_PORT = 15900
VDI = disk.vdi
HOST_IP = `ifconfig en0 | grep "inet " | cut -d" " -f2`

define make_wrapper
	$(if $(shell if [ -e /etc/bootstrapped ]; then echo "guest"; fi), \
	  # guest environment
	  $(MAKE) -f $(RULE_FILE) $(1), \
	  # host environment
	  $(if $(shell ssh -F .ssh_config default "exit"; if [ $$? != 0 ]; then echo "no-guest"; fi), rm -f .ssh_config
	  vagrant halt
	  vagrant up
	  vagrant ssh-config > .ssh_config; )
	  ssh -F .ssh_config default "cd /vagrant/; env MAKEFLAGS=$(MAKEFLAGS) make -f $(RULE_FILE) $(1)"
	)
endef

define runcmd_wrapper
	$(if $(shell if [ -e /etc/bootstrapped ]; then echo "guest"; fi), \
	  # guest environment
	  $(1), \
	  # host environment
	  $(if $(shell ssh -F .ssh_config default "exit"; if [ $$? != 0 ]; then echo "no-guest"; fi), rm -f .ssh_config
	  vagrant halt
	  vagrant up
	  vagrant ssh-config > .ssh_config; )
	  ssh -F .ssh_config default "cd /vagrant/; $(1)"
	)
endef

define check_guest
	$(if $(shell if [ -e /etc/bootstrapped ]; then echo "guest"; fi), \
	  @echo "error: run this command on the host environment."; exit 1)
endef

UNAME = ${shell uname}
ifeq ($(OS),Windows_NT)
define vnc
	@echo Windows is not supported; exit 1
endef
else ifeq ($(UNAME),Linux)
define vnc
	@echo open with VNC Viewer.
	@echo Please install it in advance.
	vncviewer localhost::$(VNC_PORT)
endef
else ifeq ($(UNAME),Darwin)
define vnc
	@echo open with the default VNC viewer.
	open vnc://localhost:$(VNC_PORT)
endef
else
define vnc
	@echo non supported OS; exit 1
endef
endif

###################################
# for remote host (Vagrant)
###################################

default:
	$(call make_wrapper, image)

.PHONY: run hd image mount umount clean

run:
	$(call make_wrapper, run)

hd:
	$(call make_wrapper, hd)

image:
	$(call make_wrapper, image)

mount:
	$(call make_wrapper, mount)

umount:
	$(call make_wrapper, umount)

clean:
	$(call make_wrapper, clean)


.PHONY: vnc debug vboxrun vboxkill run_pxeserver pxeimg burn_ipxe burn_ipxe_remote debugqemu showerror numerror doc
vnc:
	$(call check_guest)
	@echo info: vnc password is "a"
	$(call vnc)

debug:
	vagrant ssh -c "cd /vagrant/; gdb -x .gdbinit_for_kernel"

vboxrun: vboxkill synctime
	@$(SSH_CMD) "$(REMOTE_COMMAND) cd /vagrant/; make -j3 _cpimage"
	-vboxmanage unregistervm RK_Test --delete
	-rm $(VDI)
	vboxmanage createvm --name RK_Test --register
	vboxmanage modifyvm RK_Test --cpus 4 --ioapic on --chipset ich9 --hpet on --x2apic on --nic1 nat --nictype1 82540EM --firmware efi
	vboxmanage convertfromraw $(IMAGEFILE) $(VDI)
	vboxmanage storagectl RK_Test --name SATAController --add sata --controller IntelAHCI --bootable on
	vboxmanage storageattach RK_Test --storagectl SATAController --port 0 --device 0 --type hdd --medium disk.vdi
	vboxmanage startvm RK_Test --type gui

vboxkill:
	-vboxmanage controlvm RK_Test poweroff

run_ipxeserver:
	make pxeimg
	@echo info: allow port 8080 in your firewall settings
	@echo info: type following command in iPXE console:
	@echo info: ----
	@echo info: dhcp
	@echo info: kernel http://$(HOST_IP):8080/memdisk
	@echo info: initrd http://$(HOST_IP):8080/$(IMAGEFILE).gz
	@echo info: boot
	@echo info: ----
	cd net; python -m SimpleHTTPServer 8080

ipxeimg:
	$(call make_wrapper, _cpimage)
	gzip $(IMAGEFILE)
	mv $(IMAGEFILE).gz net/

IPXE_SRC_DIR=/home/vagrant/ipxe/src/
IPXE_IMG=bin-x86_64-pcbios/ipxe.usb
IPXE_CFG=/vagrant/load.cfg
ipxe_usb_img:
	sed -e "s/HOST/$(HOST_IP)/g" memdisk.cfg > load.cfg
	 $(call runcmd_wrapper, make -C $(IPXE_SRC_DIR) $(IPXE_IMG) EMBED=$(IPXE_CFG) )
	 $(call runcmd_wrapper, cp $(IPXE_SRC_DIR)$(IPXE_IMG) /vagrant/)

debugqemu:
	$(call make_wrapper, debugqemu)

showerror:
	$(call make_wrapper, showerror)

numerror:
	$(call make_wrapper, numerror)

doc:
	$(call make_wrapper, doc)

.ssh_config:
	vagrant ssh-config > .ssh_config
