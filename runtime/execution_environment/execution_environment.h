/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/execution_environment/root_device_environment.h"
#include "core/utilities/reference_tracked_object.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"

#include <mutex>

namespace NEO {
class BuiltIns;
class CommandStreamReceiver;
class CompilerInterface;
class GmmHelper;
class MemoryManager;
class MemoryOperationsHandler;
class OSInterface;
class RootDevice;
class SourceLevelDebugger;
struct HardwareInfo;

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
    bool initializeCommandStreamReceiver(uint32_t rootDeviceIndex, uint32_t internalDeviceIndex, uint32_t deviceCsrIndex);
    MOCKABLE_VIRTUAL bool initializeRootCommandStreamReceiver(RootDevice &rootDevice);
    void initializeMemoryManager();
    void initSourceLevelDebugger();
    void setHwInfo(const HardwareInfo *hwInfo);
    const HardwareInfo *getHardwareInfo() const { return hwInfo.get(); }
    HardwareInfo *getMutableHardwareInfo() const { return hwInfo.get(); }
    bool isFullRangeSvm() const;

    GmmHelper *getGmmHelper() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<MemoryManager> memoryManager;
    std::vector<RootDeviceEnvironment> rootDeviceEnvironments{1};
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace NEO
