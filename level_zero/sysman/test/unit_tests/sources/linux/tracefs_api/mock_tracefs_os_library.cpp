/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/test/unit_tests/sources/linux/tracefs_api/mock_tracefs_os_library.h"

#include "gtest/gtest.h"

#include <cstring>

namespace L0 {
namespace Sysman {
namespace ult {

struct tracefs_instance MockTraceFsOsLibrary::mockTraceFsInstance;
struct tep_handle MockTraceFsOsLibrary::mockTepHandle;

extern "C" {

struct tracefs_instance *mockTraceFsInstanceCreate(const char *name) {
    if (name != nullptr) {
        EXPECT_STREQ(MockTraceFsOsLibrary::mockInstanceName, name);
    }
    return &MockTraceFsOsLibrary::mockTraceFsInstance;
}

void mockTraceFsInstanceDestroy(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
}

void mockTraceFsInstanceFree(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
}

const char *mockTraceFsInstanceGetName(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    return MockTraceFsOsLibrary::mockInstanceName;
}

const char *mockTraceFsInstanceGetTraceDir(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    return MockTraceFsOsLibrary::mockTraceDir;
}

int mockTraceFsInstanceFileOpen(struct tracefs_instance *instance, const char *file, int mode) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockFileName, file);
    EXPECT_EQ(MockTraceFsOsLibrary::mockFileMode, mode);
    return MockTraceFsOsLibrary::mockFileFd;
}

char *mockTraceFsInstanceFileRead(struct tracefs_instance *instance, const char *file, int *psize) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockFileName, file);
    if (psize != nullptr) {
        *psize = strlen(MockTraceFsOsLibrary::mockFileContent);
    }
    return const_cast<char *>(MockTraceFsOsLibrary::mockFileContent);
}

int mockTraceFsInstanceFileWrite(struct tracefs_instance *instance, const char *file, const char *str) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockFileName, file);
    EXPECT_NE(nullptr, str);
    return 0;
}

int mockTraceFsInstanceFileAppend(struct tracefs_instance *instance, const char *file, const char *str) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockFileName, file);
    EXPECT_NE(nullptr, str);
    return 0;
}

int mockTraceFsTraceOn(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    return 0;
}

int mockTraceFsTraceOff(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    return 0;
}

int mockTraceFsEventEnable(struct tracefs_instance *instance, const char *system, const char *event) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockSystemName, system);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockEventName, event);
    return 0;
}

int mockTraceFsEventDisable(struct tracefs_instance *instance, const char *system, const char *event) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockSystemName, system);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockEventName, event);
    return 0;
}

struct tep_handle *mockTraceFsLocalEvents(const char *tracingDir) {
    EXPECT_STREQ(MockTraceFsOsLibrary::mockTraceDir, tracingDir);
    return &MockTraceFsOsLibrary::mockTepHandle;
}

void mockTraceFsLocalEventsFree(struct tep_handle *tep) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTepHandle, tep);
}

int mockTraceFsInstanceGetBufferPercent(struct tracefs_instance *instance) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    return MockTraceFsOsLibrary::mockBufferPercent;
}

int mockTraceFsInstanceSetBufferPercent(struct tracefs_instance *instance, int val) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_EQ(MockTraceFsOsLibrary::mockBufferPercent, val);
    return 0;
}

long long mockTraceFsInstanceGetBufferSize(struct tracefs_instance *instance, int cpu) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_EQ(MockTraceFsOsLibrary::mockCpu, cpu);
    return MockTraceFsOsLibrary::mockBufferSize;
}

int mockTraceFsInstanceSetBufferSize(struct tracefs_instance *instance, size_t size, int cpu) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_EQ(MockTraceFsOsLibrary::mockSize, size);
    EXPECT_EQ(MockTraceFsOsLibrary::mockCpu, cpu);
    return 0;
}

char *mockTraceFsInstanceGetFile(struct tracefs_instance *instance, const char *file) {
    EXPECT_EQ(&MockTraceFsOsLibrary::mockTraceFsInstance, instance);
    EXPECT_STREQ(MockTraceFsOsLibrary::mockFileName, file);
    return const_cast<char *>(MockTraceFsOsLibrary::mockFileName);
}

char *mockTraceFsGetTracingFile(const char *file) {
    EXPECT_STREQ(MockTraceFsOsLibrary::mockFileName, file);
    return const_cast<char *>(MockTraceFsOsLibrary::mockFileName);
}
}

MockTraceFsOsLibrary::MockTraceFsOsLibrary() {
    funcMap["tracefs_instance_create"] = reinterpret_cast<void *>(mockTraceFsInstanceCreate);
    funcMap["tracefs_instance_destroy"] = reinterpret_cast<void *>(mockTraceFsInstanceDestroy);
    funcMap["tracefs_instance_free"] = reinterpret_cast<void *>(mockTraceFsInstanceFree);
    funcMap["tracefs_instance_get_name"] = reinterpret_cast<void *>(mockTraceFsInstanceGetName);
    funcMap["tracefs_instance_get_trace_dir"] = reinterpret_cast<void *>(mockTraceFsInstanceGetTraceDir);
    funcMap["tracefs_instance_file_open"] = reinterpret_cast<void *>(mockTraceFsInstanceFileOpen);
    funcMap["tracefs_instance_file_read"] = reinterpret_cast<void *>(mockTraceFsInstanceFileRead);
    funcMap["tracefs_instance_file_write"] = reinterpret_cast<void *>(mockTraceFsInstanceFileWrite);
    funcMap["tracefs_instance_file_append"] = reinterpret_cast<void *>(mockTraceFsInstanceFileAppend);
    funcMap["tracefs_trace_on"] = reinterpret_cast<void *>(mockTraceFsTraceOn);
    funcMap["tracefs_trace_off"] = reinterpret_cast<void *>(mockTraceFsTraceOff);
    funcMap["tracefs_event_enable"] = reinterpret_cast<void *>(mockTraceFsEventEnable);
    funcMap["tracefs_event_disable"] = reinterpret_cast<void *>(mockTraceFsEventDisable);
    funcMap["tracefs_local_events"] = reinterpret_cast<void *>(mockTraceFsLocalEvents);
    funcMap["tracefs_local_events_free"] = reinterpret_cast<void *>(mockTraceFsLocalEventsFree);
    funcMap["tracefs_instance_get_buffer_percent"] = reinterpret_cast<void *>(mockTraceFsInstanceGetBufferPercent);
    funcMap["tracefs_instance_set_buffer_percent"] = reinterpret_cast<void *>(mockTraceFsInstanceSetBufferPercent);
    funcMap["tracefs_instance_get_buffer_size"] = reinterpret_cast<void *>(mockTraceFsInstanceGetBufferSize);
    funcMap["tracefs_instance_set_buffer_size"] = reinterpret_cast<void *>(mockTraceFsInstanceSetBufferSize);
    funcMap["tracefs_instance_get_file"] = reinterpret_cast<void *>(mockTraceFsInstanceGetFile);
    funcMap["tracefs_get_tracing_file"] = reinterpret_cast<void *>(mockTraceFsGetTracingFile);
}

void *MockTraceFsOsLibrary::getProcAddress(const std::string &procName) {
    auto it = funcMap.find(procName);
    if (it != funcMap.end()) {
        return it->second;
    }
    return nullptr;
}

void MockTraceFsOsLibrary::deleteEntryPoint(const std::string &procName) {
    funcMap.erase(procName);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
