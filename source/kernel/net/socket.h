
extern CpuId network_cpu;

enum class EthernetDataType {
  kUnknown,
  kARP,
};

struct EthernetFrame {
 public:
  uint8_t dst_eth_addr[6];
  uint8_t src_eth_addr[6];
  uint8_t type[2];
  uint8_t data[];
  static void PrintEthernetAddr(uint8_t eth_addr[6]) {
    gtty->Printf("%02X:%02X:%02X:%02X:%02X:%02X", eth_addr[0], eth_addr[1],
                 eth_addr[2], eth_addr[3], eth_addr[4], eth_addr[5]);
  }
  EthernetDataType GetEthernetDataType() {
    if (this->type[0] == 0x08 && this->type[1] == 0x06) {
      return EthernetDataType::kARP;
    }
    return EthernetDataType::kUnknown;
  }

 private:
};

struct ArpData {
 public:
  uint8_t htype[2];
  uint8_t ptype[2];
  uint8_t hlen, plen;
  uint8_t oper[2];
  uint8_t src_eth_addr[6];
  uint8_t src_ip_addr[4];
  uint8_t dst_eth_addr[6];
  uint8_t dst_ip_addr[4];
  static void PrintIpAddr(uint8_t ip_addr[4]) {
    gtty->Printf("%d.%d.%d.%d", ip_addr[0], ip_addr[1], ip_addr[2], ip_addr[3]);
  }
  bool IsRequest() { return oper[1] == 0x01; }
  bool IsReply() { return oper[1] == 0x02; }

 private:
};

class SocketCtrl {
 public:
  void AttachAll() {
    auto devices = netdev_ctrl->GetNamesOfAllDevices();
    for (size_t i = 0; i < devices->GetLen(); i++) {
      const char *dev_name = (*devices)[i];
      Attach(dev_name);
    }
  }
  void Attach(const char *dev_name) {
    NetDevCtrl::NetDevInfo *dev_info = netdev_ctrl->GetDeviceInfo(dev_name);
    if (dev_info == nullptr) {
      gtty->Printf("%s: Interface not found.\n", dev_name);
      return;
    }
    auto dev = dev_info->device;
    if (dev->GetStatus() != NetDev::LinkStatus::kUp) {
      gtty->Printf("skip %s (Link Down)\n", dev_name);
      return;
    }
    uint8_t eth_addr[6];
    static_cast<DevEthernet *>(dev)->GetEthAddr(eth_addr);
    gtty->Printf("listening on %s: (%02X-%02X-%02X-%02X-%02X-%02X)\n", dev_name,
                 eth_addr[0], eth_addr[1], eth_addr[2], eth_addr[3],
                 eth_addr[4], eth_addr[5]);
    dev->SetReceiveCallback(
        network_cpu,
        make_uptr(new Function<NetDev *>(
            [](NetDev *eth) {
              NetDev::Packet *rpacket;
              if (!eth->ReceivePacket(rpacket)) {
                return;
              }
              EthernetFrame *eth_data =
                  reinterpret_cast<EthernetFrame *>(rpacket->GetBuffer());
              gtty->Printf("recv\n");
              gtty->Printf("  dst: ");
              EthernetFrame::PrintEthernetAddr(eth_data->dst_eth_addr);
              gtty->Printf("\n");
              gtty->Printf("  src: ");
              EthernetFrame::PrintEthernetAddr(eth_data->src_eth_addr);
              gtty->Printf("\n");
              gtty->Printf("  typ: %x %x\n", eth_data->type[0],
                           eth_data->type[1]);

              EthernetDataType eth_type = eth_data->GetEthernetDataType();
              if (eth_type == EthernetDataType::kARP) {
                ProcessArp(eth, rpacket, eth_data);
              } else {
                gtty->Printf("  Unknown (typ: 0x%x 0x%x)\n", eth_data->type[0],
                             eth_data->type[1]);
              }

              /*
              uint32_t my_addr_int_;
              assert(eth->GetIpv4Address(my_addr_int_));
              uint8_t my_addr[4];
              *(reinterpret_cast<uint32_t *>(my_addr)) =
                  my_addr_int_;
                  */
              // received packet
              /*
              if (rpacket->GetBuffer()[12] == 0x08 &&
                  rpacket->GetBuffer()[13] == 0x06 &&
                  rpacket->GetBuffer()[21] == 0x02) {
                // ARP Reply
                uint32_t target_addr_int =
              (rpacket->GetBuffer()[31] << 24) |
                                           (rpacket->GetBuffer()[30]
              << 16) | (rpacket->GetBuffer()[29] << 8) |
                                           rpacket->GetBuffer()[28];
                arp_table->Set(target_addr_int,
              rpacket->GetBuffer() + 22, eth);
              }
              if (rpacket->GetBuffer()[12] == 0x08 &&
                  rpacket->GetBuffer()[13] == 0x06 &&
                  rpacket->GetBuffer()[21] == 0x01 &&
                  (memcmp(rpacket->GetBuffer() + 38,
              my_addr, 4) == 0)) {
                // ARP Request
                uint8_t data[] = {
                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
              Target MAC Address 0x00, 0x00, 0x00, 0x00,
              0x00, 0x00,  // Source MAC Address 0x08, 0x06,
              // Type: ARP
                    // ARP Packet
                    0x00, 0x01,  // HardwareType: Ethernet
                    0x08, 0x00,  // ProtocolType: IPv4
                    0x06,        // HardwareLength
                    0x04,        // ProtocolLength
                    0x00, 0x02,  // Operation: ARP Reply
                    0x00, 0x00, 0x00, 0x00, 0x00,
                    0x00,                    // Source
              Hardware Address 0x00, 0x00, 0x00, 0x00,  //
              Source Protocol Address 0x00, 0x00, 0x00,
              0x00, 0x00, 0x00,                    // Target
              Hardware Address 0x00, 0x00, 0x00, 0x00,  //
              Target Protocol Address
                };
                memcpy(data, rpacket->GetBuffer() + 6, 6);
                static_cast<DevEthernet
              *>(eth)->GetEthAddr(data + 6); memcpy(data +
              22, data + 6, 6); memcpy(data + 28, my_addr,
              4); memcpy(data + 32, rpacket->GetBuffer() +
              22, 6); memcpy(data + 38, rpacket->GetBuffer()
              + 28, 4);

                uint32_t len = sizeof(data) /
              sizeof(uint8_t); NetDev::Packet *tpacket;
                kassert(eth->GetTxPacket(tpacket));
                memcpy(tpacket->GetBuffer(), data, len);
                tpacket->len = len;
                eth->TransmitPacket(tpacket);
              }
              */
              gtty->Printf("recv\n");
              eth->ReuseRxBuffer(rpacket);
            },
            dev)));
  }

 private:
  static void ProcessArp(NetDev *eth, NetDev::Packet *rpacket,
                         EthernetFrame *eth_data) {
    ArpData *arp_data = reinterpret_cast<ArpData *>(eth_data->data);
    gtty->Printf("  ARP from ");
    ArpData::PrintIpAddr(arp_data->src_ip_addr);
    if (arp_data->IsRequest()) {
      gtty->Printf(" (Request)\n");
      ProcessArpRequest(eth, arp_data);
    } else {
      gtty->Printf(" (Reply)\n");
    }
  }
  static void ProcessArpRequest(NetDev *eth, ArpData *arp_data) {
    uint8_t data[sizeof(EthernetFrame) + sizeof(ArpData)];
    EthernetFrame *packet_eth = reinterpret_cast<EthernetFrame *>(data);
    ArpData *packet_arp = reinterpret_cast<ArpData *>(packet_eth->data);
    /*

    = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                      Target MAC Address 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00,  // Source MAC Address 0x08, 0x06,
                             // Type: ARP
                      // ARP Packet
                      0x00, 0x01,  // HardwareType: Ethernet
                      0x08, 0x00,  // ProtocolType: IPv4
                      0x06,        // HardwareLength
                      0x04,        // ProtocolLength
                      0x00, 0x02,  // Operation: ARP Reply
                      0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00,                                     // Source
                      Hardware Address 0x00, 0x00, 0x00, 0x00,  //
                      Source Protocol Address 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00,                                     // Target
                      Hardware Address 0x00, 0x00, 0x00, 0x00,  //
                      Target Protocol Address};
                      */
    /*
    memcpy(data, rpacket->GetBuffer() + 6, 6);
    static_cast<DevEthernet *>(eth)->GetEthAddr(data + 6);
    memcpy(data + 22, data + 6, 6);
    memcpy(data + 28, my_addr, 4);
    memcpy(data + 32, rpacket->GetBuffer() + 22, 6);
    memcpy(data + 38, rpacket->GetBuffer() + 28, 4);

    uint32_t len = sizeof(data) / sizeof(uint8_t);
    NetDev::Packet *tpacket;
    kassert(eth->GetTxPacket(tpacket));
    memcpy(tpacket->GetBuffer(), data, len);
    tpacket->len = len;
    eth->TransmitPacket(tpacket);
    */
  }
};
