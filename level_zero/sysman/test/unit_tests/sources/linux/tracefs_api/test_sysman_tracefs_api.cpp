/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/test/unit_tests/sources/linux/tracefs_api/mock_tracefs_api.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/tracefs_api/mock_tracefs_os_library.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

class SysmanTraceFsApiFixture : public ::testing::Test {
  protected:
    PublicTraceFsApi testTraceFsApi;

    void SetUp() override {
        auto mockTraceFsOsLibrary = std::make_unique<MockTraceFsOsLibrary>();

        testTraceFsApi.traceFsLibraryHandle = std::move(mockTraceFsOsLibrary);
        EXPECT_TRUE(testTraceFsApi.loadEntryPointsFromBase());
        EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    }

    void TearDown() override {
    }

    bool testLoadEntryPointsWithMissingFunction(const std::string &procName) {
        PublicTraceFsApi localTraceFsApi;
        auto mockTraceFsOsLibrary = std::make_unique<MockTraceFsOsLibrary>();
        mockTraceFsOsLibrary->deleteEntryPoint(procName);
        localTraceFsApi.traceFsLibraryHandle = std::move(mockTraceFsOsLibrary);

        return localTraceFsApi.loadEntryPointsFromBase();
    }

    void loadEntryPointsWithMissingFunction(PublicTraceFsApi &traceFsApi, const std::string &procName) {
        auto mockTraceFsOsLibrary = std::make_unique<MockTraceFsOsLibrary>();
        mockTraceFsOsLibrary->deleteEntryPoint(procName);
        traceFsApi.traceFsLibraryHandle = std::move(mockTraceFsOsLibrary);

        EXPECT_FALSE(traceFsApi.loadEntryPointsFromBase());
    }
};

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenMissingLibraryEntryPointThenVerifyLoadEntryPointsFails) {
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_create"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_is_new"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_destroy"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_free"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_get_name"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_get_trace_dir"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_file_open"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_file_read"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_file_write"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_file_append"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_trace_on"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_trace_off"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_event_enable"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_event_disable"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_local_events"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_get_file"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_get_tracing_file"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_put_tracing_file"));
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenMissingLibraryHandleThenVerifyLoadEntryPointsFails) {
    testTraceFsApi.traceFsLibraryHandle.reset();
    EXPECT_FALSE(testTraceFsApi.loadEntryPointsFromBase());
}

TEST_F(SysmanTraceFsApiFixture, GivenNullTraceFsLibraryHandleWhenLoadEntryPointsSucceedsThenVerifyLoadEntryPointsSucceeds) {
    testTraceFsApi.traceFsLibraryHandle.reset();

    auto mockLoadFunc = [](const NEO::OsLibraryCreateProperties &) -> NEO::OsLibrary * {
        return new MockTraceFsOsLibrary();
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup(&NEO::OsLibrary::loadFunc, mockLoadFunc);

    EXPECT_TRUE(testTraceFsApi.loadEntryPointsFromBase());
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceCreateCalledThenVerifyReturnValue) {
    auto instance = testTraceFsApi.traceFsInstanceCreate(MockTraceFsOsLibrary::mockInstanceName);
    EXPECT_NE(nullptr, instance);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceCreateCalledWithLoadedEntryPointThenVerifyEntryPointIsCalledAndValidInstanceReturned) {
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    auto instance = testTraceFsApi.traceFsInstanceCreateBase(MockTraceFsOsLibrary::mockInstanceName);
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceIsNewCalledThenWhatTheLibraryReportsIsReturned) {
    VariableBackup<bool> instanceIsNewBackup(&MockTraceFsOsLibrary::mockInstanceIsNew, true);
    EXPECT_TRUE(testTraceFsApi.traceFsInstanceIsNew(&MockTraceFsOsLibrary::mockTraceFsInstance));

    MockTraceFsOsLibrary::mockInstanceIsNew = false;
    EXPECT_FALSE(testTraceFsApi.traceFsInstanceIsNew(&MockTraceFsOsLibrary::mockTraceFsInstance));
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceDestroyCalledThenVerifySuccess) {
    testTraceFsApi.traceFsInstanceDestroy(&MockTraceFsOsLibrary::mockTraceFsInstance);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceDestroyCalledWithLoadedEntryPointThenVerifyEntryPointIsCalled) {
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    testTraceFsApi.traceFsInstanceDestroyBase(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(1u, MockTraceFsOsLibrary::instanceDestroyCallCount);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceDestroyCalledWithoutEntryPointThenVerifyEntryPointIsNotCalled) {
    PublicTraceFsApi localTraceFsApi;
    loadEntryPointsWithMissingFunction(localTraceFsApi, "tracefs_instance_destroy");

    localTraceFsApi.traceFsInstanceDestroyBase(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0u, MockTraceFsOsLibrary::instanceDestroyCallCount);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceFreeCalledThenVerifySuccess) {
    testTraceFsApi.traceFsInstanceFree(&MockTraceFsOsLibrary::mockTraceFsInstance);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceFreeCalledWithLoadedEntryPointThenVerifyEntryPointIsCalled) {
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    testTraceFsApi.traceFsInstanceFreeBase(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(1u, MockTraceFsOsLibrary::instanceFreeCallCount);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceFreeCalledWithoutEntryPointThenVerifyEntryPointIsNotCalled) {
    PublicTraceFsApi localTraceFsApi;
    loadEntryPointsWithMissingFunction(localTraceFsApi, "tracefs_instance_free");

    localTraceFsApi.traceFsInstanceFreeBase(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0u, MockTraceFsOsLibrary::instanceFreeCallCount);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetInstanceNameCalledThenVerifyReturnValue) {
    const char *name = testTraceFsApi.traceFsInstanceGetName(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockInstanceName, name);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetTraceDirCalledThenVerifyReturnValue) {
    const char *dir = testTraceFsApi.traceFsInstanceGetTraceDir(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockTraceDir, dir);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenFileOpenCalledThenVerifyReturnValue) {
    int fd = testTraceFsApi.traceFsInstanceFileOpen(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                    MockTraceFsOsLibrary::mockFileName,
                                                    MockTraceFsOsLibrary::mockFileMode);
    EXPECT_EQ(MockTraceFsOsLibrary::mockFileFd, fd);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenFileReadCalledThenVerifyReturnValue) {
    int size = 0;
    char *content = testTraceFsApi.traceFsInstanceFileRead(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                           MockTraceFsOsLibrary::mockFileName,
                                                           &size);
    EXPECT_NE(nullptr, content);
    EXPECT_GT(size, 0);
    free(content);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenFileWriteCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsInstanceFileWrite(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                         MockTraceFsOsLibrary::mockFileName,
                                                         "test_data");
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenFileAppendCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsInstanceFileAppend(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                          MockTraceFsOsLibrary::mockFileName,
                                                          "test_data");
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenTraceOnCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsTraceOn(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenTraceOnCalledWithLoadedEntryPointThenVerifyEntryPointIsCalledAndValidValueReturned) {

    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    int result = testTraceFsApi.traceFsTraceOnBase(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenTraceOffCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsTraceOff(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenTraceOffCalledWithLoadedEntryPointThenVerifyEntryPointIsCalledAndValidValueReturned) {
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    int result = testTraceFsApi.traceFsTraceOffBase(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenEventEnableCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsEventEnable(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                   MockTraceFsOsLibrary::mockSystemName,
                                                   MockTraceFsOsLibrary::mockEventName);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenEventEnableCalledWithLoadedEntryPointThenVerifyEntryPointIsCalledAndValidValueReturned) {
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    int result = testTraceFsApi.traceFsEventEnableBase(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                       MockTraceFsOsLibrary::mockSystemName,
                                                       MockTraceFsOsLibrary::mockEventName);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenEventDisableCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsEventDisable(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                    MockTraceFsOsLibrary::mockSystemName,
                                                    MockTraceFsOsLibrary::mockEventName);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenEventDisableCalledWithLoadedEntryPointThenVerifyEntryPointIsCalledAndValidValueReturned) {
    EXPECT_TRUE(testTraceFsApi.allEntryPointsLoaded());
    int result = testTraceFsApi.traceFsEventDisableBase(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                        MockTraceFsOsLibrary::mockSystemName,
                                                        MockTraceFsOsLibrary::mockEventName);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLocalEventsCalledThenVerifyReturnValue) {
    struct tep_handle *tep = testTraceFsApi.traceFsLocalEvents(MockTraceFsOsLibrary::mockTraceDir);
    EXPECT_NE(nullptr, tep);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetBufferPercentCalledThenVerifyReturnValue) {
    int percent = testTraceFsApi.traceFsInstanceGetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, percent);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenSetBufferPercentCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsInstanceSetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                                MockTraceFsOsLibrary::mockBufferPercent);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetBufferSizeCalledThenVerifyReturnValue) {
    long long size = testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                                 MockTraceFsOsLibrary::mockCpu);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferSize, size);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenSetBufferSizeCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsInstanceSetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                             MockTraceFsOsLibrary::mockSize,
                                                             MockTraceFsOsLibrary::mockCpu);
    EXPECT_EQ(0, result);
    EXPECT_EQ(MockTraceFsOsLibrary::mockSize, PublicTraceFsApi::lastSetBufferSize);
    EXPECT_EQ(MockTraceFsOsLibrary::mockCpu, PublicTraceFsApi::lastSetBufferSizeCpu);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetFileCalledThenVerifyReturnValue) {
    char *file = testTraceFsApi.traceFsInstanceGetFile(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                       MockTraceFsOsLibrary::mockFileName);
    EXPECT_NE(nullptr, file);
    free(file);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetTracingFileCalledThenVerifyReturnValue) {
    char *file = testTraceFsApi.traceFsGetTracingFile(MockTraceFsOsLibrary::mockFileName);
    EXPECT_NE(nullptr, file);
    free(file);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenPutTracingFileCalledWithoutEntryPointThenVerifyEntryPointIsNotCalled) {
    PublicTraceFsApi localTraceFsApi;
    loadEntryPointsWithMissingFunction(localTraceFsApi, "tracefs_put_tracing_file");

    localTraceFsApi.traceFsPutTracingFile(nullptr);
    EXPECT_EQ(0u, MockTraceFsOsLibrary::putTracingFileCallCount);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLibraryAvailableThenIsAvailableReturnsTrue) {
    EXPECT_TRUE(testTraceFsApi.isAvailable());
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLibraryNotAvailableThenIsAvailableReturnsFalse) {
    testTraceFsApi.traceFsLibraryHandle.reset();
    EXPECT_FALSE(testTraceFsApi.isAvailable());
}

class TraceFsApiWithFileAccessBehavior : public L0::Sysman::TraceFsApi {
  public:
    const char *fileContent = nullptr;
    int fileWriteResult = 0;
    std::string lastFileRead;
    std::string lastFileWritten;
    std::string lastValueWritten;

    char *traceFsInstanceFileRead(struct tracefs_instance *instance, const char *file, int *psize) override {
        lastFileRead = file;
        if (fileContent == nullptr) {
            return nullptr;
        }
        if (psize != nullptr) {
            *psize = static_cast<int>(strlen(fileContent));
        }
        return strdup(fileContent);
    }

    int traceFsInstanceFileWrite(struct tracefs_instance *instance, const char *file, const char *str) override {
        lastFileWritten = file;
        lastValueWritten = str;
        return fileWriteResult;
    }
};

class SysmanTraceFsApiFileAccessFixture : public ::testing::Test {
  protected:
    TraceFsApiWithFileAccessBehavior testTraceFsApi;
};

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsApiWhenGetBufferPercentCalledThenTheBufferPercentFileIsRead) {
    testTraceFsApi.fileContent = "40";

    EXPECT_EQ(40, testTraceFsApi.traceFsInstanceGetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance));
    EXPECT_EQ("buffer_percent", testTraceFsApi.lastFileRead);
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsApiWhenSetBufferPercentCalledThenTheValueIsWrittenToTheBufferPercentFile) {
    testTraceFsApi.fileWriteResult = 1;

    EXPECT_EQ(0, testTraceFsApi.traceFsInstanceSetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance, 0));
    EXPECT_EQ("buffer_percent", testTraceFsApi.lastFileWritten);
    EXPECT_EQ("0", testTraceFsApi.lastValueWritten);
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsApiWhenGetBufferSizeCalledThenTheRequestedCpuSelectsTheFileRead) {
    testTraceFsApi.fileContent = "4096";

    EXPECT_EQ(4096LL, testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, -1));
    EXPECT_EQ("buffer_total_size_kb", testTraceFsApi.lastFileRead);

    EXPECT_EQ(4096LL, testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, 3));
    EXPECT_EQ("per_cpu/cpu3/buffer_size_kb", testTraceFsApi.lastFileRead);
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsApiWhenSetBufferSizeCalledThenTheRequestedCpuSelectsTheFileWritten) {
    testTraceFsApi.fileWriteResult = 3;

    EXPECT_EQ(0, testTraceFsApi.traceFsInstanceSetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, 512, -1));
    EXPECT_EQ("buffer_size_kb", testTraceFsApi.lastFileWritten);
    EXPECT_EQ("512", testTraceFsApi.lastValueWritten);

    EXPECT_EQ(0, testTraceFsApi.traceFsInstanceSetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, 512, 2));
    EXPECT_EQ("per_cpu/cpu2/buffer_size_kb", testTraceFsApi.lastFileWritten);
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsFileCannotBeReadWhenGettingBufferPercentAndBufferSizeThenErrorIsReturned) {
    testTraceFsApi.fileContent = nullptr;

    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceGetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance));
    EXPECT_EQ(-1LL, testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, -1));
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsFileDoesNotHoldANumberWhenGettingBufferPercentAndBufferSizeThenErrorIsReturned) {
    testTraceFsApi.fileContent = "not a number";

    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceGetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance));
    EXPECT_EQ(-1LL, testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, -1));

    testTraceFsApi.fileContent = "";

    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceGetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance));
    EXPECT_EQ(-1LL, testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, -1));
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenTraceFsFileHoldsANumberOutOfRangeWhenGettingBufferPercentAndBufferSizeThenErrorIsReturned) {
    testTraceFsApi.fileContent = "-99999999999999999999999999";

    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceGetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance));
    EXPECT_EQ(-1LL, testTraceFsApi.traceFsInstanceGetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, -1));
}

TEST_F(SysmanTraceFsApiFileAccessFixture, GivenNothingIsWrittenToTheTraceFsFileWhenSettingBufferPercentAndBufferSizeThenErrorIsReturned) {
    testTraceFsApi.fileWriteResult = 0;

    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceSetBufferPercent(&MockTraceFsOsLibrary::mockTraceFsInstance, 0));
    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceSetBufferSize(&MockTraceFsOsLibrary::mockTraceFsInstance, 512, -1));
}

class SysmanTraceFsApiNullEntryFixture : public ::testing::Test {
  protected:
    PublicTraceFsApi testTraceFsApi;
};

TEST_F(SysmanTraceFsApiNullEntryFixture, GivenTraceFsApiWhenAllApisCalledWithNoEntryPointThenVerifyDefaultValuesReturned) {
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceCreate(MockTraceFsOsLibrary::mockInstanceName));
    VariableBackup<bool> instanceIsNewBackup(&MockTraceFsOsLibrary::mockInstanceIsNew, true);
    EXPECT_FALSE(testTraceFsApi.traceFsInstanceIsNew(nullptr));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceGetName(nullptr));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceGetTraceDir(nullptr));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsLocalEvents(MockTraceFsOsLibrary::mockTraceDir));

    auto instanceFile = std::unique_ptr<char, decltype(&free)>(
        testTraceFsApi.traceFsInstanceGetFile(nullptr, MockTraceFsOsLibrary::mockFileName), free);
    EXPECT_EQ(nullptr, instanceFile);

    auto tracingFile = std::unique_ptr<char, decltype(&free)>(
        testTraceFsApi.traceFsGetTracingFile(MockTraceFsOsLibrary::mockFileName), free);
    EXPECT_EQ(nullptr, tracingFile);

    int size = 0;
    auto content = std::unique_ptr<char, decltype(&free)>(
        testTraceFsApi.traceFsInstanceFileRead(nullptr, MockTraceFsOsLibrary::mockFileName, &size), free);
    EXPECT_EQ(nullptr, content);

    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceFileOpen(nullptr, MockTraceFsOsLibrary::mockFileName, MockTraceFsOsLibrary::mockFileMode));
    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceFileWrite(nullptr, MockTraceFsOsLibrary::mockFileName, "test_data"));
    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceFileAppend(nullptr, MockTraceFsOsLibrary::mockFileName, "test_data"));
    EXPECT_EQ(-1, testTraceFsApi.traceFsTraceOn(nullptr));
    EXPECT_EQ(-1, testTraceFsApi.traceFsTraceOff(nullptr));
    EXPECT_EQ(-1, testTraceFsApi.traceFsEventEnable(nullptr, MockTraceFsOsLibrary::mockSystemName, MockTraceFsOsLibrary::mockEventName));
    EXPECT_EQ(-1, testTraceFsApi.traceFsEventDisable(nullptr, MockTraceFsOsLibrary::mockSystemName, MockTraceFsOsLibrary::mockEventName));
    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceGetBufferPercent(nullptr));
    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceSetBufferPercent(nullptr, MockTraceFsOsLibrary::mockBufferPercent));
    EXPECT_EQ(-1LL, testTraceFsApi.traceFsInstanceGetBufferSize(nullptr, MockTraceFsOsLibrary::mockCpu));
    EXPECT_EQ(-1, testTraceFsApi.traceFsInstanceSetBufferSize(nullptr, MockTraceFsOsLibrary::mockSize, MockTraceFsOsLibrary::mockCpu));

    testTraceFsApi.traceFsInstanceDestroy(nullptr);
    testTraceFsApi.traceFsInstanceFree(nullptr);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
