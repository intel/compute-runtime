/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/timestamp.h"
#include "shared/source/os_interface/linux/drm_debug.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface.h"

#include <bitset>
#include <mutex>
#include <optional>

namespace NEO {

namespace XeDrm {
struct drm_xe_engine_class_instance; // NOLINT(readability-identifier-naming)
struct drm_xe_query_gt_list;         // NOLINT(readability-identifier-naming)
struct drm_xe_query_config;          // NOLINT(readability-identifier-naming)
} // namespace XeDrm

enum class EngineClass : uint16_t;

struct BindInfo {
    uint64_t userptr;
    uint64_t addr;
};

class IoctlHelperXe : public IoctlHelper {
  public:
    using GtIdContainer = StackVec<int, 4>;

    using IoctlHelper::IoctlHelper;
    static std::unique_ptr<IoctlHelperXe> create(Drm &drmArg);
    static bool queryDeviceIdAndRevision(Drm &drm);
    IoctlHelperXe(Drm &drmArg);
    ~IoctlHelperXe() override;
    int ioctl(DrmIoctl request, void *arg) override;
    int ioctl(int fd, DrmIoctl request, void *arg) override;
    bool initialize() override;
    bool isSetPairAvailable() override;
    bool isChunkingAvailable() override;
    bool isVmBindAvailable() override;
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, uint64_t patIndex, std::optional<uint32_t> vmId, int32_t pairHandle, bool isChunked, uint32_t numOfChunks, std::optional<uint32_t> memPolicyMode, std::optional<std::vector<unsigned long>> memPolicyNodemask, std::optional<bool> isCoherent) override;
    uint32_t createGem(uint64_t size, uint32_t memoryBanks, std::optional<bool> isCoherent) override;
    CacheRegion closAlloc(CacheLevel cacheLevel) override;
    uint16_t closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(CacheRegion closIndex) override;
    int waitUserFence(uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags,
                      bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait) override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getAtomicAccess(AtomicAccessMode mode) override;
    uint64_t getPreferredLocationArgs(MemAdvise memAdviseOp) override;
    uint32_t getPreferredLocationAdvise() override;
    std::optional<MemoryClassInstance> getPreferredLocationRegion(PreferredLocation memoryLocation, uint32_t memoryInstance) override;
    bool setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) override;
    bool setVmSharedSystemMemAdvise(uint64_t handle, const size_t size, const uint32_t attribute, const uint64_t param, const std::vector<uint32_t> &vmIds) override;
    AtomicAccessMode getVmSharedSystemAtomicAttribute(uint64_t handle, const size_t size, const uint32_t vmId) override;
    bool setVmBoAdviseForChunking(int32_t handle, uint64_t start, uint64_t length, uint32_t attribute, void *region) override;
    bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override;
    bool setGemTiling(void *setTiling) override;
    bool getGemTiling(void *setTiling) override;
    uint32_t getDirectSubmissionFlag() override;
    std::unique_ptr<uint8_t[]> prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles, uint64_t cookie) override;
    uint64_t getFlagsForVmBind(bool bindCapture, bool bindImmediate, bool bindMakeResident, bool bindLock, bool readOnlyResource) override;
    virtual std::string xeGetBindFlagNames(int bindFlags);
    int queryDistances(std::vector<QueryItem> &queryItems, std::vector<DistanceInfo> &distanceInfos) override;
    uint16_t getWaitUserFenceSoftFlag() override;
    int execBuffer(ExecBuffer *execBuffer, uint64_t completionGpuAddress, TaskCountType counterValue) override;
    bool completionFenceExtensionSupported(const bool isVmBindAvailable) override;
    bool isPageFaultSupported() override;
    std::unique_ptr<uint8_t[]> createVmControlExtRegion(const std::optional<MemoryClassInstance> &regionInstanceClass) override;
    uint32_t getFlagsForVmCreate(bool disableScratch, bool enablePageFault, bool useVmBind) override;
    uint32_t createContextWithAccessCounters(GemContextCreateExt &gcc) override;
    uint32_t createCooperativeContext(GemContextCreateExt &gcc) override;
    void fillVmBindExtSetPat(VmBindExtSetPatT &vmBindExtSetPat, uint64_t patIndex, uint64_t nextExtension) override;
    void fillVmBindExtUserFence(VmBindExtUserFenceT &vmBindExtUserFence, uint64_t fenceAddress, uint64_t fenceValue, uint64_t nextExtension) override;
    void setVmBindUserFence(VmBindParams &vmBind, VmBindExtUserFenceT vmBindUserFence) override;
    std::optional<uint32_t> getVmAdviseAtomicAttribute() override;
    int vmBind(const VmBindParams &vmBindParams) override;
    int vmUnbind(const VmBindParams &vmBindParams) override;
    int getResetStats(ResetStats &resetStats, uint32_t *status, ResetStatsFault *resetStatsFault) override;
    bool isEuStallSupported() override;
    uint32_t getEuStallFdParameter() override;
    bool perfOpenEuStallStream(uint32_t euStallFdParameter, uint32_t &samplingPeriodNs, uint64_t engineInstance, uint64_t notifyNReports, uint64_t gpuTimeStampfrequency, int32_t *stream) override;
    bool perfDisableEuStallStream(int32_t *stream) override;
    MOCKABLE_VIRTUAL int perfOpenIoctl(DrmIoctl request, void *arg);
    unsigned int getIoctlRequestValuePerf(DrmIoctl ioctlRequest) const;
    UuidRegisterResult registerUuid(const std::string &uuid, uint32_t uuidClass, uint64_t ptr, uint64_t size) override;
    UuidRegisterResult registerStringClassUuid(const std::string &uuid, uint64_t ptr, uint64_t size) override;
    int unregisterUuid(uint32_t handle) override;
    bool isContextDebugSupported() override;
    int setContextDebugFlag(uint32_t drmContextId) override;
    bool isDebugAttachAvailable() override;
    int getEuDebugSysFsEnable() override;
    unsigned int getIoctlRequestValue(DrmIoctl ioctlRequest) const override;
    unsigned int getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const;
    int getDrmParamValue(DrmParam drmParam) const override;
    int getDrmParamValueBase(DrmParam drmParam) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex, bool allocateInterrupt) override;
    std::string getDrmParamString(DrmParam param) const override;
    bool getTopologyDataAndMap(const HardwareInfo &hwInfo, DrmQueryTopologyData &topologyData, TopologyMap &topologyMap) override;
    std::string getFileForMaxGpuFrequency() const override;
    std::string getFileForMaxGpuFrequencyOfSubDevice(int subDeviceId) const override;
    std::string getFileForMaxMemoryFrequencyOfSubDevice(int subDeviceId) const override;
    void configureCcsMode(std::vector<std::string> &files, const std::string expectedPrefix, uint32_t ccsMode,
                          std::vector<std::tuple<std::string, uint32_t>> &deviceCcsModeVec) override;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override;
    bool isWaitBeforeBindRequired(bool bind) const override;
    std::unique_ptr<EngineInfo> createEngineInfo(bool isSysmanEnabled) override;
    std::unique_ptr<MemoryInfo> createMemoryInfo() override;
    size_t getLocalMemoryRegionsSize(const MemoryInfo *memoryInfo, uint32_t subDevicesCount, uint32_t deviceBitfield) const override;
    void setupIpVersion() override;

    bool setGpuCpuTimes(TimeStampData *pGpuCpuTime, OSTime *osTime) override;
    bool getFdFromVmExport(uint32_t vmId, uint32_t flags, int32_t *fd) override;
    bool isImmediateVmBindRequired() const override;
    void fillExecObject(ExecObject &execObject, uint32_t handle, uint64_t gpuAddress, uint32_t drmContextId, bool bindInfo, bool isMarkedForCapture) override;
    void logExecObject(const ExecObject &execObject, std::stringstream &logger, size_t size) override;
    void fillExecBuffer(ExecBuffer &execBuffer, uintptr_t buffersPtr, uint32_t bufferCount, uint32_t startOffset, uint32_t size, uint64_t flags, uint32_t drmContextId) override;
    void logExecBuffer(const ExecBuffer &execBuffer, std::stringstream &logger) override;
    bool setDomainCpu(uint32_t handle, bool writeEnable) override;
    uint16_t getCpuCachingMode(std::optional<bool> isCoherent, bool allocationInSystemMemory) const;
    void addDebugMetadata(DrmResourceClass type, uint64_t *offset, uint64_t size);
    uint32_t registerResource(DrmResourceClass classType, const void *data, size_t size) override;
    void unregisterResource(uint32_t handle) override;
    void insertEngineToContextParams(ContextParamEngines<> &contextParamEngines, uint32_t engineId, const EngineClassInstance *engineClassInstance, uint32_t tileId, bool hasVirtualEngines) override;
    void registerBOBindHandle(Drm *drm, DrmAllocation *drmAllocation) override;
    bool resourceRegistrationEnabled() override { return true; }
    bool isPreemptionSupported() override { return true; }
    bool isTimestampsRefreshEnabled() override { return true; }
    uint32_t getTileIdFromGtId(uint32_t gtId) const override {
        return gtIdToTileId[gtId];
    }
    uint32_t getGtIdFromTileId(uint32_t tileId, uint16_t engineClass) const override;
    bool makeResidentBeforeLockNeeded() const override;
    bool isSmallBarConfigAllowed() const override { return false; }
    void *pciBarrierMmap() override;
    bool retrieveMmapOffsetForBufferObject(BufferObject &bo, uint64_t flags, uint64_t &offset) override;
    bool is2MBSizeAlignmentRequired(AllocationType allocationType) const override;

  protected:
    static constexpr uint32_t maxContextSetProperties = 4;

    virtual const char *xeGetClassName(int className) const;
    const char *xeGetBindOperationName(int bindOperation);
    const char *xeGetAdviseOperationName(int adviseOperation);

    const char *xeGetengineClassName(uint32_t engineClass);
    template <typename DataType>
    std::vector<DataType> queryData(uint32_t queryId);
    virtual int xeWaitUserFence(uint32_t ctxId, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout, bool userInterrupt, uint32_t externalInterruptId, GraphicsAllocation *allocForInterruptWait);
    void setupXeWaitUserFenceStruct(void *arg, uint32_t ctxId, uint16_t op, uint64_t addr, uint64_t value, int64_t timeout);
    int xeVmBind(const VmBindParams &vmBindParams, bool bindOp);
    void xeShowBindTable();
    void updateBindInfo(uint64_t userPtr);
    int debuggerOpenIoctl(DrmIoctl request, void *arg);
    int debuggerMetadataCreateIoctl(DrmIoctl request, void *arg);
    int debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg);
    int getEudebugExtProperty();
    uint64_t getEudebugExtPropertyValue();
    virtual bool isMediaEngine(uint16_t engineClass) const { return false; }
    virtual std::optional<uint32_t> getCxlType() { return {}; }
    virtual bool isMediaGt(uint16_t gtType) const;

    struct UserFenceExtension {
        static constexpr uint32_t tagValue = 0x123987;
        uint32_t tag;
        uint64_t addr;
        uint64_t value;
    };

    uint16_t getDefaultEngineClass(const aub_stream::EngineType &defaultEngineType);
    void setOptionalContextProperties(const OsContextLinux &osContext, Drm &drm, void *extProperties, uint32_t &extIndexInOut);
    virtual void setContextProperties(const OsContextLinux &osContext, uint32_t deviceIndex, void *extProperties, uint32_t &extIndexInOut);
    virtual void applyContextFlags(void *execQueueCreate, bool allocateInterrupt);

    struct GtIpVersion {
        uint16_t major;
        uint16_t minor;
        uint16_t revision;
    };
    bool queryHwIpVersion(GtIpVersion &gtIpVersion);

    bool isLowLatencyHintAvailable = false;
    int maxExecQueuePriority = 0;
    std::mutex xeLock;
    std::mutex gemCloseLock;
    std::vector<BindInfo> bindInfo;
    std::vector<uint32_t> hwconfig;
    std::vector<XeDrm::drm_xe_engine_class_instance> contextParamEngine;

    std::vector<uint64_t> queryGtListData;
    constexpr static int invalidIndex = -1;
    GtIdContainer gtIdToTileId;
    GtIdContainer tileIdToGtId;
    GtIdContainer mediaGtIdToTileId;
    GtIdContainer tileIdToMediaGtId;
    XeDrm::drm_xe_query_gt_list *xeGtListData = nullptr;

    std::unique_ptr<XeDrm::drm_xe_engine_class_instance> defaultEngine;
    struct DebugMetadata {
        DrmResourceClass type;
        uint64_t offset;
        uint64_t size;
        bool isCookie;
    };

    template <typename... XeLogArgs>
    void xeLog(XeLogArgs &&...args) const;

    struct ExecObjectXe {
        uint64_t gpuAddress;
        uint32_t handle;
    };

    struct ExecBufferXe {
        ExecObjectXe *execObject;
        uint64_t startOffset;
        uint32_t drmContextId;
    };

    std::unique_ptr<EuDebugInterface> euDebugInterface;
};

template <typename... XeLogArgs>
void IoctlHelperXe::xeLog(XeLogArgs &&...args) const {
    if (debugManager.flags.PrintXeLogs.get()) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintXeLogs.get(), stderr, args...);
    }
}

} // namespace NEO
