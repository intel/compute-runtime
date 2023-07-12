/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <mutex>
#include <unordered_map>
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
    void adjustCcsCount();
    void adjustCcsCount(const uint32_t rootDeviceIndex) const;
    void sortNeoDevices();
    void adjustRootDeviceEnvironments();
    void prepareForCleanup() const;
    void setDebuggingMode(DebuggingMode debuggingMode) {
        debuggingEnabledMode = debuggingMode;
    }
    DebuggingMode getDebuggingMode() const { return debuggingEnabledMode; }
    bool isDebuggingEnabled() const { return debuggingEnabledMode != DebuggingMode::Disabled; }
    void setMetricsEnabled(bool value) {
        this->metricsEnabled = value;
    }
    void setExposeSubDevicesAsDevices(bool value) {
        this->subDevicesAsDevices = value;
    }
    bool isExposingSubDevicesAsDevices() const { return this->subDevicesAsDevices; }
    bool areMetricsEnabled() { return this->metricsEnabled; }
    void setFP64EmulationEnabled() {
        fp64EmulationEnabled = true;
    }
    bool isFP64EmulationEnabled() const { return fp64EmulationEnabled; }

    DirectSubmissionController *initializeDirectSubmissionController();

    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<DirectSubmissionController> directSubmissionController;
    std::unique_ptr<OsEnvironment> osEnvironment;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    void releaseRootDeviceEnvironmentResources(RootDeviceEnvironment *rootDeviceEnvironment);

  protected:
    static bool comparePciIdBusNumber(std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment1, std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment2);
    void parseCcsCountLimitations();
    void adjustCcsCountImpl(RootDeviceEnvironment *rootDeviceEnvironment) const;
    void configureNeoEnvironment();
    bool metricsEnabled = false;
    bool fp64EmulationEnabled = false;
    bool subDevicesAsDevices = false;

    DebuggingMode debuggingEnabledMode = DebuggingMode::Disabled;
    std::unordered_map<uint32_t, uint32_t> rootDeviceNumCcsMap;
    std::mutex initializeDirectSubmissionControllerMutex;
};
} // namespace NEO
