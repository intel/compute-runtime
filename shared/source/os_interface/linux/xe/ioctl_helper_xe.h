/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"

#include <bitset>
#include <mutex>

struct drm_xe_engine_class_instance;

// Arbitratry value for easier identification in the logs for now
#define XE_NEO_BIND_CAPTURE_FLAG 0x1
#define XE_NEO_BIND_IMMEDIATE_FLAG 0x20
#define XE_NEO_BIND_MAKERESIDENT_FLAG 0x300

#define XE_NEO_VMCREATE_DISABLESCRATCH_FLAG 0x100000
#define XE_NEO_VMCREATE_ENABLEPAGEFAULT_FLAG 0x20000
#define XE_NEO_VMCREATE_USEVMBIND_FLAG 0x3000

#define PRELIM_I915_UFENCE_WAIT_SOFT (1 << 15)

#define XE_ONE_SEC 1000000000

#define DRM_XE_UFENCE_WAIT_MASK_U8 0xffu
#define DRM_XE_UFENCE_WAIT_MASK_U16 0xffffu
#define DRM_XE_UFENCE_WAIT_MASK_U32 0xffffffffu
#define DRM_XE_UFENCE_WAIT_MASK_U64 0xffffffffffffffffu

namespace NEO {

enum class EngineClass : uint16_t;

struct BindInfo {
    uint32_t handle;
    uint64_t userptr;
    uint64_t addr;
    uint64_t size;
};

class IoctlHelperXe : public IoctlHelper {
  public:
    using IoctlHelper::IoctlHelper;

    IoctlHelperXe(Drm &drmArg);
    ~IoctlHelperXe() override;
    int ioctl(DrmIoctl request, void *arg) override;

    bool initialize() override;
    bool isSetPairAvailable() override;
    bool isChunkingAvailable() override;
    bool isVmBindAvailable() override;
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask) override;
    uint32_t createGem(uint64_t size, uint32_t memoryBanks) override;
    CacheRegion closAlloc() override;
    uint16_t closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(CacheRegion closIndex) override;
    int waitUserFence(uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getAtomicAccess(AtomicAccessMode mode) override;
    uint32_t getPreferredLocationAdvise() override;
    std::optional<MemoryClassInstance> getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) override;
    bool setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) override;
    bool setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) override;
    bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override;
    bool setGemTiling(void *setTiling) override;
    bool getGemTiling(void *setTiling) override;
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident) override;
    int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    std::optional<DrmParam> getHasPageFaultParamId() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    std::optional<uint64_t> getCopyClassSaturatePCIECapability() override;
    std::optional<uint64_t> getCopyClassSaturateLinkCapability() override;
    uint32_t getVmAdviseAtomicAttribute() override;
    int vmBind(const VmBindParams &vmBindParams) override;
    int vmUnbind(const VmBindParams &vmBindParams) override;
    bool getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize, uint64_t samplingRate, uint64_t pollPeriod,
                              uint64_t engineInstance, uint64_t notifyNReports) override;
    uint32_t getEuStallFdParameter() override;
    UuidRegisterResult registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(uint32_t handle) override;
    bool isContextDebugSupported() override;
    int setContextDebugFlag(uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    unsigned int getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const;
    int getDrmParamValue(DrmParam drmParam) const override;
    int getDrmParamValueBase(DrmParam drmParam) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex) override;
    std::string getDrmParamString(DrmParam param) const override;
    bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override;
    std::string getFileForMaxGpuFrequency() const override;
    std::string getFileForMaxGpuFrequencyOfSubDevice(int subDeviceId) const override;
    std::string getFileForMaxMemoryFrequencyOfSubDevice(int subDeviceId) const override;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override;
    bool isWaitBeforeBindRequired(bool bind) const override;
    std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) override;
    std::unique_ptr<MemoryInfo> createMemoryInfo() override;
    void getTopologyData(size_t nTiles, std::vector<std::bitset<8>> *geomDss, std::vector<std::bitset<8>> *computeDss, std::vector<std::bitset<8>> *euDss, DrmQueryTopologyData &topologyData, bool &isComputeDssEmpty);
    void getTopologyMap(size_t nTiles, std::vector<std::bitset<8>> *dssInfo, TopologyMap &topologyMap);

    bool setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    bool getTimestampFrequency(uint64_t &frequency);

    void fillBindInfoForIpcHandle(uint32_t handle, size_t size) override;
    bool getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) override;
    bool isImmediateVmBindRequired() const override;
    void fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) override;
    void logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) override;
    void fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) override;
    void logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) override;
    bool setDomainCpu(uint32_t handle, bool writeEnable) override;
    uint16_t getCpuCachingMode();
    void addDebugMetadata(DrmResourceClass type, uint64_t *offset, uint64_t size);
    void addDebugMetadataCookie(uint64_t cookie);

  protected:
    const char *xeGetClassName(int className);
    const char *xeGetBindOperationName(int bindOperation);
    const char *xeGetBindFlagsName(int bindFlags);

    const char *xeGetengineClassName(uint32_t engineClass);
    template <typename DataType>
    std::vector<DataType> queryData(uint32_t queryId);
    int xeWaitUserFence(uint32_t ctxId, uint64_t mask, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout);
    int xeVmBind(const VmBindParams &vmBindParams, bool bindOp);
    void xeShowBindTable();
    void updateBindInfo(uint32_t handle, uint64_t userPtr, uint64_t size);
    void *allocateDebugMetadata();
    void *freeDebugMetadata(void *metadata);

    struct UserFenceExtension {
        static constexpr uint32_t tagValue = 0x123987;
        uint32_t tag;
        uint64_t addr;
        uint64_t value;
    };

    void setDefaultEngine();

    int chipsetId = 0;
    int revId = 0;
    int defaultAlignment = 0;
    int hasVram = 0;
    uint32_t xeVmId = 0;
    uint32_t userPtrHandle = 0;
    int xeFileHandle = 0;
    std::mutex xeLock;
    std::vector<BindInfo> bindInfo;
    int instance = 0;
    uint32_t xeTimestampFrequency = 0;
    std::vector<uint32_t> hwconfig;
    std::vector<drm_xe_engine_class_instance> contextParamEngine;
    std::vector<drm_xe_engine_class_instance> allEngines;

    drm_xe_engine_class_instance *defaultEngine = nullptr;

    struct DebugMetadata {
        DrmResourceClass type;
        uint64_t offset;
        uint64_t size;
        bool isCookie;
    };
    std::vector<DebugMetadata> debugMetadata;

  private:
    template <typename... XeLogArgs>
    void xeLog(XeLogArgs &&...args) const;
    drm_xe_engine_class_instance *xeFindMatchingEngine(uint16_t engineClass, uint16_t engineInstance);

    struct ExecObjectXe {
        uint64_t gpuAddress;
        uint32_t handle;
    };

    struct ExecBufferXe {
        ExecObjectXe *execObject;
        uint64_t startOffset;
        uint32_t drmContextId;
    };
};

template <typename... XeLogArgs>
void IoctlHelperXe::xeLog(XeLogArgs &&...args) const {
    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, args...);
}

} // namespace NEO
