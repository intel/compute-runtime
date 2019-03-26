/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/utilities/perf_profiler.h"

#include "test.h"

#include "gtest/gtest.h"

#include <chrono>
#include <thread>

using namespace NEO;

using namespace std;

TEST(PerfProfiler, create) {
    PerfProfiler *ptr = PerfProfiler::create();
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());
    PerfProfiler *ptr2 = PerfProfiler::create();
    EXPECT_EQ(ptr, ptr2);
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());

    PerfProfiler::destroyAll();
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
    EXPECT_EQ(nullptr, PerfProfiler::getObject(0));
}

TEST(PerfProfiler, createDestroyCreate) {
    // purpose of this test is multiple create and destroy, so check the state machine works correctly
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
    PerfProfiler *ptr = PerfProfiler::create();
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());
    EXPECT_EQ(ptr, PerfProfiler::getObject(0));
    PerfProfiler::destroyAll();
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
    EXPECT_EQ(nullptr, PerfProfiler::getObject(0));
    PerfProfiler *ptr2 = PerfProfiler::create();
    ASSERT_NE(nullptr, ptr2);
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());
    EXPECT_EQ(ptr2, PerfProfiler::getObject(0));

    uint32_t systemId = 1;
    const char *func = "createDestroyCreate()";

    ptr2->apiEnter();
    ptr2->systemEnter();
    ptr2->systemLeave(systemId);
    ptr2->apiLeave(func);

    PerfProfiler::destroyAll();
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
    EXPECT_EQ(nullptr, PerfProfiler::getObject(0));
}

TEST(PerfProfiler, destroyAll) {
    struct PerfProfilerMock : PerfProfiler {
        static void addNullObjects() {
            PerfProfiler::objects[0] = nullptr;
            PerfProfiler::counter = 1;
        }
    };

    PerfProfiler::destroyAll(); // destroy 0 objects
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
    PerfProfilerMock::addNullObjects(); // skip null objects
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());
    PerfProfiler::destroyAll(); //destroy no object altough counter is incorrect
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
    EXPECT_EQ(nullptr, PerfProfiler::getObject(0));
}

TEST(PerfProfiler, PerfProfilerXmlVerifier) {
    std::unique_ptr<std::stringstream> logs = std::unique_ptr<std::stringstream>(new std::stringstream());
    std::unique_ptr<std::stringstream> sysLogs = std::unique_ptr<std::stringstream>(new std::stringstream());
    std::unique_ptr<PerfProfiler> ptr(new PerfProfiler(1, std::move(logs), std::move(sysLogs)));
    ASSERT_NE(nullptr, ptr.get());

    uint32_t systemId = 10;
    const std::string func = "PerfProfilerXmlVerifier()";

    ptr->apiEnter();
    ptr->systemEnter();
    ptr->systemLeave(systemId);
    ptr->apiLeave(func.c_str());

    {
        std::stringstream logDump{static_cast<std::stringstream *>(ptr->getLogStream())->str()};
        bool caughtException = false;
        try {
            long long startR = -1;
            long long endR = -1;
            long long spanR = -1;
            unsigned long long totalSystemR = UINT64_MAX;
            std::string functionR = "";
            PerfProfiler::readAndVerify(logDump, "<report>\n");
            PerfProfiler::LogBuilder::read(logDump, startR, endR, spanR, totalSystemR, functionR);
            char c = 0;
            logDump.read(&c, 1);
            EXPECT_TRUE(logDump.eof());
            EXPECT_EQ(func, functionR);
            EXPECT_LE(0, startR);
            EXPECT_LE(0, endR);
            EXPECT_LE(startR, endR);
            EXPECT_LE(0, spanR);
        } catch (...) {
            caughtException = true;
        }
        EXPECT_FALSE(caughtException);
    }

    {
        std::stringstream sysLogDump{static_cast<std::stringstream *>(ptr->getSystemLogStream())->str()};
        bool caughtException = false;
        try {
            long long startR = -1;
            unsigned int funcId = 0;
            unsigned long long totalSystemR = UINT64_MAX;
            PerfProfiler::readAndVerify(sysLogDump, "<report>\n");
            PerfProfiler::SysLogBuilder::read(sysLogDump, startR, totalSystemR, funcId);
            char c = 0;
            sysLogDump.read(&c, 1);
            EXPECT_TRUE(sysLogDump.eof());
            EXPECT_LE(0, startR);
            EXPECT_EQ(systemId, funcId);
        } catch (...) {
            caughtException = true;
        }
        EXPECT_FALSE(caughtException);
    }
}

TEST(PerfProfiler, ApiEnterLeave) {
    PerfProfiler *ptr = PerfProfiler::create(false);
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());
    const char *func = "ApiEnterLeave()";
    ptr->apiEnter();
    ptr->apiLeave(func);
    {
        bool caughtException = false;
        try {
            std::stringstream logDump{static_cast<std::stringstream *>(ptr->getLogStream())->str()};
            std::string functionR = "";
            long long startR = -1;
            long long endR = -1;
            long long spanR = -1;
            unsigned long long totalSystemR = UINT64_MAX;
            PerfProfiler::readAndVerify(logDump, "<report>\n");
            PerfProfiler::LogBuilder::read(logDump, startR, endR, spanR, totalSystemR, functionR);
            EXPECT_EQ(func, functionR);
        } catch (...) {
            caughtException = true;
        }
        EXPECT_FALSE(caughtException);
    }

    PerfProfiler::destroyAll();
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
}

TEST(PerfProfiler, SystemEnterLeave) {
    PerfProfiler *ptr = PerfProfiler::create(false);
    ASSERT_NE(nullptr, ptr);
    EXPECT_EQ(1, PerfProfiler::getCurrentCounter());

    uint32_t systemId = 3;

    ptr->systemEnter();
    ptr->systemLeave(systemId);

    {
        std::string sysString = static_cast<std::stringstream *>(ptr->getSystemLogStream())->str();
        std::stringstream sysLogDump{sysString};
        bool caughtException = false;
        try {
            PerfProfiler::readAndVerify(sysLogDump, "<report>\n");
            char c = 0;
            sysLogDump.read(&c, 1);
            EXPECT_TRUE(sysLogDump.eof());
        } catch (...) {
            caughtException = true;
        }
        EXPECT_FALSE(caughtException);
    }

    PerfProfiler::destroyAll();
    EXPECT_EQ(0, PerfProfiler::getCurrentCounter());
}

TEST(PerfProfiler, readAndVerify) {
    std::string log = "someData";
    std::stringstream in{log + log};
    bool exceptionCaught = false;
    try {
        PerfProfiler::readAndVerify(in, "some");
        EXPECT_FALSE(in.eof());
        PerfProfiler::readAndVerify(in, "Data");
        EXPECT_FALSE(in.eof());
        PerfProfiler::readAndVerify(in, "some");
        EXPECT_FALSE(in.eof());
        PerfProfiler::readAndVerify(in, "Data");
    } catch (const std::runtime_error &) {
        exceptionCaught = true;
    }
    EXPECT_FALSE(exceptionCaught);

    try {
        PerfProfiler::readAndVerify(in, "anything");
    } catch (const std::runtime_error &) {
        exceptionCaught = true;
    }
    EXPECT_TRUE(exceptionCaught);

    exceptionCaught = false;
    std::stringstream in2{"someData"};
    try {
        PerfProfiler::readAndVerify(in2, "somXXata");
    } catch (const std::runtime_error &) {
        exceptionCaught = true;
    }
    EXPECT_TRUE(exceptionCaught);

    exceptionCaught = false;
    std::stringstream in3{"someData"};
    try {
        PerfProfiler::readAndVerify(in3, "someDataX");
    } catch (const std::runtime_error &) {
        exceptionCaught = true;
    }
    EXPECT_TRUE(exceptionCaught);
}

TEST(PerfProfiler, LogBuilderReadAndWrite) {
    std::stringstream out;
    long long startW = 3, startR = 0;
    long long endW = 5, endR = 0;
    long long spanW = 7, spanR = 0;
    unsigned long long totalSystemW = 11, totalSystemR = 0;
    std::string functionW = {"someFunc"}, functionR;
    PerfProfiler::LogBuilder::write(out, startW, endW, spanW, totalSystemW, functionW.c_str());

    std::stringstream in(out.str());
    PerfProfiler::LogBuilder::read(in, startR, endR, spanR, totalSystemR, functionR);

    char end = 0;
    in.read(&end, 1);
    EXPECT_TRUE(in.eof());
    EXPECT_EQ(startW, startR);
    EXPECT_EQ(endW, endR);
    EXPECT_EQ(spanW, spanR);
    EXPECT_EQ(totalSystemW, totalSystemR);
    EXPECT_EQ(functionW, functionR);
}

TEST(PerfProfiler, LogBuilderGivenLogWithBrokenFunctionNameWhenCantFindTrailingQuotationThenWillThrowException) {
    std::stringstream in{"<api name=\"funcName"};
    long long startR = 0;
    long long endR = 0;
    long long spanR = 0;
    unsigned long long totalSystemR = 0;
    std::string functionR;

    bool exceptionCaught = false;
    try {
        PerfProfiler::LogBuilder::read(in, startR, endR, spanR, totalSystemR, functionR);
    } catch (const std::runtime_error &) {
        exceptionCaught = true;
    }
    EXPECT_TRUE(exceptionCaught);
}

TEST(PerfProfiler, SysLogBuilderReadAndWrite) {
    std::stringstream out;
    long long startW = 3, startR = 0;
    unsigned long long timeW = 7, timeR = 0;
    unsigned int idW = 11, idR = 0;
    PerfProfiler::SysLogBuilder::write(out, startW, timeW, idW);

    std::stringstream in(out.str());
    PerfProfiler::SysLogBuilder::read(in, startR, timeR, idR);
    char end = 0;
    in.read(&end, 1);
    EXPECT_TRUE(in.eof());
    EXPECT_EQ(startW, startR);
    EXPECT_EQ(timeW, timeR);
    EXPECT_EQ(idW, idR);
}
