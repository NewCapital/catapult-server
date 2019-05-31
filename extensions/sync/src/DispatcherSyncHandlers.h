/**
*** Copyright (c) 2016-present,
*** Jaguar0625, gimre, BloodyRookie, Tech Bureau, Corp. All rights reserved.
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

#pragma once
#include "catapult/consumers/BlockChainSyncHandlers.h"

namespace catapult {
	namespace config { class CatapultDataDirectory; }
	namespace extensions { class LocalNodeChainScore; }
}

namespace catapult { namespace sync {

	/// Creates a commit step handler around \a dataDirectory.
	consumers::BlockChainSyncHandlers::CommitStepFunc CreateCommitStepHandler(const config::CatapultDataDirectory& dataDirectory);

	/// Updates \a syncHandlers to support supplemental data resiliency given \a dataDirectory, \a cache and \a score.
	void AddSupplementalDataResiliency(
			consumers::BlockChainSyncHandlers& syncHandlers,
			const config::CatapultDataDirectory& dataDirectory,
			const cache::CatapultCache& cache,
			const extensions::LocalNodeChainScore& score);
}}
