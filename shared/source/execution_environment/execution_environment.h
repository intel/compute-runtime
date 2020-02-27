/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/reference_tracked_object.h"

#include <vector>

namespace NEO {
class MemoryManager;
class Debugger;
struct RootDeviceEnvironment;
struct HardwareInfo;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {

  protected:
    std::unique_ptr<HardwareInfo> hwInfo;

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    void initializeMemoryManager();
    void initDebugger();
    void calculateMaxOsContextCount();
    void setHwInfo(const HardwareInfo *hwInfo);
    const HardwareInfo *getHardwareInfo() const { return hwInfo.get(); }
    HardwareInfo *getMutableHardwareInfo() const { return hwInfo.get(); }
    bool isFullRangeSvm() const;
    void prepareRootDeviceEnvironments(uint32_t numRootDevices);

    std::unique_ptr<MemoryManager> memoryManager;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    std::unique_ptr<Debugger> debugger;
};
} // namespace NEO
