/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"
#include "shared/source/helpers/device_hierarchy_mode.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <mutex>
#include <stdarg.h>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace NEO {
class DirectSubmissionController;
class UnifiedMemoryReuseCleaner;
class GfxCoreHelper;
class MemoryManager;
struct OsEnvironment;
struct RootDeviceEnvironment;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    MOCKABLE_VIRTUAL bool initializeMemoryManager();
    void calculateMaxOsContextCount();
    int clearErrorDescription();
    virtual void prepareRootDeviceEnvironments(uint32_t numRootDevices);
    void prepareRootDeviceEnvironment(const uint32_t rootDeviceIndexForReInit);
    void parseAffinityMask();
    void adjustCcsCount();
    void adjustCcsCount(const uint32_t rootDeviceIndex) const;
    void sortNeoDevices();
    void setDeviceHierarchyMode(const GfxCoreHelper &gfxCoreHelper);
    void setDeviceHierarchyMode(const DeviceHierarchyMode deviceHierarchyMode) {
        this->deviceHierarchyMode = deviceHierarchyMode;
    }
    DeviceHierarchyMode getDeviceHierarchyMode() const { return deviceHierarchyMode; }
    void adjustRootDeviceEnvironments();
    void prepareForCleanup() const;
    void configureCcsMode();
    void setDebuggingMode(DebuggingMode debuggingMode) {
        debuggingEnabledMode = debuggingMode;
    }
    DebuggingMode getDebuggingMode() const { return debuggingEnabledMode; }
    bool isDebuggingEnabled() const { return debuggingEnabledMode != DebuggingMode::disabled; }
    void setMetricsEnabled(bool value) {
        this->metricsEnabled = value;
    }
    void getErrorDescription(const char **ppString);
    bool getSubDeviceHierarchy(uint32_t index, std::tuple<uint32_t, uint32_t, uint32_t> *subDeviceMap);
    bool areMetricsEnabled() { return this->metricsEnabled; }
    int setErrorDescription(const std::string &str);
    void setFP64EmulationEnabled() {
        fp64EmulationEnabled = true;
    }
    bool isFP64EmulationEnabled() const { return fp64EmulationEnabled; }
    void setDevicePermissionError(bool value) {
        devicePermissionError = value;
    }
    bool isDevicePermissionError() const { return devicePermissionError; }

    void setOneApiPvcWaEnv(bool val) {
        oneApiPvcWaEnv = val;
    }
    bool isOneApiPvcWaEnv() const { return oneApiPvcWaEnv; }

    DirectSubmissionController *initializeDirectSubmissionController();
    void initializeUnifiedMemoryReuseCleaner(bool isAnyDirectSubmissionLightEnabled);

    std::unique_lock<std::mutex> obtainPeerAccessQueryLock() {
        return std::unique_lock<std::mutex>(peerAccessQueryMutex);
    }

    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<UnifiedMemoryReuseCleaner> unifiedMemoryReuseCleaner;
    std::unique_ptr<DirectSubmissionController> directSubmissionController;
    std::unique_ptr<OsEnvironment> osEnvironment;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    void releaseRootDeviceEnvironmentResources(RootDeviceEnvironment *rootDeviceEnvironment);
    // Map of Sub Device Indices set during Affinity Mask in the form of:
    // <RootDeviceIndex, SubDeviceIndex, SubDeviceCount>
    // Primarily used by the Metrics Library to communicate the actual Sub Device Index being used in queries.
    std::unordered_map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t>> mapOfSubDeviceIndices;
    std::unordered_map<std::thread::id, std::string> errorDescs;
    std::mutex errorDescsMutex;
    std::mutex peerAccessQueryMutex;

  protected:
    static bool comparePciIdBusNumber(std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment1, std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment2);
    void parseCcsCountLimitations();
    void adjustCcsCountImpl(RootDeviceEnvironment *rootDeviceEnvironment) const;
    void configureNeoEnvironment();
    void restoreCcsMode();
    bool metricsEnabled = false;
    bool fp64EmulationEnabled = false;
    bool oneApiPvcWaEnv = true;
    bool devicePermissionError = false;

    DeviceHierarchyMode deviceHierarchyMode = DeviceHierarchyMode::composite;
    DebuggingMode debuggingEnabledMode = DebuggingMode::disabled;
    std::unordered_map<uint32_t, uint32_t> rootDeviceNumCcsMap;
    std::mutex initializeDirectSubmissionControllerMutex;
    std::mutex initializeUnifiedMemoryReuseCleanerMutex;
    std::vector<std::tuple<std::string, uint32_t>> deviceCcsModeVec;
};
} // namespace NEO
