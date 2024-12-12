#ifndef INC_GETIFADDRS
#define INC_GETIFADDRS

#include <sys/types.h>
#include <netinet/in.h>

#include <vector>

#include "address.h"

class NetworkInterface {
  public:
    std::string  name;
    unsigned int index;
    unsigned int flags;
    std::vector<Address*> addrvec;
    NetworkInterface(std::string name, unsigned int index, unsigned int flags);
};

std::vector<NetworkInterface*>  get_network_interfaces(std::string ifname="",
                                      unsigned int reqfamily=AF_UNSPEC,
                                      unsigned int reqscope=SCP_UNSPEC);

NetworkInterface* find_interface_address(std::string address);

NetworkInterface* find_interface(std::string name,   std::vector<NetworkInterface*> nvec);
NetworkInterface* find_interface(unsigned int index, std::vector<NetworkInterface*> nvec);

#endif

