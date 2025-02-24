/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

inline constexpr int testValueVmId = 0x5764;
inline constexpr int testValueMapOff = 0x7788;
inline constexpr int testValuePrime = 0x4321;
inline constexpr uint32_t testValueGemCreate = 0x8273;
struct DrmMockXe : public DrmMockCustom {
    using Drm::engineInfo;

    static std::unique_ptr<DrmMockXe> create(RootDeviceEnvironment &rootDeviceEnvironment);

    void testMode(int f, int a = 0);
    int ioctl(DrmIoctl request, void *arg) override;
    virtual void handleUserFenceWaitExtensions(drm_xe_wait_user_fence *userFenceWait) {}
    virtual void handleContextCreateExtensions(drm_xe_user_extension *extension) {}

    void addMockedQueryTopologyData(uint16_t gtId, uint16_t maskType, uint32_t nBytes, const std::vector<uint8_t> &mask);

    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
    int gemVmBindReturn = 0;
    GemClose passedGemClose{};
    int gemCloseCalled = 0;
    int gemMmapOffsetCalled = 0;
    int gemDestroyContextCalled = 0;

    uint64_t queryConfig[7]{}; // 1 qword for num params and 1 qwords per param
    uint32_t mockExecQueueId = 1234;
    static constexpr int32_t mockMaxExecQueuePriority = 3;
    static constexpr int32_t mockDefaultCxlType = 0;
    static constexpr uint32_t mockTimestampFrequency = 12500000;
    static_assert(sizeof(drm_xe_engine) == 4 * sizeof(uint64_t), "");
    uint64_t queryEngines[52]{}; // 1 qword for num engines and 4 qwords per engine
    static_assert(sizeof(drm_xe_mem_region) == 11 * sizeof(uint64_t), "");
    uint64_t queryMemUsage[34]{}; // 1 qword for num regions and 11 qwords per region
    static_assert(sizeof(drm_xe_gt) == 12 * sizeof(uint64_t), "");
    StackVec<uint64_t, 49> queryGtList{}; // 1 qword for num gts and 12 qwords per gt
    alignas(64) std::vector<uint8_t> queryTopology;
    static_assert(sizeof(drm_xe_query_engine_cycles) == 5 * sizeof(uint64_t), "");
    uint64_t queryEngineCycles[5]{}; // 1 qword for eci and 4 qwords
    StackVec<drm_xe_wait_user_fence, 1> waitUserFenceInputs;
    StackVec<drm_xe_vm_bind, 1> vmBindInputs;
    StackVec<drm_xe_sync, 1> syncInputs;
    StackVec<drm_xe_ext_set_property, 1> execQueueProperties;
    drm_xe_exec_queue_create latestExecQueueCreate = {};
    std::vector<drm_xe_engine_class_instance> latestQueueEngineClassInstances;

    int waitUserFenceReturn = 0;
    int execQueueBanPropertyReturn = 0;
    uint32_t createParamsFlags = 0u;
    uint16_t createParamsCpuCaching = 0u;
    uint32_t createParamsPlacement = 0u;
    bool ioctlCalled = false;
    bool forceMmapOffsetFail = false;

  protected:
    // Don't call directly, use the create() function
    DrmMockXe(RootDeviceEnvironment &rootDeviceEnvironment)
        : DrmMockCustom(std::make_unique<HwDeviceIdDrm>(mockFd, mockPciPath), rootDeviceEnvironment) {}

    virtual void initInstance();
};
