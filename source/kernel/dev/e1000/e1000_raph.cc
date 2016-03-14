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

#include "e1000_raph.h"
#include "e1000.h"
#include "../pci.h"
#include "../../mem/virtmem.h"

uint16_t pci_get_vendor(device_t dev) {
  return dev->parent->ReadReg<uint16_t>(PCICtrl::kVendorIDReg);
}

uint16_t pci_get_device(device_t dev) {
  return dev->parent->ReadReg<uint16_t>(PCICtrl::kDeviceIDReg);
}

uint16_t pci_get_subvendor(device_t dev) {
  kassert((dev->parent->ReadReg<uint8_t>(PCICtrl::kHeaderTypeReg) & PCICtrl::kHeaderTypeRegMaskDeviceType) == PCICtrl::kHeaderTypeRegValueDeviceTypeNormal);
  return dev->parent->ReadReg<uint16_t>(PCICtrl::kSubVendorIdReg);
}

uint16_t pci_get_subdevice(device_t dev) {
  kassert((dev->parent->ReadReg<uint8_t>(PCICtrl::kHeaderTypeReg) & PCICtrl::kHeaderTypeRegMaskDeviceType) == PCICtrl::kHeaderTypeRegValueDeviceTypeNormal);
  return dev->parent->ReadReg<uint16_t>(PCICtrl::kSubsystemIdReg);
}

struct resource *bus_alloc_resource_from_bar(device_t dev, int bar) {
  struct resource *r = reinterpret_cast<struct resource *>(virtmem_ctrl->Alloc(sizeof(struct resource)));
  uint32_t addr = dev->parent->ReadReg<uint32_t>(static_cast<uint32_t>(bar));
  if ((bar & PCICtrl::kRegBaseAddrFlagIo) != 0) {
    r->type = BUS_SPACE_PIO;
    r->addr = addr & PCICtrl::kRegBaseAddrMaskIoAddr;
  } else {
    r->type = BUS_SPACE_MEMIO;
    r->data.mem.is_prefetchable =  ((bar & PCICtrl::kRegBaseAddrIsPrefetchable) != 0);
    r->addr = addr & PCICtrl::kRegBaseAddrMaskMemAddr;
    if ((bar & PCICtrl::kRegBaseAddrMaskMemType) == PCICtrl::kRegBaseAddrValueMemType64) {
      r->addr |= static_cast<uint64_t>(dev->parent->ReadReg<uint32_t>(static_cast<uint32_t>(bar + 4))) << 32;
    }
  }
  return r;
}