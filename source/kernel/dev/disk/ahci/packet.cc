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

#include "ahci-raph.h"
#include <tty.h>
#include <global.h>

PacketAtaioCtrl PacketAtaioCtrl::_ctrl;
bool PacketAtaioCtrl::_is_initailized = false;

void PacketAtaio::XptDone() {
  if ((func_code & XPT_FC_QUEUED) == 0) {
    return;
  }
  for(int i = 0; i < 512; i++) {
    //gtty->CprintfRaw("%x ", data_ptr[i]);
  }
  _channel->doneq.Push(this);
}

