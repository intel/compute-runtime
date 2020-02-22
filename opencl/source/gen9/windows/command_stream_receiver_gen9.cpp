/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "opencl/source/os_interface/windows/device_command_stream.inl"
#include "opencl/source/os_interface/windows/wddm_device_command_stream.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<SKLFamily>;
template class WddmCommandStreamReceiver<SKLFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<SKLFamily>>;
} // namespace NEO
