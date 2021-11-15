/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.inl"
#include "shared/source/os_interface/linux/device_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream.inl"
#include "shared/source/os_interface/linux/drm_command_stream_xehp_and_later.inl"

namespace NEO {

template class DeviceCommandStreamReceiver<XE_HPG_COREFamily>;
template class DrmCommandStreamReceiver<XE_HPG_COREFamily>;
template class CommandStreamReceiverWithAUBDump<DrmCommandStreamReceiver<XE_HPG_COREFamily>>;
} // namespace NEO
