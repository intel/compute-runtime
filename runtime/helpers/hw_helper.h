/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/command_stream/linear_stream.h"
#include "runtime/built_ins/sip.h"
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/mem_obj/buffer.h"

#include "CL/cl.h"

#include <cstdint>
#include <string>
#include <type_traits>

namespace NEO {
class ExecutionEnvironment;
class GraphicsAllocation;
struct HardwareCapabilities;
class GmmHelper;

class HwHelper {
  public:
    static HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getBindingTableStateSurfaceStatePointer(void *pBindingTable, uint32_t index) = 0;
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
    virtual uint32_t getConfigureAddressSpaceMode() = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(aub_stream::EngineType engineType) const = 0;
    virtual bool hvAlign4Required() const = 0;
    virtual bool obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const = 0;
    virtual void checkResourceCompatibility(Buffer *buffer, cl_int &errorCode) = 0;
    static bool renderCompressedBuffersSupported(const HardwareInfo &hwInfo);
    static bool renderCompressedImagesSupported(const HardwareInfo &hwInfo);
    static bool cacheFlushAfterWalkerSupported(const HardwareInfo &hwInfo);
    virtual bool timestampPacketWriteSupported() const = 0;
    virtual size_t getRenderSurfaceStateSize() const = 0;
    virtual void setRenderSurfaceStateForBuffer(ExecutionEnvironment &executionEnvironment,
                                                void *surfaceStateBuffer,
                                                size_t bufferSize,
                                                uint64_t gpuVa,
                                                size_t offset,
                                                uint32_t pitch,
                                                GraphicsAllocation *gfxAlloc,
                                                cl_mem_flags flags,
                                                uint32_t surfaceType,
                                                bool forceNonAuxMode) = 0;
    virtual const std::vector<aub_stream::EngineType> getGpgpuEngineInstances() const = 0;
    virtual bool getEnableLocalMemory(const HardwareInfo &hwInfo) const = 0;
    virtual std::string getExtensions() const = 0;
    static uint32_t getMaxThreadsForVfe(const HardwareInfo &hwInfo);
    virtual uint32_t getMetricsLibraryGenId() const = 0;
    virtual uint32_t getMocsIndex(GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const = 0;
    virtual bool requiresAuxResolves() const = 0;
    virtual bool tilingAllowed(bool isSharedContext, const cl_image_desc &imgDesc, bool forceLinearStorage) = 0;

    static constexpr uint32_t lowPriorityGpgpuEngineIndex = 1;

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

    uint32_t getBindingTableStateSurfaceStatePointer(void *pBindingTable, uint32_t index) override {
        using BINDING_TABLE_STATE = typename GfxFamily::BINDING_TABLE_STATE;

        BINDING_TABLE_STATE *bindingTableState = static_cast<BINDING_TABLE_STATE *>(pBindingTable);
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

    uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const override;

    uint32_t getPitchAlignmentForImage(const HardwareInfo *hwInfo) override;

    void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo) override;

    void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) override;

    bool isL3Configurable(const HardwareInfo &hwInfo) override;

    SipKernelType getSipKernelType(bool debuggingActive) override;

    uint32_t getConfigureAddressSpaceMode() override;

    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) const override;

    bool hvAlign4Required() const override;

    bool obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const override;

    void checkResourceCompatibility(Buffer *buffer, cl_int &errorCode) override;

    bool timestampPacketWriteSupported() const override;

    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override;

    void setRenderSurfaceStateForBuffer(ExecutionEnvironment &executionEnvironment,
                                        void *surfaceStateBuffer,
                                        size_t bufferSize,
                                        uint64_t gpuVa,
                                        size_t offset,
                                        uint32_t pitch,
                                        GraphicsAllocation *gfxAlloc,
                                        cl_mem_flags flags,
                                        uint32_t surfaceType,
                                        bool forceNonAuxMode) override;

    const std::vector<aub_stream::EngineType> getGpgpuEngineInstances() const override;

    bool getEnableLocalMemory(const HardwareInfo &hwInfo) const override;

    std::string getExtensions() const override;

    uint32_t getMetricsLibraryGenId() const override;

    uint32_t getMocsIndex(GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const override;

    bool requiresAuxResolves() const override;

    bool tilingAllowed(bool isSharedContext, const cl_image_desc &imgDesc, bool forceLinearStorage) override;

    static AuxTranslationMode getAuxTranslationMode();

  protected:
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
struct PipeControlHelper {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
    using POST_SYNC_OPERATION = typename GfxFamily::PIPE_CONTROL::POST_SYNC_OPERATION;
    static PIPE_CONTROL *obtainPipeControlAndProgramPostSyncOperation(LinearStream &commandStream,
                                                                      POST_SYNC_OPERATION operation,
                                                                      uint64_t gpuAddress,
                                                                      uint64_t immediateData,
                                                                      bool dcFlush, const HardwareInfo &hwInfo);
    static void addPipeControlWA(LinearStream &commandStream, const HardwareInfo &hwInfo);
    static PIPE_CONTROL *addPipeControl(LinearStream &commandStream, bool dcFlush);
    static size_t getSizeForPipeControlWithPostSyncOperation(const HardwareInfo &hwInfo);
    static size_t getSizeForSinglePipeControl();

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
