/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/ioctl_helper.h"

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

namespace NEO {

struct bind_info {
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
    bool isVmBindAvailable() override;
    int createGemExt(const MemRegionsVec &memClassInstances, size_t allocSize, uint32_t &handle, std::optional<uint32_t> vmId, int32_t pairHandle) override;
    CacheRegion closAlloc() override;
    uint16_t closAllocWays(CacheRegion closIndex, uint16_t cacheLevel, uint16_t numWays) override;
    CacheRegion closFree(CacheRegion closIndex) override;
    int waitUserFence(uint32_t ctxId, uint64_t address,
                      uint64_t value, uint32_t dataWidth, int64_t timeout, uint16_t flags) override;
    uint32_t getAtomicAdvise(bool isNonAtomic) override;
    uint32_t getPreferredLocationAdvise() override;
    bool setVmBoAdvise(int32_t handle, uint32_t attribute, void *region) override;
    bool setVmPrefetch(uint64_t start, uint64_t length, uint32_t region, uint32_t vmId) override;
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
    int getDrmParamValue(DrmParam drmParam) const override;
    int getDrmParamValueBase(DrmParam drmParam) const override;
    std::string getIoctlString(DrmIoctl ioctlRequest) const override;
    int createDrmContext(Drm &drm, OsContextLinux &osContext, uint32_t drmVmId, uint32_t deviceIndex) override;
    std::string getDrmParamString(DrmParam param) const override;

    std::string getFileForMaxGpuFrequency() const override;
    std::string getFileForMaxGpuFrequencyOfSubDevice(int subDeviceId) const override;
    std::string getFileForMaxMemoryFrequencyOfSubDevice(int subDeviceId) const override;
    bool getFabricLatency(uint32_t fabricId, uint32_t &latency, uint32_t &bandwidth) override;
    bool isWaitBeforeBindRequired(bool bind) const override;

    std::vector<uint8_t> xeRebuildi915Topology(std::vector<uint8_t> *geomDss, std::vector<uint8_t> *computeDss, std::vector<uint8_t> *euDss);

  private:
    template <typename... XeLogArgs>
    void xeLog(XeLogArgs &&...args) const;
    void xeWaitUserFence(uint64_t mask, uint16_t op, uint64_t addr, uint64_t value, struct drm_xe_engine_class_instance *eci, int64_t timeout);
    int xeVmBind(const VmBindParams &vmBindParams, bool bindOp);
    uint32_t xeSyncObjCreate(uint32_t flags);
    bool xeSyncObjWait(uint32_t *handles, uint32_t count, uint64_t absTimeoutNsec, uint32_t flags, uint32_t *firstSignaled);
    void xeSyncObjDestroy(uint32_t handle);
    void xeShowBindTable();
    int xeGetQuery(Query *data);
    struct drm_xe_engine_class_instance *xeFindMatchingEngine(uint16_t engineClass, uint16_t engineInstance);

  protected:
    uint64_t xeDecanonize(uint64_t address);
    const char *xeGetClassName(int className);
    const char *xeGetBindOpName(int bindOp);
    const char *xeGetengineClassName(uint32_t engineClass);

  protected:
    int chipsetId = 0;
    int revId = 0;
    int defaultAlignment = 0;
    int hasVram = 0;
    uint32_t xeVmId = 0;
    uint32_t userPtrHandle = 0;
    uint32_t addressWidth = 48;
    int numberHwEngines = 0;
    std::unique_ptr<struct drm_xe_engine_class_instance[]> hwEngines;
    int xe_fileHandle = 0;
    std::mutex xeLock;
    std::vector<bind_info> bindInfo;
    int instance = 0;
    uint64_t xeMemoryRegions = 0;
    uint32_t xeTimestampFrequency = 0;
    std::vector<uint8_t> memQueryFakei915;
    std::vector<uint8_t> engineFakei915;
    std::vector<uint8_t> hwconfigFakei915;
    std::vector<uint8_t> topologyFakei915;
    std::vector<drm_xe_engine_class_instance> contextParamEngine;
};

} // namespace NEO
