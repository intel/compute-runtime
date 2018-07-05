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

#include "runtime/command_stream/experimental_command_buffer.h"
#include "runtime/command_stream/experimental_command_buffer.inl"
#include "runtime/helpers/hw_helper.h"

namespace OCLRT {
typedef SKLFamily GfxFamily;

template void ExperimentalCommandBuffer::injectBufferStart<GfxFamily>(LinearStream &parentStream, size_t cmdBufferOffset);
template size_t ExperimentalCommandBuffer::getRequiredInjectionSize<GfxFamily>() noexcept;

template size_t ExperimentalCommandBuffer::programExperimentalCommandBuffer<GfxFamily>();
template size_t ExperimentalCommandBuffer::getTotalExperimentalSize<GfxFamily>() noexcept;

template void ExperimentalCommandBuffer::addTimeStampPipeControl<GfxFamily>();
template size_t ExperimentalCommandBuffer::getTimeStampPipeControlSize<GfxFamily>() noexcept;

template void ExperimentalCommandBuffer::addExperimentalCommands<GfxFamily>();
template size_t ExperimentalCommandBuffer::getExperimentalCommandsSize<GfxFamily>() noexcept;
} // namespace OCLRT
