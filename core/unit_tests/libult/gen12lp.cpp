/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/base_object.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

namespace NEO {

typedef TGLLPFamily Family;

static auto gfxCore = IGFX_GEN12LP_CORE;

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

template class UltCommandStreamReceiver<TGLLPFamily>;
} // namespace NEO
