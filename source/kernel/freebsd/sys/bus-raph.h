/*
 *
 * Copyright (c) 2016 Raphine Project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Author: Liva
 * 
 */

#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/types-raph.h>
#include <sys/rman.h>
#include <raph.h>
#include <task.h>
#include <idt.h>
#include <apic.h>
#include <global.h>
#include <dev/pci.h>

#ifndef __RAPH_KERNEL_FREEBSD_SYS_BUS_RAPH_H__
#define __RAPH_KERNEL_FREEBSD_SYS_BUS_RAPH_H__

class BsdDevBus : public BsdDevice {
public:
  BsdDevBus(BsdDevice *parent, const char *name_, int unit_) : BsdDevice(parent, name_, unit_) {
    SetBusClass(this);
  }
  BsdDevBus() {
    SetBusClass(this);
  }
  virtual int DevMethodBusProbe() = 0;
  virtual int DevMethodBusAttach() = 0;
  virtual int DevMethodBusSetupIntr(struct resource *r, int flags, driver_filter_t filter, driver_intr_t ithread, void *arg, void **cookiep) {
    kernel_panic("bus", "no method\n");
    return -1;
  }
  virtual struct resource *DevMethodBusAllocResource(int type, int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags) {
    kernel_panic("bus", "no method\n");
    return nullptr;
  }
  virtual int DevMethodBusReleaseResource(int type, int rid, struct resource *r) {
    kernel_panic("bus", "no method\n");
    return -1;
  }
  virtual int DevMethodProbe() override final {
    if (DevMethodBusProbe() == BUS_PROBE_DEFAULT) {
      return 0;
    } else {
      return -1;
    }
  }
  virtual int DevMethodAttach() override final {
    return DevMethodBusAttach();
  }
private:
};

class BsdDevPci : public BsdDevBus {
public:
  class IntContainer {
  public:
    IntContainer() {
      _ctask.SetFunc(cpu_ctrl->RetainCpuIdForPurpose(CpuPurpose::kLowPriority), make_uptr(new ClassFunction<IntContainer, void *>(this, &IntContainer::HandleSub, nullptr)));
    }
    void SetVector(int vector) {
      _vector = vector;
    }
    void Handle() {
      if (_filter != nullptr) {
        _filter(_farg);
        apic_ctrl->SendEoi(_vector);
      }
      if (_ithread != nullptr) {
        apic_ctrl->MaskInt(_vector, true);
        _ctask.Inc();
      }
    }
    void SetFilter(driver_filter_t filter, void *arg) {
      _filter = filter;
      _farg = arg;
    }
    void SetIthread(driver_intr_t ithread, void *arg) {
      _ithread = ithread;
      _iarg = arg;
    }
  private:
    void HandleSub(void *) {
      if (_ithread != nullptr) {
        _ithread(_iarg);
        if (_ctask.GetCnt() == 0) {
          apic_ctrl->MaskInt(_vector, false);
        }
      }
    }
    CountableTask _ctask;
    
    driver_filter_t _filter = nullptr;
    void *_farg;
    
    driver_intr_t _ithread = nullptr;
    void *_iarg;

    int _vector = -1;
  };
  BsdDevPci(uint8_t bus, uint8_t device, uint8_t function) : _pci(bus, device, function, this) {
    SetPciClass(this);
  }
  virtual ~BsdDevPci() {
  }
  IntContainer *GetIntContainerStruct(int id) {
    if (_is_legacy_interrupt_enable) {
      if (id == 0) {
        if (_icontainer_list == nullptr) {
          SetupLegacyIntContainers();
        }
        return &_icontainer_list[id];
      } else {
        return NULL;
      }
    } else {
      if (id <= 0) {
        return NULL;
      } else {
        return &_icontainer_list[id - 1];
      }
    }
  }
  void SetupMsi() {
    ReleaseMsi();
    int count = _pci.GetMsiCount();
    if (count == 0) {
      return;
    }
    _icontainer_list = new IntContainer[count];
    int_callback callbacks[count];
    void *args[count];
    for (int i = 0; i < count; i++) {
      callbacks[i] = HandleSubInt;
      args[i] = reinterpret_cast<void *>(_icontainer_list + i);
    }
    _is_legacy_interrupt_enable = false;
    CpuId cpuid = cpu_ctrl->RetainCpuIdForPurpose(CpuPurpose::kLowPriority);
    int vector = idt->SetIntCallback(cpuid, callbacks, args, count, Idt::EoiType::kLapic);
    for (int i = 0; i < count; i++) {
      _icontainer_list[i].SetVector(vector + i);
    }
    _pci.SetMsi(cpuid, vector); // TODO it will not work if count >= 2
    assert(count == 1); // to notice this issue
  }
  void ReleaseMsi() {
    if (_icontainer_list != nullptr) {
      // TODO 割り込み開放
      delete[] _icontainer_list;
    }
  }
  DevPci &GetDevPci() {
    return _pci;
  }
private:
  virtual int DevMethodBusSetupIntr(struct resource *r, int flags, driver_filter_t filter, driver_intr_t ithread, void *arg, void **cookiep) override {
    IntContainer *icontainer = GetIntContainerStruct(r->r_bushandle);
    if (icontainer == NULL) {
      return -1;
    }
    icontainer->SetFilter(filter, arg);
    icontainer->SetIthread(ithread, arg);
    return 0;
  }
  virtual struct resource *DevMethodBusAllocResource(int type, int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags) {
    struct resource *r;
    switch(type) {
    case SYS_RES_MEMORY: {
      int bar = *rid;
      uint32_t addr = GetDevPci().ReadPciReg<uint32_t>(static_cast<uint32_t>(bar));
      if ((addr & PciCtrl::kRegBaseAddrFlagIo) != 0) {
        return NULL;
      }
      r = rman_reserve_resource(nullptr, 0, ~0, 1, 0, this);
      r->r_bustag = BUS_SPACE_MEMIO;
      r->r_bushandle = addr & PciCtrl::kRegBaseAddrMaskMemAddr;

      if ((addr & PciCtrl::kRegBaseAddrMaskMemType) == PciCtrl::kRegBaseAddrValueMemType64) {
        r->r_bushandle |= static_cast<uint64_t>(GetDevPci().ReadPciReg<uint32_t>(static_cast<uint32_t>(bar + 4))) << 32;
      }
      r->r_bushandle = p2v(r->r_bushandle);
      break;
    }
    case SYS_RES_IOPORT: {
      int bar = *rid;
      uint32_t addr = GetDevPci().ReadPciReg<uint32_t>(static_cast<uint32_t>(bar));
      if ((addr & PciCtrl::kRegBaseAddrFlagIo) == 0) {
        return NULL;
      }
      r = rman_reserve_resource(nullptr, 0, ~0, 1, 0, this);
      r->r_bustag = BUS_SPACE_PIO;
      r->r_bushandle = addr & PciCtrl::kRegBaseAddrMaskIoAddr;
      break;
    }
    case SYS_RES_IRQ: {
      r = rman_reserve_resource(nullptr, 0, ~0, 1, 0, this);
      r->r_bushandle = *rid;
      break;
    }
    default: {
      kassert(false);
    }
    }
    return r;
  }
  virtual int DevMethodBusReleaseResource(int type, int rid, struct resource *r) override {
    // TODO
    return rman_release_resource(r);
  }
  void SetupLegacyIntContainers() {
    _icontainer_list = new IntContainer[1];
    int vector = _pci.SetLegacyInterrupt(HandleSubLegacy, reinterpret_cast<void *>(_icontainer_list), Idt::EoiType::kLapic);
    _icontainer_list[0].SetVector(vector);
  }
  static void HandleSubLegacy(void *arg) {
    IntContainer *icontainer = reinterpret_cast<IntContainer *>(arg);
    icontainer->Handle();
  }
  static void HandleSubInt(Regs *rs, void *arg) {
    IntContainer *icontainer = reinterpret_cast<IntContainer *>(arg);
    icontainer->Handle();
  }

  class BsdDevPciSub : public DevPci {
  public:
    BsdDevPciSub(uint8_t bus, uint8_t device, uint8_t function, BsdDevPci *master) : DevPci(bus, device, function), _master(master) {
    }
    virtual void Attach() override {
      _master->DevMethodAttach();
    }
  private:
    BsdDevPci *const _master;
  } _pci;
  bool _is_legacy_interrupt_enable = true;
  IntContainer *_icontainer_list = nullptr;
};

#endif // __RAPH_KERNEL_FREEBSD_SYS_BUS_RAPH_H__
