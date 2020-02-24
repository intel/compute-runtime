/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/kmd_notify_properties.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "test.h"

namespace NEO {

class MockKmdNotifyHelper : public KmdNotifyHelper {
  public:
    using KmdNotifyHelper::getBaseTimeout;

    MockKmdNotifyHelper(const KmdNotifyProperties *newProperties) : KmdNotifyHelper(newProperties){};
};

TEST(KmdNotifyLinuxTests, givenTaskCountDiffGreaterThanOneWhenBaseTimeoutRequestedThenMultiply) {
    auto localProperties = (platformDevices[0]->capabilityTable.kmdNotifyProperties);
    localProperties.delayKmdNotifyMicroseconds = 10;
    const int64_t multiplier = 10;

    MockKmdNotifyHelper helper(&localProperties);
    EXPECT_EQ(localProperties.delayKmdNotifyMicroseconds * multiplier, helper.getBaseTimeout(multiplier));
}
} // namespace NEO
