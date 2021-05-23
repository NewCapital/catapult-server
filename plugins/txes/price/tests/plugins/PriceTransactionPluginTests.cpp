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

#include "src/plugins/PriceTransactionPlugin.h"
#include "sdk/src/extensions/ConversionExtensions.h"
#include "src/model/PriceNotifications.h"
#include "src/model/PriceTransaction.h"
#include "catapult/utils/MemoryUtils.h"
#include "tests/test/core/mocks/MockNotificationSubscriber.h"
#include "tests/test/plugins/TransactionPluginTestUtils.h"
#include "tests/TestHarness.h"

using namespace catapult::model;

namespace catapult { namespace plugins {

#define TEST_CLASS PriceTransactionPluginTests

	// region test utils

	namespace {
		DEFINE_TRANSACTION_PLUGIN_TEST_TRAITS(Price, 1, 1,)

		template<typename TTraits>
		auto CreatePriceTransaction(uint64_t lowPrice, uint64_t highPrice) {
			using TransactionType = typename TTraits::TransactionType;
			uint32_t entitySize = SizeOf32<TransactionType>();
			auto pTransaction = utils::MakeUniqueWithSize<TransactionType>(entitySize);
			test::FillWithRandomData({ reinterpret_cast<uint8_t*>(pTransaction.get()), entitySize });

			pTransaction->Size = entitySize;
			pTransaction->lowPrice = lowPrice;
			pTransaction->highPrice = highPrice;
			return pTransaction;
		}
	}

	DEFINE_BASIC_EMBEDDABLE_TRANSACTION_PLUGIN_TESTS(TEST_CLASS, , , Entity_Type_Price)

	// endregion

	// region publish - neither message nor mosaics

	namespace {
		template<typename TTraits>
		void AddCommonExpectations(
				typename test::TransactionPluginTestUtils<TTraits>::PublishTestBuilder& builder,
				const typename TTraits::TransactionType& transaction) {
			builder.template addExpectation<PriceMessageNotification>([&transaction](const auto& notification) {
				EXPECT_EQ(transaction.SignerPublicKey, notification.SenderPublicKey);
			});
		}
	}

	PLUGIN_TEST(CanPublishAllNotificationsInCorrectOrderWhenNeitherMessageNorMosaicsArePresent) {
		// Arrange:
		typename TTraits::TransactionType transaction;
		test::FillWithRandomData(transaction);
		transaction.lowPrice = 1;
		transaction.highPrice = 2;

		// Act + Assert:
		test::TransactionPluginTestUtils<TTraits>::AssertNotificationTypes(transaction, {
			PriceMessageNotification::Notification_Type
		});
	}

	PLUGIN_TEST(CanPublishAllNotificationsWhenNeitherMessageNorMosaicsArePresent) {
		// Arrange:
		typename TTraits::TransactionType transaction;
		test::FillWithRandomData(transaction);
		transaction.lowPrice = 1;
		transaction.highPrice = 2;

		typename test::TransactionPluginTestUtils<TTraits>::PublishTestBuilder builder;
		AddCommonExpectations<TTraits>(builder, transaction);

		// Act + Assert:
		builder.runTest(transaction);
	}

	// endregion

	// region publish - message only

	namespace {
		template<typename TTransaction>
		void PrepareMessageOnlyTransaction(TTransaction& transaction) {
			test::FillWithRandomData(transaction);
			transaction.lowPrice = 1;
			transaction.highPrice = 2;
		}
	}

	PLUGIN_TEST(CanPublishAllNotificationsInCorrectOrderWhenMessageOnlyIsPresent) {
		// Arrange:
		auto pTransaction = CreatePriceTransaction<TTraits>(1, 2);
		PrepareMessageOnlyTransaction(*pTransaction);

		// Act + Assert:
		test::TransactionPluginTestUtils<TTraits>::AssertNotificationTypes(*pTransaction, {
			PriceMessageNotification::Notification_Type
		});
	}

	PLUGIN_TEST(CanPublishAllNotificationsWhenMessageOnlyIsPresent) {
		// Arrange:
		auto pTransaction = CreatePriceTransaction<TTraits>(1, 2);
		PrepareMessageOnlyTransaction(*pTransaction);

		const auto& transaction = *pTransaction;
		typename test::TransactionPluginTestUtils<TTraits>::PublishTestBuilder builder;
		AddCommonExpectations<TTraits>(builder, transaction);
		builder.template addExpectation<PriceMessageNotification>([&transaction](const auto& notification) {
			EXPECT_EQ(transaction.SignerPublicKey, notification.SenderPublicKey);
			EXPECT_EQ(1u, notification.lowPrice);
			EXPECT_EQ(2u, notification.highPrice);
			EXPECT_EQ(transaction.lowPrice, 1u);
			EXPECT_EQ(transaction.highPrice, 2u);
		});

		// Act + Assert:
		builder.runTest(transaction);
	}

	/*PLUGIN_TEST(CanPublishAllNotificationsWhenMosaicsOnlyArePresent) {
		// Arrange:
		auto pTransaction = CreatePriceTransaction<TTraits>(2, 0);
		PrepareMosaicsOnlyTransaction(*pTransaction);

		const auto& transaction = *pTransaction;
		typename test::TransactionPluginTestUtils<TTraits>::PublishTestBuilder builder;
		AddCommonExpectations<TTraits>(builder, transaction);
		for (auto i = 0u; i < 2u; ++i) {
			builder.template addExpectation<BalancePriceNotification>(i, [&transaction, i](const auto& notification) {
				EXPECT_EQ(GetSignerAddress(transaction), notification.Sender);
				EXPECT_EQ(transaction.MosaicsPtr()[i].MosaicId, notification.MosaicId);
				EXPECT_EQ(transaction.MosaicsPtr()[i].Amount, notification.Amount);
				EXPECT_EQ(transaction.RecipientAddress, notification.Recipient);
				EXPECT_EQ(BalancePriceNotification::AmountType::Static, notification.PriceAmountType);
			});
		}

		builder.template addExpectation<PriceMosaicsNotification>([&transaction](const auto& notification) {
			EXPECT_EQ(transaction.MosaicsCount, notification.MosaicsCount);
			EXPECT_EQ(transaction.MosaicsPtr(), notification.MosaicsPtr);
		});

		// Act + Assert:
		builder.runTest(transaction);
	}*/

	// endregion
}}
