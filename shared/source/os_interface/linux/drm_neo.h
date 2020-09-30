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
#include "shared/source/utilities/stackvec.h"

#include "drm/i915_drm.h"
#include "engine_node.h"
#include "igfxfmid.h"

#include <array>
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>
#include <vector>

struct GT_SYSTEM_INFO;

namespace NEO {
#define I915_CONTEXT_PRIVATE_PARAM_BOOST 0x80000000

class BufferObject;
class DeviceFactory;
class OsContext;
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
    enum class ResourceClass : uint32_t {
        Elf,
        Isa,
        ModuleHeapDebugArea,
        ContextSaveArea,
        SbaTrackingBuffer,
        MaxSize
    };

    static const std::array<const char *, size_t(ResourceClass::MaxSize)> classNames;
    virtual ~Drm();

    virtual int ioctl(unsigned long request, void *arg);

    int getDeviceID(int &devId);
    int getDeviceRevID(int &revId);
    int getExecSoftPin(int &execSoftPin);
    int enableTurboBoost();
    int getEuTotal(int &euTotal);
    int getSubsliceTotal(int &subsliceTotal);

    int getMaxGpuFrequency(HardwareInfo &hwInfo, int &maxGpuFrequency);
    int getEnabledPooledEu(int &enabled);
    int getMinEuInPool(int &minEUinPool);

    int queryGttSize(uint64_t &gttSizeOutput);
    bool isPreemptionSupported() const { return preemptionSupported; }

    MOCKABLE_VIRTUAL void checkPreemptionSupport();
    inline int getFileDescriptor() const { return hwDeviceId->getFileDescriptor(); }
    int createDrmVirtualMemory(uint32_t &drmVmId);
    void destroyDrmVirtualMemory(uint32_t drmVmId);
    uint32_t createDrmContext(uint32_t drmVmId);
    void destroyDrmContext(uint32_t drmContextId);
    int queryVmId(uint32_t drmContextId, uint32_t &vmId);
    void setLowPriorityContextParam(uint32_t drmContextId);

    unsigned int bindDrmContext(uint32_t drmContextId, uint32_t deviceIndex, aub_stream::EngineType engineType);

    void setGtType(GTTYPE eGtType) { this->eGtType = eGtType; }
    GTTYPE getGtType() const { return this->eGtType; }
    MOCKABLE_VIRTUAL int getErrno();
    bool setQueueSliceCount(uint64_t sliceCount);
    void checkQueueSliceSupport();
    uint64_t getSliceMask(uint64_t sliceCount);
    MOCKABLE_VIRTUAL bool queryEngineInfo();
    MOCKABLE_VIRTUAL bool queryMemoryInfo();
    bool queryTopology(int &sliceCount, int &subSliceCount, int &euCount);
    bool createVirtualMemoryAddressSpace(uint32_t vmCount);
    void destroyVirtualMemoryAddressSpace();
    uint32_t getVirtualMemoryAddressSpace(uint32_t vmId);
    int bindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo);
    int unbindBufferObject(OsContext *osContext, uint32_t vmHandleId, BufferObject *bo);
    int setupHardwareInfo(DeviceDescriptor *, bool);

    bool areNonPersistentContextsSupported() const { return nonPersistentContextsSupported; }
    void checkNonPersistentContextsSupport();
    void setNonPersistentContext(uint32_t drmContextId);
    bool isPerContextVMRequired() {
        return requirePerContextVM;
    }

    MOCKABLE_VIRTUAL bool registerResourceClasses();

    MOCKABLE_VIRTUAL uint32_t registerResource(ResourceClass classType, void *data, size_t size);
    MOCKABLE_VIRTUAL void unregisterResource(uint32_t handle);

    MemoryInfo *getMemoryInfo() const {
        return memoryInfo.get();
    }

    EngineInfo *getEngineInfo() const {
        return engineInfo.get();
    }
    RootDeviceEnvironment &getRootDeviceEnvironment() {
        return rootDeviceEnvironment;
    }

    bool resourceRegistrationEnabled() {
        return classHandles.size() > 0;
    }

    static inline uint32_t createMemoryRegionId(uint16_t type, uint16_t instance) {
        return (1u << (type + 16)) | (1u << instance);
    }
    static inline uint16_t getMemoryTypeFromRegion(uint32_t region) { return Math::log2(region >> 16); };
    static inline uint16_t getMemoryInstanceFromRegion(uint32_t region) { return Math::log2(region & 0xFFFF); };

    static bool isi915Version(int fd);

    static Drm *create(std::unique_ptr<HwDeviceId> hwDeviceId, RootDeviceEnvironment &rootDeviceEnvironment);
    static void overrideBindSupport(bool &useVmBind);

    bool isBindAvailable() {
        return this->bindAvailable;
    }
    void setBindAvailable() {
        this->bindAvailable = true;
    }

  protected:
    int getQueueSliceCount(drm_i915_gem_context_param_sseu *sseu);
    std::string generateUUID();
    bool sliceCountChangeSupported = false;
    drm_i915_gem_context_param_sseu sseu{};
    bool preemptionSupported = false;
    bool nonPersistentContextsSupported = false;
    bool requirePerContextVM = false;
    bool bindAvailable = false;
    std::unique_ptr<HwDeviceId> hwDeviceId;
    int deviceId = 0;
    int revisionId = 0;
    GTTYPE eGtType = GTTYPE_UNDEFINED;
    RootDeviceEnvironment &rootDeviceEnvironment;
    uint64_t uuid = 0;

    Drm(std::unique_ptr<HwDeviceId> hwDeviceIdIn, RootDeviceEnvironment &rootDeviceEnvironment);
    std::unique_ptr<EngineInfo> engineInfo;
    std::unique_ptr<MemoryInfo> memoryInfo;
    std::vector<uint32_t> virtualMemoryIds;

    std::string getSysFsPciPath();
    std::unique_ptr<uint8_t[]> query(uint32_t queryId, int32_t &length);

    StackVec<uint32_t, size_t(ResourceClass::MaxSize)> classHandles;

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
  private:
    int getParamIoctl(int param, int *dstValue);
};
} // namespace NEO
