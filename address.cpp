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
static logptr_t logger = Logger::get_logger("ADDRESS");

// map describing the address families used in this module
// map<int, string> family_map;

std::map<int, std::string> family_map = { {AF_LOCAL_L2, "link layer"},
                                          {AF_INET,     "IPv4"},
                                          {AF_INET6,    "IPv6"} };

//// Address base class. Constructor and methods
//
Address::Address(int fam, const string &host) : family(fam), printable(host) {}
// This function may be overriden by children (IPv6 may append '%scope_zone')
//
string Address::print() {
  return printable;
}

//// IP v4 address
// Constructor takes binary and textual formats of address
//
IPv4Address::IPv4Address(struct in_addr addr, const string &host) :
                Address(AF_INET, host), address(addr) {
  ipv4mapped = "::ffff:" + host;
}

bool IPv4Address::is_multicast() {

  return IN_MULTICAST(address.s_addr);
}

//// IP v6 address
//
IPv6Address::IPv6Address(struct in6_addr addr, const string &host, int sid) :
                Address(AF_INET6, host), address(addr), scope_id(sid) {
}

bool IPv6Address::is_multicast() {

  // check if IN6_IS_ADDR_V4MAPPED(address) and v4 address is multicast
  return IN6_IS_ADDR_MULTICAST(&address);
}

string IPv6Address::print() {
  if (scope_id > 0) {
    char ifname[IFNAMSIZ];

    if (if_indextoname(scope_id, ifname))
      return printable + "%" + string(ifname);
  }

  return printable;
}

//// Link layer address
//
LinkLayerAddress::LinkLayerAddress(struct mac_addr &addr, const string &host): 
                             Address(AF_LOCAL_L2, host), address(addr) {}

string LinkLayerAddress::print() {
  char buff[32];
  
  if (snprintf(buff, sizeof(buff), "%02x:%02x:%02x:%02x:%02x:%02x",
                 address.sl2_addr[0], address.sl2_addr[1],
                 address.sl2_addr[2], address.sl2_addr[3],
                 address.sl2_addr[4], address.sl2_addr[5])  < 0) {
    perror("snprintf");
    return string();
  }

  return string(buff);
}

// Factory functions to create addresses based on textual representation
//
Address* get_ip_address(const string &host, const string &service,
                        int family, int type) {
  int  res;
  char buff[256];

  struct addrinfo   aih;
  struct addrinfo*  pai;
  struct sockaddr_in*  psin;
  struct sockaddr_in6* psin6;
  Address* addr;
  
  aih.ai_flags     = AI_NUMERICHOST;
  aih.ai_family    = AF_UNSPEC;
  aih.ai_socktype  = SOCK_DGRAM;
  aih.ai_protocol  = 0;
  aih.ai_addrlen   = 0;
  aih.ai_addr      = nullptr;
  aih.ai_canonname = nullptr;
  aih.ai_next      = nullptr;
  
  res = getaddrinfo(host.c_str(), service.c_str(), &aih, &pai);
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
    inet_ntop(AF_INET, &psin->sin_addr, buff, sizeof(buff));
    const string thost = string(buff);
    addr = new IPv4Address(psin->sin_addr, thost);
  }
  else if (pai->ai_addr->sa_family == AF_INET6) {
    psin6 = (struct sockaddr_in6*) pai->ai_addr;
    inet_ntop(AF_INET6, &psin6->sin6_addr, buff, sizeof(buff));
    const string thost = string(buff);
    addr = new IPv6Address(psin6->sin6_addr, thost, psin6->sin6_scope_id);
  }
  else {
    logger->error("getaddrinfo: invalid address family");
    addr = nullptr;
  }

  freeaddrinfo(pai);

  return addr;
}

Address* get_mac_address(const string &host) {
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
      // field two big. Occupies more than two hex chars
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

  return new LinkLayerAddress(macb, host);
}

// Factory function to create addresses based on textual representation
//
Address* get_address(string &host, const string &service, int family, int type)
{
  Address* addr = nullptr;

  // Build appropriate INADDR_ANY address for each family
  if (host.empty()) {
    if (family == AF_INET)
      host = string("0.0.0.0");
    else if (family == AF_INET6)
      host = string("::");
    else {
      if (family == AF_LOCAL_L2)
        logger->error("Invalid NULL MAC address");
      else   // AF_UNSPEC
        logger->error("Ambiguous NULL address. "
                     "Specify '0.0.0.0', or '::' for IPv6");
      return addr;
    }
  }

  if (host.size() > MAX_HOST_STRLEN) {
    logger->error("Maximum address length exceeded");
    return addr;
  }

  const string address(host); 

  switch(family) {
    case AF_LOCAL_L2:  addr = get_mac_address(address);
                       break;
    case AF_UNSPEC:    addr = get_ip_address(address, service, family, type);
                       break;
    case AF_INET:      addr = get_ip_address(address, service, AF_INET, type);
                       break;
    case AF_INET6:     addr = get_ip_address(address, service, AF_INET6, type);
                       break;
    default:           logger->error("Invalid address family: %d", family);
                       break;
  }

  return addr;
}

