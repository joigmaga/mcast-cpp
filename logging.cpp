/*
A multicast interface to the socket library

  Basic logging interface

    Ignacio Martinez (igmartin@movistar.es)
    January 2023

*/

#include <string.h>

#include <ctime>
#include <cstdarg>
#include <fstream>
#include <iostream>
#include <sstream>

#include "logging.h"

using namespace std;

// This is the local logger instance for the logging module (compilation unit)
//
// There are two interfaces:
//   - pointer based (mandatory): logger_ptr->log(...)
//   - reference based (optional): logger.log(...)
//
static logptr_t logger_ptr = Logger::get_logger("_LOGGING", WARNING, STDERR);
// optional and additional to the above (requires the pointer interface)
static logref_t logger = *logger_ptr;

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
                          parent(nullptr)      { };
// non-root Logger constructor
Logger::Logger(Private, const string& module) : modname(module),
                                                loglevel(NOTSET),
                                                outstream(nullptr),
                                                propagate(true)       { };

// Destructor. Update loggers tree and close log file
Logger::~Logger() {
  // Update a tree entry only if it has a parent and has no children 

  lock_guard<mutex> lock(logmutex);
  if (dict.size() == 0) {                 // we do not have children. OK
    bool orphan = false;                  // Are there other pointers to us?
    if (parent) {                         // not root. Check parents dict
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
        // this will call parent's destructor
        parent.reset();                          // cancel pointer to parent
        // close log file
        if (logfile.is_open())
          logfile.close();
      }
    }
  }
}
// Factories for root and regular loggers
//
logptr_t Logger::get_logger(int level, int stream) {
// *** Used exclusively for creating the root instance ***
// Instance gets created and initialized the first time this method is called
// The root instance is unique. This method always return the same pointer
//
  static logptr_t root_instance = make_shared<Logger>(Private());

  root_instance->set_loglevel(level);
  root_instance->set_streamer(stream);

  return root_instance;
}
//
logptr_t Logger::get_logger(const string& module, int level, int stream) {
  logptr_t  instance;
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
      cerr << "(logging) max number of subfields exceeded" << endl;
      break;
    }

    lock_guard<mutex> lock(instance->logmutex);

    if (instance->dict.count(submod) > 0) {
      // (sub) module exists. Fetch its pointer in the dictionary
      instance = instance->dict[submod].lock();   // weak pointer -> shared
    }
    else {
      // create new shared pointer and store it in dict
      logptr_t new_instance = make_shared<Logger>(Private(), submod);
      
      instance->dict[submod] = new_instance;   // store as weak pointer
      new_instance->parent = instance;         // upwards pointer
      instance = new_instance;                 // instance refcount++
    }

    if (pos == string::npos)     // end of string reached
      break;

    dotpos = pos+1;              // move one char beyond the '.'
  }

  if (not instance)
    throw runtime_error(string("null instance returned for module ") + module);

  instance->set_loglevel(level);
  instance->set_streamer(stream);
  
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

  if (ferror)
    cerr << "error opening log file '" << fname
         << "': " << strerror(errno) << endl;
}
// select an output stream (safe)
void Logger::set_streamer(int streamval) {

  lock_guard<mutex> lock(logmutex);
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
    default:         break;
  }
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

  if (modname.size() > 8)
    modname = modname.substr(0,8);

  char entry[256];
  if (snprintf(entry, sizeof(entry), "%s %s: [%s] %s", timestamp,
        modname.c_str(), level_to_string(level).c_str(), message.c_str()) < 0)
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
