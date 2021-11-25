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

#include "Observers.h"
#include "catapult/cache_core/AccountStateCache.h"
#include "catapult/cache_core/AccountStateCacheUtils.h"
#include "catapult/model/InflationCalculator.h"
#include "catapult/model/Mosaic.h"
#include "catapult/utils/Logging.h"
#include "catapult/model/Address.h"
#include "plugins/txes/price/src/observers/priceUtil.cpp"
// Not ideal but the implementation file can't be found otherwise before the header is included

namespace catapult { namespace observers {

	namespace {
		using Notification = model::BlockNotification;

		class FeeApplier {
		public:
			FeeApplier(MosaicId currencyMosaicId, ObserverContext& context)
					: m_currencyMosaicId(currencyMosaicId)
					, m_context(context)
			{}

		public:
			void apply(const Address& address, Amount amount) {
				auto& cache = m_context.Cache.sub<cache::AccountStateCache>();
				auto feeMosaic = model::Mosaic{ m_currencyMosaicId, amount };
				cache::ProcessForwardedAccountState(cache, address, [&feeMosaic, &context = m_context](auto& accountState) {
					ApplyFee(accountState, context.Mode, feeMosaic, context.StatementBuilder());
				});
			}

		private:
			static void ApplyFee(
					state::AccountState& accountState,
					NotifyMode notifyMode,
					const model::Mosaic& feeMosaic,
					ObserverStatementBuilder& statementBuilder) {
				if (NotifyMode::Rollback == notifyMode) {
					accountState.Balances.debit(feeMosaic.MosaicId, feeMosaic.Amount);
					return;
				}

				accountState.Balances.credit(feeMosaic.MosaicId, feeMosaic.Amount);

				// add fee receipt
				auto receiptType = model::Receipt_Type_Harvest_Fee;
				model::BalanceChangeReceipt receipt(receiptType, accountState.Address, feeMosaic.MosaicId, feeMosaic.Amount);
				statementBuilder.addReceipt(receipt);
			}

		private:
			MosaicId m_currencyMosaicId;
			ObserverContext& m_context;
		};

		bool ShouldShareFees(const Notification& notification, uint8_t harvestBeneficiaryPercentage) {
			return 0u < harvestBeneficiaryPercentage && notification.Harvester != notification.Beneficiary;
		}
	}

	DECLARE_OBSERVER(HarvestFee, Notification)(const HarvestFeeOptions& options, const model::InflationCalculator& calculator) {
		return MAKE_OBSERVER(HarvestFee, Notification, ([options, calculator](const Notification& notification, ObserverContext& context) {
			
			if (catapult::plugins::totalSupply.size() == 0) {
				// if there are no records, load them from the files
				catapult::plugins::loadEpochFeeFromFile();
				catapult::plugins::loadPricesFromFile();
				catapult::plugins::loadTotalSupplyFromFile();
				catapult::plugins::totalSupply.push_front({0, 10000000000, 10000000000}); // initial supply
			}

			Amount inflationAmount = Amount(0);
			Amount totalAmount = Amount(0);
			double multiplier = 1;
			uint64_t feeToPay = 0u;
			uint64_t totalSupply = 0u;
			uint64_t collectedEpochFees = 0u;
			uint64_t inflation = 0u;
			std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::reverse_iterator itFees;
			std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::reverse_iterator itTotal;

			if (NotifyMode::Commit == context.Mode) {
				multiplier = catapult::plugins::getCoinGenerationMultiplier(context.Height.unwrap());
				feeToPay = catapult::plugins::getFeeToPay(context.Height.unwrap());
				if (catapult::plugins::epochFees.size() > 0) {
					for (itFees = catapult::plugins::epochFees.rbegin(); itFees != catapult::plugins::epochFees.rend(); ++itFees) {
						if (context.Height.unwrap() > std::get<0>(*itFees)) {
							collectedEpochFees = std::get<1>(*itFees);
							break;
						}
					}
				} else {
					CATAPULT_LOG(warning) << "Warning: epoch fees list is empty\n";
				}
				collectedEpochFees += notification.TotalFee.unwrap();
				catapult::plugins::addEpochFeeEntry(context.Height.unwrap(), collectedEpochFees, feeToPay, model::AddressToString(notification.Harvester));
				if (catapult::plugins::totalSupply.size() > 0) {
					for (itTotal = catapult::plugins::totalSupply.rbegin(); itTotal != catapult::plugins::totalSupply.rend(); ++itTotal) {
						if (context.Height.unwrap() >= std::get<0>(*itTotal))
							totalSupply = std::get<1>(*itTotal);
							break;
					}
				} else {
					CATAPULT_LOG(warning) << "Warning: total supply list is empty\n";
				}
				inflation = static_cast<uint64_t>(static_cast<double>(totalSupply) * multiplier / 52560000 + 0.5);
				if (totalSupply + inflation > 100000000000) {
					inflation = 100000000000 - totalSupply;
				}
				totalSupply += inflation;
				if (context.Height.unwrap() > 1) {
					catapult::plugins::addTotalSupplyEntry(context.Height.unwrap(), totalSupply, inflation);
				}
				
				inflationAmount = Amount(inflation);
				totalAmount = Amount(inflation + feeToPay);

			} else if (NotifyMode::Rollback == context.Mode) {
				multiplier = catapult::plugins::getCoinGenerationMultiplier(context.Height.unwrap(), true);
				feeToPay = catapult::plugins::getFeeToPay(context.Height.unwrap(), true);
				if (catapult::plugins::epochFees.size() > 0) {
					for (itFees = catapult::plugins::epochFees.rbegin(); itFees != catapult::plugins::epochFees.rend(); ++itFees) {         
						if (std::get<0>(*itFees) == context.Height.unwrap() && std::get<2>(*itFees) == feeToPay
								&& std::get<3>(*itFees) == model::AddressToString(notification.Harvester)) {
							collectedEpochFees = std::get<1>(*itFees);
							break;
						}
						if (context.Height.unwrap() > std::get<0>(*itFees)) {
							CATAPULT_LOG(error) << "Error: epoch fee entry for block " << context.Height.unwrap() <<
								" can't be found\n";
							break;
						}
					}
				} else {
					CATAPULT_LOG(error) << "Error: epoch fees list is empty, rollback mode\n";
				}
				catapult::plugins::removeEpochFeeEntry(context.Height.unwrap(), collectedEpochFees, feeToPay, model::AddressToString(notification.Harvester));
				if (catapult::plugins::totalSupply.size() > 0) {
					for (itTotal = catapult::plugins::totalSupply.rbegin(); itTotal != catapult::plugins::totalSupply.rend(); ++itTotal) {         
						if (std::get<0>(*itTotal) == context.Height.unwrap()) {
							totalSupply = std::get<1>(*itTotal);
							break;
						}
						if (context.Height.unwrap() > std::get<0>(*itTotal)) {
							CATAPULT_LOG(error) << "Error: total supply entry for block " << context.Height.unwrap() <<
								" can't be found\n";
							CATAPULT_LOG(error) << catapult::plugins::totalSupplyToString();
							break;
						}
					}
				} else {
					CATAPULT_LOG(error) << "Error: total supply list is empty, rollback mode\n";
				}
				inflation = static_cast<uint64_t>(static_cast<double>(totalSupply) * multiplier / 52560000 + 0.5);
				if (totalSupply + inflation > 100000000000) {
					inflation = 100000000000 - totalSupply;
				}

				inflationAmount = Amount(inflation);
				totalAmount = Amount(inflation + feeToPay);
			}

			if (context.Height.unwrap() == 1) {
				totalAmount = Amount(0);
				inflationAmount = Amount(0);
			}
			auto networkAmount = Amount(totalAmount.unwrap() * options.HarvestNetworkPercentage / 100);
			auto beneficiaryAmount = ShouldShareFees(notification, options.HarvestBeneficiaryPercentage)
					? Amount(totalAmount.unwrap() * options.HarvestBeneficiaryPercentage / 100)
					: Amount();
			auto harvesterAmount = totalAmount - networkAmount - beneficiaryAmount;

			// always create receipt for harvester
			FeeApplier applier(options.CurrencyMosaicId, context);
			applier.apply(notification.Harvester, harvesterAmount);

			// only if amount is non-zero create receipt for network sink account
			if (Amount() != networkAmount)
				applier.apply(options.HarvestNetworkFeeSinkAddress.get(context.Height), networkAmount);

			// only if amount is non-zero create receipt for beneficiary account
			if (Amount() != beneficiaryAmount)
				applier.apply(notification.Beneficiary, beneficiaryAmount);

			// add inflation receipt
			if (Amount() != inflationAmount && NotifyMode::Commit == context.Mode) {
				model::InflationReceipt receipt(model::Receipt_Type_Inflation, options.CurrencyMosaicId, inflationAmount);
				context.StatementBuilder().addReceipt(receipt);
			}
		}));
	}
}}
