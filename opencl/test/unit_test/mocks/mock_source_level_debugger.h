/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/source_level_debugger/source_level_debugger.h"

#include "gmock/gmock.h"

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

    MOCK_METHOD(bool, notifyDeviceDestruction, (), (override));
    MOCK_METHOD(bool, notifyKernelDebugData, (const DebugData *debugData, const std::string &name, const void *isa, size_t isaSize), (const, override));
    MOCK_METHOD(bool, isOptimizationDisabled, (), (const, override));
    MOCK_METHOD(bool, notifyNewDevice, (uint32_t), (override));
    MOCK_METHOD(bool, notifySourceCode, (const char *, size_t, std::string &), (const, override));
    MOCK_METHOD(bool, initialize, (bool), (override));
};
