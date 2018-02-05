/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/helpers/translationtable_callbacks.h"
#include "runtime/command_stream/linear_stream.h"
#include "runtime/helpers/hw_helper.h"
#include <cstdint>

namespace OCLRT {
template <typename GfxFamily>
int __stdcall TTCallbacks<GfxFamily>::writeL3Address(void *queueHandle, uint64_t l3GfxAddress, uint64_t regOffset) {

    auto cmdStream = reinterpret_cast<LinearStream *>(queueHandle);

    auto lri1 = LriHelper<GfxFamily>::program(cmdStream,
                                              static_cast<uint32_t>(regOffset & 0xFFFFFFFF),
                                              static_cast<uint32_t>(l3GfxAddress & 0xFFFFFFFF));
    appendLriParams(lri1);

    auto lri2 = LriHelper<GfxFamily>::program(cmdStream,
                                              static_cast<uint32_t>(regOffset >> 32),
                                              static_cast<uint32_t>(l3GfxAddress >> 32));
    appendLriParams(lri2);

    return 1;
};

template <typename GfxFamily>
void TTCallbacks<GfxFamily>::appendLriParams(MI_LOAD_REGISTER_IMM *lri) {}
} // namespace OCLRT
