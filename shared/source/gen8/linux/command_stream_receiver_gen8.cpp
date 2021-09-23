/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream_bdw_and_later.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<BDWFamily>;
template class DrmCommandStreamReceiver<BDWFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<BDWFamily>>;
} // namespace NEO
