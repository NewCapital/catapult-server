/**
*** Copyright (c) 2016-2019, Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp.
*** Copyright (c) 2020-present, Jaguar0625, gimre, BloodyRookie.
*** All rights reserved.
***
*** This file is part of Catapult.
***
*** Catapult is free software: you can redistribute it and/or modify
*** it under the terms of the GNU Lesser General Public License as published by
*** the Free Software Foundation, either version 3 of the License, or
*** (at your option) any later version.
***
*** Catapult is distributed in the hope that it will be useful,
*** but WITHOUT ANY WARRANTY; without even the implied warranty of
*** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
*** GNU Lesser General Public License for more details.
***
*** You should have received a copy of the GNU Lesser General Public License
*** along with Catapult. If not, see <http://www.gnu.org/licenses/>.
**/

#include "src/observers/Observers.h"
#include "catapult/config/CatapultDataDirectory.h"
#include "catapult/io/IndexFile.h"
#include "tests/test/nodeps/Filesystem.h"
#include "tests/test/plugins/ObserverTestUtils.h"
#include "tests/TestHarness.h"
#include "src/observers/priceUtil.h"
#include <queue>
#include "stdint.h"

namespace catapult { namespace observers {

#define TEST_CLASS PriceMessageObserverTests

	/**
	 *  wowazzz: from Clang compiler:
	 *  error: must specify at least one argument for '...' parameter of variadic macro
	 */
	DEFINE_COMMON_OBSERVER_TESTS(PriceMessage,)

	// region traits

	namespace {
		using namespace catapult::plugins;

		struct CommitTraits {
			//static constexpr uint8_t Message_First_Byte = 0;
			static constexpr auto Notify_Mode = NotifyMode::Commit;
		};

		struct RollbackTraits {
			//static constexpr uint8_t Message_First_Byte = 1;
			static constexpr auto Notify_Mode = NotifyMode::Rollback;
		};
	}

#define MESSAGE_OBSERVER_TRAITS_BASED_TEST(TEST_NAME) \
	template<typename TTraits> void TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)(); \
	TEST(TEST_CLASS, TEST_NAME##_Commit) { TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)<CommitTraits>(); } \
	TEST(TEST_CLASS, TEST_NAME##_Rollback) { TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)<RollbackTraits>(); } \
	template<typename TTraits> void TRAITS_TEST_NAME(TEST_CLASS, TEST_NAME)()

	// endregion

	// region test context

	namespace {
		class PriceMessageObserverTestContext {
		public:
			explicit PriceMessageObserverTestContext(NotifyMode notifyMode) : m_notifyMode(notifyMode)
			{}

		public:
			void observe(const model::PriceMessageNotification& notification) {
				auto pObserver = CreatePriceMessageObserver();
				test::ObserverTestContext context(m_notifyMode);

				priceList.clear();
				if (m_notifyMode == NotifyMode::Rollback)
					priceList.push_front({3u, 1u, 1u, 1}); // add a price to remove

				test::ObserveNotification(*pObserver, notification, context);

				if (m_notifyMode == NotifyMode::Rollback)
					EXPECT_EQ(priceList.size(), 0u); // removes the price

				if (m_notifyMode == NotifyMode::Commit)
					EXPECT_EQ(priceList.size(), 1u); // adds a price
			}

		private:
			NotifyMode m_notifyMode;
			test::TempDirectoryGuard m_tempDir;
		};
	}

	// endregion

	// region not filtered - files created

	MESSAGE_OBSERVER_TRAITS_BASED_TEST(MessageIsWrittenWhenMarkerAndRecipientBothMatch) {
		// Arrange:
		PriceMessageObserverTestContext context(TTraits::Notify_Mode);

		auto sender = test::GenerateRandomByteArray<Key>();
		auto notification = model::PriceMessageNotification(sender, 3u, 1u, 1u);

		// Act:
		context.observe(notification);
	}

	// endregion
}}
