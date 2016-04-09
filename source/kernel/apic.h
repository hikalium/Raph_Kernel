/*
 *
 * Copyright (c) 2015 Raphine Project
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

#ifndef __RAPH_KERNEL_APIC_H__
#define __RAPH_KERNEL_APIC_H__

#include <stdint.h>
#include <raph.h>
#include <acpi.h>

struct MADT {
  ACPISDTHeader header;
  uint32_t lapicCtrlAddr;
  uint32_t flags;
  uint8_t table[0];
} __attribute__ ((packed));

enum class MADTStType : uint8_t {
  kLocalAPIC = 0,
  kIOAPIC = 1,
};

struct MADTSt {
 MADTStType type;
  uint8_t length;
} __attribute__ ((packed));

// see acpi spec
struct MADTStLAPIC {
  MADTSt st;
  uint8_t pid;
  uint8_t apicId;
  uint32_t flags;
} __attribute__ ((packed));

// see acpi spec
struct MADTStIOAPIC {
  MADTSt st;
  uint8_t ioapicId;
  uint8_t reserved;
  uint32_t ioapicAddr;
  uint32_t glblIntBase;
} __attribute__ ((packed));

class Regs;

class ApicCtrl {
public:
  ApicCtrl() {
  }
  void Setup();
  void SetMADT(MADT *table) {
    _madt = table;
  }
  void StartAPs();
  void BootBSP() {
    _lapic.Setup();
  }
  void BootAP() {
    _started = true;
    _lapic.Setup();
  }
  static constexpr int lapicMaxNumber = 128;
  volatile uint8_t GetApicId() {
    return _lapic.GetApicId();
  }
  volatile bool IsBootupAll() {
    return _all_bootup;
  }
  int GetHowManyCpus() {
    return _lapic._ncpu;
  }
  void SetupPicInt(uint32_t irq){
    _pic.SetupInt(irq);
  }
  bool SetupIoInt(uint32_t irq, uint8_t lapicid, uint8_t vector) {
    kassert(vector >= 32);
    return _ioapic.SetupInt(irq, lapicid, vector);
  }
  void SendEoi() {
    _lapic.SendEoi();
  }
  void SendPicEoi() {
    _pic.SendEoi();
  }
  void SendIpi(uint8_t destid) {
    _lapic.SendIpi(destid);
  }
  void SetupTimer(uint32_t irq) {
    _lapic.SetupTimer(irq);
  }
  static const int kIrq0 = 0x20;
private:
  static void TmrCallback(Regs *rs) {
  }
  static void IpiCallback(Regs *rs) {
  }
  class Lapic {
  public:
    // setup local APIC respond to specified index
    void Setup();
    void SetCtrlAddr(uint32_t *ctrlAddr) {
      if (_ctrlAddr == nullptr) {
        _ctrlAddr = ctrlAddr;
      } else {
        kassert(_ctrlAddr == ctrlAddr);
      }
    }
    int Cpunum() {
      if(_ctrlAddr == nullptr) {
        return 0;
      }
      for(int n = 0; n < _ncpu; n++) {
        if(_ctrlAddr[kRegId] >> 24 == _apicIds[n])
          return n;
      }
      return 0;
    }
    // start local APIC respond to specified index with apicId
    void Start(uint8_t apicId, uint64_t entryPoint);
    int _ncpu;
    uint8_t _apicIds[lapicMaxNumber];

    volatile uint8_t GetApicId() {
      return _ctrlAddr[kRegId] >> 24;
    }
    void SendEoi() {
      _ctrlAddr[kRegEoi] = 0;
    }
    void SendIpi(uint8_t destid);
    void SetupTimer(uint32_t irq);
  private:
    volatile uint32_t *_ctrlAddr = nullptr;
    static const int kIa32ApicBaseMsr = 0x1B;
    static const uint32_t kApicGlobalEnableFlag = 1 << 11;
    // see intel64 manual vol3 Table 10-1 (Local APIC Register Address Map)
    static const int kRegId = 0x20 / sizeof(uint32_t);
    static const int kRegEoi = 0xB0 / sizeof(uint32_t);
    static const int kRegSvr = 0xF0 / sizeof(uint32_t);
    static const int kRegIcrLo = 0x300 / sizeof(uint32_t);
    static const int kRegIcrHi = 0x310 / sizeof(uint32_t);
    static const int kRegLvtCmci = 0x2F0 / sizeof(uint32_t);
    static const int kRegLvtTimer = 0x320 / sizeof(uint32_t);
    static const int kRegLvtThermalSensor = 0x330 / sizeof(uint32_t);
    static const int kRegLvtPerformanceCnt = 0x340 / sizeof(uint32_t);
    static const int kRegLvtLint0 = 0x350 / sizeof(uint32_t);
    static const int kRegLvtLint1 = 0x360 / sizeof(uint32_t);
    static const int kRegLvtErr = 0x370 / sizeof(uint32_t);
    static const int kRegTimerInitCnt = 0x380 / sizeof(uint32_t);
    static const int kRegTimerCurCnt = 0x390 / sizeof(uint32_t);
    static const int kRegDivConfig = 0x3E0 / sizeof(uint32_t);

    // see intel64 manual vol3 Figure 10-10 (Divide Configuration Register)
    static const uint32_t kDivVal1 = 0xB;
    static const uint32_t kDivVal16 = 0x3;

    // see intel64 manual vol3 Figure 10-8 (Local Vector Table)
    static const int kRegLvtMask = 1 << 16;
    static const int kRegTimerPeriodic = 1 << 17;

    // see intel64 manual vol3 Figure 10-23 (Spurious-Interrupt Vector Register)
    static const uint32_t kRegSvrApicEnableFlag = 1 << 8;

    // see intel64 manual vol3 10.5.1 (Delivery Mode)
    static const uint32_t kDeliverModeFixed   = 0 << 8;
    static const uint32_t kDeliverModeLowest  = 1 << 8;
    static const uint32_t kDeliverModeSmi     = 2 << 8;
    static const uint32_t kDeliverModeNmi     = 4 << 8;
    static const uint32_t kDeliverModeInit    = 5 << 8;
    static const uint32_t kDeliverModeStartup = 6 << 8;

    // see intel64 manual vol3 Figure 10-12 (Interrupt Command Register)
    static const uint32_t kRegIcrLevelAssert   = 1 << 14;
    static const uint32_t kRegIcrTriggerModeEdge  = 0 << 15;
    static const uint32_t kRegIcrTriggerModeLevel = 1 << 15;
    static const uint32_t kRegIcrDestShorthandNoShortHand    = 0 << 18;
    static const uint32_t kRegIcrDestShorthandSelf           = 1 << 18;
    static const uint32_t kRegIcrDestShorthandAllIncludeSelf = 2 << 18;
    static const uint32_t kRegIcrDestShorthandAllExcludeSelf = 3 << 18;

    // see intel MPspec Appendix B.5
    static const uint32_t kIoRtc = 0x70;

    // write to 64-bit value to local APIC ICR (Interrupt Command Register)
    void WriteIcr(uint32_t hi, uint32_t lo);
    void Outb(int pin, uint8_t data) {
      asm volatile("outb %%al, %%dx"::"d"(pin), "a"(data));
    }
  } _lapic;
  class Ioapic {
  public:
    void Setup();
    uint32_t Read(uint32_t index) {
      _reg[kIndex] = index;
      return _reg[kData];
    }
    void Write(uint32_t index, uint32_t data) {
      _reg[kIndex] = index;
      _reg[kData] = data;
    }
    void SetReg(uint32_t *reg) {
      _reg = reg;
    }
    bool SetupInt(uint32_t irq, uint8_t lapicid, uint8_t vector) {
      kassert(irq <= this->GetMaxIntr());
      if ((Read(kRegRedTbl + 2 * irq) | kRegRedTblFlagMask) == 0) {
        return false;
      }
      Write(kRegRedTbl + 2 * irq,
            kRegRedTblFlagValueDeliveryLow |
            kRegRedTblFlagDestModePhys |
            kRegRedTblFlagTriggerModeEdge |
            vector);
      Write(kRegRedTbl + 2 * irq + 1, lapicid << kRegRedTblOffsetDest);
      return true;
    }
  private:
    uint32_t GetMaxIntr() {
      // see IOAPIC manual 3.2.2 (IOAPIC Version Register)
      return (Read(kRegVer) >> 16) & 0xff;
    }
    uint32_t *_reg = nullptr;
    
    // see IOAPIC manual 3.1 (Memory Mapped Registers for Accessing IOAPIC Registers)
    static const int kIndex = 0x0;
    static const int kData = 0x10 / sizeof(uint32_t);

    // see IOAPIC manual 3.2 (IOAPIC Registers)
    static const uint32_t kRegVer = 0x1;
    static const uint32_t kRegRedTbl = 0x10;

    // see IOAPIC manual 3.2.4 (I/O Redirection Table Registers)
    static const uint32_t kRegRedTblFlagValueDeliveryLow = 1 << 8;
    static const uint32_t kRegRedTblFlagDestModePhys = 0 << 11;
    static const uint32_t kRegRedTblFlagDestModeLogical = 1 << 11;
    static const uint32_t kRegRedTblFlagTriggerModeEdge = 0 << 15;
    static const uint32_t kRegRedTblFlagTriggerModeLevel = 1 << 15;
    static const uint32_t kRegRedTblFlagMask = 1 << 16;
    static const int kRegRedTblOffsetDest = 24;
    } _ioapic;
  class Pic {
  public:
    void Setup();
    void SetupInt(int irq);
    void SendEoi();
  private:
    uint16_t _irqMask = 0xFFFF; //the initial value masks all the irqs.
    static const int kIopicMaster = 0x20;
    static const int kIopicSlave = 0xA0;
    static const int kIrqSlave = 2;
    static const int MasterCommand = kIopicMaster;
    static const int MasterStatus = kIopicMaster;
    static const int MasterMask = kIopicMaster + 1;
    static const int MasterData = kIopicMaster + 1;
    static const int SlaveCommand = kIopicSlave;
    static const int SlaveStatus = kIopicSlave;
    static const int SlaveMask = kIopicSlave + 1;
    static const int SlaveData = kIopicSlave + 1;

    //see 8259A data sheet
    static const uint8_t kEOI = 1 << 5;
  }_pic;
  MADT *_madt = nullptr;
  static const uint32_t kMadtFlagLapicEnable = 1;
  volatile bool _started = false;
  volatile bool _all_bootup = false;
};

#endif /* __RAPH_KERNEL_APIC_H__ */
