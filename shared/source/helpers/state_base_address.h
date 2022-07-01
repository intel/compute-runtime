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
class LogicalStateHelper;
struct DispatchFlags;
struct HardwareInfo;

template <typename GfxFamily>
struct StateBaseAddressHelper {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    static void *getSpaceForSbaCmd(LinearStream &cmdStream);

    static void programStateBaseAddress(
        STATE_BASE_ADDRESS *stateBaseAddress,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        uint64_t generalStateBase,
        bool setGeneralStateBaseAddress,
        uint32_t statelessMocsIndex,
        uint64_t indirectObjectHeapBaseAddress,
        uint64_t instructionHeapBaseAddress,
        uint64_t globalHeapsBaseAddress,
        bool setInstructionStateBaseAddress,
        bool useGlobalHeapsBaseAddress,
        GmmHelper *gmmHelper,
        bool isMultiOsContextCapable,
        MemoryCompressionState memoryCompressionState,
        bool useGlobalAtomics,
        bool areMultipleSubDevicesInContext,
        LogicalStateHelper *logicalStateHelper);

    static void appendIohParameters(STATE_BASE_ADDRESS *stateBaseAddress, const IndirectHeap *ioh, bool useGlobalHeapsBaseAddress, uint64_t indirectObjectHeapBaseAddress);

    static void appendStateBaseAddressParameters(
        STATE_BASE_ADDRESS *stateBaseAddress,
        const IndirectHeap *ssh,
        bool setGeneralStateBaseAddress,
        uint64_t indirectObjectHeapBaseAddress,
        GmmHelper *gmmHelper,
        bool isMultiOsContextCapable,
        MemoryCompressionState memoryCompressionState,
        bool overrideBindlessSurfaceStateBase,
        bool useGlobalAtomics,
        bool areMultipleSubDevicesInContext);

    static void appendExtraCacheSettings(STATE_BASE_ADDRESS *stateBaseAddress, const HardwareInfo *hwInfo);

    static void programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper);

    static uint32_t getMaxBindlessSurfaceStates();
};
} // namespace NEO
