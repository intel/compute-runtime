/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/log_utility/linux/linux_logging.h"

#include <gtest/gtest.h>

namespace L0 {
namespace ult {
class MockNeoLogger : public NEO::LinuxLogger {
  public:
    bool mockSpdloggerObjectCallmade = false;
    bool mockSpdCreateCallmade = false;
    bool mockSpdTraceCallmade = false;
    bool mockSpdDebugCallmade = false;
    bool mockSpdInfoCallmade = false;
    bool mockSpdWarningCallmade = false;
    bool mockSpdErrorCallmade = false;
    bool mockSpdCriticalCallmade = false;
    bool mockSpdLevelCallmade = false;
    MockNeoLogger() : NEO::LinuxLogger() {
    }
    void setSpdLevel(uint32_t logLevel) override {
        mockSpdLevelCallmade = true;
    }
    void logSpdTrace(const char *msg) override {
        mockSpdTraceCallmade = true;
    }
    void logSpdDebug(const char *msg) override {
        mockSpdDebugCallmade = true;
    }
    void logSpdInfo(const char *msg) override {
        mockSpdInfoCallmade = true;
    }
    void logSpdWarning(const char *msg) override {
        mockSpdWarningCallmade = true;
    }
    void logSpdError(const char *msg) override {
        mockSpdErrorCallmade = true;
    }
    void logSpdFatal(const char *msg) override {
        mockSpdCriticalCallmade = true;
    }
    int32_t createSpdLogger(std::string loggerName, std::string filepath) override {
        mockSpdCreateCallmade = true;
        return 0;
    }
};

class CoreLoggingFixture : public ::testing::Test {
  protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(CoreLoggingFixture, GivenMockLoggerObjectWhenSpdLoggerObjectIsNullThenNoLoggingCallsAreMade) {
    MockNeoLogger logger;
    int32_t ret = logger.createLogger("sample", "sample.log", 3);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(true, logger.mockSpdCreateCallmade);
    logger.setLevel(3);
    EXPECT_EQ(false, logger.mockSpdLevelCallmade);
    logger.logTrace("test");
    EXPECT_EQ(false, logger.mockSpdTraceCallmade);
    logger.logDebug("test");
    EXPECT_EQ(false, logger.mockSpdDebugCallmade);
    logger.logInfo("test");
    EXPECT_EQ(false, logger.mockSpdInfoCallmade);
    logger.logWarning("test");
    EXPECT_EQ(false, logger.mockSpdWarningCallmade);
    logger.logError("test");
    EXPECT_EQ(false, logger.mockSpdErrorCallmade);
    logger.logFatal("test");
    EXPECT_EQ(false, logger.mockSpdCriticalCallmade);
}

} // namespace ult
} // namespace L0
