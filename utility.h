#ifndef UTILITY_H
#define UTILITY_H

#include <sys/mman.h>
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
#ifdef USING_BOOST_IPC
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/algorithm/string.hpp>
#endif

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

        std::regex r("\\w+");
        for(std::sregex_iterator i = std::sregex_iterator(input_text.begin(), input_text.end(), r);
            i != std::sregex_iterator();
            ++i)
        {
            ++pattern_frequency_table[i->str()];
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
        std::cout << std::this_thread::get_id() << " Complete in : " << diff.count() << "ms." << std::endl;

        MergeWithMasterTable(std::move(pattern_frequency_table));

        return "success";
    }

    std::string UpdateFrequency(boost::interprocess::file_mapping &input_file_mapped,
                                boost::interprocess::offset_t offset,
                                std::size_t size){

        auto start = std::chrono::high_resolution_clock::now();

        boost::interprocess::mapped_region mapped_region(input_file_mapped,
                                                         boost::interprocess::read_only,
                                                         offset,size);

        const int individual_chunk_count = 4;

        const char* start_address = reinterpret_cast<const char*>(mapped_region.get_address());
        auto mapped_region_size = mapped_region.get_size();
        auto mapped_region_chunk_size = mapped_region_size/individual_chunk_count;
        std::unordered_map<std::string,size_t> pattern_frequency_table;

        int count = individual_chunk_count;
        while(count--){

            start_address = reinterpret_cast<const char*>(mapped_region.get_address());
            const char* end_address;
            if(count == 0)
                end_address = start_address + mapped_region.get_size();
            else
                end_address = start_address + mapped_region_chunk_size;

            std::locale loc;
            bool word_started=false;
            for(auto word_start=start_address;start_address != end_address;start_address++){

                //for;--the
                if(std::isalpha(*start_address,loc) && !word_started){

                    word_started = true;
                    word_start = start_address;
                }else{
                    if((std::isspace(*start_address,loc)
                        || (*start_address == '\n')
                        || (std::ispunct(*start_address,loc))) && word_started){

                        std::string temp;
                        temp.resize(std::distance(word_start,start_address));
                        temp.assign(word_start,std::distance(word_start,start_address));
                        ++pattern_frequency_table[temp];
                        word_started = false;
                    }else{
                        if(std::isalpha(*start_address,loc))
                            continue;
                        else
                            word_started = false;
                    }
                }
            }
            mapped_region.shrink_by(mapped_region_chunk_size,false);
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
