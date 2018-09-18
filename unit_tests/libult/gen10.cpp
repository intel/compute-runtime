/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/base_object.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

namespace OCLRT {

typedef CNLFamily Family;

static auto gfxCore = IGFX_GEN10_CORE;

extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];

template <>
void populateFactoryTable<UltCommandStreamReceiver<Family>>() {
    commandStreamReceiverFactory[IGFX_MAX_CORE + gfxCore] = UltCommandStreamReceiver<Family>::create;
}

struct enableGen10 {
    enableGen10() {
        populateFactoryTable<UltCommandStreamReceiver<Family>>();
    }
};

static enableGen10 enable;

template class UltCommandStreamReceiver<CNLFamily>;
} // namespace OCLRT
