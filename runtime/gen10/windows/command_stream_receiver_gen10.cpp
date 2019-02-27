/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "runtime/os_interface/windows/device_command_stream.inl"
#include "runtime/os_interface/windows/wddm_device_command_stream.inl"

namespace OCLRT {

template class DeviceCommandStreamReceiver<CNLFamily>;
template class WddmCommandStreamReceiver<CNLFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<CNLFamily>>;
} // namespace OCLRT
