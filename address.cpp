/*
A multicast interface to the socket library 

  Address library functions

    Ignacio Martinez (igmartin@movistar.es)
    January 2023

*/

// C includes
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

// C++ includes
#include <iostream>
#include <string>
#include <cstring>
#include <map>

// local includes
#include "address.h"
#include "logging.h"

using namespace std;

// logging instance for this module
//
static logptr_t logger = Logger::get_logger("ADDRESS", WARNING, STDLOG);

// map describing the address families used in this module
//
static std::map<int, std::string> family_map = { {AF_LOCAL_L2, "link layer"},
                                                 {AF_INET,     "IPv4"},
                                                 {AF_INET6,    "IPv6"} };

// Address union packs together the address variants we use in each case
// Constructors
Addrunion::Addrunion(struct in_addr  addr) : in(addr)  {}
Addrunion::Addrunion(struct in6_addr addr) : in6(addr) {}
Addrunion::Addrunion(struct mac_addr addr) : mac(addr) {}

//// Address base class. Constructor and methods
//
Address::Address(struct in_addr addr)  : address(addr), family(AF_INET),     host("")  {}
Address::Address(struct in6_addr addr) : address(addr), family(AF_INET6),    host("")  {}
Address::Address(struct mac_addr addr) : address(addr), family(AF_LOCAL_L2), host("")  {}

// Destructor
Address::~Address() {}

sa_family_t Address::get_family() const {
  return family;
}

// These are virtual functions to be overriden in derived classes
//
bool Address::operator==(const Address& other) const {
  cout << "comparing gets at Address base" << endl;
  return false;
}

bool Address::is_multicast() {
  return false;
}

string Address::print() {
  return host;
}

void* Address::get_source() const {
  return nullptr;
}

////////   IPv4Address is a derived class from Address
//
// Constructor takes the binary form of the address and generates a
// canonical form of the textual representation
// This is required because user-provided text addresses may be incomplete
// or ambiguous (e.g  '::', '192.1', '1:2:3:a:b:c') 
//
IPv4Address::IPv4Address(struct in_addr addr) : Address(addr) {
  char buff[64];

  if (inet_ntop(AF_INET, &address.in, buff, sizeof(buff))) {
    host = string(buff);
    ipv4mapped = "::ffff:" + host;
  }
  else {
    logger->critical("Invalid address for family AF_INET");
    throw("Configured address has no textual representation. Aborting");
  }
}

// destructor
IPv4Address::~IPv4Address() {}

void* IPv4Address::get_source() const {
  return (void *) &address.in;
}

bool IPv4Address::operator==(const Address& other) const {

  cout << "comparing gets at IPv4 Address" << endl;

  auto inaddr = (struct in_addr*) other.get_source();

  return (other.get_family() == AF_INET) and (inaddr->s_addr == address.in.s_addr);
}

bool IPv4Address::is_multicast() {

  return IN_MULTICAST(address.in.s_addr);
}

////////   IPv6Address is a derived class from Address
//
IPv6Address::IPv6Address(struct in6_addr addr, int sid) :
                Address(addr), scope_id(sid) {
  char buff[64];

  if (inet_ntop(AF_INET6, &address.in6, buff, sizeof(buff))) {
    host = string(buff);
  }
  else {
    logger->critical("Invalid address for family AF_INET6");
    throw("Configured address has no textual representation. Aborting");
  }
}

// destructor
IPv6Address::~IPv6Address() {}

void* IPv6Address::get_source() const {
  return (void *) &address.in6;
}

bool IPv6Address::operator==(const Address& other) const {

  cout << "comparing gets at IPv6 Address" << endl;

  if (other.get_family() != AF_INET6)
    return false;

  auto in6addr = (struct in6_addr*) other.get_source();

  for (int i=0; i<16; i++) {
     if (in6addr->s6_addr[i] != address.in6.s6_addr[i])
       return false;
  }

  return true;
}

bool IPv6Address::is_multicast() {

  // check if IN6_IS_ADDR_V4MAPPED(&address) and v4 address is multicast
  if (IN6_IS_ADDR_V4MAPPED(&address.in6)) {
    in_addr_t v4addr = ntohl(*(const in_addr_t *) &address.in6.s6_addr[12]);
    return IN_MULTICAST(v4addr);
  }

  return IN6_IS_ADDR_MULTICAST(&address.in6);
}

string IPv6Address::print() {

  if (scope_id > 0) {
    unsigned int scope = get_scope();

    if (not IN6_IS_ADDR_UNSPECIFIED(&address.in6) and
        not IN6_IS_ADDR_LOOPBACK(&address.in6)    and
        not (scope == SCP_GLOBAL)) {

      char ifname[IFNAMSIZ];

      if (if_indextoname(scope_id, ifname))
        return host + "%" + string(ifname);
    }
  }

  return host;
}

unsigned int IPv6Address::get_scope() {

  if (IN6_IS_ADDR_UNSPECIFIED(&address.in6))
    return SCP_INVSCOPE;

  if (IN6_IS_ADDR_LOOPBACK(&address.in6) or IN6_IS_ADDR_LINKLOCAL(&address.in6))
    return SCP_LINKLOCAL;

  if IN6_IS_ADDR_MULTICAST(&address.in6)
    return (unsigned int) address.in6.s6_addr[1] & 0x0f;

  return SCP_GLOBAL;
}

//// Link layer address
//
LinkLayerAddress::LinkLayerAddress(mac_addr addr): Address(addr) {
  char buff[32];
  
  if (snprintf(buff, sizeof(buff), "%02x:%02x:%02x:%02x:%02x:%02x",
                 address.mac.sl2_addr[0], address.mac.sl2_addr[1],
                 address.mac.sl2_addr[2], address.mac.sl2_addr[3],
                 address.mac.sl2_addr[4], address.mac.sl2_addr[5])  < 0) {
    logger->critical("Invalid address for Link Layer address family");
    throw("Configured address has no textual representation. Aborting");
  }

  host = string(buff);
}

// destructor
LinkLayerAddress::~LinkLayerAddress() {}

void* LinkLayerAddress::get_source() const {
  return (void *) &address.mac;
}

bool LinkLayerAddress::operator==(const Address& other) const {

  if (other.get_family() != AF_LOCAL_L2)
    return false;

  auto macaddr = (struct mac_addr*) other.get_source();

  for (int i=0; i<6; i++) {
     if (macaddr->sl2_addr[i] != address.mac.sl2_addr[i])
       return false;
  }

  return true;
}

// Factory functions to create addresses based on textual representation
//
Address* get_ip_address(const string& host, const string& service,
                        int family, int type) {
  int  res;

  struct addrinfo   aih;
  struct addrinfo*  pai;
  struct sockaddr_in*  psin;
  struct sockaddr_in6* psin6;
  Address* addr;
  
  aih.ai_flags     = AI_NUMERICHOST;
  aih.ai_family    = family;
  aih.ai_socktype  = SOCK_DGRAM;
  aih.ai_protocol  = 0;
  aih.ai_addrlen   = 0;
  aih.ai_addr      = nullptr;
  aih.ai_canonname = nullptr;
  aih.ai_next      = nullptr;
  
  res = getaddrinfo(host.c_str(),
                    service.empty() ? nullptr : service.c_str(), &aih, &pai);
  if (res != 0) {
    logger->error("getaddrinfo error: %s", gai_strerror(res));
    return nullptr;
  }

  // while getaddrinfo may return multiple addresses matching requirements,
  // we expect just a single address here
  // May change in the future if 'domain' addresses are used 
  // Note that host addresses are re-checked
  //   - IPv4 addresses may be written in 'network format' ("192.1")
  //   - IPv6 addresses may contain 'zone_id' ("fe80::1%eth0")

  if (pai->ai_addr->sa_family == AF_INET) {
    psin = (struct sockaddr_in*) pai->ai_addr;
    addr = new IPv4Address(psin->sin_addr);
  }
  else if (pai->ai_addr->sa_family == AF_INET6) {
    psin6 = (struct sockaddr_in6*) pai->ai_addr;
    addr = new IPv6Address(psin6->sin6_addr, psin6->sin6_scope_id);
  }
  else {
    logger->error("getaddrinfo: invalid address family");
    addr = nullptr;
  }

  freeaddrinfo(pai);

  return addr;
}

Address* get_mac_address(const string& host) {
  // Converts from 'string' host to internal 'mac_addr' format
  // syntax is  'nhSnhSnhSnhSnh$nh'
  // where S is a valid separator character (:;.|)
  // 'nh' is a group of 0, 1 or 2 hex characters (0-9a-fA-F)
  // no whitespace allowed inside the string
  //
  struct mac_addr macb;
  int   pos;
  const char* p;
  char*       q;
  char  sep = '\0';

  pos = 0;
  p = host.c_str();
  while(pos < 6 and *p) {
    unsigned long int li;

    // read hex characters until next separator and build value
    li = strtoul(p, &q, 16);
    if ((q-p) > 2)
      // field is too big. Occupies more than two hex chars
      break;
    if (*q and not strchr(MAC_SEPARATORS, *q))
      // Invalid separator character
      break;
    if (sep and *q and *q != sep)
      // multiple separator characters used      
      break;
    if (*q)
      // update separator char
      sep = *q;

    // load value into mac_addr structure
    macb.sl2_addr[pos++] = (unsigned short) li; 

    // advance pointer to next field
    p = q;
    // check wether string is exhausted or all the fields have been filled
    if (pos < 6 and *p)
      p++;
  }

  if (pos < 6 or *p) {
    // wrong syntax
    logger->error("wrong link layer address syntax: %s", host.c_str());
    return nullptr;
  }

  return new LinkLayerAddress(macb);
}

// Factory function to create addresses based on textual representation
//
Address* get_address(const string& host, const string& service,
                       int family, int type) {
  Address* addr = nullptr;

  if (host.size() > MAX_HOST_STRLEN) {
    logger->error("Maximum address length exceeded");
    return addr;
  }

  string tmphost(host);

  // Build appropriate INADDR_ANY address for each family
  if (host.empty()) {
    if (family == AF_INET)
      tmphost = string("0.0.0.0");
    else if (family == AF_INET6)
      tmphost = string("::");
    else {
      if (family == AF_LOCAL_L2)
        logger->error("Invalid NULL MAC address");
      else   // AF_UNSPEC
        logger->error("Ambiguous NULL address. "
                      "Specify '0.0.0.0', or '::' for IPv6");
      return addr;
    }
  }

  const string chost(tmphost); 

  switch(family) {
    case AF_LOCAL_L2:  addr = get_mac_address(chost);
                       break;
    case AF_INET:
    case AF_INET6:
    case AF_UNSPEC:    addr = get_ip_address(chost, service, family, type);
                       break;
    default:           logger->error("Invalid address family: %d", family);
                       break;
  }

  return addr;
}
