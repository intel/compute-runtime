/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/source_level_debugger/source_level_debugger.h"

namespace OCLRT {
const char *SourceLevelDebugger::dllName = "";

SourceLevelDebugger::SourceLevelDebugger(OsLibrary *library) {
    debuggerLibrary.reset(library);
}

SourceLevelDebugger::~SourceLevelDebugger() {
}

SourceLevelDebugger *SourceLevelDebugger::create() {
    return nullptr;
}

bool SourceLevelDebugger::isDebuggerActive() {
    return false;
}

bool SourceLevelDebugger::notifyNewDevice(uint32_t deviceHandle) {
    return false;
}

bool SourceLevelDebugger::notifyDeviceDestruction() {
    return false;
}

bool SourceLevelDebugger::notifySourceCode(const char *sourceCode, size_t size, std::string &filename) const {
    return false;
}

bool SourceLevelDebugger::isOptimizationDisabled() const {
    return false;
}

bool SourceLevelDebugger::notifyKernelDebugData(const KernelInfo *kernelInfo) const {
    return false;
}

bool SourceLevelDebugger::initialize(bool useLocalMemory) {
    return false;
}

} // namespace OCLRT
