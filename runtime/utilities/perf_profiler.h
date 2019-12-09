/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/options.h"
#include "core/utilities/timer_util.h"

#include <atomic>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace NEO {
class PerfProfiler {

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

    PerfProfiler(int id, std::unique_ptr<std::ostream> logOut = {nullptr},
                 std::unique_ptr<std::ostream> sysLogOut = {nullptr});
    ~PerfProfiler();

    void apiEnter() {
        totalSystemTime = 0;
        systemLogs.clear();
        systemLogs.reserve(20);
        ApiTimer.start();
    }

    void apiLeave(const char *func) {
        ApiTimer.end();
        logTimes(ApiTimer.getStart(), ApiTimer.getEnd(), ApiTimer.get(), totalSystemTime, func);
    }

    void logTimes(long long start, long long end, long long span, unsigned long long totalSystem, const char *function);
    void logSysTimes(long long start, unsigned long long time, unsigned int id);

    void systemEnter() {
        SystemTimer.start();
    }

    void systemLeave(unsigned int id) {
        SystemTimer.end();
        logSysTimes(SystemTimer.getStart(), SystemTimer.get(), id);
        totalSystemTime += SystemTimer.get();
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
    Timer ApiTimer;
    Timer SystemTimer;
    unsigned long long totalSystemTime;
    std::unique_ptr<std::ostream> logFile;
    std::unique_ptr<std::ostream> sysLogFile;
    std::vector<SystemLog> systemLogs;
};

#if KMD_PROFILING == 1

extern thread_local PerfProfiler *gPerfProfiler;

struct PerfProfilerApiWrapper {
    PerfProfilerApiWrapper(const char *funcName)
        : funcName(funcName) {
        PerfProfiler::create();
        gPerfProfiler->apiEnter();
    }

    ~PerfProfilerApiWrapper() {
        gPerfProfiler->apiLeave(funcName);
    }

    const char *funcName;
};
#endif
}; // namespace NEO
