/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/sysman/test/unit_tests/sources/linux/tracefs_api/mock_tracefs_os_library.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

class PublicTraceFsApi : public L0::Sysman::TraceFsApi {
  public:
    using L0::Sysman::TraceFsApi::traceFsLibraryHandle;

    bool loadEntryPointsFromBase() {
        return L0::Sysman::TraceFsApi::loadEntryPoints();
    }

    bool allEntryPointsLoaded() const {
        return traceFsInstanceCreateEntry != nullptr &&
               traceFsInstanceDestroyEntry != nullptr &&
               traceFsInstanceFreeEntry != nullptr &&
               traceFsInstanceGetNameEntry != nullptr &&
               traceFsInstanceGetTraceDirEntry != nullptr &&
               traceFsInstanceFileOpenEntry != nullptr &&
               traceFsInstanceFileReadEntry != nullptr &&
               traceFsInstanceFileWriteEntry != nullptr &&
               traceFsInstanceFileAppendEntry != nullptr &&
               traceFsTraceOnEntry != nullptr &&
               traceFsTraceOffEntry != nullptr &&
               traceFsEventEnableEntry != nullptr &&
               traceFsEventDisableEntry != nullptr &&
               traceFsLocalEventsEntry != nullptr &&
               traceFsLocalEventsFreeEntry != nullptr &&
               traceFsInstanceGetBufferPercentEntry != nullptr &&
               traceFsInstanceSetBufferPercentEntry != nullptr &&
               traceFsInstanceGetBufferSizeEntry != nullptr &&
               traceFsInstanceSetBufferSizeEntry != nullptr &&
               traceFsInstanceGetFileEntry != nullptr &&
               traceFsGetTracingFileEntry != nullptr;
    }
};

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
};

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenMissingLibraryEntryPointThenVerifyLoadEntryPointsFails) {
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_create"));
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
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_local_events_free"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_get_buffer_percent"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_set_buffer_percent"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_get_buffer_size"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_set_buffer_size"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_instance_get_file"));
    EXPECT_FALSE(testLoadEntryPointsWithMissingFunction("tracefs_get_tracing_file"));
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

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceDestroyCalledThenVerifySuccess) {
    testTraceFsApi.traceFsInstanceDestroy(&MockTraceFsOsLibrary::mockTraceFsInstance);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenInstanceFreeCalledThenVerifySuccess) {
    testTraceFsApi.traceFsInstanceFree(&MockTraceFsOsLibrary::mockTraceFsInstance);
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

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenTraceOffCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsTraceOff(&MockTraceFsOsLibrary::mockTraceFsInstance);
    EXPECT_EQ(0, result);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenEventEnableCalledThenVerifyReturnValue) {
    int result = testTraceFsApi.traceFsEventEnable(&MockTraceFsOsLibrary::mockTraceFsInstance,
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

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLocalEventsCalledThenVerifyReturnValue) {
    struct tep_handle *tep = testTraceFsApi.traceFsLocalEvents(MockTraceFsOsLibrary::mockTraceDir);
    EXPECT_NE(nullptr, tep);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLocalEventsFreeCalledThenVerifySuccess) {
    testTraceFsApi.traceFsLocalEventsFree(&MockTraceFsOsLibrary::mockTepHandle);
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
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetFileCalledThenVerifyReturnValue) {
    char *file = testTraceFsApi.traceFsInstanceGetFile(&MockTraceFsOsLibrary::mockTraceFsInstance,
                                                       MockTraceFsOsLibrary::mockFileName);
    EXPECT_NE(nullptr, file);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenGetTracingFileCalledThenVerifyReturnValue) {
    char *file = testTraceFsApi.traceFsGetTracingFile(MockTraceFsOsLibrary::mockFileName);
    EXPECT_NE(nullptr, file);
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLibraryAvailableThenIsAvailableReturnsTrue) {
    EXPECT_TRUE(testTraceFsApi.isAvailable());
}

TEST_F(SysmanTraceFsApiFixture, GivenTraceFsApiWhenLibraryNotAvailableThenIsAvailableReturnsFalse) {
    testTraceFsApi.traceFsLibraryHandle.reset();
    EXPECT_FALSE(testTraceFsApi.isAvailable());
}

class SysmanTraceFsApiNullEntryFixture : public ::testing::Test {
  protected:
    PublicTraceFsApi testTraceFsApi;
};

TEST_F(SysmanTraceFsApiNullEntryFixture, GivenTraceFsApiWhenAllApisCalledWithNoEntryPointThenVerifyDefaultValuesReturned) {
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceCreate(MockTraceFsOsLibrary::mockInstanceName));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceGetName(nullptr));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceGetTraceDir(nullptr));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsLocalEvents(MockTraceFsOsLibrary::mockTraceDir));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceGetFile(nullptr, MockTraceFsOsLibrary::mockFileName));
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsGetTracingFile(MockTraceFsOsLibrary::mockFileName));

    int size = 0;
    EXPECT_EQ(nullptr, testTraceFsApi.traceFsInstanceFileRead(nullptr, MockTraceFsOsLibrary::mockFileName, &size));

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
    testTraceFsApi.traceFsLocalEventsFree(nullptr);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
