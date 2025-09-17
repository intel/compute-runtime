/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/ze_intel_gpu.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithoutPriorityExtensionWhenExtractingPropertiesThenPriorityLevelIsNotSet) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr; // No extensions at all
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_FALSE(queueProperties.priorityLevel.has_value());
}

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithHighPriorityExtensionWhenExtractingPropertiesThenPriorityLevelIsSet) {
    ze_queue_priority_desc_t priorityDesc = {};
    priorityDesc.stype = ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC;
    priorityDesc.pNext = nullptr;
    priorityDesc.priority = -3; // High priority value

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &priorityDesc; // Priority extension provided
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_TRUE(queueProperties.priorityLevel.has_value());
    EXPECT_EQ(-3, queueProperties.priorityLevel.value());
}

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithLowPriorityExtensionWhenExtractingPropertiesThenPriorityLevelIsSet) {
    ze_queue_priority_desc_t priorityDesc = {};
    priorityDesc.stype = ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC;
    priorityDesc.pNext = nullptr;
    priorityDesc.priority = 4; // Low priority value

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &priorityDesc;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_TRUE(queueProperties.priorityLevel.has_value());
    EXPECT_EQ(4, queueProperties.priorityLevel.value());
}

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithZeroPriorityExtensionWhenExtractingPropertiesThenPriorityLevelIsSet) {
    ze_queue_priority_desc_t priorityDesc = {};
    priorityDesc.stype = ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC;
    priorityDesc.pNext = nullptr;
    priorityDesc.priority = 0; // Zero priority value (normal)

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &priorityDesc;
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_TRUE(queueProperties.priorityLevel.has_value());
    EXPECT_EQ(0, queueProperties.priorityLevel.value());
}

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithMultipleExtensionsIncludingPriorityWhenExtractingPropertiesThenAllAreProcessedCorrectly) {
    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {};
    copyOffloadDesc.stype = ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES;
    copyOffloadDesc.pNext = nullptr;
    copyOffloadDesc.copyOffloadEnabled = true;

    ze_queue_priority_desc_t priorityDesc = {};
    priorityDesc.stype = ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC;
    priorityDesc.pNext = &copyOffloadDesc; // Chain to copy offload extension
    priorityDesc.priority = -1;            // High priority

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &priorityDesc; // Start of extension chain
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_TRUE(queueProperties.priorityLevel.has_value());
    EXPECT_EQ(-1, queueProperties.priorityLevel.value());
    EXPECT_TRUE(queueProperties.copyOffloadHint);
}

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithMultipleExtensionsWithoutPriorityWhenExtractingPropertiesThenPriorityLevelIsNotSet) {
    zex_intel_queue_copy_operations_offload_hint_exp_desc_t copyOffloadDesc = {};
    copyOffloadDesc.stype = ZEX_INTEL_STRUCTURE_TYPE_QUEUE_COPY_OPERATIONS_OFFLOAD_HINT_EXP_PROPERTIES;
    copyOffloadDesc.pNext = nullptr;
    copyOffloadDesc.copyOffloadEnabled = true;

    zex_intel_queue_allocate_msix_hint_exp_desc_t msixDesc = {};
    msixDesc.stype = ZEX_INTEL_STRUCTURE_TYPE_QUEUE_ALLOCATE_MSIX_HINT_EXP_PROPERTIES;
    msixDesc.pNext = &copyOffloadDesc;
    msixDesc.uniqueMsix = true;

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &msixDesc; // Extensions without priority
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_FALSE(queueProperties.priorityLevel.has_value());
    EXPECT_TRUE(queueProperties.copyOffloadHint);
    EXPECT_TRUE(queueProperties.interruptHint);
}

TEST(CommandQueuePriorityExtensionTest, givenQueueDescWithCopyOffloadFlagButNoPriorityExtensionWhenExtractingPropertiesThenPriorityLevelIsNotSet) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr; // No extensions
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = ZE_COMMAND_QUEUE_FLAG_COPY_OFFLOAD_HINT; // Flag set
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_FALSE(queueProperties.priorityLevel.has_value());
    EXPECT_TRUE(queueProperties.copyOffloadHint);
}

TEST(CommandQueuePriorityExtensionTest, givenNoPriorityExtensionWhenCreatingCommandQueueThenCsrGetsNulloptPriority) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr; // No priority extension
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_FALSE(queueProperties.priorityLevel.has_value());

    // This would be passed to getCsrForOrdinalAndIndex as std::nullopt
    // which means no priority logic would execute in the CSR creation
}

TEST(CommandQueuePriorityExtensionTest, givenPriorityExtensionWhenCreatingCommandQueueThenCsrGetsSpecificPriority) {
    ze_queue_priority_desc_t priorityDesc = {};
    priorityDesc.stype = ZE_STRUCTURE_TYPE_QUEUE_PRIORITY_DESC;
    priorityDesc.pNext = nullptr;
    priorityDesc.priority = -2; // Specific priority value

    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = &priorityDesc; // Priority extension provided
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_TRUE(queueProperties.priorityLevel.has_value());
    EXPECT_EQ(-2, queueProperties.priorityLevel.value());

    // This would be passed to getCsrForOrdinalAndIndex with the specific value
    // which means priority logic would execute with value -2
}

TEST(CommandQueuePriorityExtensionTest, givenLegacyQueueCreationWithoutAnyExtensionsWhenExtractingPropertiesThenBehaviorIsPreserved) {
    ze_command_queue_desc_t queueDesc = {};
    queueDesc.stype = ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC;
    queueDesc.pNext = nullptr; // No extensions - legacy behavior
    queueDesc.ordinal = 0;
    queueDesc.index = 0;
    queueDesc.flags = 0;
    queueDesc.mode = ZE_COMMAND_QUEUE_MODE_DEFAULT;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_PRIORITY_HIGH; // Only basic priority

    auto queueProperties = CommandQueue::extractQueueProperties(queueDesc);

    EXPECT_FALSE(queueProperties.priorityLevel.has_value());
    EXPECT_FALSE(queueProperties.copyOffloadHint);
    EXPECT_FALSE(queueProperties.interruptHint);

    // Only the basic ze_command_queue_priority_t would be used, not the optional int
}

} // namespace ult
} // namespace L0
