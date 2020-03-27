/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/basic_math.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/utilities/api_intercept.h"

#include "drm/i915_drm.h"
#include "engine_node.h"
#include "igfxfmid.h"

#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

struct GT_SYSTEM_INFO;

namespace NEO {
#define I915_CONTEXT_PRIVATE_PARAM_BOOST 0x80000000

class DeviceFactory;
struct HardwareInfo;
struct RootDeviceEnvironment;

struct DeviceDescriptor { // NOLINT(clang-analyzer-optin.performance.Padding)
    unsigned short deviceId;
    const HardwareInfo *pHwInfo;
    void (*setupHardwareInfo)(HardwareInfo *, bool);
    GTTYPE eGtType;
};

extern const DeviceDescriptor deviceDescriptorTable[];

class Drm {
    friend DeviceFactory;

  public:
    virtual ~Drm();

    virtual int ioctl(unsigned long request, void *arg);

    int getDeviceID(int &devId);
    int getDeviceRevID(int &revId);
    int getExecSoftPin(int &execSoftPin);
    int enableTurboBoost();
    int getEuTotal(int &euTotal);
    int getSubsliceTotal(int &subsliceTotal);

    int getMaxGpuFrequency(int &maxGpuFrequency);
    int getEnabledPooledEu(int &enabled);
    int getMinEuInPool(int &minEUinPool);

    int queryGttSize(uint64_t &gttSizeOutput);
    bool isPreemptionSupported() const { return preemptionSupported; }

    MOCKABLE_VIRTUAL void checkPreemptionSupport();
    inline int getFileDescriptor() const { return hwDeviceId->getFileDescriptor(); }
    uint32_t createDrmContext();
    void destroyDrmContext(uint32_t drmContextId);
    void setLowPriorityContextParam(uint32_t drmContextId);

    unsigned int bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType);

    void setGtType(GTTYPE eGtType) { this->eGtType = eGtType; }
    GTTYPE getGtType() const { return this->eGtType; }
    MOCKABLE_VIRTUAL int getErrno();
    bool setQueueSliceCount(uint64_t sliceCount);
    void checkQueueSliceSupport();
    uint64_t getSliceMask(uint64_t sliceCount);
    bool queryEngineInfo();
    bool queryMemoryInfo();
    int setupHardwareInfo(DeviceDescriptor *, bool);

    bool areNonPersistentContextsSupported() const { return nonPersistentContextsSupported; }
    void checkNonPersistentContextsSupport();
    void setNonPersistentContext(uint32_t drmContextId);

    MemoryInfo *getMemoryInfo() const {
        return memoryInfo.get();
    }
    static bool isi915Version(int fd);

    static Drm *create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);

  protected:
    int getQueueSliceCount(drm_i915_gem_context_param_sseu *sseu);
    bool sliceCountChangeSupported = false;
    drm_i915_gem_context_param_sseu sseu{};
    bool preemptionSupported = false;
    bool nonPersistentContextsSupported = false;
    std::unique_ptr<HwDeviceId> hwDeviceId;
    int deviceId = 0;
    int revisionId = 0;
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    RootDeviceEnvironment &rootDeviceEnvironment;
    Drm(std::unique_ptr<HwDeviceId> hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment) : hwDeviceId(std::move(hwDeviceIdIn)), rootDeviceEnvironment(rootDeviceEnvironment) {}
    std::unique_ptr<EngineInfo> engineInfo;
    std::unique_ptr<MemoryInfo> memoryInfo;

    std::string getSysFsPciPath(int deviceID);
    std::unique_ptr<uint8_t[]> query(uint32_t queryId);

#pragma pack(1)
    struct PCIConfig {
        uint16_t VendorID;
        uint16_t DeviceID;
        uint16_t Command;
        uint16_t Status;
        uint8_t Revision;
        uint8_t ProgIF;
        uint8_t Subclass;
        uint8_t ClassCode;
        uint8_t cacheLineSize;
        uint8_t LatencyTimer;
        uint8_t HeaderType;
        uint8_t BIST;
        uint32_t BAR0[6];
        uint32_t CardbusCISPointer;
        uint16_t SubsystemVendorID;
        uint16_t SubsystemDeviceID;
        uint32_t ROM;
        uint8_t Capabilities;
        uint8_t Reserved[7];
        uint8_t InterruptLine;
        uint8_t InterruptPIN;
        uint8_t MinGrant;
        uint8_t MaxLatency;
    };
#pragma pack()
    static const char *sysFsDefaultGpuPath;
    static const char *maxGpuFrequencyFile;
    static const char *configFileName;

  private:
    int getParamIoctl(int param, int *dstValue);
};
} // namespace NEO
