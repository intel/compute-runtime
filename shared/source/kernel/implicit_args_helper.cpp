/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/kernel/implicit_args.h"
#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {

std::array<uint8_t, 3> ImplicitArgsHelper::getDimensionOrderForLocalIds(const uint8_t *workgroupDimensionsOrder, bool generationOfLocalIdsByRuntime, uint32_t walkOrderForHwGenerationOfLocalIds) {
    if (generationOfLocalIdsByRuntime) {
        UNRECOVERABLE_IF(!workgroupDimensionsOrder);
        return {{
            workgroupDimensionsOrder[0],
            workgroupDimensionsOrder[1],
            workgroupDimensionsOrder[2],
        }};
    }

    UNRECOVERABLE_IF(walkOrderForHwGenerationOfLocalIds >= HwWalkOrderHelper::walkOrderPossibilties);
    return HwWalkOrderHelper::compatibleDimensionOrders[walkOrderForHwGenerationOfLocalIds];
}
} // namespace NEO
