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
 * Author: hikalium
 * 
 */

#include <tty.h>
#include <mem/physmem.h>
#include <string.h>
#include <stdlib.h>
#include <x86.h>
#include <syscall.h>
#include <gdt.h>
#include <global.h>

extern "C" int64_t syscall_handler();
extern size_t syscall_handler_stack; 

extern "C" int64_t syscall_handler_sub(SystemCallCtrl::Args *args, int index) {
  return SystemCallCtrl::Handler(args, index);
}


void SystemCallCtrl::Init() {
  // IA32_EFER.SCE = 1
  const uint64_t bit_efer_SCE = 0x01;
  uint64_t efer = rdmsr(kIA32EFER);
  efer |= bit_efer_SCE;
  wrmsr(kIA32EFER, efer);
  // set vLSTAR
  wrmsr(kIA32STAR, (static_cast<uint64_t>(KERNEL_CS) << 32) | ((static_cast<uint64_t>(USER_DS) - 8) << 48));
  wrmsr(kIA32LSTAR, reinterpret_cast<uint64_t>(syscall_handler));
  wrmsr(kIA32KernelGsBase, KERNEL_DS);
  syscall_handler_stack = KernelStackCtrl::GetCtrl().AllocThreadStack(cpu_ctrl->GetCpuId());
}

int64_t SystemCallCtrl::Handler(Args *args, int index) {
  switch(index) {
  case 2:
    {
      // // open
      // gtty->Printf("<%s>", args->arg1);
      // return 3;
      break;
    }
  case 16:
    {
      // ioctl
      if (args->arg1 == 1) {
        // stdout
        switch (args->arg2) {
        case TCGETS:
          {
            return 0;
          }
        case TIOCGWINSZ:
          {
            winsize *ws = reinterpret_cast<winsize *>(args->arg3);
            ws->ws_row = gtty->GetRow();
            ws->ws_col = gtty->GetColumn();
            ws->ws_xpixel = 0;
            ws->ws_ypixel = 0;
            return 0;
          }
        default:
          {
            gtty->DisablePrint();
            gtty->ErrPrintf("%x\n", args->arg2);
            kernel_panic("Sysctrl", "unknown argument(ioctrl)");
          }
        };
      } else {
        kernel_panic("Sysctrl", "unknown fd(ioctrl)");
      }
      break;
    }
  case 20:
    // writev
    {
      if (args->arg1 == 1) {
        // stdout
        iovec *iv_array = reinterpret_cast<iovec *>(args->arg2);
        int rval = 0;
        for (int i = 0; i < args->arg3; i++) {
          for (int j = 0; j < iv_array[i].iov_len; j++) {
            gtty->Printf("%c", reinterpret_cast<char *>(iv_array[i].iov_base)[j]);
            rval++;
          }
        }
        gtty->Flush();
        return rval;
      } else {
        gtty->DisablePrint();
        gtty->ErrPrintf("<%llx>", args->arg1);
        kernel_panic("Sysctrl", "unknown fd(writev)");
      }
      break;
    }
  case 63:
    // uname
    {
#define __NEW_UTS_LEN 64
      struct new_utsname {
        char sysname[__NEW_UTS_LEN + 1];
        char nodename[__NEW_UTS_LEN + 1];
        char release[__NEW_UTS_LEN + 1];
        char version[__NEW_UTS_LEN + 1];
        char machine[__NEW_UTS_LEN + 1];
        char domainname[__NEW_UTS_LEN + 1];
      };
      new_utsname tmp;
      strcpy(tmp.sysname, "");
      strcpy(tmp.nodename, "");
      strcpy(tmp.release, "");
      strcpy(tmp.version, "");
      strcpy(tmp.machine, "");
      strcpy(tmp.domainname, "");
      memcpy(reinterpret_cast<void *>(args->arg1), &tmp, sizeof(new_utsname));
      return 0;
    }
  case 158:
    {
      gtty->Printf("sys_arch_prctl code=%llx addr=%llx\n", args->arg1, args->arg2);
      switch(args->arg1){
      case kArchSetGs:
        wrmsr(kIA32GsBase, args->arg2);
        break;
      case kArchSetFs:
        wrmsr(kIA32FsBase, args->arg2);
        break;
      case kArchGetFs:
        *reinterpret_cast<uint64_t *>(args->arg2) = rdmsr(kIA32FsBase);
        break;
      case kArchGetGs:
        *reinterpret_cast<uint64_t *>(args->arg2) = rdmsr(kIA32GsBase);
        break;
      default:
        gtty->DisablePrint();
        gtty->ErrPrintf("Invalid args\n", index);
        kassert(false);
      }
      return 0;
    }
  case 231:
    {
      // exit group
      gtty->Printf("user program called exit\n");
      gtty->Flush();
      while(true) {asm volatile("hlt;");}
    }
  }
  gtty->DisablePrint();
  gtty->ErrPrintf("syscall(%d) (return address %llx)\n", index, args->raddr);
  kernel_panic("Syscall", "unimplemented systemcall");
}

