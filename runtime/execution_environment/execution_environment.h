/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "engine_node.h"
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

using CsrContainer = std::vector<std::array<std::unique_ptr<CommandStreamReceiver>, EngineInstanceConstants::numGpgpuEngineInstances>>;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {
  private:
    std::mutex mtx;
    DeviceFactoryCleaner cleaner;

  protected:
    std::unique_ptr<GmmHelper> gmmHelper;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    MOCKABLE_VIRTUAL void initAubCenter(const HardwareInfo *hwInfo, bool localMemoryEnabled, const std::string &aubFileName);
    void initGmm(const HardwareInfo *hwInfo);
    bool initializeCommandStreamReceiver(const HardwareInfo *pHwInfo, uint32_t deviceIndex, uint32_t deviceCsrIndex);
    void initializeMemoryManager(bool enable64KBpages, bool enableLocalMemory, uint32_t deviceIndex, uint32_t deviceCsrIndex);
    void initSourceLevelDebugger(const HardwareInfo &hwInfo);

    GmmHelper *getGmmHelper() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<AubCenter> aubCenter;
    CsrContainer commandStreamReceivers;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace OCLRT
