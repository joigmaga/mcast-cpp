#ifndef INC_LOGGING
#define INC_LOGGING

#include <stdarg.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <queue>

#define UNCHANGED (-1)

/* used for stream selection
   have nothing to do with known file descriptors
*/

#define DEVNULL    0
#define STDOUT     1
#define STDERR     2
#define STDLOG     3

#define NOTSET   0
#define DEBUG    1
#define INFO     2
#define WARNING  3
#define ERROR    4
#define CRITICAL 5
#define MINLOG   NOTSET
#define MAXLOG   CRITICAL

#define MAX_MODULE_SUBFIELDS  32

#define TIMEFMT  "%Y/%m/%d:%H:%M:%S"

// The logger class
// 
class Logger : public std::enable_shared_from_this<Logger> {
  private:
    // local typedefs
    typedef std::shared_ptr<Logger> logptr_t;
    typedef std::weak_ptr<Logger>   logwptr_t;
    // this artifact prevents creation of instances from outside the class
    struct Private {
      explicit Private() = default;
    };
    //
    std::string   modname;      // Module name
    int           loglevel;     // Current log level
    std::ofstream logfile;      // File stream for the log file
    std::string   filename;     // Active log file
    std::ostream* outstream;    // Pointer to output stream
    std::mutex    logmutex;     // Mutex for controlling output to stream/file
    bool          propagate;    // Continue the search upwards to the root
    logptr_t      parent;       // logger's ancestor
    std::map<std::string, logwptr_t> dict;      // Loggers Dictionary
    //
    // formatting of logging records
    std::string& logrecord(std::string& record, const char* timefmt,
                      std::string modname, std::string& message, int level);
    std::string& logmessage(std::string& message,
                      const char* format, va_list vl);
    void logaux(int level, const char* format, va_list args);
    static std::string level_to_string(int level);
  public:
    // Constructors with opaque Private type argument not usable from outside 
    Logger(Private);
    Logger(Private, const std::string& module);
    //
    ~Logger();
    // Prevent copying (note: delete functions are a C++11 feature)
    Logger(Logger const&)         = delete;   // copy
    void operator=(Logger const&) = delete;   // assignment
    // Factory functions for instatiating the class
    // Root logger
    static logptr_t get_logger(int level=UNCHANGED, int stream=UNCHANGED);
    // Regular logger
    static logptr_t get_logger(const std::string& module,
                               int level=UNCHANGED, int stream=UNCHANGED);
    // Message formatting
    void log(int level, const char* format, ...);
    void critical(const char* format, ...);
    void error(const char* format, ...);
    void warning(const char* format, ...);
    void info(const char* format, ...);
    void debug(const char* format, ...);
    // Get/set current log level
    int get_loglevel();
    int set_loglevel(int level);
    // Select log file and streamer
    void set_logfile(const std::string& fname);
    void set_streamer(int streamval);
    // Control tree navigation
    bool set_propagation(bool mode);
};
// 
// These have wider scope than class typedefs
//
typedef std::shared_ptr<Logger> logptr_t;
typedef Logger&                 logref_t;

#endif
