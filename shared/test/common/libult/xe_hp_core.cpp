/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

namespace NEO {

typedef XeHpFamily Family;

static auto gfxCore = IGFX_XE_HP_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableXeHpCore {
    enableXeHpCore() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableXeHpCore enable;

template class UltCommandStreamReceiver<XeHpFamily>;
} // namespace NEO
