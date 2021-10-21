#pragma once
#include <deque>
#include "stdint.h"
#include "catapult/types.h"
#include <string>

#ifdef __APPLE__
#define NODESTROY [[clang::no_destroy]]
#else
#define NODESTROY  
#endif

namespace catapult {
	namespace plugins {

        // block height, low price and high price
		extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>> priceList;
        
        // block height, total supply and increase in total supply of the current block
		extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> totalSupply;

        // block height, collected fees this epoch and fees paid for a block (average block fee last epoch)
		extern std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>> epochFees;

        extern double currentMultiplier;
        extern uint64_t feeToPay; // fee to pay this epoch

        //region block_reward

        double approximate(double number);
        double getCoinGenerationMultiplier(uint64_t blockHeight, bool rollback = false);
        double getMultiplier(double increase30, double increase60, double increase90);
        uint64_t getFeeToPay(uint64_t blockHeight, bool rollback = false);
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
        void loadPricesFromFile();
        void processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback = false);

        //endregion price_helper

        //region total_supply_helper

        void removeOldTotalSupplyEntries(uint64_t blockHeight);
        bool addTotalSupplyEntry(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase, bool addToFile = true);
        void removeTotalSupplyEntry(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase);
        void addTotalSupplyEntryToFile(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase);
        void updateTotalSupplyFile();
        std::string totalSupplyToString();
        void loadTotalSupplyFromFile();
        
        //endregion total_supply_helper

        //region epoch_fees_helper

        void removeOldEpochFeeEntries(uint64_t blockHeight);
        bool addEpochFeeEntry(uint64_t blockHeight, uint64_t collectedFees, uint64_t currentFee, std:: string address, bool addToFile = true);
        void removeEpochFeeEntry(uint64_t blockHeight, uint64_t collectedFees, uint64_t blockFee, std::string address);
        void addEpochFeeEntryToFile(uint64_t blockHeight, uint64_t supplyAmount, uint64_t blockFee, std::string address);
        void updateEpochFeeFile();
        std::string epochFeeToString();
        void loadEpochFeeFromFile();
        
        //endregion epoch_fees_helper
	}
}
