/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/device_command_stream.inl"
#include "runtime/os_interface/windows/wddm_device_command_stream.inl"
#include "runtime/command_stream/command_stream_receiver_with_aub_dump.inl"

namespace OCLRT {

template class DeviceCommandStreamReceiver<BDWFamily>;
template class WddmCommandStreamReceiver<BDWFamily>;
template class CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<BDWFamily>>;
} // namespace OCLRT
