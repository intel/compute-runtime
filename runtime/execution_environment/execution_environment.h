/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/utilities/reference_tracked_object.h"

#include "engine_node.h"

#include <mutex>
#include <vector>

namespace OCLRT {
class AubCenter;
class BuiltIns;
class CommandStreamReceiver;
class CompilerInterface;
class GmmHelper;
class MemoryManager;
class SourceLevelDebugger;
class OSInterface;
struct EngineControl;
struct HardwareInfo;

using CsrContainer = std::vector<std::vector<std::unique_ptr<CommandStreamReceiver>>>;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {
  private:
    std::mutex mtx;
    DeviceFactoryCleaner cleaner;

  protected:
    std::unique_ptr<GmmHelper> gmmHelper;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    MOCKABLE_VIRTUAL void initAubCenter(const HardwareInfo *hwInfo, bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);
    void initGmm(const HardwareInfo *hwInfo);
    bool initializeCommandStreamReceiver(const HardwareInfo *pHwInfo, uint32_t deviceIndex, uint32_t deviceCsrIndex);
    void initializeSpecialCommandStreamReceiver(const HardwareInfo &hwInfo);
    void initializeMemoryManager(bool enable64KBpages, bool enableLocalMemory, uint32_t deviceIndex, uint32_t deviceCsrIndex);
    void initSourceLevelDebugger(const HardwareInfo &hwInfo);

    GmmHelper *getGmmHelper() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();
    EngineControl *getEngineControlForSpecialCsr();

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<AubCenter> aubCenter;
    CsrContainer commandStreamReceivers;
    std::unique_ptr<CommandStreamReceiver> specialCommandStreamReceiver;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace OCLRT
