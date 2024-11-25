#include <iostream>
#include <string>
#include <map>

#include "address.h"
#include "logging.h"

using namespace std;

static logptr_t logger = Logger::get_logger("TEST_1");

int main() {
  string host1   = "130.56.897.2";
  string host2   = "ff02::1234:5678%4";
  string host3   = "f:0:12:3:56:8";
  string service = "www";
  string serv2   = "89";
  Address* addr;

  logptr_t root_logger = Logger::get_logger();
  root_logger->set_loglevel(DEBUG);
  root_logger->set_streamer(STDLOG);

  logger->set_logfile("logfile.log");

  addr = get_address(host1, serv2);
  if (addr) {

    cout << addr->print() << endl;
    cout << addr->Address::print() << endl;
  }

  cerr << "runtime instance count: " << logger.use_count() << endl;
  logger->error("big error: %d, %s", 56, "forgot the keys");
  addr = get_address(host2, service);
  if (addr) {

    cout << addr->print() << endl;
    cout << addr->Address::print() << endl;
  }

  {
    cerr << "runtime2 instance count: " << logger.use_count() << endl;
    logptr_t logger2 = Logger::get_logger("TEST_2");
    logger2->set_logfile("logfile2.log");

    logger2->error("big error: %d, %d, %s", 98, 67, "forgot the keys again");

    addr = get_address(host3, service, AF_LOCAL_L2);
    if (addr) {

      cout << addr->print() << endl;
      cout << addr->Address::print() << endl;

      cout << "host: " << addr->print() << endl;
      cout << "host: " << addr->Address::print() << endl;
    }

    logptr_t logger3 = Logger::get_logger("TEST_2");
  }
  {
    logptr_t logger2 = Logger::get_logger("T1.T2");
    logger2->error("big error: %d, %d, %s", 9, 6, "forgot the keys once again");
    
    logptr_t logger3 = Logger::get_logger("T1.T2.T3.T4");
    logger3->error("big error: %d, %d, %s", 9, 6, "forgot the keys once again");
  } 

}
