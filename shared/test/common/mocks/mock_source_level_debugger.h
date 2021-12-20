/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/source_level_debugger/source_level_debugger.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <string>

using namespace NEO;

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

    bool isDebuggerActive() override {
        return isActive;
    }

    bool isOptimizationDisabled() const override {
        isOptimizationDisabledCalled++;
        if (callBaseIsOptimizationDisabled) {
            return SourceLevelDebugger::isOptimizationDisabled();
        }
        return isOptimizationDisabledResult;
    }

    mutable uint32_t isOptimizationDisabledCalled = 0u;
    bool isOptimizationDisabledResult = false;

    bool notifySourceCode(const char *sourceCode, size_t size, std::string &filename) const override {
        notifySourceCodeCalled++;
        if (callBaseNotifySourceCode) {
            return SourceLevelDebugger::notifySourceCode(sourceCode, size, filename);
        }
        return notifySourceCodeResult;
    }

    mutable uint32_t notifySourceCodeCalled = 0u;
    bool notifySourceCodeResult = false;

    bool notifyKernelDebugData(const DebugData *debugData, const std::string &name, const void *isa, size_t isaSize) const override {
        notifyKernelDebugDataCalled++;
        if (callBaseNotifyKernelDebugData) {
            return SourceLevelDebugger::notifyKernelDebugData(debugData, name, isa, isaSize);
        }
        return notifyKernelDebugDataResult;
    }

    mutable uint32_t notifyKernelDebugDataCalled = 0u;
    bool notifyKernelDebugDataResult = false;

    bool notifyNewDevice(uint32_t deviceHandle) override {
        notifyNewDeviceCalled++;
        if (callBaseNotifyNewDevice) {
            return SourceLevelDebugger::notifyNewDevice(deviceHandle);
        }
        return notifyNewDeviceResult;
    }

    mutable uint32_t notifyNewDeviceCalled = 0u;
    bool notifyNewDeviceResult = false;

    bool initialize(bool useLocalMemory) override {
        initializeCalled++;
        if (callBaseInitialize) {
            return SourceLevelDebugger::initialize(useLocalMemory);
        }
        return initializeResult;
    }

    mutable uint32_t initializeCalled = 0u;
    bool initializeResult = false;

    ADDMETHOD_NOBASE(notifyDeviceDestruction, bool, false, ());

    static const uint32_t mockDeviceHandle = 23;
    bool callBaseIsOptimizationDisabled = false;
    bool callBaseNotifySourceCode = false;
    bool callBaseNotifyKernelDebugData = false;
    bool callBaseNotifyNewDevice = false;
    bool callBaseInitialize = false;
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
    bool notifyKernelDebugData(const DebugData *debugData, const std::string &name, const void *isa, size_t isaSize) const override {
        return false;
    }
    bool initialize(bool useLocalMemory) override {
        return false;
    }

    static const uint32_t mockDeviceHandle = 23;
    bool isOptDisabled = false;
    std::string sourceCodeFilename;
};