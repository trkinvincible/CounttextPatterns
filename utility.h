#ifndef UTILITY_H
#define UTILITY_H

#include <string>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <iomanip>
#include <vector>
#include <functional>
#include <regex>

#ifdef USING_LOCK_FREE_CODE

class FrequencyTableMngr
{
    /*
     * Rationale:
     *
     * master data structure is std::unordered_map which internally use hashing
     * as the key is std::string no collisions are possible hence add/update/search is constanct time O(1)
     *
     * master data structure is guarded by mutex pegged to it both are not accessible outside of the class
     *
     * regular expression is used to seperate out the unique words
     *
     * displaying results:
     * convert unordered_map to vector - O(n)
     * partial_sort v - O(n(log(list_size-first)))
     *
     * unordered_map is transformed as vector so std::vector can support std::partial_sort()
     *
    */
private:
    static std::mutex mFreqTableGuard;
    static std::unordered_map<std::string,size_t> mPatterFequencyTable;
public:
    std::string UpdateFrequency(std::string input_text){

        std::unordered_map<std::string,size_t> pattern_frequency_table;
        auto start = std::chrono::high_resolution_clock::now();

        std::regex r(R"([^\W]+*)");
        for(std::sregex_iterator i = std::sregex_iterator(input_text.begin(), input_text.end(), r);
            i != std::sregex_iterator();
            ++i)
        {
            std::smatch m = *i;
            ++pattern_frequency_table[m.str()];
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
        std::cout << std::this_thread::get_id() << " Complete in : " << diff.count() << "ms." << std::endl;

        MergeWithMasterTable(std::move(pattern_frequency_table));

        return "success";
    }

    static void MergeWithMasterTable(std::unordered_map<std::string,size_t> &&t2){

        std::lock_guard<std::mutex> lock(mFreqTableGuard);
        for(auto &item : t2){

            mPatterFequencyTable[item.first] += item.second;
        }
    }

    static void DisplayResults(int list_size){

        if(list_size > mPatterFequencyTable.size()){

            list_size = mPatterFequencyTable.size();
        }

        auto comparator = [](std::pair<std::string, int> &i1 ,std::pair<std::string, int> &i2)
        {
            return i1.second > i2.second;
        };
        std::vector<std::pair<std::string, int>> v(mPatterFequencyTable.begin(),mPatterFequencyTable.end());
        std::partial_sort(v.begin(),v.begin()+list_size,v.end(),comparator);

        std::for_each(v.begin(),v.begin()+list_size,[](std::pair<std::string,int> &item){

            std::cout << std::setw(10) << item.second << "   " << item.first << std::endl;
        });
    }
};
#else
class FrequencyTableMngr
{
private:
    std::mutex mFreqTableGuard;
    std::unordered_map<std::string,size_t> mPatterFequencyTable;
    static std::shared_ptr<FrequencyTableMngr> mThisInstance;
    FrequencyTableMngr(){}
public:
    ~FrequencyTableMngr(){}
    static void DestroyCache(){

        mThisInstance.reset();
    }
    FrequencyTableMngr(const FrequencyTableMngr &rhs) = delete;
    FrequencyTableMngr(const FrequencyTableMngr &&rhs) = delete;
    FrequencyTableMngr& operator=(const FrequencyTableMngr &rhs) = delete;
    FrequencyTableMngr& operator=(const FrequencyTableMngr &&rhs) = delete;

    static std::shared_ptr<FrequencyTableMngr> globalInstance(){

        std::mutex selfMutex;
        if(!mThisInstance){

            std::lock_guard<std::mutex> lock(selfMutex);
            if(!mThisInstance){
                mThisInstance.reset(new FrequencyTableMngr);
            }
        }
        return mThisInstance;
    }

    std::string UpdateFrequency(std::string input_text){

        auto start = std::chrono::high_resolution_clock::now();

        std::regex r(R"([^\W]+*)");
        for(std::sregex_iterator i = std::sregex_iterator(input_text.begin(), input_text.end(), r);
            i != std::sregex_iterator();
            ++i)
        {
            std::smatch m = *i;
            std::lock_guard<std::mutex> lock(mFreqTableGuard);
            ++mPatterFequencyTable[m.str()];
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
        std::cout << std::this_thread::get_id() << " Complete in : " << diff.count() << "ms." << std::endl;

        return "return the individual thread's count";
    }

    void DisplayResult(int list_size){

        if(list_size > mPatterFequencyTable.size()){

            list_size = mPatterFequencyTable.size();
        }
        /*
         * convert unordered_map to vector - O(n)
         * partial_sort v - O(n(log(list_size-first)))
        */
        auto comparator = [](std::pair<std::string, int> &i1 ,std::pair<std::string, int> &i2)
        {
            return i1.second > i2.second;
        };
        std::vector<std::pair<std::string, int>> v(mPatterFequencyTable.begin(),mPatterFequencyTable.end());
        std::partial_sort(v.begin(),v.begin()+list_size,v.end(),comparator);

        std::for_each(v.begin(),v.begin()+list_size,[](std::pair<std::string,int> &item){

            std::cout << std::setw(10) << item.second << "   " << item.first << std::endl;
        });
    }
};
#endif


#endif//UTILITY_H
