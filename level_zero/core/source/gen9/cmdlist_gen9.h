/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gen9/hw_info.h"

#include "level_zero/core/source/cmdlist/cmdlist_hw.h"
#include "level_zero/core/source/cmdlist/cmdlist_hw_immediate.h"

#include "igfxfmid.h"

namespace L0 {

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListProductFamily : public CommandListCoreFamily<IGFX_GEN9_CORE> {
    using CommandListCoreFamily::CommandListCoreFamily;

    void programL3(bool isSLMused) override {
        NEO::EncodeL3State<GfxFamily>::encode(commandContainer, isSLMused);
    }
};

template <PRODUCT_FAMILY gfxProductFamily>
struct CommandListImmediateProductFamily : public CommandListCoreFamilyImmediate<IGFX_GEN9_CORE> {
    using CommandListCoreFamilyImmediate::CommandListCoreFamilyImmediate;
};

} // namespace L0
