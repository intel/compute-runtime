/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe2_hpg_core/hw_cmds.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

using Family = Xe2HpgCoreFamily;
constexpr auto gfxCore = IGFX_XE2_HPG_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct EnableXe2HpgCore {
    EnableXe2HpgCore() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static EnableXe2HpgCore enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerXe2HpgCore;
template class UltCommandStreamReceiver<Family>;
} // namespace NEO
