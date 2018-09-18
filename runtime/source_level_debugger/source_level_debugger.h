/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/os_library.h"
#include <memory>
#include <string>

namespace OCLRT {
struct KernelInfo;

class SourceLevelDebugger {
  public:
    SourceLevelDebugger(OsLibrary *library);
    virtual ~SourceLevelDebugger();
    SourceLevelDebugger(const SourceLevelDebugger &ref) = delete;
    SourceLevelDebugger &operator=(const SourceLevelDebugger &) = delete;
    static SourceLevelDebugger *create();

    MOCKABLE_VIRTUAL bool isDebuggerActive();
    MOCKABLE_VIRTUAL bool notifyNewDevice(uint32_t deviceHandle);
    MOCKABLE_VIRTUAL bool notifyDeviceDestruction();
    MOCKABLE_VIRTUAL bool notifySourceCode(const char *sourceCode, size_t size, std::string &filename) const;
    MOCKABLE_VIRTUAL bool isOptimizationDisabled() const;
    MOCKABLE_VIRTUAL bool notifyKernelDebugData(const KernelInfo *kernelInfo) const;
    MOCKABLE_VIRTUAL bool initialize(bool useLocalMemory);

  protected:
    class SourceLevelDebuggerInterface;
    SourceLevelDebuggerInterface *sourceLevelDebuggerInterface = nullptr;

    static OsLibrary *loadDebugger();
    void getFunctions();

    std::unique_ptr<OsLibrary> debuggerLibrary;
    bool isActive = false;
    uint32_t deviceHandle = 0;

    static const char *notifyNewDeviceSymbol;
    static const char *notifySourceCodeSymbol;
    static const char *getDebuggerOptionSymbol;
    static const char *notifyKernelDebugDataSymbol;
    static const char *initSymbol;
    static const char *isDebuggerActiveSymbol;
    static const char *notifyDeviceDestructionSymbol;
    // OS specific library name
    static const char *dllName;
};
} // namespace OCLRT
