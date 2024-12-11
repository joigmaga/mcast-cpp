// C includes
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#if __APPLE__
#include <net/if_dl.h>
#endif
#if __linux__
#include <linux/if_packet.h>
#endif
#include <netinet/in.h>
#include <ifaddrs.h>

// C++ includes
#include <iostream>
#include <string>
#include <cstring>
#include <vector>

// local includes
#include "getifaddrs.h"
#include "address.h"
#include "logging.h"

using namespace std;

// logging instance for this module
//
static logptr_t logger = Logger::get_logger("GETIFADD", WARNING, STDLOG);

////// Interface 

NetworkInterface::NetworkInterface(string name, unsigned int index,
          unsigned int flags) : name(name), index(index), flags(flags) {};

NetworkInterface* find_interface_address(string address) {

  Address* addr = get_address(address);
  if (not addr)
    return nullptr;

  for (auto ni : get_network_interfaces())
    for (auto ad : ni->addrvec) {
      string p1 = ad->print();
      string p2 = addr->print();
      logger->warning("---> comparing %s to %s", p1.c_str(), p2.c_str());
      if (*ad == *addr) {
        logger->warning("match for %s in %s", p1.c_str(), ni->name.c_str());
        delete addr;
        return ni;
      }
    }
  delete addr;
  return nullptr;
}

// Make a function template here
NetworkInterface* find_interface(string name,
                                 vector<NetworkInterface*> nvec) {

  for(auto it=nvec.begin(); it!=nvec.end(); it++) {
    if ((*it)->name == name)
      return *it;
  }

  return nullptr;
} 

NetworkInterface* find_interface(unsigned int index,
                                 vector<NetworkInterface*> nvec) {

  for(auto it=nvec.begin(); it!=nvec.end(); it++) {
    if ((*it)->index == index)
      return *it;
  }

  return nullptr;
} 

vector<NetworkInterface*>  get_network_interfaces(string ifname,
                       unsigned int reqfamily, unsigned int reqscope) {
  int index = 0;
  vector<NetworkInterface*> namevec = {};
  struct ifaddrs* ifap;
  struct ifaddrs* ifp;

  if (getifaddrs(&ifap) != 0) {
    logger->error("getifaddr error: %s", strerror(errno));
    throw("getifaddrs");
  }

  for (ifp=ifap; ifp; ifp=ifp->ifa_next) {
    NetworkInterface* ni = nullptr;

    // select name
    string name        = ifp->ifa_name;
    unsigned int flags = ifp->ifa_flags;
    logger->debug("name: %s, flags: 0x%x", ifp->ifa_name, flags);
    if (ifname.size() > 0 and name != ifname)
      continue;

    // If interface does not have a L2 address, the 'addr' field is NULL
    if (not ifp->ifa_addr) {
      logger->debug("  *** empty addr field");
      continue;
    }

    sa_family_t family = ifp->ifa_addr->sa_family;
    logger->debug("  family: %d", family);

    // check if interface name is already in list
    ni = find_interface(name, namevec);
    if (not ni)
      index = 0;

    // we need to know the interface index at this point, so all this thing
    // relies on having the L2 info upfront 
    int alen = 0;
#if __APPLE__
    char* pastart = nullptr;
#endif
#if __linux__
    unsigned char* pastart = nullptr;
#endif
    // If this is a L2 address record then we are fine
    if (family == AF_LOCAL_L2) {
#if __APPLE__
      auto psl2 = (struct sockaddr_dl*) ifp->ifa_addr;
      alen = psl2->sdl_alen;
      pastart = &psl2->sdl_data[psl2->sdl_nlen];
      index = psl2->sdl_index;
#endif
#if __linux__
      auto psl2 = (struct sockaddr_ll*) ifp->ifa_addr;
      alen = psl2->sll_halen;
      pastart = psl2->sll_addr;
      index = psl2->sll_ifindex;
#endif
      logger->debug("  index: %d", index);
    }

    if (not ni) {
      if (index == 0) {
        logger->debug("  *** could not find index. L2 address expected");
        continue;
      }
      ni = new NetworkInterface(name, index, flags);
      logger->debug("  created network interface %s", ifp->ifa_name);
      namevec.push_back(ni);
    }

    // select family
    if (reqfamily != AF_UNSPEC and family != reqfamily)
      continue;

    // select address scope (IPv6 addresses)
    if (reqscope != SCP_UNSPEC and family != AF_INET6)
      continue;

    Address* addr = nullptr;

    if (ifp->ifa_addr->sa_family == AF_INET) {
      auto psin = (struct sockaddr_in*) ifp->ifa_addr;
      addr = new IPv4Address(psin->sin_addr);
    }
    else if (ifp->ifa_addr->sa_family == AF_INET6) {
      auto psin6 = (struct sockaddr_in6*) ifp->ifa_addr;
      addr = new IPv6Address(psin6->sin6_addr, index);
      unsigned int scope = ((IPv6Address*) addr)->get_scope();
      if ((reqscope != SCP_UNSPEC) and (scope != reqscope)) {
        delete addr;
        continue;
      }
    }
    else if (ifp->ifa_addr->sa_family == AF_LOCAL_L2) {
      struct mac_addr maca;
      if (alen == 6) {
        memcpy(&maca.sl2_addr, pastart, alen);
        addr = new LinkLayerAddress(maca);
      }
    }

    if (addr) {
      string paddr = addr->print();
      if (ni) {
        ni->addrvec.push_back(addr);
        logger->debug("  created address: %s", paddr.c_str());
      }
      else {
        logger->debug("  error creating container for addr: %s", paddr.c_str());
        delete addr;
      }
    }
  }

  freeifaddrs(ifap);

  return namevec;
}
