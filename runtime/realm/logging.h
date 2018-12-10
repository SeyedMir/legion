/* Copyright 2018 Stanford University, NVIDIA Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// logging infrastructure for Realm

#ifndef REALM_LOGGING_H
#define REALM_LOGGING_H

#include "realm/realm_config.h"
#include "realm/utils.h"

#include <stdarg.h>
#include <vector>
#include <string>
#include <sstream>


namespace Realm {
  // this can be set at compile time to eliminate instructions for some/all logging
#ifndef REALM_LOGGING_MIN_LEVEL
  // old define name
#ifdef COMPILE_TIME_MIN_LEVEL
#define REALM_LOGGING_MIN_LEVEL COMPILE_TIME_MIN_LEVEL
#else
#define REALM_LOGGING_MIN_LEVEL LEVEL_DEBUG
#endif
#endif
  
  class LoggerMessage;
  typedef int LoggerMessageID;
  static const LoggerMessageID RESERVED_LOGGER_MESSAGE_ID = 0;
  class LoggerConfig;
  class LoggerOutputStream;
  
  class Logger {
  public:
    Logger(const std::string& _name);
    ~Logger(void);
    
    enum LoggingLevel {
      LEVEL_SPEW, // LOTS of stuff
      LEVEL_DEBUG,
      LEVEL_INFO,
      LEVEL_PRINT,
      LEVEL_WARNING,
      LEVEL_ERROR,
      LEVEL_FATAL,
      LEVEL_NONE,  // if you really want to turn EVERYTHING off
    };
    
    static void configure_from_cmdline(std::vector<std::string>& cmdline);
    
    const std::string& get_name(void) const;
    LoggingLevel get_level(void) const;
    
    LoggerMessage spew(void);
    LoggerMessage debug(void);
    LoggerMessage info(void);
    LoggerMessage print(void);
    LoggerMessage warning(void);
    LoggerMessage error(void);
    LoggerMessage fatal(void);
    
    LoggerMessage spew(LoggerMessageID);
    LoggerMessage debug(LoggerMessageID);
    LoggerMessage info(LoggerMessageID);
    LoggerMessage print(LoggerMessageID);
    LoggerMessage warning(LoggerMessageID);
    LoggerMessage error(LoggerMessageID);
    LoggerMessage fatal(LoggerMessageID);
    
    // use this only if you want a dynamic level for some reason
    LoggerMessage newmsg(LoggingLevel level);
    
    // old printf-style interface
    void spew(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void debug(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void info(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void print(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void warning(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void error(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    void fatal(const char *fmt, ...) __attribute__((format (printf, 2, 3)));
    
    // newer collated messages
    void spew(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    void debug(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    void info(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    void print(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    void warning(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    void error(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    void fatal(LoggerMessageID, const char *fmt, ...) __attribute__((format (printf, 3, 4)));
    
    
    
    
  protected:
    friend class LoggerMessage;
    
    void log_msg(LoggingLevel level, const std::string& msg);
    void log_msg(LoggingLevel level, const char *msgdata, size_t msglen);
    
    friend class LoggerConfig;
    
    void add_stream(LoggerOutputStream *s, LoggingLevel min_level,
                    bool delete_when_done, bool flush_each_write);
    
    struct LogStream {
      LoggerOutputStream *s;
      LoggingLevel min_level;
      bool delete_when_done;
      bool flush_each_write;
    };
    
    std::string name;
    std::vector<LogStream> streams;
    LoggingLevel log_level;  // the min level of any stream
  };
  
  class LoggerMessage {
  protected:
    // can only be created by a Logger
    friend class Logger;
    
    LoggerMessage(void);  // default constructor makes an inactive message
    LoggerMessage(Logger *_logger, bool _active, Logger::LoggingLevel _level);
    LoggerMessage(LoggerMessageID messageID, Logger *_logger, bool _active, Logger::LoggingLevel _level);
    
    
  public:
    LoggerMessage(const LoggerMessage& to_copy);
    ~LoggerMessage(void);
    
    template <typename T>
    LoggerMessage& operator<<(const T& val);
    
    // vprintf-style
    LoggerMessage& vprintf(const char *fmt, va_list ap);
    LoggerMessage& vprintf(const char *typeName, LoggerMessageID messageID, const char *fmt, va_list ap);
    
    bool is_active(void) const;
    
    std::ostream& get_stream(void);
    
  protected:
    LoggerMessageID messageID;
    Logger *logger;
    bool active;
    Logger::LoggingLevel level;
    // contain messages shorter than 160 characters entirely inline
    DeferredConstructor<shortstringbuf<160, 256> > buffer;
    DeferredConstructor<std::ostream> stream;
  };
  
}; // namespace Realm

#include "realm/logging.inl"

#endif
