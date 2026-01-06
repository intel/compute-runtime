/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/utilities/timer_util.h"

#include <atomic>
#include <memory>
#include <sstream>
#include <vector>

namespace NEO {
class PerfProfiler : NonCopyableAndNonMovableClass {

    struct SystemLog {
        unsigned int id;
        long long start;
        unsigned long long time;
    };

  public:
    struct LogBuilder {
        static void write(std::ostream &str, long long start, long long end, long long span, unsigned long long totalSystem, const char *function);
        static void read(std::istream &str, long long &start, long long &end, long long &span, unsigned long long &totalSystem, std::string &function);
    };

    struct SysLogBuilder {
        static void write(std::ostream &str, long long start, unsigned long long time, unsigned int id);
        static void read(std::istream &str, long long &start, unsigned long long &time, unsigned int &id);
    };

    static void readAndVerify(std::istream &stream, const std::string &token);

    PerfProfiler(int id, std::unique_ptr<std::ostream> &&logOut = {nullptr},
                 std::unique_ptr<std::ostream> &&sysLogOut = {nullptr});
    ~PerfProfiler();

    void apiEnter() {
        totalSystemTime = 0;
        systemLogs.clear();
        systemLogs.reserve(20);
        apiTimer.start();
    }

    void apiLeave(const char *func) {
        apiTimer.end();
        logTimes(apiTimer.getStart(), apiTimer.getEnd(), apiTimer.get(), totalSystemTime, func);
    }

    void logTimes(long long start, long long end, long long span, unsigned long long totalSystem, const char *function);
    void logSysTimes(long long start, unsigned long long time, unsigned int id);

    void systemEnter() {
        systemTimer.start();
    }

    void systemLeave(unsigned int id) {
        systemTimer.end();
        logSysTimes(systemTimer.getStart(), systemTimer.get(), id);
        totalSystemTime += systemTimer.get();
    }

    std::ostream *getLogStream() {
        return logFile.get();
    }

    std::ostream *getSystemLogStream() {
        return sysLogFile.get();
    }

    static PerfProfiler *create(bool dumpToFile = true);
    static void destroyAll();

    static int getCurrentCounter() {
        return counter.load();
    }

    static PerfProfiler *getObject(unsigned int id) {
        return objects[id];
    }

    static const unsigned int objectsNumber = 4096;

  protected:
    static std::atomic<int> counter;
    static PerfProfiler *objects[PerfProfiler::objectsNumber];
    Timer apiTimer;
    Timer systemTimer;
    unsigned long long totalSystemTime = 0;
    std::unique_ptr<std::ostream> logFile;
    std::unique_ptr<std::ostream> sysLogFile;
    std::vector<SystemLog> systemLogs;
};

static_assert(NonCopyableAndNonMovable<PerfProfiler>);

}; // namespace NEO
