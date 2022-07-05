/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

typedef ICLFamily Family;

constexpr auto gfxCore = IGFX_GEN11_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableGen11 {
    enableGen11() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableGen11 enable;

static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerGen11;

template class UltCommandStreamReceiver<ICLFamily>;
} // namespace NEO
