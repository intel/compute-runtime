/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

#include "hw_cmds_xe3p_core.h"

namespace NEO {

using Family = Xe3pCoreFamily;
constexpr auto gfxCore = IGFX_XE3P_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * NEO::maxCoreEnumValue];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[NEO::maxCoreEnumValue + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct EnableXe3pCore {
    EnableXe3pCore() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static EnableXe3pCore enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerXe3pCore;

template class UltCommandStreamReceiver<Family>;
} // namespace NEO
