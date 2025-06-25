/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/experimental/source/mutable_cmdlist/label.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/mutable_cmdlist/mutable_cmdlist.h"

namespace L0 {
namespace MCL {

Label::Label(ze_command_list_handle_t hCmdList, const InterfaceLabelDescriptor *desc) {
    this->hCmdList = hCmdList;
    this->name = desc->name == nullptr ? "" : std::string(desc->name);
    this->alignment = desc->alignment;
    this->gpuAddress = undefined<GpuAddress>;
}

Label *Label::create(ze_command_list_handle_t hCmdList, const InterfaceLabelDescriptor *desc) {
    return new Label(hCmdList, desc);
}

ze_result_t Label::setLabel(ze_command_list_handle_t hCmdList) {
    if (this->hCmdList != hCmdList)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;

    if (isSet())
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;

    auto cmdList = MutableCommandList::fromHandle(hCmdList);
    auto cs = cmdList->getBase()->getCmdContainer().getCommandStream();

    GpuAddress labelAddress = cs->getGraphicsAllocation()->getGpuAddress() + cs->getUsed();
    if (this->alignment) {
        const GpuAddress alignedAddress = alignUp<GpuAddress>(labelAddress, this->alignment);
        if (auto padding = alignedAddress - labelAddress) {
            if (padding % 4 != 0U) {
                return ZE_RESULT_ERROR_UNSUPPORTED_ALIGNMENT;
            }
            auto numNoops = padding >> 2;
            for (size_t i = 0; i < numNoops; i++) {
                cmdList->getBase()->appendMINoop();
            }
        }
        labelAddress = alignedAddress;
    }
    this->gpuAddress = labelAddress;

    cmdList->appendSetPredicate(NEO::MiPredicateType::disable);
    auto csCpuBase = cs->getCpuBase();
    for (const auto offset : getUsages()) {
        memcpy_s(ptrOffset(csCpuBase, offset), sizeof(GpuAddress), &labelAddress, sizeof(GpuAddress));
    }

    return ZE_RESULT_SUCCESS;
}

} // namespace MCL
} // namespace L0
