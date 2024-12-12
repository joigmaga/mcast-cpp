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

int main() {

  logptr_t logger = Logger::get_logger("TGADDR", INFO, STDLOG);

  for (auto ni : get_network_interfaces()) {
    logger->info("interface name: %s, index: %d, flags: 0x%x",
              ni->name.c_str(), ni->index, ni->flags);
    for (auto addr : ni->addrvec) {
      string paddr = addr->print();
      logger->info("  family: %d, address: %s", addr->get_family(), paddr.c_str());
    }
  }

  NetworkInterface* ni = find_interface_address("127.0.0.1");
  if (ni) {
    logger->info("loopback interface is: %s, index: %d, flags: 0x%x",
                     ni->name.c_str(), ni->index, ni->flags);
  }

  return 0;
}

