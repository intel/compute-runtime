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
#include "runtime/os_interface/device_factory.h"
#include "runtime/utilities/reference_tracked_object.h"

namespace OCLRT {
class GmmHelper;
class CommandStreamReceiver;
class MemoryManager;
class SourceLevelDebugger;
struct HardwareInfo;
class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {
  private:
    DeviceFactoryCleaner cleaner;

  protected:
    std::unique_ptr<GmmHelper> gmmHelper;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;
    void initGmm(const HardwareInfo *hwInfo);
    bool initializeCommandStreamReceiver(const HardwareInfo *pHwInfo);
    void initializeMemoryManager(bool enable64KBpages);
    void initSourceLevelDebugger(const HardwareInfo &hwInfo);
    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace OCLRT
