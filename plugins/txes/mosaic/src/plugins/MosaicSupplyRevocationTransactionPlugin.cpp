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

#include "MosaicSupplyRevocationTransactionPlugin.h"
#include "src/model/MosaicFlags.h"
#include "src/model/MosaicSupplyRevocationTransaction.h"
#include "catapult/model/NotificationSubscriber.h"
#include "catapult/model/TransactionPluginFactory.h"

using namespace catapult::model;

namespace catapult { namespace plugins {

	namespace {
		template<typename TTransaction>
		void Publish(const TTransaction& transaction, const PublishContext& context, NotificationSubscriber& sub) {
			constexpr auto Mosaic_Flags_Revokable = utils::to_underlying_type(MosaicFlags::Revokable);
			// MosaicFlagsValidator prevents any mosaics from being created with Revokable flag prior to fork block
			// consequently, MosaicSupplyRevocation transactions will be rejected until then because of Revokable flag requirement
			sub.notify(MosaicRequiredNotification(context.SignerAddress, transaction.Mosaic.MosaicId, Mosaic_Flags_Revokable));
			sub.notify(BalanceTransferNotification(
					transaction.SourceAddress,
					context.SignerAddress,
					transaction.Mosaic.MosaicId,
					transaction.Mosaic.Amount));
		}
	}

	DEFINE_TRANSACTION_PLUGIN_FACTORY(MosaicSupplyRevocation, Default, Publish)
}}
