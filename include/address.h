#ifndef INC_ADDRESS
#define INC_ADDRESS

#include <netinet/in.h>

#define MAX_HOST_STRLEN   32
#define MAC_SEPARATORS ":.|;"

#if __APPLE__
#define AF_LOCAL_L2 AF_LINK
#elif __linux__
#define AF_LOCAL_L2 AF_PACKET
#endif

#define SCP_INVSCOPE  0x00
#define SCP_NODELOCAL 0x01
#define SCP_LINKLOCAL 0x02
#define SCP_SITELOCAL 0x05
#define SCP_ORGLOBAL  0x08
#define SCP_GLOBAL    0x0e

struct mac_addr {
  unsigned char sl2_addr[6];
};

union Addrunion {
  struct in_addr  in;
  struct in6_addr in6;
  struct mac_addr mac;
  Addrunion(struct in_addr  addr);
  Addrunion(struct in6_addr addr);
  Addrunion(struct mac_addr addr);
};

typedef union Addrunion addrunion_t;

// A base class from which all types of addresses are derived 
class Address {
  // This object represents an address staisfying a given condition
  protected:
    int           family;         // Address family
    addrunion_t   address;        // Binary form of address
    std::string   host;           // Textual representation of address
    //
    Address(struct in_addr   addr);
    Address(struct in6_addr  addr);
    Address(struct mac_addr  addr);
  public:
    virtual std::string print();
    virtual bool        is_multicast();
};

class IPv4Address : public Address {
  protected:
    //struct in_addr address;
    int         service;
    std::string ipv4mapped;
    // The address can be constructed either from the binary representation
    // or from the textual one, whenever provided
  public:
    IPv4Address(struct in_addr addr);
    ~IPv4Address();
    bool        is_multicast();
};

class IPv6Address : public Address {
  protected:
    //struct in6_addr address;
    int         service;
    std::string map4;
    int         scope;
    int         scope_id;
    std::string zone_id;
  public:
    IPv6Address(struct in6_addr addr, int sid=0);
    ~IPv6Address();
    unsigned int get_scope();
    std::string print();
    bool is_multicast();
    bool is_v4mapped();
};

class LinkLayerAddress : public Address {
  protected:
  public:
    LinkLayerAddress(struct mac_addr macb);
    ~LinkLayerAddress();
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
