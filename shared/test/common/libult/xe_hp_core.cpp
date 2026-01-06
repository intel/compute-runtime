/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"

namespace NEO {

typedef XeHpFamily Family;

constexpr auto gfxCore = IGFX_XE_HP_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * NEO::maxCoreEnumValue];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[NEO::maxCoreEnumValue + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableXeHpCore {
    enableXeHpCore() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableXeHpCore enable;
static MockDebuggerL0HwPopulateFactory<gfxCore, Family> mockDebuggerXE_HP_CORE;

template class UltCommandStreamReceiver<XeHpFamily>;
} // namespace NEO
