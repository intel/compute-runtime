/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "runtime/helpers/options.h"
#include "runtime/os_interface/os_inc.h"
#include "runtime/utilities/timer_util.h"
#include <atomic>
#include <fstream>
#include <memory>
#include <sstream>
#include <vector>

namespace OCLRT {
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

#if OCL_RUNTIME_PROFILING == 1
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
}; // namespace OCLRT
