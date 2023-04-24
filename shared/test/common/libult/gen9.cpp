/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

typedef Gen9Family Family;

constexpr auto gfxCore = IGFX_GEN9_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct EnableGen9 {
    EnableGen9() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static EnableGen9 enable;

static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerGen9;

template class UltCommandStreamReceiver<Gen9Family>;
} // namespace NEO
