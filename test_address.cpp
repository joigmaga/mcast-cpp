#include <iostream>
#include <string>
#include <map>

#include "address.h"
#include "logging.h"

using namespace std;

static logptr_t logger = Logger::get_logger("TEST_1", DEBUG, STDLOG);

int main() {
  string host1   = "130.56.197.2";
  string host2   = "ff02::1234:5678%4";
  string host3   = "f:0:12:3:56:8";
  string hostv4m = "::ffff:235.34.32.11";
  string hostv4u = "::ffff:130.206.1.2";
  string service = "www";
  Address* addr;

  logger->set_logfile("logfile.log");

  {
    Address* addrx = get_address(host1);
    if (addrx) {

      cout << "gets here!" << endl;
      cout << addrx->print() << endl;
      cout << addrx->Address::print() << endl;
      delete addrx;
    }
    else
      cout << "uuuuuh" << endl;
  }

  Address* addrv4u = get_address(hostv4u);
  Address* addrv4m = get_address(hostv4m);

  logger->warning("addrv4u is multicast: %d", addrv4u->is_multicast());
  logger->warning("addrv4m is multicast: %d", addrv4m->is_multicast());

  cerr << "runtime instance count: " << logger.use_count() << endl;
  logger->error("big error: %d, %s", 56, "forgot the keys");
  addr = get_address(host2, service);
  if (addr) {

    cout << addr->print() << endl;
    cout << addr->Address::print() << endl;
  }

  {
    cerr << "runtime2 instance count: " << logger.use_count() << endl;
    logptr_t logger2 = Logger::get_logger("TEST_2", INFO, STDLOG);
    logger2->set_logfile("logfile2.log");

    logger2->error("big error: %d, %d, %s", 98, 67, "forgot the keys again");

    Address* addry = get_address(host3, service, AF_LOCAL_L2);
    if (addry) {

      cout << addry->print() << endl;
      cout << addry->Address::print() << endl;

      cout << "host: " << addry->print() << endl;
      cout << "host: " << addry->Address::print() << endl;
      delete addry;
    }

    logptr_t logger3 = Logger::get_logger("TEST_2");
  }
  {
    logptr_t logger2 = Logger::get_logger("T1.T2");
    logger2->error("big error: %d, %d, %s", 9, 6, "forgot the keys once again");
    
    logptr_t logger3 = Logger::get_logger("T1.T2.T3.T4");
    logger3->error("big error: %d, %d, %s", 9, 6, "forgot the keys once again");
  } 

  delete addr;
}
