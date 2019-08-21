/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/utilities/reference_tracked_object.h"

#include "engine_node.h"

#include <mutex>
#include <vector>

namespace NEO {
class AubCenter;
class BuiltIns;
class CommandStreamReceiver;
class CompilerInterface;
class GmmHelper;
class MemoryManager;
class SourceLevelDebugger;
class OSInterface;
class MemoryOperationsHandler;
struct EngineControl;
struct HardwareInfo;

using CsrContainer = std::vector<std::vector<std::unique_ptr<CommandStreamReceiver>>>;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {
  private:
    std::mutex mtx;
    DeviceFactoryCleaner cleaner;

  protected:
    std::unique_ptr<GmmHelper> gmmHelper;
    std::unique_ptr<HardwareInfo> hwInfo;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    MOCKABLE_VIRTUAL void initAubCenter(bool localMemoryEnabled, const std::string &aubFileName, CommandStreamReceiverType csrType);
    void initGmm();
    bool initializeCommandStreamReceiver(uint32_t deviceIndex, uint32_t deviceCsrIndex);
    void initializeSpecialCommandStreamReceiver();
    void initializeMemoryManager();
    void initSourceLevelDebugger();
    void setHwInfo(const HardwareInfo *hwInfo);
    const HardwareInfo *getHardwareInfo() const { return hwInfo.get(); }
    HardwareInfo *getMutableHardwareInfo() const { return hwInfo.get(); }
    bool isFullRangeSvm() const {
        return hwInfo->capabilityTable.gpuAddressSpace >= maxNBitValue<47>;
    }

    GmmHelper *getGmmHelper() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();
    EngineControl *getEngineControlForSpecialCsr();

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<AubCenter> aubCenter;
    CsrContainer commandStreamReceivers;
    std::unique_ptr<CommandStreamReceiver> specialCommandStreamReceiver;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace NEO
