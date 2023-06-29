/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

namespace NEO {

class MockKmdNotifyHelper : public KmdNotifyHelper {
  public:
    using KmdNotifyHelper::getBaseTimeout;

    MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};
};

TEST(KmdNotifyLinuxTests, givenTaskCountDiffGreaterThanOneWhenBaseTimeoutRequestedThenDoNotMultiply) {
    auto localProperties = (defaultHwInfo->capabilityTable.kmdNotifyProperties);
    localProperties.delayKmdNotifyMicroseconds = 10;

    MockKmdNotifyHelper helper(&localProperties);
    EXPECT_EQ(localProperties.delayKmdNotifyMicroseconds, helper.getBaseTimeout());
}
} // namespace NEO
