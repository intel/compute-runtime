/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/base_object.h"
#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"

namespace NEO {

typedef BDWFamily Family;

static const auto gfxCore = IGFX_GEN8_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableGen8 {
    enableGen8() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableGen8 enable;

template class UltCommandStreamReceiver<BDWFamily>;
} // namespace NEO
