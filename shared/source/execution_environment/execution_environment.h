/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/reference_tracked_object.h"

#include <vector>

namespace NEO {
class DirectSubmissionController;
class MemoryManager;
struct OsEnvironment;
struct RootDeviceEnvironment;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    MOCKABLE_VIRTUAL bool initializeMemoryManager();
    void calculateMaxOsContextCount();
    virtual void prepareRootDeviceEnvironments(uint32_t numRootDevices);
    void prepareRootDeviceEnvironment(const uint32_t rootDeviceIndexForReInit);
    void parseAffinityMask();
    void sortNeoDevices();
    void sortNeoDevicesDRM();
    void sortNeoDevicesWDDM();
    void prepareForCleanup() const;
    void setDebuggingEnabled() {
        debuggingEnabled = true;
    }
    bool isDebuggingEnabled() { return debuggingEnabled; }
    DirectSubmissionController *initializeDirectSubmissionController();

    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<DirectSubmissionController> directSubmissionController;
    std::unique_ptr<OsEnvironment> osEnvironment;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    void releaseRootDeviceEnvironmentResources(RootDeviceEnvironment *rootDeviceEnvironment);

  protected:
    bool debuggingEnabled = false;
};
} // namespace NEO
