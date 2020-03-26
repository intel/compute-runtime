/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/commands/bxml_generator_glue.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/hw_cmds.h"

#include "opencl/source/aub_mem_dump/aub_mem_dump.h"
#include "opencl/source/gen_common/aub_mapper.h"
#include "opencl/source/mem_obj/buffer.h"

#include <cstdint>
#include <string>
#include <type_traits>

namespace NEO {
class GraphicsAllocation;
struct RootDeviceEnvironment;
struct HardwareCapabilities;
class GmmHelper;

class HwHelper {
  public:
    using EngineInstancesContainer = StackVec<aub_stream::EngineType, 32>;
    static HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getBindingTableStateSurfaceStatePointer(const void *pBindingTable, uint32_t index) = 0;
    virtual size_t getBindingTableStateSize() const = 0;
    virtual uint32_t getBindingTableStateAlignement() const = 0;
    virtual size_t getInterfaceDescriptorDataSize() const = 0;
    virtual size_t getMaxBarrierRegisterPerSlice() const = 0;
    virtual uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const = 0;
    virtual uint32_t getPitchAlignmentForImage(const HardwareInfo *hwInfo) = 0;
    virtual void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) = 0;
    virtual void adjustDefaultEngineType(HardwareInfo *pHwInfo) = 0;
    virtual void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) = 0;
    virtual bool isL3Configurable(const HardwareInfo &hwInfo) = 0;
    virtual SipKernelType getSipKernelType(bool debuggingActive) = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const = 0;
    virtual bool hvAlign4Required() const = 0;
    virtual bool obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const = 0;
    virtual bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) = 0;
    static bool renderCompressedBuffersSupported(const HardwareInfo &hwInfo);
    static bool renderCompressedImagesSupported(const HardwareInfo &hwInfo);
    static bool cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo);
    virtual bool timestampPacketWriteSupported() const = 0;
    virtual size_t getRenderSurfaceStateSize() const = 0;
    virtual void setRenderSurfaceStateForBuffer(const RootDeviceEnvironment &rootDeviceEnvironment,
                                                void *surfaceStateBuffer,
                                                size_t bufferSize,
                                                uint64_t gpuVa,
                                                size_t offset,
                                                uint32_t pitch,
                                                GraphicsAllocation *gfxAlloc,
                                                bool isReadOnly,
                                                uint32_t surfaceType,
                                                bool forceNonAuxMode) = 0;
    virtual const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const = 0;
    virtual const StackVec<size_t, 3> getDeviceSubGroupSizes() const = 0;
    virtual bool getEnableLocalMemory(const HardwareInfo &hwInfo) const = 0;
    virtual std::string getExtensions() const = 0;
    static uint32_t getMaxThreadsForVfe(const HardwareInfo &hwInfo);
    virtual uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const;
    virtual uint32_t getMetricsLibraryGenId() const = 0;
    virtual uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const = 0;
    virtual bool requiresAuxResolves() const = 0;
    virtual bool tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) = 0;
    virtual uint32_t getBarriersCountFromHasBarriers(uint32_t hasBarriers) = 0;
    virtual uint32_t calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount,
                                                   uint32_t threadsPerEu) = 0;
    virtual uint32_t alignSlmSize(uint32_t slmSize) = 0;
    virtual bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) = 0;
    virtual uint32_t getMinimalSIMDSize() = 0;
    virtual bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) const = 0;
    virtual bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo) const = 0;

    static uint32_t getSubDevicesCount(const HardwareInfo *pHwInfo);
    static uint32_t getEnginesCount(const HardwareInfo &hwInfo);

    static constexpr uint32_t lowPriorityGpgpuEngineIndex = 1;
    static constexpr uint32_t internalUsageEngineIndex = 2;

  protected:
    HwHelper() = default;
};

template <typename GfxFamily>
class HwHelperHw : public HwHelper {
  public:
    static HwHelper &get() {
        static HwHelperHw<GfxFamily> hwHelper;
        return hwHelper;
    }

    static const aub_stream::EngineType lowPriorityEngineType;

    uint32_t getBindingTableStateSurfaceStatePointer(const void *pBindingTable, uint32_t index) override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

        const BINDING_TABLE_STATE *bindingTableState = static_cast<const BINDING_TABLE_STATE *>(pBindingTable);
        return bindingTableState[index].getRawData(0);
    }

    size_t getBindingTableStateSize() const override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
        return sizeof(BINDING_TABLE_STATE);
    }

    uint32_t getBindingTableStateAlignement() const override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;
        return BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE;
    }

    size_t getInterfaceDescriptorDataSize() const override {
        using INTERFACE_DESCRIPTOR_DATA = typename GfxFamily::INTERFACE_DESCRIPTOR_DATA;
        return sizeof(INTERFACE_DESCRIPTOR_DATA);
    }

    size_t getRenderSurfaceStateSize() const override {
        using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
        return sizeof(RENDER_SURFACE_STATE);
    }

    const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const override;

    size_t getMaxBarrierRegisterPerSlice() const override;

    uint32_t getMaxThreadsForWorkgroup(const HardwareInfo &hwInfo, uint32_t maxNumEUsPerSubSlice) const override;

    uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const override;

    uint32_t getPitchAlignmentForImage(const HardwareInfo *hwInfo) override;

    void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo) override;

    void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) override;

    bool isL3Configurable(const HardwareInfo &hwInfo) override;

    SipKernelType getSipKernelType(bool debuggingActive) override;

    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override;

    bool hvAlign4Required() const override;

    bool obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const override;

    bool checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) override;

    bool timestampPacketWriteSupported() const override;

    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override;

    bool isFenceAllocationRequired(const HardwareInfo &hwInfo) const override;

    void setRenderSurfaceStateForBuffer(const RootDeviceEnvironment &rootDeviceEnvironment,
                                        void *surfaceStateBuffer,
                                        size_t bufferSize,
                                        uint64_t gpuVa,
                                        size_t offset,
                                        uint32_t pitch,
                                        GraphicsAllocation *gfxAlloc,
                                        bool isReadOnly,
                                        uint32_t surfaceType,
                                        bool forceNonAuxMode) override;

    const EngineInstancesContainer getGpgpuEngineInstances(const HardwareInfo &hwInfo) const override;

    const StackVec<size_t, 3> getDeviceSubGroupSizes() const override;

    bool getEnableLocalMemory(const HardwareInfo &hwInfo) const override;

    std::string getExtensions() const override;

    uint32_t getMetricsLibraryGenId() const override;

    uint32_t getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const override;

    bool requiresAuxResolves() const override;

    bool tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) override;

    uint32_t getBarriersCountFromHasBarriers(uint32_t hasBarriers) override;

    uint32_t calculateAvailableThreadCount(PRODUCT_FAMILY family, uint32_t grfCount, uint32_t euCount, uint32_t threadsPerEu) override;

    uint32_t alignSlmSize(uint32_t slmSize) override;

    static AuxTranslationMode getAuxTranslationMode();

    static bool isBlitAuxTranslationRequired(const HardwareInfo &hwInfo, const MultiDispatchInfo &multiDispatchInfo);

    bool isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) const override;

    bool is3DPipelineSelectWARequired(const HardwareInfo &hwInfo) const override;

    bool isFusedEuDispatchEnabled(const HardwareInfo &hwInfo) const override;

    static bool isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo);

    bool isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) override;

    uint32_t getMinimalSIMDSize() override;

  protected:
    static const AuxTranslationMode defaultAuxTranslationMode;
    HwHelperHw() = default;
};

struct DwordBuilder {
    static uint32_t build(uint32_t bitNumberToSet, bool masked, bool set = true, uint32_t initValue = 0) {
        uint32_t dword = initValue;
        if (set) {
            dword |= (1 << bitNumberToSet);
        }
        if (masked) {
            dword |= (1 << (bitNumberToSet + 16));
        }
        return dword;
    };
};

template <typename GfxFamily>
struct LriHelper {
    using MI_LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;

    static MI_LOAD_REGISTER_IMM *program(LinearStream *cmdStream, uint32_t address, uint32_t value) {
        auto lri = (MI_LOAD_REGISTER_IMM *)cmdStream->getSpace(sizeof(MI_LOAD_REGISTER_IMM));
        *lri = GfxFamily::cmdInitLoadRegisterImm;
        lri->setRegisterOffset(address);
        lri->setDataDword(value);
        return lri;
    }
};

template <typename GfxFamily>
struct MemorySynchronizationCommands {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;
    static PIPE_CONTROL *obtainPipeControlAndProgramPostSyncOperation(LinearStream &commandStream,
                                                                      POST_SYNC_OPERATION operation,
                                                                      uint64_t gpuAddress,
                                                                      uint64_t immediateData,
                                                                      bool dcFlush, const HardwareInfo &hwInfo);
    static void addAdditionalSynchronization(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo);
    static void addPipeControlWA(LinearStream &commandStream, uint64_t gpuAddress, const HardwareInfo &hwInfo);
    static void setExtraPipeControlProperties(PIPE_CONTROL &pipeControl, const HardwareInfo &hwInfo);
    static PIPE_CONTROL *addPipeControl(LinearStream &commandStream, bool dcFlush);
    static size_t getSizeForPipeControlWithPostSyncOperation(const HardwareInfo &hwInfo);
    static size_t getSizeForSinglePipeControl();
    static size_t getSizeForSingleSynchronization(const HardwareInfo &hwInfo);
    static size_t getSizeForAdditonalSynchronization(const HardwareInfo &hwInfo);

    static PIPE_CONTROL *addFullCacheFlush(LinearStream &commandStream);
    static size_t getSizeForFullCacheFlush();
    static void setExtraCacheFlushFields(PIPE_CONTROL *pipeControl);

  protected:
    static PIPE_CONTROL *obtainPipeControl(LinearStream &commandStream, bool dcFlush);
};

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : BITFIELD_RANGE(0, 6);
        uint32_t Height : BITFIELD_RANGE(7, 20);
        uint32_t Depth : BITFIELD_RANGE(21, 31);
    } SurfaceState;
};

} // namespace NEO
