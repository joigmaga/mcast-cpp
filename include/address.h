#ifndef INC_ADDRESS
#define INC_ADDRESS

#include <netinet/in.h>

#include <string>
#include <map>

#define MAX_HOST_STRLEN   32
#define MAC_SEPARATORS ":.|;"

#if __APPLE__
#define AF_LOCAL_L2 AF_LINK
#elif __linux__
#define AF_LOCAL_L2 AF_PACKET
#endif

//std::map<int, std::string> family_map = { {AF_LOCAL_L2, "link layer"},
//                                          {AF_INET, "IPv4"},
//                                          {AF_INET6, "IPv6"} };

// A base class from which all types of addresses are derived 
class Address {
  // This object represents an address staisfying a given condition
  public:
    int           family;
    std::string   printable;
    //
    Address(int fam, const std::string &host);
    virtual std::string print();
};

class IPv4Address : public Address {
  public:
    struct in_addr address;
    int         service;
    std::string ipv4mapped;
    bool        is_multicast();

    // The address can be constructed either from the binary representation
    // or from the textual one, whenever provided
    IPv4Address(struct in_addr addr, const std::string &host=std::string());
};

class IPv6Address : public Address {
  public:
    struct in6_addr address;
    int         service;
    std::string map4;
    int         scope;
    int         scope_id;
    std::string zone_id;

    IPv6Address(struct in6_addr addr,
                const std::string &host=std::string(), int sid=0);
    std::string print();
    int get_scope();
    bool is_multicast();
    bool is_v4mapped();
};

struct mac_addr {
  unsigned char sl2_addr[6];
};

class LinkLayerAddress : public Address {
  public:
    struct mac_addr address;    
    LinkLayerAddress(struct mac_addr &macb,
                     const std::string &host=std::string());
    std::string print();
};

//Address* get_ip_address(const std::string &host, const std::string &service,
//                        int family, int type);
//
//Address* get_mac_address(const std::string &host);
//

Address* get_address(std::string &host,
                     const std::string &service=std::string(),
                     int family=AF_UNSPEC, int type=SOCK_DGRAM);

#endif
