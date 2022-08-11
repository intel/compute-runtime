/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace NEO {

enum class MemoryCompressionState;
class GmmHelper;
class IndirectHeap;
class LinearStream;
struct DispatchFlags;
struct HardwareInfo;

template <typename GfxFamily>
struct StateBaseAddressHelperArgs {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    uint64_t generalStateBase = 0;
    uint64_t indirectObjectHeapBaseAddress = 0;
    uint64_t instructionHeapBaseAddress = 0;
    uint64_t globalHeapsBaseAddress = 0;

    STATE_BASE_ADDRESS *stateBaseAddressCmd = nullptr;

    const IndirectHeap *dsh = nullptr;
    const IndirectHeap *ioh = nullptr;
    const IndirectHeap *ssh = nullptr;
    GmmHelper *gmmHelper = nullptr;

    uint32_t statelessMocsIndex = 0;
    MemoryCompressionState memoryCompressionState;

    bool setInstructionStateBaseAddress = false;
    bool setGeneralStateBaseAddress = false;
    bool useGlobalHeapsBaseAddress = false;
    bool isMultiOsContextCapable = false;
    bool useGlobalAtomics = false;
    bool areMultipleSubDevicesInContext = false;
};

template <typename GfxFamily>
struct StateBaseAddressHelper {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    static STATE_BASE_ADDRESS *getSpaceForSbaCmd(LinearStream &cmdStream);

    static void programStateBaseAddress(StateBaseAddressHelperArgs<GfxFamily> &args);

    static void appendIohParameters(StateBaseAddressHelperArgs<GfxFamily> &args);

    static void appendStateBaseAddressParameters(StateBaseAddressHelperArgs<GfxFamily> &args,
                                                 bool overrideBindlessSurfaceStateBase);

    static void appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, const HardwareInfo *hwInfo);

    static void programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper);

    static void programBindingTableBaseAddress(LinearStream &commandStream, uint64_t baseAddress, uint32_t sizeInPages, GmmHelper *gmmHelper);

    static uint32_t getMaxBindlessSurfaceStates();
};
} // namespace NEO
