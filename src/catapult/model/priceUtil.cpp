#include "catapult/utils/Logging.h"
#include "stdint.h"
#include <deque>
#include <tuple>
#include "priceUtil.h"
#include "catapult/types.h"
#include "string.h"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cmath>
#include <cstdio>
#include "src/catapult/io/FileBlockStorage.h"
#include "src/catapult/model/Elements.h"

#define PRICE_DATA_SIZE 4
#define SUPPLY_DATA_SIZE 3
#define EPOCH_FEES_DATA_SIZE 4

#ifdef __USE_GNU
typedef int errno_t;
#endif

static bool areSame(double a, double b) {
    return abs(a - b) < std::numeric_limits<double>::epsilon();
}

namespace catapult { namespace plugins {

    /**
     *  wowazzz: bad case using global variables
     *  types std::string and std::deque
     *  (Clang compiler error)
     *  -Wglobal-constructors -Wexit-time-destructors
     */
    NODESTROY std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>> priceList;
    NODESTROY std::deque<std::tuple<uint64_t, uint64_t, uint64_t>> totalSupply;
    NODESTROY std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>> epochFees;
    double currentMultiplier = 1;
    uint64_t feeToPay = 0;

    uint64_t initialSupply = 0;
    std::string pricePublisherPublicKey = "";
    uint64_t feeRecalculationFrequency = 0;
    uint64_t multiplierRecalculationFrequency = 0;
    uint64_t pricePeriodBlocks = 0;
    uint64_t entryLifetime = 0;
    uint64_t generationCeiling = 0;
    std::mutex priceMutex;
    
    const std::string priceDirectory = "./data/price";
    std::vector<std::string> priceFields {"default"};
    cache::RocksDatabaseSettings priceSettings(priceDirectory, priceFields, cache::FilterPruningMode::Disabled);
    std::unique_ptr<cache::RocksDatabase> priceDB = std::make_unique<cache::RocksDatabase>();

    //region block_reward

    void readConfig() {
        std::string line;
        std::ifstream fr("./data/config.txt");
        try {
            getline(fr, line);
            initialSupply = stoul(line);
            getline(fr, pricePublisherPublicKey);
            getline(fr, line);
            feeRecalculationFrequency = stoul(line);
            getline(fr, line);
            multiplierRecalculationFrequency = stoul(line);
            getline(fr, line);
            pricePeriodBlocks = stoul(line);
            getline(fr, line);
            entryLifetime = stoul(line);
            getline(fr, line);
            generationCeiling = stoul(line);
        } catch (...) {
            CATAPULT_LOG(error) << "Error: price config file is invalid, network-config file may be missing price plugin information.";
            CATAPULT_LOG(error) << "Price plugin configuration includes: initialSupply, pricePublisherPublicKey, feeRecalculationFrequency, multiplierRecalculationFrequency, and pricePeriodBlocks";
            throw ("Price config file is invalid, network-config file may be missing price plugin information.");
        }
    }

    void configToFile() {
        std::ofstream fw("./data/config.txt");
        fw << initialSupply << "\n";
        fw << pricePublisherPublicKey << "\n";
        fw << feeRecalculationFrequency << "\n";
        fw << multiplierRecalculationFrequency << "\n";
        fw << pricePeriodBlocks << "\n";
        fw << entryLifetime << "\n";
        fw << generationCeiling << "\n";
    }

    // leave up to 10 significant figures (max 5 decimal digits)
    double approximate(double number) {
        if (number > pow(10, 10)) {
            // if there are more than 10 digits before the decimal point, ignore the decimal digits
            /**
             * wowazzz: Is that simple round() ??
             * 
             * number = (double)(static_cast<uint64_t>(number + 0.5));
             */
            number = round(number);
        } else {
            for (int i = 0; i < 10; ++i) {
                if (pow(10, i + 1) > number) { // i + 1 digits left to the decimal point
                    if (i < 4)
                        i = 4;
                    /**
                     * wowazzz: Is that simple round() ??
                     * 
                     * number = (double)(static_cast<uint64_t>(number * pow(10, 9 - i) + 0.5)) / pow(10, 9 - i);
                     */
                    number = round(number * pow(10, 9 - i)) / pow(10, 9 - i);
                    break;
                }
            }
        }        
        return number;
    }

    double getCoinGenerationMultiplier(uint64_t blockHeight, bool rollback) {
        if (blockHeight % multiplierRecalculationFrequency > 0 && !areSame(currentMultiplier, 0) && !rollback) {
            return currentMultiplier;
        }
        else if (areSame(currentMultiplier, 0)) {
            currentMultiplier = 1;
        }
        if (rollback) {
            std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::reverse_iterator it;
            for (it = priceList.rbegin(); it != priceList.rend(); ++it) {
                if (blockHeight == std::get<0>(*it)) {
                    return std::get<3>(*it);
                }
            }
        }
        double average30 = 0, average60 = 0, average90 = 0, average120 = 0;
        getAverage(blockHeight, average30, average60, average90, average120);
        if ( areSame(average60, 0) ) { // either it hasn't been long enough or data is missing
            currentMultiplier = 1;
            return 1;
        }
        double increase30 = average30 / average60;
        double increase60 = areSame(average90, 0) ? 0 : average60 / average90;
        double increase90 = areSame(average120, 0) ? 0 : average90 / average120;
        CATAPULT_LOG(error) << "Increase 30: " << increase30 << ", incrase 60: " << increase60 << ", increase 90: " << increase90 << "\n";
        CATAPULT_LOG(error) << "Get multiplier: " << getMultiplier(increase30, increase60, increase90) << "\n";
        CATAPULT_LOG(error) << "Current multiplier: " << currentMultiplier << "\n";
        currentMultiplier = approximate(currentMultiplier * getMultiplier(increase30, increase60, increase90));
        return currentMultiplier;
    }

    double getMultiplier(double increase30, double increase60, double increase90) {
        uint64_t pricePeriodsPerYear = 1051200 / pricePeriodBlocks; // 1051200 - number of blocks in a year
        double min;
        increase30 = approximate(increase30);
        increase60 = approximate(increase60);
        increase90 = approximate(increase90);
        if (increase30 >= 1.25 && increase60 >= 1.25) {
            if (increase90 >= 1.25) {
                min = getMin(increase30, increase60, increase90);
                if (min >= 1.55)
                    return approximate(1 + 0.735 / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.45)
                    return approximate(1 + (0.67 + (min - 1.45) * 0.65) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.35)
                    return approximate(1 + (0.61 + (min - 1.35) * 0.6) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.25)
                    return approximate(1 + (0.55 + (min - 1.25) * 0.6) / static_cast<double>(pricePeriodsPerYear));
            } else {
                min = getMin(increase30, increase60);
                if (min >= 1.55)
                    return approximate(1 + 0.49 / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.45)
                    return approximate(1 + (0.43 + (min - 1.45) * 0.6) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.35)
                    return approximate(1 + (0.37 + (min - 1.35) * 0.6) / static_cast<double>(pricePeriodsPerYear));
                else if (min >= 1.25)
                    return approximate(1 + (0.31 + (min - 1.25) * 0.6) / static_cast<double>(pricePeriodsPerYear));
            }
        } else if (increase30 >= 1.05) {
            min = increase30;
            if (min >= 1.55)
                return approximate(1 + 0.25 / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.45)
                return approximate(1 + (0.19 + (min - 1.45) * 0.6) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.35)
                return approximate(1 + (0.13 + (min - 1.35) * 0.6) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.25)
                return approximate(1 + (0.095 + (min - 1.25) * 0.35) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.15)
                return approximate(1 + (0.06 + (min - 1.15) * 0.35) / static_cast<double>(pricePeriodsPerYear));
            else if (min >= 1.05)
                return approximate(1 + (0.025 + (min - 1.05) * 0.35) / static_cast<double>(pricePeriodsPerYear));
        }
        return 1;
    }

    uint64_t getFeeToPay(uint64_t blockHeight, uint64_t *collectedFees, bool rollback, std::string beneficiary) {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::reverse_iterator it;
        if (rollback) {
            if (epochFees.size() == 0) {
                feeToPay = 0;
                return feeToPay;
            }
            for (it = catapult::plugins::epochFees.rbegin(); it != catapult::plugins::epochFees.rend(); ++it) {         
                if (std::get<0>(*it) == blockHeight && std::get<3>(*it) == beneficiary) {
                    feeToPay = std::get<2>(*it);
                    *collectedFees = std::get<1>(*it);
                    break;
                } else if (std::get<0>(*it) < blockHeight) {
                    feeToPay = 0;
                    break;
                }
			}
            return feeToPay;
        }
        if (blockHeight % feeRecalculationFrequency == 0) {
            if (epochFees.size() == 0) {
                feeToPay = 0;
                return feeToPay;
            }
            for (it = catapult::plugins::epochFees.rbegin(); it != catapult::plugins::epochFees.rend(); ++it) {
                if (blockHeight - 1 == std::get<0>(*it)) {
                    *collectedFees = std::get<1>(*it);
                    break;
                }
            }
            feeToPay = static_cast<unsigned int>(static_cast<double>(*collectedFees) / static_cast<double>(feeRecalculationFrequency) + 0.5);
        }
        else if (blockHeight > feeRecalculationFrequency) {
            for (it = catapult::plugins::epochFees.rbegin(); it != catapult::plugins::epochFees.rend(); ++it) {
                if (blockHeight - 1 == std::get<0>(*it)) {
                    *collectedFees = std::get<1>(*it);
                    feeToPay = std::get<2>(*it);
                    break;
                }
            }
        }
        return feeToPay;
    }

    void getAverage(uint64_t blockHeight, double &average30, double &average60, double &average90, 
        double &average120) {
        average30 = 0;
        average60 = 0;
        average90 = 0;
        average120 = 0;
        removeOldPrices(blockHeight);
        if (priceList.size() == 0)
            return;
        int count = 0;
        uint64_t boundary = pricePeriodBlocks, prevBlock = -1;
        double *averagePtr = &average30;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::reverse_iterator it;
        for (it = priceList.rbegin(); it != priceList.rend(); ++it) {
            if (std::get<0>(*it) >= blockHeight || std::get<0>(*it) == prevBlock) {
                continue;
            }
            prevBlock = std::get<0>(*it);
            
            if (std::get<0>(*it) <= blockHeight - 1u - boundary && blockHeight > boundary) {
                if (averagePtr == &average30) {
                    averagePtr = &average60;
                    if (count > 0)
                        average30 = approximate(average30 / count / 2);
                } else if (averagePtr == &average60) {
                    averagePtr = &average90;
                    if (count > 0)
                        average60 = approximate(average60 / count / 2);
                } else if (averagePtr == &average90) {
                    averagePtr = &average120;
                    if (count > 0)
                        average90 = approximate(average90 / count / 2);
                } else {
                    break; // 120 days reached
                }
                count = 0;
                boundary += pricePeriodBlocks;
                if (blockHeight - 1u < boundary) // not enough blocks for the next 30 days
                    break;
            }
            *averagePtr += static_cast<double>(std::get<1>(*it) + std::get<2>(*it));
            ++count;
        }
        if (count > 0 && blockHeight - 1 >= boundary) {
            *averagePtr = *averagePtr / count / 2;
            approximate(*averagePtr);
        }
        else
            *averagePtr = 0;

        CATAPULT_LOG(error) << "New averages found for block height " << blockHeight
            <<": 30 day average : " << average30 << ", 60 day average: " << average60
            << ", 90 day average: " << average90 << ", 120 day average: " << average120 << "\n";
    }

    double getMin(double num1, double num2, double num3) {
        if (areSame(num3, -1)) {
            return num1 >= num2 ? num2 : num1;
        }
        return num1 >= num2 ? (num3 >= num2 ? num2 : num3) : num1 >= num3 ? num3 : num1;
    }

    void processPriceTransaction(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, bool rollback) {
        double multiplier = getCoinGenerationMultiplier(blockHeight);
        if (rollback) {
        	std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::reverse_iterator it;
			for (it = priceList.rbegin(); it != priceList.rend(); ++it) {
				if (std::get<0>(*it) < blockHeight ||
					(std::get<0>(*it) == blockHeight && 
					std::get<1>(*it) == lowPrice &&
					std::get<2>(*it) == highPrice &&
                    areSame(std::get<3>(*it), multiplier))) {
					
					catapult::plugins::removePrice(blockHeight, lowPrice, highPrice, multiplier);
				}
				if (std::get<0>(*it) < blockHeight) {
					return; // no such price found
				}

			}
			return;
		}
		catapult::plugins::addPrice(blockHeight, lowPrice, highPrice, multiplier);
    }
    
    //endregion block_reward

    //region price_helper

    void removeOldPrices(uint64_t blockHeight) {
        if (blockHeight < 345600u + entryLifetime) // no old blocks (store some additional blocks in case of a rollback)
            return;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::iterator it;
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            if (std::get<0>(*it) < blockHeight - 345599u - entryLifetime) { // older than 120 days + some entryLifetime blocks
                priceList.erase(it);
            }
            else
                return;
            if (it == priceList.end()) {
                break;
            }
        }
    }

    bool addPrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier, bool addToFile) {
        removeOldPrices(blockHeight);
        // must be non-zero
        if (!lowPrice || !highPrice) {
            if (!lowPrice)
                CATAPULT_LOG(error) << "Error: lowPrice is 0, must be non-zero number\n";
            if (!highPrice)
                CATAPULT_LOG(error) << "Error: highPrice is 0, must be non-zero number\n";
            return false;
        } else if (lowPrice > highPrice) {
            CATAPULT_LOG(error) << "Error: highPrice can't be lower than lowPrice\n";
            return false;          
        } else if (multiplier < 1) {
            CATAPULT_LOG(error) << "Error: multiplier can't be lower than 1\n";
            return false;
        }
        uint64_t previousTransactionHeight;

        if (priceList.size() > 0) {
            previousTransactionHeight = std::get<0>(priceList.back());
            if (previousTransactionHeight == blockHeight && std::get<1>(priceList.back()) == lowPrice &&
                std::get<2>(priceList.back()) == highPrice && areSame(std::get<3>(priceList.back()), multiplier)) {
                // data matches, so must be a duplicate, however, no need to resynchronise prices
                CATAPULT_LOG(warning) << "Warning: price transaction data is equal to the previous price transaction data: "
                    << "block height: " << blockHeight << ", lowPrice: " << lowPrice << ", highPrice: " << highPrice << "\n";
                return true;
            }
            
            if (previousTransactionHeight >= blockHeight) {
                CATAPULT_LOG(warning) << "Warning: price transaction block height is lower or equal to the previous: " <<
                    "Previous height: " << previousTransactionHeight << ", current height: " << blockHeight << "\n";
                auto it = catapult::plugins::priceList.rbegin();
                for (it = catapult::plugins::priceList.rbegin(); it != catapult::plugins::priceList.rend(); ++it) {
                    if (std::get<0>(*it) <= blockHeight) {
                        break;
                    }
                }
                catapult::plugins::priceList.insert(it.base(), {blockHeight, lowPrice, highPrice, multiplier});
                if (addToFile)
                    addPriceEntryToFile(blockHeight, lowPrice, highPrice, multiplier);
                    
                CATAPULT_LOG(error) << "New price added to the list for block " << blockHeight << " , lowPrice: "
                    << lowPrice << ", highPrice: " << highPrice << ", multiplier: " << multiplier << "\n";
                return true;
            }

        }
        priceList.push_back({blockHeight, lowPrice, highPrice, multiplier});
        if (addToFile)
            addPriceEntryToFile(blockHeight, lowPrice, highPrice, multiplier);

        CATAPULT_LOG(error) << "New price added to the list for block " << blockHeight << " , lowPrice: "
            << lowPrice << ", highPrice: " << highPrice << ", multiplier: " << multiplier << "\n";
        return true;
    }

    void removePrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier) {
        if (priceDB->columnFamilyNames().size() == 0) {
            priceDB.reset(new cache::RocksDatabase(priceSettings));
        }

        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::reverse_iterator it;
        for (it = priceList.rbegin(); it != priceList.rend(); ++it) {
            if (blockHeight > std::get<0>(*it))
                break;
                
            if (std::get<0>(*it) == blockHeight && std::get<1>(*it) == lowPrice &&
                std::get<2>(*it) == highPrice && areSame(std::get<3>(*it), multiplier)) {
                CATAPULT_LOG(error) << "Price removed from the list for block " << blockHeight 
                    << ", lowPrice: " << lowPrice << ", highPrice: " << highPrice << ", multiplier: "
                    << multiplier << "\n";
                priceList.erase(std::next(it).base());
                break;
            }
        }
        priceDB->del(0, rocksdb::Slice(std::to_string(blockHeight)));
        priceDB->flush();
    }

    void addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier) {
        if (priceDB->columnFamilyNames().size() == 0) {
            priceDB.reset(new cache::RocksDatabase(priceSettings));
        }

        std::string priceData[PRICE_DATA_SIZE - 1] = {
            std::to_string(lowPrice),
            std::to_string(highPrice),
            std::to_string(multiplier)
        };        
        for (int i = 0; i < PRICE_DATA_SIZE - 1; ++i) {
            std::size_t priceSize = priceData[i].length();
            if (i < PRICE_DATA_SIZE - 2) {
                for (std::size_t j = 0; j < 15 - priceSize; ++j) {
                    priceData[i] += ' ';
                }
            } else {
                for (std::size_t j = 0; j < 10 - priceSize; ++j) {
                    priceData[i] += ' ';
                }
            }
        }
        for (int i = 1; i < PRICE_DATA_SIZE - 1; ++i) {
            priceData[0] += priceData[i];
        }

        priceDB->put(0, rocksdb::Slice(std::to_string(blockHeight)), priceData[0]);
        priceDB->flush();
    }

    void updatePricesFile() {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::iterator it;
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            addPriceEntryToFile(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it), std::get<3>(*it));
        }
    }

    std::string pricesToString() {
        std::string list = "height:   lowPrice:      highPrice:    multiplier\n";
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::iterator it;
        std::size_t length = 0;
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            list += std::to_string(std::get<0>(*it));
            length = std::to_string(std::get<0>(*it)).length();
            for (std::size_t i = 0; i < 10 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<1>(*it));
            length = std::to_string(std::get<1>(*it)).length();
            for (std::size_t i = 0; i < 15 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<2>(*it));
            length = std::to_string(std::get<2>(*it)).length();
            for (std::size_t i = 0; i < 15 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<3>(*it));
            length = std::to_string(std::get<3>(*it)).length();
            for (std::size_t i = 0; i < 10 - length; ++i)
                list += ' ';
            list += '\n';
        }
        return list;
    }

    void loadPricesFromFile(uint64_t blockHeight) {
        if (blockHeight <= 1) {
            return;
        }
        if (priceDB->columnFamilyNames().size() == 0) {
            priceDB.reset(new cache::RocksDatabase(priceSettings));
        }
        cache::RdbDataIterator result;
        std::string values[PRICE_DATA_SIZE - 1];
        uint64_t key = blockHeight - 345599u - entryLifetime;
        if (key > blockHeight) {
            key = 0;
        }
        while (key <= blockHeight) {
            priceDB->get(0, rocksdb::Slice(std::to_string(key)), result);
            if (result.storage().empty()) {
                key++;
                continue;
            }
            values[0] = result.storage().ToString();
            values[1] = values[0].substr(15, 15);
            values[2] = values[0].substr(30, 10);
            values[0] = values[0].substr(0, 15);

            addPrice(key, std::stoul(values[0]), std::stoul(values[1]), std::stod(values[2]),
                false);
            result.storage().clear();
            key++;
        }
    }

    //endregion price_helper
}}
