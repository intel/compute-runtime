/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/walker_partition_xehp_and_later.h"

namespace WalkerPartition {
template <typename GfxFamily, typename WalkerType>
void appendWalkerFields(WalkerType &walkerCmd, uint32_t tileCount, uint32_t workgroupCount) {
    if constexpr (GfxFamily::template isHeaplessMode<WalkerType>()) {
        using DISPATCH_ALL_MOD_VALUE = typename GfxFamily::COMPUTE_WALKER_2::DISPATCH_ALL_MOD_VALUE;

        if (!walkerCmd.getComputeDispatchAllWalkerEnable() || tileCount == 1) {
            walkerCmd.setDispatchAllModValue(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD1);
        } else if (workgroupCount % walkerCmd.getPartitionSize() != 0) {
            if (tileCount == 2) {
                walkerCmd.setDispatchAllModValue(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD2);
            } else {
                DEBUG_BREAK_IF(tileCount != 4);
                walkerCmd.setDispatchAllModValue(DISPATCH_ALL_MOD_VALUE::DISPATCH_ALL_MOD_VALUE_MOD4);
            }
        }

        if (NEO::debugManager.flags.OverrideDispatchAllModValue.get() != -1) {
            walkerCmd.setDispatchAllModValue(static_cast<DISPATCH_ALL_MOD_VALUE>(NEO::debugManager.flags.OverrideDispatchAllModValue.get()));
        }
    }
}
} // namespace WalkerPartition
