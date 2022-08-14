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

        extern std::mutex priceMutex;
        extern double currentMultiplier;
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

        // max number of coins
        extern uint64_t generationCeiling;
        extern const std::string priceDirectory;
        extern std::vector<std::string> priceFields;
        extern cache::RocksDatabaseSettings priceSettings;
        extern std::unique_ptr<cache::RocksDatabase> priceDB;

        //region block_reward
        void configToFile();
        void readConfig();
        double approximate(double number);
        double getCoinGenerationMultiplier(uint64_t blockHeight, bool update = false, bool rollback = false);
        double getMultiplier(double increase30, double increase60, double increase90);
        uint64_t getFeeToPay(uint64_t blockHeight, uint64_t *collectedFees, bool rollback = false, std::string beneficiary = "");
        void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
            double &average120);
        double getMin(double num1, double num2, double num3 = -1);

        //endregion block_reward

        //region price_helper

        void removeOldPrices(uint64_t blockHeight);
        bool addPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier, bool addToFile = true);
        void removePrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier);
        void addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier);        
        void updatePricesFile();
        std::string pricesToString();
        void loadPricesFromFile(uint64_t blockHeight);
        void processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback = false);

        //endregion price_helper

        //region total_supply_helper

        void removeOldTotalSupplyEntries(uint64_t blockHeight);
        bool addTotalSupplyEntry(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase, bool addToFile = true);
        void removeTotalSupplyEntry(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase);
        void addTotalSupplyEntryToFile(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase);
        void updateTotalSupplyFile();
        std::string totalSupplyToString();
        void loadTotalSupplyFromFile(uint64_t blockHeight);
        
        //endregion total_supply_helper

        //region epoch_fees_helper

        void removeOldEpochFeeEntries(uint64_t blockHeight);
        bool addEpochFeeEntry(uint64_t blockHeight, uint64_t collectedFees, uint64_t currentFee, std:: string address, bool addToFile = true);
        void removeEpochFeeEntry(uint64_t blockHeight, uint64_t blockFee, uint64_t collectedFees, std::string address);
        void addEpochFeeEntryToFile(uint64_t blockHeight, uint64_t supplyAmount, uint64_t blockFee, std::string address);
        void updateEpochFeeFile();
        std::string epochFeeToString();
        void loadEpochFeeFromFile(uint64_t blockHeight);
        
        //endregion epoch_fees_helper
	}
}
