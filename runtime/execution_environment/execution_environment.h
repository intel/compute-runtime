/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/utilities/reference_tracked_object.h"
#include "runtime/os_interface/device_factory.h"

#include <mutex>
#include <vector>

namespace NEO {
class BuiltIns;
class CompilerInterface;
class GmmHelper;
class MemoryManager;
class MemoryOperationsHandler;
class OSInterface;
class SourceLevelDebugger;
struct RootDeviceEnvironment;
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

    void initGmm();
    void initializeMemoryManager();
    void initSourceLevelDebugger();
    void setHwInfo(const HardwareInfo *hwInfo);
    const HardwareInfo *getHardwareInfo() const { return hwInfo.get(); }
    HardwareInfo *getMutableHardwareInfo() const { return hwInfo.get(); }
    bool isFullRangeSvm() const;
    void prepareRootDeviceEnvironments(uint32_t numRootDevices);

    GmmHelper *getGmmHelper() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<MemoryOperationsHandler> memoryOperationsInterface;
    std::unique_ptr<MemoryManager> memoryManager;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<SourceLevelDebugger> sourceLevelDebugger;
};
} // namespace NEO
