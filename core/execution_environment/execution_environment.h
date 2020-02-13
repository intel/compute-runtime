/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/os_interface/device_factory.h"
#include "core/utilities/reference_tracked_object.h"

#include <mutex>
#include <vector>

namespace NEO {
class BuiltIns;
class CompilerInterface;
class GmmClientContext;
class GmmHelper;
class MemoryManager;
class Debugger;
struct RootDeviceEnvironment;
struct HardwareInfo;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {
  private:
    std::mutex mtx;

  protected:
    std::unique_ptr<GmmHelper> gmmHelper;
    std::unique_ptr<HardwareInfo> hwInfo;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    void initGmm();
    void initializeMemoryManager();
    void initDebugger();
    void calculateMaxOsContextCount();
    void setHwInfo(const HardwareInfo *hwInfo);
    const HardwareInfo *getHardwareInfo() const { return hwInfo.get(); }
    HardwareInfo *getMutableHardwareInfo() const { return hwInfo.get(); }
    bool isFullRangeSvm() const;
    void prepareRootDeviceEnvironments(uint32_t numRootDevices);

    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface();
    BuiltIns *getBuiltIns();

    std::unique_ptr<MemoryManager> memoryManager;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    std::unique_ptr<BuiltIns> builtins;
    std::unique_ptr<CompilerInterface> compilerInterface;
    std::unique_ptr<Debugger> debugger;
};
} // namespace NEO
