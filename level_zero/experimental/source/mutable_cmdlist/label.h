/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/arrayref.h"

#include "level_zero/core/source/mutable_cmdlist/mcl_types.h"
#include "level_zero/core/source/mutable_cmdlist/usage.h"
#include "level_zero/experimental/source/mutable_cmdlist/label_handle.h"

#include <string>

namespace L0::MCL {

struct InterfaceLabelDescriptor {
    const char *name = nullptr;
    uint32_t alignment = 0;
};

struct Label : public LabelHandle {
    Label() = delete;
    Label(ze_command_list_handle_t hCmdList, const InterfaceLabelDescriptor *desc);
    static Label *create(ze_command_list_handle_t hCmdList, const InterfaceLabelDescriptor *desc);

    static Label *fromHandle(LabelHandle *handle) { return static_cast<Label *>(handle); }
    inline LabelHandle *toHandle() { return this; }
    ze_result_t setLabel(ze_command_list_handle_t hCmdList);

    inline bool isSet() const { return gpuAddress != undefined<GpuAddress>; };
    inline GpuAddress getAddress() const { return gpuAddress; }
    std::string getName() const { return name; }

    inline void addUsage(CommandBufferOffset offset) { labelUsages.push_back(offset); }
    inline ArrayRef<const CommandBufferOffset> getUsages() const { return {labelUsages.begin(), labelUsages.size()}; }

  protected:
    ze_command_list_handle_t hCmdList;
    std::string name;
    GpuAddress alignment;
    GpuAddress gpuAddress = undefined<GpuAddress>;
    StackVec<CommandBufferOffset, 8> labelUsages;
};

} // namespace L0::MCL
