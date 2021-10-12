/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

namespace NEO {

typedef ICLFamily Family;

static auto gfxCore = IGFX_GEN11_CORE;

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

template class UltCommandStreamReceiver<ICLFamily>;
} // namespace NEO
