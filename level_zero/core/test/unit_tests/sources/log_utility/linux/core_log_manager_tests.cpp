/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/log_utility/linux/linux_log_manager.h"

#include <gtest/gtest.h>

namespace L0 {

namespace ult {
bool mockLoggerObject = true;

class MockLogger : public NEO::LinuxLogger {
  public:
    MockLogger() : NEO::LinuxLogger() {
    }

    int32_t createLogger(std::string loggerName, std::string filename, uint32_t logLevel) override {
        if (mockLoggerObject) {
            return 0;
        } else {
            return -1;
        }
    }

    void setLevel(uint32_t logLevel) override {
    }
    void logTrace(const char *msg) override {
    }
    void logDebug(const char *msg) override {
    }
    void logInfo(const char *msg) override {
    }
    void logWarning(const char *msg) override {
    }
    void logError(const char *msg) override {
    }
    void logFatal(const char *msg) override {
    }
};

class MockLogManager : public NEO::LinuxLogManager {
  public:
    bool mockCreateLogger = true;

    std::unique_ptr<NEO::LinuxLogger> createLoggerByType(LogManager::LogType logType, std::string filename, uint32_t logLevel) override {
        if (mockCreateLogger) {
            auto logObj = std::make_unique<L0::ult::MockLogger>();
            logObj->createLogger(getLoggerName(logType), filename, logLevel);
            return logObj;
        }
        return nullptr;
    }
};

class MockLoggerManager : public NEO::LinuxLogManager {
  public:
    bool mockGetLogger = true;

    std::unique_ptr<NEO::LinuxLogger> prepareLogger() override {
        return std::make_unique<MockLogger>();
    }
};

class LogManagerCoreTests : public ::testing::Test {
  protected:
    void SetUp() override {
    }

    void TearDown() override {
    }
};

TEST_F(LogManagerCoreTests, GivenLogManagerObjectWhenCallingGetLoggerNameThenCorrectNameIsReturned) {
    auto logManager = L0::ult::MockLogManager();
    EXPECT_EQ("coreLogger", logManager.getLoggerName(NEO::LogManager::LogType::coreLogger));
    EXPECT_EQ("", logManager.getLoggerName(NEO::LogManager::LogType::logMax));
}

TEST_F(LogManagerCoreTests, GivenLogManagerObjectWhenCreateLoggerByTypeFailsThenLoggerIsNotFoundWhenQueried) {
    auto logManager = L0::ult::MockLogManager();
    logManager.mockCreateLogger = false;

    logManager.createLogger(NEO::LogManager::LogType::coreLogger, "coreLogger.log", 3);
    EXPECT_EQ(nullptr, logManager.getLogger(NEO::LogManager::LogType::coreLogger));
    logManager.mockCreateLogger = true;
}

TEST_F(LogManagerCoreTests, GivenLogManagerObjectWhenLogsAtMultipleLevelAreIssuedThenOperationsCompleteSuccessfully) {
    const char *msg = "Test msg";
    auto logManager = L0::ult::MockLogManager();
    logManager.createLogger(NEO::LogManager::LogType::coreLogger, "coreLogger.log", 3);
    EXPECT_NE(nullptr, logManager.getLogger(NEO::LogManager::LogType::coreLogger));

    logManager.getLogger(NEO::LogManager::LogType::coreLogger)->logTrace(msg);
    logManager.getLogger(NEO::LogManager::LogType::coreLogger)->logDebug(msg);
    logManager.getLogger(NEO::LogManager::LogType::coreLogger)->logInfo(msg);
    logManager.getLogger(NEO::LogManager::LogType::coreLogger)->logWarning(msg);
    logManager.getLogger(NEO::LogManager::LogType::coreLogger)->logError(msg);
    logManager.getLogger(NEO::LogManager::LogType::coreLogger)->logFatal(msg);

    logManager.destroyLogger(NEO::LogManager::LogType::coreLogger);
}

TEST_F(LogManagerCoreTests, GivenLogManagerObjectWhenCallingCreateLoggerByTypeAndLogCreationFailsThenNoLoggerIsFound) {
    auto logManager = MockLoggerManager();
    mockLoggerObject = false;
    logManager.createLogger(NEO::LogManager::LogType::coreLogger, "coreLogger.log", 3);
    EXPECT_EQ(nullptr, logManager.getLogger(NEO::LogManager::LogType::coreLogger));
    mockLoggerObject = true;
}

TEST_F(LogManagerCoreTests, GivenLogManagerObjectWhenCallingCreateLoggerByTypeThenSuccessIsReturned) {
    auto logManager = MockLoggerManager();
    logManager.createLogger(NEO::LogManager::LogType::coreLogger, "coreLogger.log", 3);
    EXPECT_NE(nullptr, logManager.getLogger(NEO::LogManager::LogType::coreLogger));
    logManager.destroyLogger(NEO::LogManager::LogType::coreLogger);
}

} // namespace ult
} // namespace L0
