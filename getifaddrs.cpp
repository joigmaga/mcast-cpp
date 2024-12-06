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
#include <netinet/in.h>
#include <ifaddrs.h>

// C++ includes
#include <iostream>
#include <string>
#include <cstring>

// local includes
#include "address.h"
#include "logging.h"

using namespace std;

// logging instance for this module
//
static logptr_t logger = Logger::get_logger("GETIFADD", WARNING, STDLOG);

////// Interface 

void get_network_interfaces(string ifname,
                  unsigned int reqfamily, unsigned int reqscope) {

  int index = 0;
  struct ifaddrs* ifap;
  struct ifaddrs* ifp;

  if (getifaddrs(&ifap) != 0) {
    logger->error("getifaddr error: %s", strerror(errno));
    throw("getifaddrs");
  }

  ifp = ifap;
  while(ifp) {
    Address* addr = nullptr;
    sa_family_t family = ifp->ifa_addr->sa_family;
    logger->warning("**** name: %s, family: %d, flags: 0x%x",
                    ifp->ifa_name, family, ifp->ifa_flags);
    if (ifp->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in* psin = (struct sockaddr_in*) ifp->ifa_addr;
      addr = new IPv4Address(psin->sin_addr);
    }
    else if (ifp->ifa_addr->sa_family == AF_INET6) {
      struct sockaddr_in6* psin6 = (struct sockaddr_in6*) ifp->ifa_addr;
      addr = new IPv6Address(psin6->sin6_addr, index);
    }
    else if (ifp->ifa_addr->sa_family == AF_LOCAL_L2) {
#if __APPLE__
      struct sockaddr_dl* psl2 = (struct sockaddr_dl*) ifp->ifa_addr;
      int nlen  = psl2->sdl_nlen;
      int alen  = psl2->sdl_alen;
      char* pastart = &psl2->sdl_data[nlen];
      index = psl2->sdl_index;
#endif
#if __linux__
      struct sockaddr_ll* psl2 = (struct sockaddr_ll*) ifp->ifa_addr;
      int alen = psl2->sll_halen;
      char* pastart = psl2->sll_addr;
      index = psl2->sll_ifindex;
#endif
      logger->warning("L2, index: %d, addr len: %d", index, alen);
      struct mac_addr maca;
      if (alen == 6) {
        memcpy(&maca.sl2_addr, pastart, alen);
        addr = new LinkLayerAddress(maca);
      }
    }
    if (addr) {
      string host = addr->print();
      logger->warning("  address: %s, index: %d", host.c_str(), index);
    }
 
    ifp = ifp->ifa_next;
  }

  freeifaddrs(ifap);
}

int main() {

  get_network_interfaces("", 0, 0);
}
