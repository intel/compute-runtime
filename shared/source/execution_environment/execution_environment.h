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
struct OsEnvironment;
struct RootDeviceEnvironment;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    void initializeMemoryManager();
    void calculateMaxOsContextCount();
    void prepareRootDeviceEnvironments(uint32_t numRootDevices);

    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<OsEnvironment> osEnvironment;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
};
} // namespace NEO
