/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_interface.h"
#include "shared/source/command_container/walker_partition_xehp_and_later.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xe2_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xe_hpc_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"

using Family = NEO::Xe3pCoreFamily;

namespace NEO {

template <>
void UnitTestHelper<Family>::validateSbaMocs(uint32_t expectedMocs, CommandStreamReceiver &csr) {
}

template <>
uint32_t UnitTestHelper<Family>::getInlineDataSize(bool isHeaplessEnabled) {
    using COMPUTE_WALKER = typename Family::COMPUTE_WALKER;
    using COMPUTE_WALKER_2 = typename Family::COMPUTE_WALKER_2;

    if (isHeaplessEnabled) {
        return COMPUTE_WALKER_2::getInlineDataSize();
    } else {
        return COMPUTE_WALKER::getInlineDataSize();
    }
}

template <>
uint32_t UnitTestHelper<Family>::getProgrammedGrfValue(CommandStreamReceiver &csr, LinearStream &linearStream) {
    using INTERFACE_DESCRIPTOR_DATA = typename Family::INTERFACE_DESCRIPTOR_DATA;
    using REGISTERS_PER_THREAD = typename INTERFACE_DESCRIPTOR_DATA::REGISTERS_PER_THREAD;

    HardwareParse hwParser;
    hwParser.parseCommands<Family>(csr, linearStream);

    auto &idd = *(INTERFACE_DESCRIPTOR_DATA *)hwParser.cmdInterfaceDescriptorData;
    const auto registersPerThread = idd.getRegistersPerThread();
    std::unordered_map<REGISTERS_PER_THREAD, uint32_t> values = {{{REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_32, 32u},
                                                                  {REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_64, 64u},
                                                                  {REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_96, 96u},
                                                                  {REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_128, 128u},
                                                                  {REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_160, 160u},
                                                                  {REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_192, 192u},
                                                                  {REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_256, 256u}}};

    return values[registersPerThread];
}

template <>
uint64_t UnitTestHelper<Family>::getWalkerPartitionEstimateSpaceRequiredInCommandBuffer(bool isHeaplessEnabled, WalkerPartition::WalkerPartitionArgs &testArgs) {
    using COMPUTE_WALKER = typename Family::COMPUTE_WALKER;
    using COMPUTE_WALKER_2 = typename Family::COMPUTE_WALKER_2;

    if (isHeaplessEnabled) {
        return WalkerPartition::estimateSpaceRequiredInCommandBuffer<Family, COMPUTE_WALKER_2>(testArgs);
    } else {
        return WalkerPartition::estimateSpaceRequiredInCommandBuffer<Family, COMPUTE_WALKER>(testArgs);
    }
}

template <>
GenCmdList::iterator UnitTestHelper<Family>::findWalkerTypeCmd(GenCmdList::iterator begin, GenCmdList::iterator end) {

    auto walkerIt = find<typename Family::COMPUTE_WALKER_2 *>(begin, end);

    if (walkerIt == end) {
        walkerIt = find<typename Family::COMPUTE_WALKER *>(begin, end);
    }

    return walkerIt;
}

template <>
std::vector<GenCmdList::iterator> UnitTestHelper<Family>::findAllWalkerTypeCmds(GenCmdList::iterator begin, GenCmdList::iterator end) {
    auto cmds = findAll<typename Family::COMPUTE_WALKER *>(begin, end);
    if (cmds.empty()) {
        cmds = findAll<typename Family::COMPUTE_WALKER_2 *>(begin, end);
    }
    return cmds;
}

template <>
size_t UnitTestHelper<Family>::getWalkerSize(bool isHeaplessEnabled) {
    using COMPUTE_WALKER = typename Family::COMPUTE_WALKER;
    using COMPUTE_WALKER_2 = typename Family::COMPUTE_WALKER_2;

    if (isHeaplessEnabled) {
        return sizeof(COMPUTE_WALKER_2);
    } else {
        return sizeof(COMPUTE_WALKER);
    }
}

template <>
void UnitTestHelper<Family>::getSpaceAndInitWalkerCmd(LinearStream &stream, bool heapless) {
    using COMPUTE_WALKER = typename Family::COMPUTE_WALKER;
    using COMPUTE_WALKER_2 = typename Family::COMPUTE_WALKER_2;

    if (heapless) {
        *stream.getSpaceForCmd<COMPUTE_WALKER_2>() = Family::template getInitGpuWalker<COMPUTE_WALKER_2>();

    } else {
        *stream.getSpaceForCmd<COMPUTE_WALKER>() = Family::template getInitGpuWalker<COMPUTE_WALKER>();
    }
}

template <>
void *UnitTestHelper<Family>::getInitWalkerCmd(bool heapless) {
    using COMPUTE_WALKER = typename Family::COMPUTE_WALKER;
    using COMPUTE_WALKER_2 = typename Family::COMPUTE_WALKER_2;

    if (heapless) {
        return new COMPUTE_WALKER_2;

    } else {
        return new COMPUTE_WALKER;
    }
}

template <>
bool UnitTestHelper<Family>::isHeaplessAllowed() {
    return true;
}

template <>
template <typename WalkerType>
uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress(WalkerType *walkerCmd) {
    using COMPUTE_WALKER = typename Family::COMPUTE_WALKER;
    using COMPUTE_WALKER_2 = typename Family::COMPUTE_WALKER_2;
    if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER>) {
        return walkerCmd->getPostSync().getDestinationAddress();
    }
    if constexpr (std::is_same_v<WalkerType, COMPUTE_WALKER_2>) {
        using PostSyncData = typename Family::POSTSYNC_DATA_2;
        const std::array<PostSyncData *, 4> postSyncTable = {{{&walkerCmd->getPostSyncOpn3()},
                                                              {&walkerCmd->getPostSyncOpn2()},
                                                              {&walkerCmd->getPostSyncOpn1()},
                                                              {&walkerCmd->getPostSync()}}};
        for (auto &postSync : postSyncTable) {
            if (postSync->getDestinationAddress() != 0) {
                return postSync->getDestinationAddress();
            }
        }
    }
    return 0;
}

template struct UnitTestHelper<Family>;
template struct UnitTestHelperWithHeap<Family>;

template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER *walkerCmd);
template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::COMPUTE_WALKER_2>(Family::COMPUTE_WALKER_2 *walkerCmd);
} // namespace NEO
