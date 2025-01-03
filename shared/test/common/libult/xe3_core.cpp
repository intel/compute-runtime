/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe3_core/hw_cmds_base.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

using Family = Xe3CoreFamily;
constexpr auto gfxCore = IGFX_XE3_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct EnableXe3Core {
    EnableXe3Core() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static EnableXe3Core enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerXe3Core;

template class UltCommandStreamReceiver<Family>;
} // namespace NEO
