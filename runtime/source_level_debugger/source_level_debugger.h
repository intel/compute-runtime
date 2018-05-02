/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
