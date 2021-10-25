/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

namespace NEO {

typedef SKLFamily Family;

static const auto gfxCore = IGFX_GEN9_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableGen9 {
    enableGen9() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableGen9 enable;

template class UltCommandStreamReceiver<SKLFamily>;
} // namespace NEO
