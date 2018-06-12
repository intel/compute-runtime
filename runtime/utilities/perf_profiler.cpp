/*
 * Copyright (c) 2017 - 2018, Intel Corporation
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

#include "os_inc.h"
#include "runtime/utilities/perf_profiler.h"
#include <runtime/utilities/stackvec.h>
#include <atomic>
#include <cstring>
#include <sstream>
#include <thread>

using namespace std;

namespace OCLRT {

std::atomic<int> PerfProfiler::counter(0);

PerfProfiler *PerfProfiler::objects[PerfProfiler::objectsNumber] = {
    nullptr,
};

PerfProfiler *PerfProfiler::create(bool dumpToFile) {
    if (gPerfProfiler == nullptr) {
        int old = counter.fetch_add(1);
        if (!dumpToFile) {
            std::unique_ptr<std::stringstream> logs = std::unique_ptr<std::stringstream>(new std::stringstream());
            std::unique_ptr<std::stringstream> sysLogs = std::unique_ptr<std::stringstream>(new std::stringstream());
            gPerfProfiler = new PerfProfiler(old, std::move(logs), std::move(sysLogs));
        } else {
            gPerfProfiler = new PerfProfiler(old);
        }
        objects[old] = gPerfProfiler;
    }
    return gPerfProfiler;
}

void PerfProfiler::destroyAll() {
    int count = counter;
    for (int i = 0; i < count; i++) {
        if (objects[i] != nullptr) {
            delete objects[i];
            objects[i] = nullptr;
        }
    }
    counter = 0;
    gPerfProfiler = nullptr;
}

PerfProfiler::PerfProfiler(int id, std::unique_ptr<std::ostream> logOut, std::unique_ptr<std::ostream> sysLogOut) : totalSystemTime(0) {
    ApiTimer.setFreq();

    systemLogs.reserve(20);

    if (logOut != nullptr) {
        this->logFile = std::move(logOut);
    } else {
        stringstream filename;
        filename << "PerfReport_Thread_" << id << ".xml";

        std::unique_ptr<std::ofstream> logToFile = std::unique_ptr<std::ofstream>(new std::ofstream());
        logToFile->exceptions(std::ios::failbit | std::ios::badbit);
        logToFile->open(filename.str().c_str(), ios::trunc);
        this->logFile = std::move(logToFile);
    }

    *logFile << "<report>" << std::endl;

    if (sysLogOut != nullptr) {
        this->sysLogFile = std::move(sysLogOut);
    } else {
        stringstream filename;
        filename << "SysPerfReport_Thread_" << id << ".xml";

        std::unique_ptr<std::ofstream> sysLogToFile = std::unique_ptr<std::ofstream>(new std::ofstream());
        sysLogToFile->exceptions(std::ios::failbit | std::ios::badbit);
        sysLogToFile->open(filename.str().c_str(), ios::trunc);
        this->sysLogFile = std::move(sysLogToFile);
    }

    *sysLogFile << "<report>" << std::endl;
}

PerfProfiler::~PerfProfiler() {
    *logFile << "</report>" << std::endl;
    logFile->flush();
    *sysLogFile << "</report>" << std::endl;
    sysLogFile->flush();
    gPerfProfiler = nullptr;
}

void PerfProfiler::readAndVerify(std::istream &stream, const std::string &token) {
    StackVec<char, 4096> buff;
    buff.resize(token.size());
    size_t numRead = static_cast<size_t>(stream.readsome(&buff[0], token.size()));
    if ((numRead != token.size()) || (0 != std::strncmp(&buff[0], token.c_str(), token.size()))) {
        throw std::runtime_error("ReadAndVerify failed");
    }
}

void PerfProfiler::LogBuilder::write(std::ostream &str, long long start, long long end, long long span, unsigned long long totalSystem, const char *function) {
    str << "<api name=\"" << function << "\">\n";
    str << "<data start=\"" << start << "\" end=\"" << end << "\" time=\""
        << span << "\" api=\"" << span - totalSystem << "\" system=\"" << totalSystem << "\" />\n";
    str << "</api>\n";
}

void PerfProfiler::LogBuilder::read(std::istream &str, long long &start, long long &end, long long &span, unsigned long long &totalSystem, std::string &function) {
    StackVec<char, 4096> funcNameBuff;
    readAndVerify(str, "<api name=\"");
    while ((str.eof() == false) && (str.peek() != '\"')) {
        char c;
        str.read(&c, 1);
        funcNameBuff.push_back(c);
    }
    if (str.eof()) {
        throw std::runtime_error("Could not read function name");
    }
    readAndVerify(str, "\">\n");
    readAndVerify(str, "<data start=\"");
    str >> start;
    readAndVerify(str, "\" end=\"");
    str >> end;
    readAndVerify(str, "\" time=\"");
    str >> span;
    readAndVerify(str, "\" api=\"");
    str >> totalSystem;
    readAndVerify(str, "\" system=\"");
    str >> totalSystem;
    readAndVerify(str, "\" />\n");
    readAndVerify(str, "</api>\n");

    function.assign(funcNameBuff.begin(), funcNameBuff.end());
}

void PerfProfiler::SysLogBuilder::write(std::ostream &str, long long start, unsigned long long time, unsigned int id) {
    str << "<sys id=\"" << id << "\">\n";
    str << "<data start=\"" << start << "\" time=\"" << time << "\" />\n";
    str << "</sys>\n";
}

void PerfProfiler::SysLogBuilder::read(std::istream &str, long long &start, unsigned long long &time, unsigned int &id) {
    readAndVerify(str, "<sys id=\"");
    str >> id;
    readAndVerify(str, "\">\n");
    readAndVerify(str, "<data start=\"");
    str >> start;
    readAndVerify(str, "\" time=\"");
    str >> time;
    readAndVerify(str, "\" />\n");
    readAndVerify(str, "</sys>\n");
}

void PerfProfiler::logTimes(long long start, long long end, long long span, unsigned long long totalSystem, const char *function) {
    stringstream str;
    LogBuilder::write(str, start, end, span, totalSystem, function);
    *logFile << str.str();
    logFile->flush();

    auto it = systemLogs.begin();
    while (it != systemLogs.end()) {
        str.str(std::string());
        SysLogBuilder::write(str, it->start, it->time, it->id);
        *sysLogFile << str.str();
        it++;
    }
    sysLogFile->flush();
}

void PerfProfiler::logSysTimes(long long start, unsigned long long time, unsigned int id) {
    systemLogs.emplace_back(SystemLog{id, start, time});
}
} // namespace OCLRT
