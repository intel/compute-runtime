/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3_core/hw_cmds_base.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/unit_test_helper.inl"
#include "shared/test/common/helpers/unit_test_helper_xe2_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xe_hpc_and_later.inl"
#include "shared/test/common/helpers/unit_test_helper_xehp_and_later.inl"

using Family = NEO::Xe3CoreFamily;

namespace NEO {

template <>
void UnitTestHelper<Family>::validateSbaMocs(uint32_t expectedMocs, CommandStreamReceiver &csr) {
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

template struct UnitTestHelper<Family>;
template struct UnitTestHelperWithHeap<Family>;

template uint64_t UnitTestHelper<Family>::getWalkerActivePostSyncAddress<Family::COMPUTE_WALKER>(Family::COMPUTE_WALKER *walkerCmd);
} // namespace NEO
