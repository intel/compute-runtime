/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gen_common/aub_mapper.h"
#include "runtime/gen_common/hw_cmds.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/built_ins/sip.h"

#include <cstdint>
#include <type_traits>

namespace OCLRT {
struct HardwareCapabilities;

class HwHelper {
  public:
    static HwHelper &get(GFXCORE_FAMILY gfxCore);
    virtual uint32_t getBindingTableStateSurfaceStatePointer(void *pBindingTable, uint32_t index) = 0;
    virtual size_t getBindingTableStateSize() const = 0;
    virtual uint32_t getBindingTableStateAlignement() const = 0;
    virtual size_t getInterfaceDescriptorDataSize() const = 0;
    virtual size_t getMaxBarrierRegisterPerSlice() const = 0;
    virtual uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const = 0;
    virtual void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) = 0;
    virtual bool setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) = 0;
    virtual void adjustDefaultEngineType(HardwareInfo *pHwInfo) = 0;
    virtual void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) = 0;
    virtual SipKernelType getSipKernelType(bool debuggingActive) = 0;
    virtual uint32_t getConfigureAddressSpaceMode() = 0;
    virtual bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) = 0;
    virtual bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const = 0;
    virtual const AubMemDump::LrcaHelper &getCsTraits(EngineType engineType) const = 0;
    virtual bool supportsYTiling() const = 0;

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

    const AubMemDump::LrcaHelper &getCsTraits(EngineType engineType) const override;

    size_t getMaxBarrierRegisterPerSlice() const override;

    uint32_t getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const override;

    void setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) override;

    bool setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) override;

    void adjustDefaultEngineType(HardwareInfo *pHwInfo) override;

    void setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) override;

    SipKernelType getSipKernelType(bool debuggingActive) override;

    uint32_t getConfigureAddressSpaceMode() override;

    bool isLocalMemoryEnabled(const HardwareInfo &hwInfo) override;

    bool supportsYTiling() const override;

    bool isPageTableManagerSupported(const HardwareInfo &hwInfo) const override;

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
        *lri = MI_LOAD_REGISTER_IMM::sInit();
        lri->setRegisterOffset(address);
        lri->setDataDword(value);
        return lri;
    }
};
} // namespace OCLRT
