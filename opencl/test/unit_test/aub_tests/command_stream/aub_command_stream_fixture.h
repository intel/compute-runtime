/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/command_stream_receiver_with_aub_dump.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_banks.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/tests_configuration.h"

#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/command_stream/command_stream_fixture.h"

#include <cstdint>

namespace NEO {
class CommandStreamReceiver;

class AUBCommandStreamFixture : public AUBFixture, public CommandStreamFixture {
  public:
    void setUp(const HardwareInfo *hardwareInfo);
    void setUp();
    void tearDown();

    template <typename FamilyType>
    AUBCommandStreamReceiverHw<FamilyType> *getAubCsr() const {
        CommandStreamReceiver *csr = pCommandStreamReceiver;
        if (testMode == TestMode::aubTestsWithTbx) {
            csr = static_cast<CommandStreamReceiverWithAUBDump<TbxCommandStreamReceiverHw<FamilyType>> *>(csr)->aubCSR.get();
        }
        return static_cast<AUBCommandStreamReceiverHw<FamilyType> *>(csr);
    }

    CommandStreamReceiver *pCommandStreamReceiver = nullptr;
    volatile TagAddressType *pTagMemory = nullptr;
    MockClDevice *pClDevice = nullptr;
    MockDevice *pDevice = nullptr;

  private:
    CommandQueue *commandQueue = nullptr;
};
} // namespace NEO
