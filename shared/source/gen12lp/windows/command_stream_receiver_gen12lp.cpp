/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/windows/device_command_stream.inl"
#include "shared/source/os_interface/windows/wddm_device_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<TGLLPFamily>;
template class WddmCommandStreamReceiver<TGLLPFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<TGLLPFamily>>;
} // namespace NEO
