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

#include "PricePlugin.h"
#include "PriceTransactionPlugin.h"
#include "src/observers/Observers.h"
#include "src/validators/Validators.h"
#include "catapult/config/CatapultDataDirectory.h"
#include "catapult/config/CatapultKeys.h"
#include "catapult/crypto/OpensslKeyUtils.h"
#include "catapult/model/Address.h"
#include "catapult/plugins/PluginManager.h"
#include "catapult/model/priceUtil.h"
#include "src/config/PriceConfiguration.h"
#include <string>

namespace catapult { namespace plugins {

	void RegisterPriceSubsystem(PluginManager& manager) {
		auto config = catapult::model::LoadPluginConfiguration<config::PriceConfiguration>(manager.config(), "catapult.plugins.price");
		catapult::plugins::initialSupply = config.initialSupply;
		catapult::plugins::pricePublisherPublicKey = config.pricePublisherPublicKey;
		catapult::plugins::feeRecalculationFrequency = config.feeRecalculationFrequency;
		catapult::plugins::multiplierRecalculationFrequency = config.multiplierRecalculationFrequency;
		catapult::plugins::pricePeriodBlocks = config.pricePeriodBlocks;
		catapult::plugins::entryLifetime = config.entryLifetime;
		catapult::plugins::generationCeiling = config.generationCeiling;
		configToFile();

		manager.addTransactionSupport(CreatePriceTransactionPlugin());
    
		manager.addStatelessValidatorHook([](auto& builder) {
			builder.add(validators::CreatePriceMessageValidator());
		});

		manager.addObserverHook([](auto& builder) {
			builder.add(observers::CreatePriceMessageObserver());
		});
	}
}}

extern "C" PLUGIN_API
void RegisterSubsystem(catapult::plugins::PluginManager& manager) {
	catapult::plugins::RegisterPriceSubsystem(manager);
}
