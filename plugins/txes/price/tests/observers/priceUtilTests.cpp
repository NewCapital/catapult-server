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

#include "plugins/txes/price/src/observers/priceUtil.h"
#include "tests/TestHarness.h"
#include "stdint.h"
#include <deque>
#include <tuple>
#include <cmath>
#include <fstream>

#define BLOCKS_PER_30_DAYS 86400u // number of blocks per 30 days
// epoch = 6 hours -> 4 epochs per day; number of epochs in a year: 365 * 4 = 1460
#define EPOCHS_PER_YEAR 1460
#define INCREASE_TESTS_COUNT 41
#define MOCK_PRICES_COUNT 14u
#define MOCK_TOTAL_SUPPLY_ENTRIES 4u
#define MOCK_EPOCH_FEE_ENTRIES 4u
#define TEST_CLASS SupplyDemandModel

namespace catapult { namespace plugins {
	namespace {

        double increaseTests[INCREASE_TESTS_COUNT][4] = {
            // Test 3 averages with the same growth factors

            // TEST WITH ALL INCREASES ABOVE 25%:

            /////////////////////////////////////////////////////////////////////////////
            // All increases are the same
            {1.56, 1.56, 1.56, approximate(1 + 0.735 / EPOCHS_PER_YEAR)}, //doesn't grow more than the maximum
            {1.55, 1.55, 1.55, approximate(1 + 0.735 / EPOCHS_PER_YEAR)},
            {1.50, 1.50, 1.50, approximate(1 + 0.7025 / EPOCHS_PER_YEAR)},
            {1.45, 1.45, 1.45, approximate(1 + 0.67 / EPOCHS_PER_YEAR)},
            {1.40, 1.40, 1.40, approximate(1 + 0.64 / EPOCHS_PER_YEAR)},
            {1.35, 1.35, 1.35, approximate(1 + 0.61 / EPOCHS_PER_YEAR)},
            {1.30, 1.30, 1.30, approximate(1 + 0.58 / EPOCHS_PER_YEAR)},
            {1.25, 1.25, 1.25, approximate(1 + 0.55 / EPOCHS_PER_YEAR)},

            // Test 3 averages with the 90 day increase being the smallest
            {1.55, 1.55, 1.50, approximate(1 + 0.7025 / EPOCHS_PER_YEAR)},
            {1.55, 1.45, 1.40, approximate(1 + 0.64 / EPOCHS_PER_YEAR)},
            {1.55, 1.35, 1.30, approximate(1 + 0.58 / EPOCHS_PER_YEAR)},

            // Test 3 averages with the 60 day increase being the smallest
            {1.55, 1.50, 1.55, approximate(1 + 0.7025 / EPOCHS_PER_YEAR)},
            {1.55, 1.40, 1.45, approximate(1 + 0.64 / EPOCHS_PER_YEAR)},
            {1.55, 1.30, 1.35, approximate(1 + 0.58 / EPOCHS_PER_YEAR)},

            // Test 3 averages with the 30 day increase being the smallest
            {1.5, 1.55, 1.55, approximate(1 + 0.7025 / EPOCHS_PER_YEAR)},
            {1.4, 1.45, 1.45, approximate(1 + 0.64 / EPOCHS_PER_YEAR)},
            {1.3, 1.35, 1.35, approximate(1 + 0.58 / EPOCHS_PER_YEAR)},

            /////////////////////////////////////////////////////////////////////////////

            // TEST WITH 30 AND 60 DAY INCREASES ABOVE 25%

            /////////////////////////////////////////////////////////////////////////////
            // Both incrases are the same
            {1.56, 1.56, 1, approximate(1 + 0.49 / EPOCHS_PER_YEAR)}, //doesn't grow more than the maximum
            {1.55, 1.55, 1, approximate(1 + 0.49 / EPOCHS_PER_YEAR)},
            {1.50, 1.50, 1, approximate(1 + 0.46 / EPOCHS_PER_YEAR)},
            {1.45, 1.45, 1, approximate(1 + 0.43 / EPOCHS_PER_YEAR)},
            {1.40, 1.40, 1, approximate(1 + 0.40 / EPOCHS_PER_YEAR)},
            {1.35, 1.35, 1, approximate(1 + 0.37 / EPOCHS_PER_YEAR)},
            {1.30, 1.30, 1, approximate(1 + 0.34 / EPOCHS_PER_YEAR)},
            {1.25, 1.25, 1, approximate(1 + 0.31 / EPOCHS_PER_YEAR)},

            // Test 2 averages with the 60 day increase being the smallest
            {1.55, 1.50, 1, approximate(1 + 0.46 / EPOCHS_PER_YEAR)},
            {1.55, 1.40, 1, approximate(1 + 0.40 / EPOCHS_PER_YEAR)},
            {1.55, 1.30, 1, approximate(1 + 0.34 / EPOCHS_PER_YEAR)},
            
            // Test 2 averages with the 30 day increase being the smallest
            {1.5, 1.55, 1, approximate(1 + 0.46 / EPOCHS_PER_YEAR)},
            {1.4, 1.45, 1, approximate(1 + 0.40 / EPOCHS_PER_YEAR)},
            {1.3, 1.35, 1, approximate(1 + 0.34 / EPOCHS_PER_YEAR)},
            
            /////////////////////////////////////////////////////////////////////////////

            // TEST WITH ONLY 30 DAY INCREASE
            
            /////////////////////////////////////////////////////////////////////////////

            {1.56, 1, 1, approximate(1 + 0.25 / EPOCHS_PER_YEAR)}, // Maximum reached
            {1.55, 1, 1, approximate(1 + 0.25 / EPOCHS_PER_YEAR)},
            {1.45, 1, 1, approximate(1 + 0.19 / EPOCHS_PER_YEAR)},
            {1.35, 1, 1, approximate(1 + 0.13 / EPOCHS_PER_YEAR)},
            {1.25, 1, 1, approximate(1 + 0.095 / EPOCHS_PER_YEAR)},
            {1.15, 1, 1, approximate(1 + 0.06 / EPOCHS_PER_YEAR)},
            {1.05, 1, 1, approximate(1 + 0.025 / EPOCHS_PER_YEAR)},
            {1.04, 1, 1, 1}, // Too small growth

            /////////////////////////////////////////////////////////////////////////////

            // OTHER TESTS
            
            /////////////////////////////////////////////////////////////////////////////
            // if too small, return a factor for only 30 day average
            {1.24, 1.24, 1.24, approximate(1 + 0.0915 / EPOCHS_PER_YEAR)},
            {1.55, 1.24, 1.55, approximate(1 + 0.25 / EPOCHS_PER_YEAR)},
        };

        std::tuple<uint64_t, uint64_t, uint64_t, double> mockPrices[MOCK_PRICES_COUNT] = {
            // Should be sorted by the blockHeight from the lowest (top) to the highest (bottom)
            // <blockHeight, lowPrice, highPrice, multiplier>
            {0u, 1, 2, 1},
            {1u, 1, 1, 1},
            {2u, 1, 3, 1},
            {86399u, 2, 3, 1},
            {86400u, 3, 4, 1},
            {86401u, 2, 4, 1}, // too early, we need at least 60 days of data for a possible change in multiplier value
            {172799u, 4, 6, 1.00017},
            {172800u, 4, 6, 1.00017},
            {172801u, 2, 4, 1.00017},
            {259199u, 5, 7, 1.00006}, // 60 days, but the last 30 day increase is 1.21740 (doesn't reach 1.25 boundary),
                                      // so the first 30 days are ignored 
            {259200u, 6, 6, 1.00004}, // 60 days, but the last 30 day increase is 1.15385 (doesn't reach 1.25 boundary),
                                      // so the first 30 days are ignored 
            {259201u, 5, 6, 1.00025}, // 60 days and the last 30 day increase is 1.34616, so multiplier is according to
                                      // all 60 days instead of the last 30
            {345599u, 4, 7, 1.00006}, // 90 days, but the last 30 day increase - 1.21429
            {345600u, 4, 7, 1.00003}  // 90 days, but the last 30 day increase - 1.10000
        };

        std::tuple<uint64_t, uint64_t, uint64_t> mockTotalSupply[MOCK_TOTAL_SUPPLY_ENTRIES] = {
            // Should be sorted by the blockHeight from the lowest (top) to the highest (bottom)
            // <blockHeight, total supply amount, increase in coins>
            {0u, 5, 5},
            {100u, 10, 5},
            {200u, 15, 5},
            {300u, 20, 5}
        };

        NODESTROY std::tuple<uint64_t, uint64_t, uint64_t, std::string> mockEpochFees[MOCK_EPOCH_FEE_ENTRIES] = {
            // Should be sorted by the blockHeight from the lowest (top) to the highest (bottom)
            // <blockHeight, fees collected this epoch, fee paid for a block>
            {0u, 5, 5, "address"},
            {100u, 10, 5, "address"},
            {200u, 15, 5, "address"},
            {300u, 20, 5, "address"}
        };

        void generateEpochFees() {
            for (long unsigned int i = 0; i < MOCK_EPOCH_FEE_ENTRIES; ++i) {
                epochFees.push_back(mockEpochFees[i]);
            }
        }

        void generateTotalSupply() {
            for (long unsigned int i = 0; i < MOCK_TOTAL_SUPPLY_ENTRIES; ++i) {
                totalSupply.push_back(mockTotalSupply[i]);
            }
        }

        void generatePriceList() {
            for (long unsigned int i = 0; i < MOCK_PRICES_COUNT; ++i) {
                priceList.push_back(mockPrices[i]);
            }
        }

        void resetTests() {
            priceList.clear();
            totalSupply.clear();
            epochFees.clear();
            currentMultiplier = 0;
            feeToPay = 0;
        }

        void comparePrice(std::tuple<uint64_t, uint64_t, uint64_t, double> price,
            std::tuple<uint64_t, uint64_t, uint64_t, double> price2) {
            EXPECT_EQ(std::get<0>(price), std::get<0>(price2));
            EXPECT_EQ(std::get<1>(price), std::get<1>(price2));
            EXPECT_EQ(std::get<2>(price), std::get<2>(price2));
            EXPECT_EQ(std::get<3>(price), std::get<3>(price2));
        }

        void compareTotalSupply(std::tuple<uint64_t, uint64_t, uint64_t> supply,
            std::tuple<uint64_t, uint64_t, uint64_t> supply2) {
            EXPECT_EQ(std::get<0>(supply), std::get<0>(supply2));
            EXPECT_EQ(std::get<1>(supply), std::get<1>(supply2));
            EXPECT_EQ(std::get<2>(supply), std::get<2>(supply2));
        }

        void compareTotalEpochFees(std::tuple<uint64_t, uint64_t, uint64_t, std::string> entry,
            std::tuple<uint64_t, uint64_t, uint64_t, std::string> entry2) {
            EXPECT_EQ(std::get<0>(entry), std::get<0>(entry2));
            EXPECT_EQ(std::get<1>(entry), std::get<1>(entry2));
            EXPECT_EQ(std::get<2>(entry), std::get<2>(entry2));
            EXPECT_EQ(std::get<3>(entry), std::get<3>(entry2));
        }

        double getMockPriceAverage(uint64_t end, uint64_t start = 0) {
            double average = 0;
            int count = 0;
            uint64_t blockHeight;
            for (long unsigned int i = 0; i < MOCK_PRICES_COUNT; ++i) {
                blockHeight = std::get<0>(mockPrices[i]);
                if (blockHeight > end || blockHeight < start)
                    continue;
                
                average += static_cast<double>(std::get<1>(mockPrices[i]) + std::get<2>(mockPrices[i])) / 2;
                ++count;
            }
            return approximate(average / count);
        }

        void assertAverages(double average30, double average60, double average90, 
            double average120, uint64_t highestBlock) {
            if (highestBlock < BLOCKS_PER_30_DAYS - 1)
                return;
            EXPECT_EQ(average30, getMockPriceAverage(highestBlock,
                highestBlock - BLOCKS_PER_30_DAYS + 1));

            if (highestBlock < BLOCKS_PER_30_DAYS * 2 - 1)
                return;
            EXPECT_EQ(average60, getMockPriceAverage(highestBlock - BLOCKS_PER_30_DAYS, 
                highestBlock - BLOCKS_PER_30_DAYS * 2 + 1));
            
            if (highestBlock < BLOCKS_PER_30_DAYS * 3 - 1)
                return;
            EXPECT_EQ(average90, getMockPriceAverage(highestBlock - BLOCKS_PER_30_DAYS * 2, 
                highestBlock - BLOCKS_PER_30_DAYS * 3 + 1));

            if (highestBlock < BLOCKS_PER_30_DAYS * 4 - 1)
                return;
            EXPECT_EQ(average120, getMockPriceAverage(highestBlock - BLOCKS_PER_30_DAYS * 3, 
                highestBlock - BLOCKS_PER_30_DAYS * 4 + 1));
        }

        /*TEST(TEST_CLASS, CanRemoveOldPrices) {
            resetTests();
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it = priceList.end();
            int remainingPricesExpected = MOCK_PRICES_COUNT - 2;
            generatePriceList();
            removeOldPrices(4 * BLOCKS_PER_30_DAYS + 101); // blocks: 2 - 345601
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            for (int i = 0; i < remainingPricesExpected; ++i) {
                if (it == priceList.begin())
                    break;
                comparePrice(*--it, mockPrices[i]);
            }
	    }

        TEST(TEST_CLASS, CanGetCorrectAverages) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT; // no prices removed
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4; // blocks: 1 - 345600
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, IgnoresFuturePricesForAverages) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4 - 1u; // blocks: 0 - 345599
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan120MoreThan90Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 3; // blocks: 0 - 259200
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan90MoreThan60Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 2; // blocks: 0 - 172800
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan60MoreThan30Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS; // blocks: 0 - 86400
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan30Days) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = 1; // blocks: 0 - 1
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanAddPriceToPriceList) {
            resetTests();
            int remainingPricesExpected = 1;
            EXPECT_EQ(priceList.size(), 0);
            addPrice(1u, 2u, 2u);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, CantAddInvalidPriceToPriceList) {
            resetTests();
            addPrice(1u, 2u, 1u); // lowPrice can't be higher than highPrice
            addPrice(2u, 0u, 2u); // neither lowPrice nor highPrice can be 0
            addPrice(3u, 2u, 0u);
            addPrice(4u, 0u, 0u);
            EXPECT_EQ(priceList.size(), 0);
            generatePriceList();
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
            addPrice(std::get<0>(mockPrices[MOCK_PRICES_COUNT - 1]) - 1, 3u, 4u);
                // block lower than the one of an already existing price, therefore invalid
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
	    }

        TEST(TEST_CLASS, CanRemovePrice) {
            resetTests();
            int remainingPricesExpected = MOCK_PRICES_COUNT - 1;
            // Remove the third price from the end
            uint64_t blockHeight = std::get<0>(mockPrices[MOCK_PRICES_COUNT - 3]);
            uint64_t lowPrice = (uint64_t) std::get<1>(mockPrices[MOCK_PRICES_COUNT - 3]);
            uint64_t highPrice = (uint64_t) std::get<2>(mockPrices[MOCK_PRICES_COUNT - 3]);
            generatePriceList();
            removePrice(blockHeight, lowPrice, highPrice);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, DoesNotRemoveAnythingIfPriceNotFound) {
            resetTests();
            generatePriceList();
            int remainingPricesExpected = MOCK_PRICES_COUNT;
            // Make sure such a price doesn't exist
            uint64_t blockHeight = 751u;
            uint64_t lowPrice = 696u;
            uint64_t highPrice = 697u;
            removePrice(blockHeight, lowPrice, highPrice);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, getMinChecks) {
            resetTests();
            EXPECT_EQ(getMin(1, 2), 1);
            EXPECT_EQ(getMin(2, 2), 2);
            EXPECT_EQ(getMin(3, 2, 1), 1);
            EXPECT_EQ(getMin(2, 3, 2), 2);
            EXPECT_EQ(getMin(3, 3, 3), 3);
	    }

        TEST(TEST_CLASS, getMultiplierTests) {
            resetTests();
            double multiplier;
            for (int i = 0; i < INCREASE_TESTS_COUNT; ++i) {
                multiplier = getMultiplier(increaseTests[i][0], increaseTests[i][1], increaseTests[i][2]);
                EXPECT_EQ(multiplier, increaseTests[i][3]);
            }
	    }

        // check if initial multiplier value returned (when it's initially 1) is correct
        TEST(TEST_CLASS, getCoinGenerationMultiplierTestsIndividualMultipliers) {
            resetTests();
            generatePriceList();
            double multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2 - 1);
            EXPECT_EQ(multiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            // namespace variable should be updated too
            EXPECT_EQ(currentMultiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            resetTests();
            generatePriceList();
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 3 - 1);
            EXPECT_EQ(multiplier, 1 + (0.06 + (28.0 / 23.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR);
            resetTests();
            generatePriceList();
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 4 - 1);
            EXPECT_EQ(multiplier, 1 + (0.06 + (34.0 / 28.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR);
	    }

        // Check if the multiplier value changes according to the existing multiplier value (when it's not 1)
        TEST(TEST_CLASS, getCoinGenerationMultiplierTestsMultipleUpdates) {
            resetTests();
            generatePriceList();
            double multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2);
            EXPECT_EQ(multiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            EXPECT_EQ(currentMultiplier, 1 + 0.25 / EPOCHS_PER_YEAR);
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 3);
            EXPECT_EQ(multiplier, (1 + (0.06 + (30.0 / 26.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR)
                * (1 + 0.25 / EPOCHS_PER_YEAR));
            EXPECT_EQ(currentMultiplier, (1 + (0.06 + (30.0 / 26.0 - 1.15) * 0.35) / EPOCHS_PER_YEAR)
                * (1 + 0.25 / EPOCHS_PER_YEAR));

            // Not enough blocks should reset the multiplier value to 1
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 1);
            EXPECT_EQ(multiplier, 1);
            EXPECT_EQ(currentMultiplier, 1);

            // If not yet time to update the multiplier, it shouldn't change
            currentMultiplier = 1.5;
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2 - 1);
            EXPECT_EQ(currentMultiplier, 1.5);
            EXPECT_EQ(multiplier, 1.5);
	    }

        // getFeeToPay function should return the current value and not update anything
        TEST(TEST_CLASS, getFeeToPayTest_NotUpdateBlock) {
            resetTests();
            uint64_t blockHeight = 1;
            epochFees = 2;
            feeToPay = 10;
            uint64_t fee = getFeeToPay(blockHeight);
            EXPECT_EQ(fee, 10);
            EXPECT_EQ(epochFees, 2);
	    }

        // getFeeToPay function should update the feeToPay value and should reset epochFees to 0
        TEST(TEST_CLASS, getFeeToPayTest_UpdateBlock) {
            resetTests();
            uint64_t blockHeight = 720;
            epochFees = 720;
            feeToPay = 10;
            uint64_t fee = getFeeToPay(blockHeight);
            EXPECT_EQ(fee, 1);
            EXPECT_EQ(epochFees, 0);
	    }*/

        // check if data can be written to and read from the priceData.txt file 
        /*TEST(TEST_CLASS, PricesFileTest) {
            resetTests();
            addPrice(1, 20, 20, true);
            addPrice(86399, 30, 30, true);
            addPrice(86400, 30, 31);
            updatePricesFile();
            loadPricesFromFile();
            EXPECT_EQ(pricesToString(), "");
            totalSupply = 100;
            addSupplyEntryToFile(0, 100);
            totalSupply = 500;
            addSupplyEntryToFile(0, 400);
            //addPriceEntryToFile(86400, 30, 31);
            totalSupply = 123;
            feeToPay = 234;
            epochFees = 345;
            currentMultiplier = 1.23;
            addPrice(1, 2, 3);
            addPrice(4, 5, 5);
            writeToFile();
            resetTests();
            readFromFile();
            EXPECT_EQ(totalSupply, 123);
            EXPECT_EQ(feeToPay, 234);
            EXPECT_EQ(epochFees, 345);
            EXPECT_EQ(currentMultiplier, 1.23);
            
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it = priceList.begin();
            EXPECT_EQ(std::get<0>(*it), 1);
            EXPECT_EQ(std::get<1>(*it), 2);
            EXPECT_EQ(std::get<2>(*it), 3);
            it++;

            EXPECT_EQ(std::get<0>(*it), 4);
            EXPECT_EQ(std::get<1>(*it), 5);
            EXPECT_EQ(std::get<2>(*it), 5);
	    }

        // If the priceData.txt file is empty, values shouldn't change
        TEST(TEST_CLASS, PriceDataFileTest_emptyFile) {
            resetTests();
            std::ofstream fr("priceData.txt"); // empty up the file
            //readFromFile();
            EXPECT_EQ(totalSupply, 0); // values should not change
            EXPECT_EQ(feeToPay, 0);
            EXPECT_EQ(epochFees, 0);
            EXPECT_EQ(currentMultiplier, 0);
	    }*/

        //region block_reward

        TEST(TEST_CLASS, approximateTests) {
            double number = 12345678901.5123456789;
            // bigger than 10e10, so a rounded integer (in form of double) should be returned
            EXPECT_EQ(12345678902.0, approximate(number));

            number = 12345678.123456789; // smaller than 10e10, so up to 10 significant figures (5 decimal places max)
            EXPECT_EQ(12345678.12, approximate(number));
            
            number = 12345678.125456789; // should round the last digit
            EXPECT_EQ(12345678.13, approximate(number));

            number = 0.123456789; // only up to 5 decimal places + rounding
            EXPECT_EQ(0.12346, approximate(number));

            number = 0.123; // leave as is
            EXPECT_EQ(0.123, approximate(number));
	    }

        TEST(TEST_CLASS, getMinTests) {
            EXPECT_EQ(1.2, getMin(1.2, 2.3));
            EXPECT_EQ(1.2, getMin(2.3, 1.2));
            EXPECT_EQ(1.2, getMin(1.2, 2.3, 2.1));
            EXPECT_EQ(1.2, getMin(2.3, 1.2, 2.1));
            EXPECT_EQ(1.2, getMin(2.3, 2.1, 1.2));
	    }

        TEST(TEST_CLASS, IgnoresFuturePricesForAverages) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4 - 1u; // blocks: 0 - 345599
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, getAverageRemovesOldPrices) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT - 1;
            double average30, average60, average90, average120;
            // priceList keeps prices of 100 extra blocks in case of a rollback
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4 + 100; // blocks: 101 - 345700
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetCorrectAverages) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT; // no prices removed
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 4; // blocks: 1 - 345600
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }
        
        TEST(TEST_CLASS, CanGetAveragesForFewerThan120MoreThan90Days) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 3; // blocks: 0 - 259200
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan90MoreThan60Days) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS * 2; // blocks: 0 - 172800
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan60MoreThan30Days) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = BLOCKS_PER_30_DAYS; // blocks: 0 - 86400
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        TEST(TEST_CLASS, CanGetAveragesForFewerThan30Days) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT;
            double average30, average60, average90, average120;
            uint64_t highestBlock = 1; // blocks: 0 - 1
            generatePriceList();
            getAverage(highestBlock, average30, average60, average90, average120);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            assertAverages(average30, average60, average90, average120, highestBlock);
	    }

        // check if initial multiplier value returned (when it's initially 1) is correct
        TEST(TEST_CLASS, getCoinGenerationMultiplierTestsIndividualMultipliers) {
            resetTests();
            generatePriceList();
            double multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2 - 1);
            EXPECT_EQ(multiplier, approximate(1 + 0.25 / EPOCHS_PER_YEAR));

            // namespace currentMultiplier variable should be updated too
            EXPECT_EQ(currentMultiplier, approximate(1 + 0.25 / EPOCHS_PER_YEAR));

            resetTests();
            generatePriceList();
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 3 - 1);
            EXPECT_EQ(multiplier, approximate(1 + (0.06 + (approximate(28.0 / 23.0) - 1.15) * 0.35) / EPOCHS_PER_YEAR));

            resetTests();
            generatePriceList();
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 4 - 1);
            EXPECT_EQ(multiplier, approximate(1 + (0.06 + (approximate(34.0 / 28.0) - 1.15) * 0.35) / EPOCHS_PER_YEAR));
	    }

        // Check if the multiplier value changes according to the existing multiplier value (when it's not 1)
        TEST(TEST_CLASS, getCoinGenerationMultiplierTestsMultipleUpdates) {
            resetTests();
            generatePriceList();
            double multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2);
            EXPECT_EQ(multiplier, approximate(1 + 0.25 / EPOCHS_PER_YEAR));
            EXPECT_EQ(currentMultiplier, approximate(1 + 0.25 / EPOCHS_PER_YEAR));
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 3);
            EXPECT_EQ(multiplier, approximate((1 + (0.06 + (approximate(30.0 / 26.0) - 1.15) * 0.35) / EPOCHS_PER_YEAR)
                * (1 + 0.25 / EPOCHS_PER_YEAR)));
            EXPECT_EQ(currentMultiplier, approximate((1 + (0.06 + (approximate(30.0 / 26.0) - 1.15) * 0.35) / EPOCHS_PER_YEAR)
                * (1 + 0.25 / EPOCHS_PER_YEAR)));

            // Not enough blocks should reset the multiplier value to 1
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 1);
            EXPECT_EQ(multiplier, 1.0);
            EXPECT_EQ(currentMultiplier, 1.0);

            // If it's not yet time (block isn't multiple of 720) to update the multiplier, it shouldn't change
            currentMultiplier = 1.5;
            multiplier = getCoinGenerationMultiplier(BLOCKS_PER_30_DAYS * 2 - 1);
            EXPECT_EQ(currentMultiplier, 1.5);
            EXPECT_EQ(multiplier, 1.5);
	    }

        TEST(TEST_CLASS, getMultiplierTests) {
            resetTests();
            double multiplier;
            for (int i = 0; i < INCREASE_TESTS_COUNT; ++i) {
                multiplier = getMultiplier(increaseTests[i][0], increaseTests[i][1], increaseTests[i][2]);
                EXPECT_EQ(multiplier, increaseTests[i][3]);
            }
	    }

        // getFeeToPay function should return the current value
        TEST(TEST_CLASS, getFeeToPayTest_NotUpdateBlock) {
            resetTests();
            feeToPay = 10u;
            uint64_t fee = getFeeToPay(1);
            EXPECT_EQ(fee, 10u);
	    }

        TEST(TEST_CLASS, getFeeToPayTest_UpdateBlock) {
            resetTests();
            epochFees.push_back({719, 720, 3, "address"});
            uint64_t fee = getFeeToPay(720);
            EXPECT_EQ(fee, 1u);
            EXPECT_EQ(feeToPay, 1u);
	    }

        TEST(TEST_CLASS, getFeeToPayTest_UpdateBlock_EmptyEpochFees) {
            resetTests();
            uint64_t fee = getFeeToPay(720);
            EXPECT_EQ(fee, 0u);
            EXPECT_EQ(feeToPay, 0u);
	    }

        TEST(TEST_CLASS, getFeeToPayTest_Rollback) {
            resetTests();
            epochFees.push_back({719, 1440, 3, "address"});
            epochFees.push_back({720, 0, 2, "address"});
            uint64_t fee = getFeeToPay(720, true);
            EXPECT_EQ(fee, 2u);
            EXPECT_EQ(feeToPay, 2u);
            fee = getFeeToPay(719u, true);
            EXPECT_EQ(fee, 3u);
            EXPECT_EQ(feeToPay, 3u);
	    }

        //endregion block_reward

        //region price_helper

        TEST(TEST_CLASS, CanRemoveOldPrices) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT - 2;
            generatePriceList();
            removeOldPrices(4 * BLOCKS_PER_30_DAYS + 101); // blocks: 2 - 345601
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::iterator it = priceList.begin();
            for (size_t i = 2; i < remainingPricesExpected; ++i) {
                comparePrice(*it++, mockPrices[i]);
            }
            removeOldPrices(8 * BLOCKS_PER_30_DAYS + 101); // remove all
            EXPECT_EQ(priceList.size(), 0u);
	    }

        TEST(TEST_CLASS, CanAddPriceToPriceList) {
            resetTests();
            size_t remainingPricesExpected = 1;
            EXPECT_EQ(priceList.size(), 0u);
            addPrice(1u, 2u, 2u, 1);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
            EXPECT_EQ(std::get<0>(priceList.front()), 1u);
            EXPECT_EQ(std::get<1>(priceList.front()), 2u);
            EXPECT_EQ(std::get<2>(priceList.front()), 2u);
            EXPECT_EQ(std::get<3>(priceList.front()), 1u);
	    }

        TEST(TEST_CLASS, CantAddInvalidPriceToPriceList) {
            resetTests();
            addPrice(1u, 2u, 1u, 1); // lowPrice can't be higher than highPrice
            addPrice(2u, 0u, 2u, 1); // neither lowPrice nor highPrice can be 0
            addPrice(3u, 2u, 0u, 1);
            addPrice(4u, 0u, 0u, 1);
            EXPECT_EQ(priceList.size(), 0u);
            generatePriceList();
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
            addPrice(std::get<0>(mockPrices[MOCK_PRICES_COUNT - 1]) - 1, 3u, 4u, 1);
                // block lower than the one of an already existing price, therefore invalid
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
	    }

        TEST(TEST_CLASS, CanRemovePrice) {
            resetTests();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT - 1;
            // Remove the third price from the end
            uint64_t blockHeight = std::get<0>(mockPrices[MOCK_PRICES_COUNT - 3]);
            uint64_t lowPrice = std::get<1>(mockPrices[MOCK_PRICES_COUNT - 3]);
            uint64_t highPrice = std::get<2>(mockPrices[MOCK_PRICES_COUNT - 3]);
            double multiplier = std::get<3>(mockPrices[MOCK_PRICES_COUNT - 3]);
            generatePriceList();
            removePrice(blockHeight, lowPrice, highPrice, multiplier);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, DoesNotRemoveAnythingIfPriceNotFound) {
            resetTests();
            generatePriceList();
            size_t remainingPricesExpected = MOCK_PRICES_COUNT;
            // Make sure such a price doesn't exist
            uint64_t blockHeight = 751u;
            uint64_t lowPrice = 696u;
            uint64_t highPrice = 697u;
            double multiplier = 2.0341;
            removePrice(blockHeight, lowPrice, highPrice, multiplier);
            EXPECT_EQ(priceList.size(), remainingPricesExpected);
	    }

        // check if data can be written to and read from the prices.txt file 
        TEST(TEST_CLASS, PricesFileTest) {
            resetTests();
            generatePriceList();
            updatePricesFile();
            resetTests();
            loadPricesFromFile();
            EXPECT_EQ(priceList.size(), MOCK_PRICES_COUNT);
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::iterator it = priceList.begin();
            for (size_t i = 0; i < MOCK_PRICES_COUNT; ++i) {
                comparePrice(*it++, mockPrices[i]);
            }
	    }

        // If the prices.txt file is empty, values shouldn't change
        TEST(TEST_CLASS, PricesFileTest_emptyFile) {
            resetTests();
            std::ofstream fr("prices.txt"); // empty up the file
            loadPricesFromFile();
            EXPECT_EQ(priceList.size(), 0u);
	    }

        //endregion price_helper

        //region total_supply_helper

        TEST(TEST_CLASS, CanRemoveOldTotalSupplyEntries) {
            resetTests();
            generateTotalSupply();
            EXPECT_EQ(totalSupply.size(), MOCK_TOTAL_SUPPLY_ENTRIES);
            EXPECT_EQ(std::get<0>(totalSupply.front()), 0u);
            removeOldTotalSupplyEntries(100);
            EXPECT_EQ(totalSupply.size(), MOCK_TOTAL_SUPPLY_ENTRIES - 1);
            EXPECT_EQ(std::get<0>(totalSupply.front()), 100u);
            removeOldTotalSupplyEntries(300);
            EXPECT_EQ(totalSupply.size(), MOCK_TOTAL_SUPPLY_ENTRIES - 3);
            EXPECT_EQ(std::get<0>(totalSupply.front()), 300u);
            removeOldTotalSupplyEntries(400);
            EXPECT_EQ(totalSupply.size(), 0u);
	    }

        TEST(TEST_CLASS, CanAddTotalSupplyEntry) {
            resetTests();
            EXPECT_EQ(totalSupply.size(), 0u);
            addTotalSupplyEntry(1u, 2u, 2u);
            EXPECT_EQ(totalSupply.size(), 1u);
            EXPECT_EQ(std::get<0>(totalSupply.front()), 1u);
	    }

        TEST(TEST_CLASS, CantAddInvalidTotalSupplyEntries) {
            resetTests();
            addTotalSupplyEntry(5u, 10u, 10u);
            addTotalSupplyEntry(1u, 1u, 2u); // total supply must be higher than increase
            addTotalSupplyEntry(1u, 0u, 2u); // entries can't be added to past blocks
            addTotalSupplyEntry(6u, 8u, 2u); // total supply can't be lower than previously specified
            addTotalSupplyEntry(4u, 13u, 1u); // total supply != previous total supply + increase
            EXPECT_EQ(totalSupply.size(), 1u);
	    }

        TEST(TEST_CLASS, CanRemoveSupplyEntry) {
            resetTests();
            size_t remainingPricesExpected = MOCK_TOTAL_SUPPLY_ENTRIES - 1;
            // Remove the third entry from the end
            uint64_t blockHeight = std::get<0>(mockTotalSupply[MOCK_TOTAL_SUPPLY_ENTRIES - 3]);
            uint64_t supply = std::get<1>(mockTotalSupply[MOCK_TOTAL_SUPPLY_ENTRIES - 3]);
            uint64_t increase = std::get<2>(mockTotalSupply[MOCK_TOTAL_SUPPLY_ENTRIES - 3]);
            generateTotalSupply();
            removeTotalSupplyEntry(blockHeight, supply, increase);
            EXPECT_EQ(totalSupply.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, DoesNotRemoveAnythingIfEntryNotFound) {
            resetTests();
            generateTotalSupply();
            size_t remainingPricesExpected = MOCK_TOTAL_SUPPLY_ENTRIES;
            EXPECT_EQ(totalSupply.size(), remainingPricesExpected);
            // Make sure such an entry doesn't exist
            uint64_t blockHeight = 751;
            uint64_t supply = 696;
            uint64_t increase = 69;
            removeTotalSupplyEntry(blockHeight, supply, increase);
            EXPECT_EQ(totalSupply.size(), remainingPricesExpected);
	    }

        // check if data can be written to and read from the totalSupply.txt file 
        TEST(TEST_CLASS, totalSupplyFileTest) {
            resetTests();
            generateTotalSupply();
            updateTotalSupplyFile();
            resetTests();
            loadTotalSupplyFromFile();
            EXPECT_EQ(totalSupply.size(), 1u);
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it = totalSupply.begin();
            for (size_t i = MOCK_TOTAL_SUPPLY_ENTRIES - 1; i < MOCK_TOTAL_SUPPLY_ENTRIES; ++i) {
                compareTotalSupply(*it++, mockTotalSupply[i]);
            }
	    }

        // If the totalSupply.txt file is empty, values shouldn't change
        TEST(TEST_CLASS, totalSupplyFileTest_emptyFile) {
            resetTests();
            std::ofstream fr("totalSupply.txt"); // empty up the file
            loadTotalSupplyFromFile();
            EXPECT_EQ(totalSupply.size(), 0u);
	    }

        //endregion total_supply_helper
        
        //region epoch_fees_helper

        TEST(TEST_CLASS, CanRemoveOldEpochFeeEntries) {
            resetTests();
            generateEpochFees();
            EXPECT_EQ(epochFees.size(), MOCK_EPOCH_FEE_ENTRIES);
            EXPECT_EQ(std::get<0>(epochFees.front()), 0u);
            removeOldEpochFeeEntries(100);
            EXPECT_EQ(epochFees.size(), MOCK_EPOCH_FEE_ENTRIES - 1);
            EXPECT_EQ(std::get<0>(epochFees.front()), 100u);
            removeOldEpochFeeEntries(300);
            EXPECT_EQ(epochFees.size(), MOCK_EPOCH_FEE_ENTRIES - 3);
            EXPECT_EQ(std::get<0>(epochFees.front()), 300u);
            removeOldEpochFeeEntries(400);
            EXPECT_EQ(epochFees.size(), 0u);
	    }

        TEST(TEST_CLASS, CanAddTotalEpochFeeEntry) {
            resetTests();
            EXPECT_EQ(epochFees.size(), 0u);
            addEpochFeeEntry(1u, 2u, 2u, "address");
            EXPECT_EQ(epochFees.size(), 1u);
            EXPECT_EQ(std::get<0>(epochFees.front()), 1u);
	    }

        TEST(TEST_CLASS, CantAddInvalidEpochFeeEntries) {
            resetTests();
            addEpochFeeEntry(5u, 10u, 10u, "address");
            addEpochFeeEntry(3, 1, 1, "address"); // block lower than the previous
            EXPECT_EQ(epochFees.size(), 1u);
	    }

        TEST(TEST_CLASS, CanRemoveEpochFeeEntry) {
            resetTests();
            size_t remainingPricesExpected = MOCK_EPOCH_FEE_ENTRIES - 1;
            // Remove the third entry from the end
            uint64_t blockHeight = std::get<0>(mockEpochFees[MOCK_EPOCH_FEE_ENTRIES - 3]);
            uint64_t collectedFees = std::get<1>(mockEpochFees[MOCK_EPOCH_FEE_ENTRIES - 3]);
            uint64_t blockFee = std::get<2>(mockEpochFees[MOCK_EPOCH_FEE_ENTRIES - 3]);
            generateEpochFees();
            removeEpochFeeEntry(blockHeight, collectedFees, blockFee, "address");
            EXPECT_EQ(epochFees.size(), remainingPricesExpected);
	    }

        TEST(TEST_CLASS, DoesNotRemoveAnythingIfEpochFeeEntryNotFound) {
            resetTests();
            generateEpochFees();
            size_t remainingPricesExpected = MOCK_EPOCH_FEE_ENTRIES;
            EXPECT_EQ(epochFees.size(), remainingPricesExpected);
            // Make sure such an entry doesn't exist
            uint64_t blockHeight = 751;
            uint64_t collectedFees = 696;
            uint64_t blockFee = 69;
            removeEpochFeeEntry(blockHeight, collectedFees, blockFee, "address");
            EXPECT_EQ(epochFees.size(), remainingPricesExpected);
	    }
        
        // check if data can be written to and read from the epochFees.txt file 
        TEST(TEST_CLASS, epochFeesFileTest) {
            resetTests();
            generateEpochFees();
            updateEpochFeeFile();
            resetTests();
            loadEpochFeeFromFile();
            EXPECT_EQ(epochFees.size(), 1u);
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::iterator it = epochFees.begin();
            for (size_t i = MOCK_EPOCH_FEE_ENTRIES - 1; i < MOCK_EPOCH_FEE_ENTRIES; ++i) {
                compareTotalEpochFees(*it++, mockEpochFees[i]);
            }
	    }

        // If the totalSupply.txt file is empty, values shouldn't change
        TEST(TEST_CLASS, epochFeesFileTest_emptyFile) {
            resetTests();
            std::ofstream fr("epochFees.txt"); // empty up the file
            loadEpochFeeFromFile();
            EXPECT_EQ(totalSupply.size(), 0u);
	    }

        //region epoch_fees_helper
    }
}}
