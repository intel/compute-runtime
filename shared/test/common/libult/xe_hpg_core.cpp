/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpg_core/hw_cmds.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

typedef XE_HPG_COREFamily Family;

constexpr auto gfxCore = IGFX_XE_HPG_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableXeHpgCore {
    enableXeHpgCore() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableXeHpgCore enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerXeHpgCore;

template class UltCommandStreamReceiver<XE_HPG_COREFamily>;
} // namespace NEO
