/*
A multicast interface to the socket library

  Basic logging interface

    Ignacio Martinez (igmartin@movistar.es)
    January 2023

*/

#include <string.h>

#include <thread>
#include <ctime>
#include <cstdarg>
#include <fstream>
#include <iostream>
#include <sstream>

#include "logging.h"

using namespace std;

// This could be the logger instance for a module (compilation unit) 'MOD_NAME'
//
// There are two interfaces:
//   - pointer based (off the shelve):  logger_ptr->log(...)
//   - reference based (optional):      logger.log(...)
//
// static logptr_t logger_ptr = Logger::get_logger("MOD_NAME", WARNING, STDLOG);
//
//// optional, additional to the above (requires the pointer interface)
//
// static logref_t logger = *logger_ptr;

static thread::id main_thread_id = this_thread::get_id();

///////////// Logger class
//
// Logger instances are created on a per-module basis
// A 'module' has no particular meaning and may refer to any piece of code
// module duration is tied to the duration of the returned (smart) pointer
// Loggers get instantiated using a factory function (get_logger(module))
// Loggers form a hierarchy. There is a 'root' Logger that gets instantiated
// using get_logger(), which invokes an especial constructor
// All calls to get_logger() for the same module return the same instance 
// Once a logger is out of scope, the object pointed to is destroyed and the
// instance tree is rebuilt
//

// Logger class methods
//
// root Logger constructor (default)
// note that "" as a name is meaningless and only intended for visual purposes
// It could be used as a name by any non-root Logger eventually
//
Logger::Logger(Private) : modname(""),
                          loglevel(WARNING),
                          outstream(nullptr),
                          propagate(false),
                          parent(nullptr)      { 

  main_thread_id = this_thread::get_id();
  int level = set_loglevel(DEBUG);
  ostream* os = set_streamer(STDLOG);
  debug("created root logging instance");
  outstream = os;
  set_loglevel(level);
};
// non-root Logger constructor
Logger::Logger(Private, const string& module) : modname(module),
                                                loglevel(NOTSET),
                                                outstream(nullptr),
                                                propagate(true)       { 
  ostream* os = set_streamer(STDLOG);
  debug("created logging module %s", modname.c_str());
  outstream = os;
};

// Destructor. Update loggers tree and close log file
Logger::~Logger() {
  string module = modname;

  if (not parent) {                               // root
    info("destroying root logging module");       // use root logger
    module = "root";                              // rename modname
  }
  else {
    info("destroying logging module '%s'", modname.c_str());

    lock_guard<mutex> lock(logmutex);
    if (dict.size() == 0) {                 // we do not have children. OK
      bool orphan = false;                  // Are there other pointers to us?
      {
        // lock parent and look for a weak pointer to us in its dict
        lock_guard<mutex> lockparent(parent->logmutex);
        logptr_t child_ptr = parent->dict[modname].lock();      // weak ptr
        // check if other pointer(s) have been created in the meantime
        orphan = child_ptr == nullptr;
        if (orphan)
          // remove weak pointer from parent's dict
          parent->dict.erase(modname);
      }
      if (orphan) {
        // this will safely call parent's destructor since it is unlocked now
        parent.reset();                          // cancel pointer to parent
        // close log file
        if (logfile.is_open())
          logfile.close();
      }
    }
  }
  info("%s destroyed", module.c_str());
}
// Factories for root and regular loggers
//
logptr_t Logger::get_logger(int level, int stream) {
// *** Used exclusively for creating the root instance ***
// Instance gets created and initialized the first time this method is called
// The root instance is unique. This method always return the same pointer
//
//  static logptr_t root_instance = make_shared<Logger>(Private());

  static logptr_t root_instance = nullptr;

  if (not root_instance) {
    root_instance = make_shared<Logger>(Private());
  }

  root_instance->set_loglevel(level);
  root_instance->set_streamer(stream);

  return root_instance;
}
//
logptr_t Logger::get_logger(const string& module, int level, int stream) {
  logptr_t  instance;
  string    action;
  size_t dotpos = 0;               // position of '.' character in name
  int exit_loop = 0;               // to avoid excesive number of sub modules

  instance = get_logger();         // this is the root instance

  while(instance) {
    size_t pos;
    string submod;

    pos = module.find('.', dotpos);
    submod = module.substr(0, pos);

    if (++exit_loop > MAX_MODULE_SUBFIELDS) {
      // don't do any more searching
      instance->error("max number of module subfields exceeded");
      break;
    }

    instance->debug("fetching logger");

    {
      lock_guard<mutex> lock(instance->logmutex);
      if (instance->dict.count(submod) > 0) {
        // (sub) module exists. Fetch its pointer in the dictionary
        instance = instance->dict[submod].lock();   // weak pointer -> shared
        action = "found";
      }
      else {
        // create new shared pointer and store it in dict
        logptr_t new_instance = make_shared<Logger>(Private(), submod);
      
        instance->dict[submod] = new_instance;   // store as weak pointer
        new_instance->parent = instance;         // upwards pointer
        instance = new_instance;                 // instance refcount++
        action = "create";
      }
    }

    if (pos == string::npos) {                   // end of string reached
      instance->set_loglevel(level);
      instance->set_streamer(stream);
      instance->info("%s logger", action.c_str()); 
      break;
    }

    instance->info("%s logger", action.c_str()); 

    dotpos = pos+1;              // move one char beyond the '.'
  }

  if (not instance)
    throw runtime_error(string("null instance returned for module ") + module);

  
  return instance;
}
// get/set current log level (safe)
//
int Logger::get_loglevel() {
  // return current log level

  lock_guard<mutex> lock(logmutex);
  return loglevel;
}
int Logger::set_loglevel(int level) {
  // set log level to new level and return current level

  lock_guard<mutex> lock(logmutex);
  int curlevel = loglevel;
  if (level != UNCHANGED)
    loglevel = min(MAXLOG, max(MINLOG, abs(level)));

  return curlevel;
}
// set propagation mode for a logger (safe)
bool Logger::set_propagation(bool mode) {

  lock_guard<mutex> lock(logmutex);
  bool curmode = propagate;
  propagate = mode;

  return curmode;
}
// Configure the log file for a logger (safe)
void Logger::set_logfile(const string& fname) {
  string newfname;
  bool ferror = false;

  // If file provided and different from current file, open it and
  // close existing file
  // Use absolute pathnames for file name comparison

  lock_guard<mutex> lock(logmutex);
  if (not fname.empty()) {
    char*    p;

    // Convert file path name to absolute
    p = realpath(fname.c_str(), nullptr);
    if (p) {
      newfname = string(p);
      free(p);
    }
    else {
      ofstream ofs;

      // Log file does not exist. Try to create it
      ofs.open(fname, ios::trunc); 
      if (ofs.is_open()) {
        ofs.close();
        // Try to build the path name again
        p = realpath(fname.c_str(), nullptr);
        newfname = string(p);
        free(p);
      }
      else
        ferror = true;
    }
  }

  if (newfname != filename) {
    // close current log file
    if (logfile.is_open())
      logfile.close();
    filename = string();

    if (not newfname.empty()) {
      // open new log file
      logfile.open(newfname, ios::app);
      if (logfile.is_open())
        filename = newfname;
      else
        ferror = true;
    }
  }

  if (ferror) {
    logmutex.unlock();
    error("error opening log file '%s': %s",  fname.c_str(), strerror(errno));
  }
}
// select an output stream (safe)
ostream* Logger::set_streamer(int streamval) {

  lock_guard<mutex> lock(logmutex);
  ostream* curos = outstream;
  switch(streamval) {
    case STDOUT:     outstream = &cout;
                     break;
    case STDERR:     outstream = &cerr;
                     break;
    case STDLOG:     outstream = &clog;
                     break;
    case DEVNULL:    outstream = nullptr;
                     break;
    case UNCHANGED:
    default:         outstream = curos;
                     break;
  }

  return curos;
}
string Logger::level_to_string(int level) {
  switch (level) {
    case NOTSET:   return "unset";
    case DEBUG:    return "debug";
    case INFO:     return "info";
    case WARNING:  return "warning";
    case ERROR:    return "error";
    case CRITICAL: return "critical";
    default:       return "unknown";
  }
}
void Logger::critical(const char* format, ...) {
  va_list vl;

  va_start(vl, format);
  Logger::logaux(CRITICAL, format, vl);
  va_end(vl);
} 
void Logger::error(const char* format, ...) {
  va_list vl;

  va_start(vl, format);
  Logger::logaux(ERROR, format, vl);
  va_end(vl);
} 
void Logger::warning(const char* format, ...) {
  va_list vl;

  va_start(vl, format);
  Logger::logaux(WARNING, format, vl);
  va_end(vl);
} 
void Logger::info(const char* format, ...) {
  va_list vl;

  va_start(vl, format);
  Logger::logaux(INFO, format, vl);
  va_end(vl);
} 
void Logger::debug(const char* format, ...) {
  va_list vl;

  va_start(vl, format);
  Logger::logaux(DEBUG, format, vl);
  va_end(vl);
} 
void Logger::log(int level, const char* format, ...) {
  va_list vl;

  va_start(vl, format);
  Logger::logaux(level, format, vl);
  va_end(vl);
}

// Logs a message with a given log level
string& Logger::logmessage(string& message,
                                const char* format, va_list vl) {
  char msg[256];

  // Create log message
  // note that long messages get truncated to 256 characters
  if (vsnprintf(msg, sizeof(msg), format, vl) < 0)
    snprintf(msg, sizeof(msg), "logging error: %s", strerror(errno));

  message = string(msg);

  return message; 
}

string& Logger::logrecord(string& record, const char* timefmt,
                          string modname, string& message, int level) {

  // Get current timestamp
  char timestamp[32];
  time_t now = time(0);
  tm* timeinfo = localtime(&now);
  strftime(timestamp, sizeof(timestamp), timefmt, timeinfo);

  if (modname.size() > MODULE_NAME_SIZE)
    modname = modname.substr(0, MODULE_NAME_SIZE);

  string msep;
  if (modname.size() > 0)
    msep = ": ";

  // this is a bit tricky, but need to pass the thread id type anyway
  char tids[64];
  tids[0] = '\0';
  thread::id tid = this_thread::get_id();
  if (tid != main_thread_id) {
    string fmt("(%x) "); 
    snprintf(tids, sizeof(tids), fmt.c_str(), tid);
  }

  // the final record formatting
  char entry[256];
  if (snprintf(entry, sizeof(entry), "%s %s%s%s[%s] %s",
        timestamp, modname.c_str(), msep.c_str(), tids,
        level_to_string(level).c_str(), message.c_str()) < 0)
    snprintf(entry, sizeof(entry), "logging error: %s", strerror(errno));
    
  record = string(entry);

  return record;
}  
void Logger::logaux(int level, const char* format, va_list vl) {
  const char* timefmt = TIMEFMT;
  string  msg, rec;
  string& message = msg;
  string& record  = rec;
  Logger* instance;

  // retain original 'level' and 'modname' values across potential loggers
  // Message formatting
  message = logmessage(message, format, vl);
  // Record formatting as a log message wrapper
  record  = logrecord(record, timefmt, modname, message, level);

  instance = this;
  while (instance) {

    // lock here to prevent other threads from changing level, stream, etc
    lock_guard<mutex> lock(instance->logmutex);

    if (level >= instance->loglevel) {
      // log to stream if configured
      if (instance->outstream) {
        *instance->outstream << record << endl;
      }

      // log to log file if open
      if (instance->logfile.is_open()) {
        instance->logfile << record << endl;
        instance->logfile.flush();
      }
    }

    if (not instance->propagate)
      break;

    instance = instance->parent.get();
  }
}
