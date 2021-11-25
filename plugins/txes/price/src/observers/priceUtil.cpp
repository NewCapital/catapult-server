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

// epoch = 6 hours -> 4 epochs per day; number of epochs in a year: 365 * 4 = 1460
#define EPOCHS_PER_YEAR 1460
#define BLOCKS_PER_30_DAYS 86400
#define PRICE_DATA_SIZE 4
#define SUPPLY_DATA_SIZE 3
#define EPOCH_FEES_DATA_SIZE 4
#define FEE_RECALCULATION_FREQUENCY 10
#define MULTIPLIER_RECALCULATION_FREQUENCY 5

#ifdef __USE_GNU
typedef int errno_t;
#endif

#if defined(__USE_GNU) || defined(__APPLE__)
static errno_t fopen_s(FILE **f, const char *name, const char *mode) {
    errno_t ret = 0;
    assert(f);
    *f = fopen(name, mode);
    if (!*f)
        ret = errno;
    return ret;
}
#endif

static bool areSame(double a, double b) {
    return std::fabs(a - b) < std::numeric_limits<double>::epsilon();
}

// TODO: move to config files
#define FEE_RECALCULATION_FREQUENCY 10
#define MULTIPLIER_RECALCULATION_FREQUENCY 5

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

    //region block_reward

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
        if (blockHeight % MULTIPLIER_RECALCULATION_FREQUENCY > 0 && !areSame(currentMultiplier, 0) != 0 && !rollback) // recalculate only every 720 blocks
            return currentMultiplier;
        else if (areSame(currentMultiplier, 0))
            currentMultiplier = 1;

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
        currentMultiplier = approximate(currentMultiplier * getMultiplier(increase30, increase60, increase90));
        return currentMultiplier;
    }

    double getMultiplier(double increase30, double increase60, double increase90) {
        double min;
        increase30 = approximate(increase30);
        increase60 = approximate(increase60);
        increase90 = approximate(increase90);
        if (increase30 >= 1.25 && increase60 >= 1.25) {
            if (increase90 >= 1.25) {
                min = getMin(increase30, increase60, increase90);
                if (min >= 1.55)
                    return approximate(1 + 0.735 / static_cast<double>(EPOCHS_PER_YEAR));
                else if (min >= 1.45)
                    return approximate(1 + (0.67 + (min - 1.45) * 0.65) / static_cast<double>(EPOCHS_PER_YEAR));
                else if (min >= 1.35)
                    return approximate(1 + (0.61 + (min - 1.35) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
                else if (min >= 1.25)
                    return approximate(1 + (0.55 + (min - 1.25) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
            } else {
                min = getMin(increase30, increase60);
                if (min >= 1.55)
                    return approximate(1 + 0.49 / static_cast<double>(EPOCHS_PER_YEAR));
                else if (min >= 1.45)
                    return approximate(1 + (0.43 + (min - 1.45) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
                else if (min >= 1.35)
                    return approximate(1 + (0.37 + (min - 1.35) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
                else if (min >= 1.25)
                    return approximate(1 + (0.31 + (min - 1.25) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
            }
        } else if (increase30 >= 1.05) {
            min = increase30;
            if (min >= 1.55)
                return approximate(1 + 0.25 / static_cast<double>(EPOCHS_PER_YEAR));
            else if (min >= 1.45)
                return approximate(1 + (0.19 + (min - 1.45) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
            else if (min >= 1.35)
                return approximate(1 + (0.13 + (min - 1.35) * 0.6) / static_cast<double>(EPOCHS_PER_YEAR));
            else if (min >= 1.25)
                return approximate(1 + (0.095 + (min - 1.25) * 0.35) / static_cast<double>(EPOCHS_PER_YEAR));
            else if (min >= 1.15)
                return approximate(1 + (0.06 + (min - 1.15) * 0.35) / static_cast<double>(EPOCHS_PER_YEAR));
            else if (min >= 1.05)
                return approximate(1 + (0.025 + (min - 1.05) * 0.35) / static_cast<double>(EPOCHS_PER_YEAR));
        }
        return 1;
    }

    uint64_t getFeeToPay(uint64_t blockHeight, bool rollback) {
        uint64_t collectedEpochFees = 0;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::reverse_iterator it;
        if (rollback) {
            if (epochFees.size() == 0) {
                feeToPay = 0;
                return feeToPay;
            }
            for (it = catapult::plugins::epochFees.rbegin(); it != catapult::plugins::epochFees.rend(); ++it) {         
                if (std::get<0>(*it) == blockHeight) {
                    feeToPay = std::get<2>(*it);
                } else if (std::get<0>(*it) < blockHeight) {
                    feeToPay = 0;
                    break;
                }
			}
            return feeToPay;
        }
        if (blockHeight % FEE_RECALCULATION_FREQUENCY == 0) {
            if (epochFees.size() == 0) {
                feeToPay = 0;
                return feeToPay;
            }
            for (it = catapult::plugins::epochFees.rbegin(); it != catapult::plugins::epochFees.rend(); ++it) {
                if (blockHeight - 1 == std::get<0>(*it)) {
                    collectedEpochFees = std::get<1>(*it);
                    break;
                }
            }
            feeToPay = static_cast<unsigned int>(static_cast<double>(collectedEpochFees) / static_cast<double>(FEE_RECALCULATION_FREQUENCY) + 0.5);
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
        uint64_t boundary = 300; // number of blocks equivalent to 30 days
        double *averagePtr = &average30;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::reverse_iterator it;
        // we also need to visit priceList.begin(), so we just break when we reach it
        for (it = priceList.rbegin(); it != priceList.rend(); ++it) {
            if (std::get<0>(*it) < blockHeight + 1u - boundary && blockHeight + 1u >= boundary) {
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
                boundary += 300;
                if (blockHeight + 1u < boundary) // not enough blocks for the next 30 days
                    break;
            } else if (std::get<0>(*it) > blockHeight) {
                // ignore price messages into the future
                continue;
            }
            *averagePtr += static_cast<double>(std::get<1>(*it) + std::get<2>(*it));
            ++count;
        }
        if (count > 0 && blockHeight + 1u >= boundary) {
            *averagePtr = *averagePtr / count / 2;
            approximate(*averagePtr);
        }
        else
            *averagePtr = 0;

        CATAPULT_LOG(info) << "New averages found for block height " << blockHeight
            <<": 30 day average : " << average30 << ", 60 day average: " << average60
            << ", 90 day average: " << average90 << ", 120 day average: " << average120 << "\n";
    }

    double getMin(double num1, double num2, double num3) {
        if (areSame(num3, -1)) {
            return num1 >= num2 ? num2 : num1;
        }
        return num1 >= num2 ? (num3 >= num2 ? num2 : num3) : num1; 
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
        if (blockHeight < 345600u + 100u) // no old blocks (store additional 100 blocks in case of a rollback)
            return;
        bool updated = false;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::iterator it;
        for (it = priceList.begin(); it != priceList.end(); ++it) {
            if (std::get<0>(*it) < blockHeight - 345599u - 100u) { // older than 120 days + 100 blocks
                priceList.erase(it);
                updated = true;
            }
            else
                return;
            if (it == priceList.end()) {
                break;
            }
        }
        if (updated)
            updatePricesFile();
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
            if (previousTransactionHeight >= blockHeight) {
                CATAPULT_LOG(warning) << "Warning: price transaction block height is lower or equal to the previous: " <<
                    "Previous height: " << previousTransactionHeight << ", current height: " << blockHeight << "\n";
                return false;
            }

            if (previousTransactionHeight == blockHeight && std::get<1>(priceList.back()) == lowPrice &&
                std::get<2>(priceList.back()) == highPrice && areSame(std::get<3>(priceList.back()), multiplier)) {
                // data matches, so must be a duplicate, however, no need to resynchronise prices
                CATAPULT_LOG(warning) << "Warning: price transaction data is equal to the previous price transaction data: "
                    << "block height: " << blockHeight << ", lowPrice: " << lowPrice << ", highPrice: " << highPrice << "\n";
                return true;
            }
        }
        priceList.push_back({blockHeight, lowPrice, highPrice, multiplier});
        CATAPULT_LOG(info) << "\n" << pricesToString() << "\n";
        if (addToFile)
            addPriceEntryToFile(blockHeight, lowPrice, highPrice, multiplier);

        CATAPULT_LOG(info) << "New price added to the list for block " << blockHeight << " , lowPrice: "
            << lowPrice << ", highPrice: " << highPrice << ", multiplier: " << multiplier << "\n";
        return true;
    }

    void removePrice(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier) {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, double>>::reverse_iterator it;
        for (it = priceList.rbegin(); it != priceList.rend(); ++it) {
            if (blockHeight > std::get<0>(*it))
                break;
                
            if (std::get<0>(*it) == blockHeight && std::get<1>(*it) == lowPrice &&
                std::get<2>(*it) == highPrice && areSame(std::get<3>(*it), multiplier)) {
                it = decltype(it)(priceList.erase(std::next(it).base()));
                CATAPULT_LOG(info) << "Price removed from the list for block " << blockHeight 
                    << ", lowPrice: " << lowPrice << ", highPrice: " << highPrice << ", multiplier: "
                    << multiplier << "\n";
                break;
            }
        }
        updatePricesFile(); // update data in the file
    }

    void addPriceEntryToFile(uint64_t blockHeight, uint64_t lowPrice, uint64_t highPrice, double multiplier) {
        long size = 0;
        FILE *file;
        errno_t err = fopen_s(&file, "prices.txt", "r+");
        if (err != 0) {
            err = fopen_s(&file, "prices.txt", "w+");
            if (err != 0) {
                CATAPULT_LOG(error) << "Error: Can't open nor create the prices.txt file\n";
                return;
            }
        } else {
            fseek(file, 0, SEEK_END);
            size = ftell(file);
            if (size == -1) {
                CATAPULT_LOG(error) << "Error: problem with fseek in prices.txt file\n";
                fclose(file);
                return;
            }
        }
        if (size % 50 > 0) {
            CATAPULT_LOG(error) << "Fatal error: prices.txt file is corrupt/invalid\n";
            return;
        }
        
        std::string priceData[PRICE_DATA_SIZE] = {
            std::to_string(blockHeight),
            std::to_string(lowPrice),
            std::to_string(highPrice),
            std::to_string(multiplier)
        };
        
        for (int i = 0; i < PRICE_DATA_SIZE; ++i) {
            // add spaces to string as padding
            /** 
             * wowazzz: One variable `size` is used for 2 meaning?..weird
             */
            std::size_t priceSize = priceData[i].length();
            if (i > 0 && i < PRICE_DATA_SIZE - 1) {
                for (std::size_t j = 0; j < 15 - priceSize; ++j) {
                    priceData[i] += ' ';
                }
            } else {
                for (std::size_t j = 0; j < 10 - priceSize; ++j) {
                    priceData[i] += ' ';
                }
            }
            fputs(priceData[i].c_str(), file);
        }
        fclose(file);
    }

    void updatePricesFile() {
        FILE *file;
        errno_t err = fopen_s(&file, "prices.txt", "w"); // erase and rewrite the prices
        if (err == 0)
            fclose(file);
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

    void loadPricesFromFile() {
        long size;
        double multiplier;
        bool errors = false;
        std::string blockHeight = "", lowPrice = "", highPrice = "", multiplierStr = "";
        priceList.clear();
        FILE *file;
        errno_t err = fopen_s(&file, "prices.txt", "r+");
        if (err != 0) {
            CATAPULT_LOG(warning) << "Warning: prices.txt does not exist\n";
            return;
        }
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        if (size == -1) {
            CATAPULT_LOG(error) << "Error: problem with fseek in prices.txt file\n";
            fclose(file);
            return;
        } else if (size % 100 > 0) {
            CATAPULT_LOG(error) << "Error: prices.txt content is invalid\n";
            fclose(file);
            return;
        }
        fseek(file, 0, SEEK_SET);
        while (ftell(file) < size) {
            blockHeight = "";
            lowPrice = "";
            highPrice = "";
            multiplierStr = "";
            for (int i = 0; i < 10 && !feof(file); ++i) {
                blockHeight += static_cast<char>(getc(file));
            } for (int i = 0; i < 15 && !feof(file); ++i) {
                lowPrice += static_cast<char>(getc(file));
            } for (int i = 0; i < 15 && !feof(file); ++i) {
                highPrice += static_cast<char>(getc(file));
            } for (int i = 0; i < 10 && !feof(file); ++i) {
                multiplierStr += static_cast<char>(getc(file));
            }

            errors = !addPrice(std::stoul(blockHeight), std::stoul(lowPrice), std::stoul(highPrice), std::stod(multiplierStr),
                false);
            currentMultiplier = 0;
            multiplier = getCoinGenerationMultiplier(std::stoul(blockHeight));
            if ( !areSame(multiplier, stod(multiplierStr)) ) {
                CATAPULT_LOG(error) << "Error: actual multiplier (" << multiplier << ") doesn't match the multiplier"
                    << " specified in the file (" << multiplierStr << ")\n";
                errors = true;
            }
            if (errors) {
                CATAPULT_LOG(error) << "Fatal error: prices.txt file is corrupt/invalid\n";
                break;
            }
        }
        fclose(file);
    }

    //endregion price_helper

    //region total_supply_helper

    void removeOldTotalSupplyEntries(uint64_t blockHeight) {
        if (blockHeight < 100u)
            return;
        bool updated = false;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        for (it = totalSupply.begin(); it != totalSupply.end(); ++it) {
            if (std::get<0>(*it) < blockHeight - 99u) { // older than 100 blocks
                totalSupply.erase(it);
                updated = true;
            }
            else
                return; // Entries are ordered, so no need to look further
            if (it == totalSupply.end()) {
                break;
            }
        }
        if (updated)
            updateTotalSupplyFile();
    }

    bool addTotalSupplyEntry(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase, bool addToFile) {
        removeOldTotalSupplyEntries(blockHeight);

        uint64_t previousEntryHeight, previousEntrySupply;

        if (increase > supplyAmount) {
            CATAPULT_LOG(error) << "Error: increase can't be bigger than total supply amount\n";
            return false;
        }

        if (totalSupply.size() > 0) {
            previousEntryHeight = std::get<0>(totalSupply.back());
            previousEntrySupply = std::get<1>(totalSupply.back());
            if (previousEntryHeight >= blockHeight) {
                CATAPULT_LOG(warning) << "Warning: total supply block height is lower or equal to the previous: " <<
                    "Previous height: " << previousEntryHeight << ", current height: " << blockHeight << "\n";
                return false;
            }

            if (previousEntrySupply > supplyAmount) {
                CATAPULT_LOG(warning) << "Warning: total supply is lower than the previous supply\n";
                return false;
            }

            if (previousEntrySupply + increase != supplyAmount) {
                CATAPULT_LOG(error) << "Error: total supply is not equal to the increase + total supply of the last entry\n";
                return false;
            }

            if (previousEntryHeight == blockHeight) {
                // data matches, so must be a duplicate, however, no need to resynchronise prices, just ignore it
                CATAPULT_LOG(warning) << "Warning: total supply block is equal to the previous entry block height: "
                    << "block height: " << blockHeight << "\n";
                return false;
            }
        }
        totalSupply.push_back({blockHeight, supplyAmount, increase});
        CATAPULT_LOG(info) << "\n" << totalSupplyToString() << "\n";
        if (addToFile)
            addTotalSupplyEntryToFile(blockHeight, supplyAmount, increase);

        CATAPULT_LOG(info) << "New total supply entry added to the list for block " << blockHeight
            << " , suply: " << supplyAmount << ", increase: " << increase << "\n";
        return true;
    }

    void removeTotalSupplyEntry(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase) {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::reverse_iterator it;
        for (it = totalSupply.rbegin(); it != totalSupply.rend(); ++it) {
            if (blockHeight > std::get<0>(*it))
                break;

            if (std::get<0>(*it) == blockHeight && std::get<1>(*it) == supplyAmount &&
                std::get<2>(*it) == increase) {
                it = decltype(it)(totalSupply.erase(std::next(it).base()));
                CATAPULT_LOG(info) << "Total supply entry removed from the list for block " << blockHeight 
                    << ", supplyAmount: " << supplyAmount << ", increase: " << increase << "\n";
                break;
            }
        }
        updateTotalSupplyFile(); // update data in the file
    }

    void addTotalSupplyEntryToFile(uint64_t blockHeight, uint64_t supplyAmount, uint64_t increase) {
        long size = 0;
        FILE *file;
        errno_t err = fopen_s(&file, "totalSupply.txt", "r+");
        if (err != 0) {
            err = fopen_s(&file, "totalSupply.txt", "w+");
            if (err != 0) {
                CATAPULT_LOG(error) << "Error: Can't open nor create the totalSupply.txt file\n";
                return;
            }
        } else {
            fseek(file, 0, SEEK_END);
            size = ftell(file);
            if (size == -1) {
                CATAPULT_LOG(error) << "Error: problem with fseek in totalSupply.txt file\n";
                fclose(file);
                return;
            }
        }
        if (size % 34 > 0) {
            CATAPULT_LOG(error) << "Fatal error: totalSupply.txt file is corrupt/invalid\n";
            return;
        }

        std::string supplyData[SUPPLY_DATA_SIZE] = {
            std::to_string(blockHeight),
            std::to_string(supplyAmount),
            std::to_string(increase)
        };
        
        for (int i = 0; i < SUPPLY_DATA_SIZE; ++i) {
            // add spaces to string as padding
            /** 
             * wowazzz: One variable `size` is used for 2 meaning?..weird
             */
            std::size_t supplySize = supplyData[i].length();
            if (i > 0) {
                for (std::size_t j = 0; j < 12 - supplySize; ++j) {
                    supplyData[i] += ' ';
                }
            } else {
                for (std::size_t j = 0; j < 10 - supplySize; ++j) {
                    supplyData[i] += ' ';
                }
            }
            fputs(supplyData[i].c_str(), file);
        }
        fclose(file);
    }

    void updateTotalSupplyFile() {
        FILE *file;
        errno_t err = fopen_s(&file, "totalSupply.txt", "w"); // erase and rewrite the prices
        if (err == 0)
            fclose(file);
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        for (it = totalSupply.begin(); it != totalSupply.end(); ++it) {
            addTotalSupplyEntryToFile(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it));
        }
    }

    std::string totalSupplyToString() {
        std::string list = "height:   supply:     increase:   \n";
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t>>::iterator it;
        std::size_t length = 0;
        for (it = totalSupply.begin(); it != totalSupply.end(); ++it) {
            list += std::to_string(std::get<0>(*it));
            length = std::to_string(std::get<0>(*it)).length();
            for (std::size_t i = 0; i < 10 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<1>(*it));
            length = std::to_string(std::get<1>(*it)).length();
            for (std::size_t i = 0; i < 12 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<2>(*it));
            length = std::to_string(std::get<2>(*it)).length();
            for (std::size_t i = 0; i < 12 - length; ++i)
                list += ' ';
            list += '\n';
        }
        return list;
    }

    void loadTotalSupplyFromFile() {
        long size;
        bool errors = false;
        std::string blockHeight = "", supply = "", increase = "";
        totalSupply.clear();
        FILE *file;
        errno_t err = fopen_s(&file, "totalSupply.txt", "r+");
        if (err != 0) {
            CATAPULT_LOG(warning) << "Warning: totalSupply.txt does not exist\n";
            return;
        }
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        if (size == -1) {
            CATAPULT_LOG(error) << "Error: problem with fseek in totalSupply.txt file\n";
            fclose(file);
            return;
        } else if (size % 34 > 0) {
            CATAPULT_LOG(error) << "Error: totalSupply.txt content is invalid\n";
            fclose(file);
            return;
        }
        fseek(file, 0, SEEK_SET);
        while (ftell(file) < size) {
            blockHeight = "";
            supply = "";
            increase = "";
            for (int i = 0; i < 10 && !feof(file); ++i) {
                blockHeight += static_cast<char>(getc(file));
            } for (int i = 0; i < 12 && !feof(file); ++i) {
                supply += static_cast<char>(getc(file));
            } for (int i = 0; i < 12 && !feof(file); ++i) {
                increase += static_cast<char>(getc(file));
            }

            errors = !addTotalSupplyEntry(std::stoul(blockHeight), std::stoul(supply), std::stoul(increase), false);
            if (errors) {
                CATAPULT_LOG(error) << "Fatal error: totalSupply.txt file is corrupt/invalid\n";
                break;
            }
        }
        fclose(file);
    }
    
    //endregion total_supply_helper

    //region epoch_fees_helper

    void removeOldEpochFeeEntries(uint64_t blockHeight) {
        if (blockHeight < 100u)
            return;
        bool updated = false;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::iterator it;
        for (it = epochFees.begin(); it != epochFees.end(); ++it) {
            if (std::get<0>(*it) < blockHeight - 99u) { // older than 100 blocks
                epochFees.erase(it);
                updated = true;
            }
            else
                break; // Entries are ordered, so no need to look further
            if (it == epochFees.end()) {
                break;
            }
        }
        if (updated)
            updateEpochFeeFile();
    }

    bool addEpochFeeEntry(uint64_t blockHeight, uint64_t collectedFees, uint64_t currentFee, std::string address, bool addToFile) {
        removeOldEpochFeeEntries(blockHeight);

        uint64_t previousEntryHeight;
        std::vector<std::string> prevAddresses;
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::reverse_iterator it;

        if (epochFees.size() > 0) {
            previousEntryHeight = std::get<0>(epochFees.back());
            for (it = catapult::plugins::epochFees.rbegin(); it != catapult::plugins::epochFees.rend(); ++it) {         
                if (std::get<0>(*it) == previousEntryHeight) {
                    prevAddresses.push_back(std::get<3>(*it));
                } else if (std::get<0>(*it) < previousEntryHeight) {
                    break;
                }
			}
            if (previousEntryHeight > blockHeight) {
                CATAPULT_LOG(warning) << "Warning: epoch fee entry block height is lower to the previous: " <<
                    "Previous height: " << previousEntryHeight << ", current height: " << blockHeight << "\n";
                return false;
            } else if (previousEntryHeight == blockHeight) {
                for (unsigned int i = 0; i < prevAddresses.size(); ++i) {
                    if (prevAddresses[i] == address) {
                        CATAPULT_LOG(warning) << "Warning: skipping a duplicate epoch fee entry for block: " << blockHeight <<
                            ", collected fees: " << collectedFees << ", currentFee: " << currentFee << ", address: " << address << "\n";
                        return false;
                    }
                }
            }
        }
        epochFees.push_back({blockHeight, collectedFees, currentFee, address});
        CATAPULT_LOG(info) << "\n" << epochFeeToString() << "\n";
        if (addToFile)
            addEpochFeeEntryToFile(blockHeight, collectedFees, currentFee, address);

        CATAPULT_LOG(info) << "New epoch fee entry added to the list for block " << blockHeight
            << " , collectedFees: " << collectedFees << ", feeToPay: " << currentFee << ", address: " << address << "\n";
        return true;
    }

    void removeEpochFeeEntry(uint64_t blockHeight, uint64_t collectedFees, uint64_t blockFee, std::string address) {
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::reverse_iterator it;
        for (it = epochFees.rbegin(); it != epochFees.rend(); ++it) {
            if (blockHeight > std::get<0>(*it))
                break;

            if (std::get<0>(*it) == blockHeight && std::get<1>(*it) == collectedFees &&
                std::get<2>(*it) == blockFee && std::get<3>(*it) == address) {
                it = decltype(it)(epochFees.erase(std::next(it).base()));
                CATAPULT_LOG(info) << "Epoch fee entry removed from the list for block " << blockHeight 
                    << ", collectedFees: " << collectedFees << ", feeToPay: " << blockFee << ", address: " << address << "\n";
                break;
            }
        }
        updateEpochFeeFile(); // update data in the file
    }

    void addEpochFeeEntryToFile(uint64_t blockHeight, uint64_t collectedFees, uint64_t blockFee, std::string address) {
        long size = 0;
        FILE *file;
        errno_t err = fopen_s(&file, "epochFees.txt", "r+");
        if (err != 0) {
            err = fopen_s(&file, "epochFees.txt", "w+");
            if (err != 0) {
                CATAPULT_LOG(error) << "Error: Can't open nor create the epochFees.txt file\n";
                return;
            }
        } else {
            fseek(file, 0, SEEK_END);
            size = ftell(file);
            if (size == -1) {
                CATAPULT_LOG(error) << "Error: problem with fseek in epochFees.txt file\n";
                fclose(file);
                return;
            }
        }
        if (size % 84 > 0) {
            CATAPULT_LOG(error) << "Fatal error: epochFees.txt file is corrupt/invalid\n";
            return;
        }

        std::string epochFeesData[EPOCH_FEES_DATA_SIZE] = {
            std::to_string(blockHeight),
            std::to_string(collectedFees),
            std::to_string(blockFee),
            address
        };
        
        for (int i = 0; i < EPOCH_FEES_DATA_SIZE; ++i) {
            // add spaces to string as padding
            /** 
             * wowazzz: One variable `size` is used for 2 meaning?..weird
             */
            std::size_t epochSize = epochFeesData[i].length();
            if (i > 0 && i < EPOCH_FEES_DATA_SIZE - 1) {
                for (std::size_t j = 0; j < 12 - epochSize; ++j) {
                    epochFeesData[i] += ' ';
                }
            } else if (i == EPOCH_FEES_DATA_SIZE - 1) {
                for (std::size_t j = 0; j < 50 - epochSize; ++j) {
                    epochFeesData[i] += ' ';
                }
            } else {
                for (std::size_t j = 0; j < 10 - epochSize; ++j) {
                    epochFeesData[i] += ' ';
                }
            }
            fputs(epochFeesData[i].c_str(), file);
        }
        fclose(file);
    }

    void updateEpochFeeFile() {
        FILE *file;
        errno_t err = fopen_s(&file, "epochFees.txt", "w"); // erase and rewrite the data
        if (err == 0)
            fclose(file);
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::iterator it;
        for (it = epochFees.begin(); it != epochFees.end(); ++it) {
            addEpochFeeEntryToFile(std::get<0>(*it), std::get<1>(*it), std::get<2>(*it), std::get<3>(*it));
        }
    }

    std::string epochFeeToString() {
        std::string list = "height:   collected:  feeToPay:  \n";
        std::deque<std::tuple<uint64_t, uint64_t, uint64_t, std::string>>::iterator it;
        std::size_t length = 0;
        for (it = epochFees.begin(); it != epochFees.end(); ++it) {
            list += std::to_string(std::get<0>(*it));
            length = std::to_string(std::get<0>(*it)).length();
            for (std::size_t i = 0; i < 10 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<1>(*it));
            length = std::to_string(std::get<1>(*it)).length();
            for (std::size_t i = 0; i < 12 - length; ++i)
                list += ' ';
            list += std::to_string(std::get<2>(*it));
            length = std::to_string(std::get<2>(*it)).length();
            for (std::size_t i = 0; i < 12 - length; ++i)
                list += ' ';
            list += (std::get<3>(*it));
            length = std::get<3>(*it).length();
            for (std::size_t i = 0; i < 50 - length; ++i)
                list += ' ';
            list += '\n';
        }
        return list;
    }

    void loadEpochFeeFromFile() {
        long size;
        bool errors = false;
        char character;
        std::string blockHeight = "", collectedFees = "", currentFee = "", address = "";
        epochFees.clear();
        FILE *file;
        errno_t err = fopen_s(&file, "epochFees.txt", "r+");
        if (err != 0) {
            CATAPULT_LOG(warning) << "Warning: epochFees.txt does not exist\n";
            return;
        }
        fseek(file, 0, SEEK_END);
        size = ftell(file);
        if (size == -1) {
            CATAPULT_LOG(error) << "Error: problem with fseek in epochFees.txt file\n";
            fclose(file);
            return;
        } else if (size % 84 > 0) {
            CATAPULT_LOG(error) << "Error: epochFees.txt content is invalid\n";
            fclose(file);
            return;
        }
        fseek(file, 0, SEEK_SET);
        while (ftell(file) < size) {
            blockHeight = "";
            collectedFees = "";
            currentFee = "";
            address = "";
            for (int i = 0; i < 10 && !feof(file); ++i) {
                blockHeight += static_cast<char>(getc(file));
            } for (int i = 0; i < 12 && !feof(file); ++i) {
                collectedFees += static_cast<char>(getc(file));
            } for (int i = 0; i < 12 && !feof(file); ++i) {
                currentFee += static_cast<char>(getc(file));
            } for (int i = 0; i < 50 && !feof(file); ++i) {
                character = static_cast<char>(getc(file));
                if (character != ' ') {
                    address += character;
                }
            }

            errors = !addEpochFeeEntry(std::stoul(blockHeight), std::stoul(collectedFees), std::stoul(currentFee), address, false);
            if (errors) {
                CATAPULT_LOG(error) << "Fatal error: epochFees.txt file is corrupt/invalid\n";
                break;
            }
        }
        fclose(file);
    }

    //endregion epoch_fees_helper
}}
