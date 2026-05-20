/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/queue_throttle.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/cmdqueue/internal_queue_throttle_ext.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST(CommandQueueThrottleExtensionTest, givenQueueDescWithoutThrottleExtensionWhenExtractingPropertiesThenThrottleIsMedium) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_EQ(NEO::QueueThrottle::MEDIUM, queueProperties.throttle);
}

TEST(CommandQueueThrottleExtensionTest, givenQueueDescWithLowThrottleExtensionWhenExtractingPropertiesThenThrottleIsLow) {
    ze_queue_throttle_ext_desc_t throttleDesc;
    throttleDesc.throttle = NEO::QueueThrottle::LOW;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &throttleDesc;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_EQ(NEO::QueueThrottle::LOW, queueProperties.throttle);
}

TEST(CommandQueueThrottleExtensionTest, givenQueueDescWithHighThrottleExtensionWhenExtractingPropertiesThenThrottleIsHigh) {
    ze_queue_throttle_ext_desc_t throttleDesc;
    throttleDesc.throttle = NEO::QueueThrottle::HIGH;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &throttleDesc;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_EQ(NEO::QueueThrottle::HIGH, queueProperties.throttle);
}

TEST(CommandQueueThrottleExtensionTest, givenQueueDescWithMediumThrottleExtensionWhenExtractingPropertiesThenThrottleIsMedium) {
    ze_queue_throttle_ext_desc_t throttleDesc;
    throttleDesc.throttle = NEO::QueueThrottle::MEDIUM;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &throttleDesc;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_EQ(NEO::QueueThrottle::MEDIUM, queueProperties.throttle);
}

} // namespace ult
} // namespace L0
