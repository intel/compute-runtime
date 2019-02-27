/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/source_level_debugger/source_level_debugger.h"

#include "gmock/gmock.h"

#include <string>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace OCLRT;

class MockSourceLevelDebugger : public SourceLevelDebugger {
  public:
    using SourceLevelDebugger::debuggerLibrary;
    using SourceLevelDebugger::deviceHandle;

    MockSourceLevelDebugger() : SourceLevelDebugger(SourceLevelDebugger::loadDebugger()) {
        this->deviceHandle = mockDeviceHandle;
    }

    MockSourceLevelDebugger(OsLibrary *library) : SourceLevelDebugger(library) {
        this->deviceHandle = mockDeviceHandle;
    }

    void setActive(bool active) {
        isActive = active;
    }
    static const uint32_t mockDeviceHandle = 23;
};

class MockActiveSourceLevelDebugger : public SourceLevelDebugger {
  public:
    using SourceLevelDebugger::debuggerLibrary;
    using SourceLevelDebugger::deviceHandle;

    MockActiveSourceLevelDebugger() : SourceLevelDebugger(nullptr) {
        this->deviceHandle = mockDeviceHandle;
        isActive = true;
    }

    MockActiveSourceLevelDebugger(OsLibrary *library) : SourceLevelDebugger(library) {
        this->deviceHandle = mockDeviceHandle;
        isActive = true;
    }

    void setActive(bool active) {
        isActive = active;
    }
    bool isOptimizationDisabled() const override {
        return isOptDisabled;
    }
    bool isDebuggerActive() override {
        return isActive;
    }
    bool notifyNewDevice(uint32_t deviceHandle) override {
        return false;
    }
    bool notifyDeviceDestruction() override {
        return true;
    }
    bool notifySourceCode(const char *sourceCode, size_t size, std::string &filename) const override {
        filename = sourceCodeFilename;
        return true;
    }
    bool notifyKernelDebugData(const KernelInfo *kernelInfo) const override {
        return false;
    }
    bool initialize(bool useLocalMemory) override {
        return false;
    }

    static const uint32_t mockDeviceHandle = 23;
    bool isOptDisabled = false;
    std::string sourceCodeFilename;
};

class GMockSourceLevelDebugger : public SourceLevelDebugger {
  public:
    GMockSourceLevelDebugger(OsLibrary *library) : SourceLevelDebugger(library) {
    }
    void setActive(bool active) {
        isActive = active;
    }

    bool isDebuggerActive() override {
        return isActive;
    }

    MOCK_METHOD0(notifyDeviceDestruction, bool(void));
    MOCK_CONST_METHOD1(notifyKernelDebugData, bool(const KernelInfo *));
    MOCK_CONST_METHOD0(isOptimizationDisabled, bool());
    MOCK_METHOD1(notifyNewDevice, bool(uint32_t));
    MOCK_CONST_METHOD3(notifySourceCode, bool(const char *, size_t, std::string &));
    MOCK_METHOD1(initialize, bool(bool));
};
