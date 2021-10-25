/*
 * Copyright (C) 2018-2021 Intel Corporation
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
    void parseAffinityMask();
    void sortNeoDevices();
    void setDebuggingEnabled() {
        debuggingEnabled = true;
    }
    bool isDebuggingEnabled() { return debuggingEnabled; }
    DirectSubmissionController *getDirectSubmissionController();

    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<OsEnvironment> osEnvironment;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;

  protected:
    std::unique_ptr<DirectSubmissionController> directSubmissionController;

    bool debuggingEnabled = false;
};
} // namespace NEO
