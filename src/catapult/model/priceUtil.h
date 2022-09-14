#pragma once
#include <deque>
#include "stdint.h"
#include "catapult/types.h"
#include <string>
#include "catapult/cache_db/RocksDatabase.h"
#include "catapult/cache_db/RocksInclude.h"
#include <mutex>

#ifdef __APPLE__
#define NODESTROY [[clang::no_destroy]]
#else
#define NODESTROY  
#endif

namespace catapult {
	namespace plugins {

        extern uint64_t feeToPay; // fee to pay this epoch

        // initial supply of the network
        extern uint64_t initialSupply;

        // price publisher address string
        extern std::string pricePublisherPublicKey;

        // fee recalculation frequency
        extern uint64_t feeRecalculationFrequency;

        // multiplier recalculation frequency
        extern uint64_t multiplierRecalculationFrequency;

        // number of blocks to be included in calculating price averages (originally 30 days)
        extern uint64_t pricePeriodBlocks;

        // total supply and epoch fee entry life timein terms of blocks
        extern uint64_t entryLifetime;

        extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> priceList;

        // max number of coins
        extern uint64_t generationCeiling;
        extern const std::string priceDirectory;
        extern std::vector<std::string> priceFields;
        extern cache::RocksDatabaseSettings priceSettings;

        //region block_reward
        bool areSame(double a, double b);
        void configToFile();
        void readConfig();
        double approximate(double number);
        double getCoinGenerationMultiplier(uint64_t blockHeight);
        double getMultiplier(double increase30, double increase60, double increase90);
        void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
            double &average120);
        double getMin(double num1, double num2, double num3 = -1);

        //endregion block_reward

        //region price_helper

        void removeOldPrices(uint64_t blockHeight);
        bool addPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool addToFile = true);
        void removePrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);
        void addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice);        
        void updatePricesFile();
        void loadPricesFromFile(uint64_t blockHeight);
        void processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback = false);

        //endregion price_helper
	}
}
