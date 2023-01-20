/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

typedef Gen12LpFamily Family;

constexpr auto gfxCore = IGFX_GEN12LP_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableGen12LP {
    enableGen12LP() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableGen12LP enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerGen12lp;

template class UltCommandStreamReceiver<Gen12LpFamily>;
} // namespace NEO
