/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

typedef XeHpgCoreFamily Family;

constexpr auto gfxCore = IGFX_XE_HPG_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * NEO::maxCoreEnumValue];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[NEO::maxCoreEnumValue + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct EnableXeHpgCore {
    EnableXeHpgCore() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static EnableXeHpgCore enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerXeHpgCore;

template class UltCommandStreamReceiver<XeHpgCoreFamily>;
} // namespace NEO
