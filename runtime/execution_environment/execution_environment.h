/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/os_interface/device_factory.h"
#include "runtime/utilities/reference_tracked_object.h"

#include <mutex>
#include <vector>

namespace OCLRT {
class AubCenter;
class GmmHelper;
class CommandStreamReceiver;
class MemoryManager;
class SourceLevelDebugger;
class CompilerInterface;
class BuiltIns;
struct HardwareInfo;
class OSInterface;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {
  private:
    std::mutex mtx;
    DeviceFactoryCleaner cleaner;

  protected:
    std::unique_ptr<GmmHelper> gmmHelper;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    void initAubCenter();
    void initGmm(const HardwareInfo *hwInfo);
    bool initializeCommandStreamReceiver(const HardwareInfo *pHwInfo, uint32_t deviceIndex);
    void initializeMemoryManager(bool enable64KBpages, bool enableLocalMemory, uint32_t deviceIndex);
    void initSourceLevelDebugger(const HardwareInfo &hwInfo);

    GmmHelper *getGmmHelper() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<AubCenter> aubCenter;
    std::vector<std::unique_ptr<CommandStreamReceiver>> commandStreamReceivers;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace OCLRT
