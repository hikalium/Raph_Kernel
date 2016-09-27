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

#include "ff.h"
#include "diskio.h"
#include "fat.h"
#include <raph.h>
#include <tty.h>
#include <global.h>

extern "C" {
  DSTATUS disk_initialize (BYTE pdrv) {
    FatFs::InitializeDisk();
    return 0;
  }
  DSTATUS disk_status (BYTE pdrv) {
    kassert(false);
  }
  DRESULT disk_read (BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    kassert(false);
  }
  DRESULT disk_write (BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    kassert(false);
  }
  DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void* buff) {
    kassert(false);
  }
}