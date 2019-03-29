#include <iostream>
#include <thread>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <fstream>
#include <thread>
#include <future>
#include "utility.h"

#ifdef USING_BOOST_IPC
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#endif
#ifndef USING_LOCK_FREE_CODE
std::shared_ptr<FrequencyTableMngr> FrequencyTableMngr::mThisInstance = nullptr;
#endif

/*
 * # - read the input file with a single thread (multiple threads will not help but further slow down)
 * # - take the hw concurrency supported by the system 'T'
 * # - take the size on input file 'S'
 * # - partition the file into S/T pieces and store in tmp folder
 * # - [A-Za-z]\w+
 * # - read the input file and write to partitions (until size of file is met)
 * # - on every piece being created release the thread and spawn a thread to do processing and once processed remove the file
 * # - use unordered map to store "text "(key) and value(frequency) using emplace() so search is always 0(1)
 * # - once all thread has exited do partial_sort for only N items so 0(n(log(N-first)))
 * # - sudo apt-get install libboost-iostreams-dev
*/

int main(int argc, char *argv[])
{
    if(argc < 2){
        std::cout << "Error!! input file required: " << std::endl;
        return 0;
    }
    std::string input_file_name = "./"+std::string(argv[1]);
    uint number_of_cores_available = std::thread::hardware_concurrency();
    std::size_t individual_buffer_size = 0;

#ifndef USING_LOCK_FREE_CODE
    std::vector<std::future<std::string>> vec_future;
#else
    std::vector<std::future<std::unordered_map<std::string,size_t>>> vec_future;
#endif

#ifdef USING_BOOST_IPC
    //Create a file mapping
    boost::interprocess::file_mapping input_file_mapped(input_file_name.c_str(),boost::interprocess::read_only);
    boost::interprocess::mapped_region mapped_region(input_file_mapped, boost::interprocess::read_only);
    const char* start_address = reinterpret_cast<const char*>(mapped_region.get_address());
    std::size_t size  = mapped_region.get_size();
    individual_buffer_size = (size / number_of_cores_available);

    for(int i=1;i <= number_of_cores_available;i++,start_address+=individual_buffer_size){

        std::string str;
        str.assign(start_address,individual_buffer_size);

        if(!str.empty()){

#ifndef USING_LOCK_FREE_CODE
            auto func = [=](std::string input_buffer){

                std::shared_ptr<FrequencyTableMngr> mngr = FrequencyTableMngr::globalInstance();
                return mngr->UpdateFrequency(input_buffer);
            };
            std::packaged_task<std::string(std::string)> task(func);
            vec_future.push_back(std::move(task.get_future()));
            std::thread(std::move(task),std::move(std::string(&str[0]))).detach();
            str.resize(0);
        }
#else
            std::packaged_task<std::unordered_map<std::string,size_t>(std::string)> task(UpdateFrequency);
            vec_future.push_back(std::move(task.get_future()));
            std::thread(std::move(task),std::move(std::string(&str[0]))).detach();
            str.resize(0);
#endif
        }
        if(i == number_of_cores_available){

            individual_buffer_size = size - (i*individual_buffer_size);
        }
    }
#else
    std::ifstream input_file_stream(input_file_name, std::ios::in);
    if (!input_file_stream.is_open()) {

        std::cout << "failed to open " << input_file_name << '\n';
    }else {

        auto size = input_file_stream.tellg();
        individual_buffer_size = (size / number_of_cores_available);
        input_file_stream.seekg(0);
        std::string str(individual_buffer_size, '\0');
        for(int i=1;i <= number_of_cores_available;i++){

            if(input_file_stream.read(&str[0], individual_buffer_size)){

                auto func = [=](std::string input_buffer){
                    return mngr->UpdateFrequency(input_buffer);
                };
                std::packaged_task<std::string(std::string)> task(func);
                vec_future.push_back(task.get_future());
                std::thread(std::move(task),std::move(std::string(&str[0]))).detach();
                str.resize(0);
            }
            if(i == number_of_cores_available){

                individual_buffer_size = size - input_file_stream.tellg();
            }
        }
    }
#endif

#ifndef USING_LOCK_FREE_CODE
    std::for_each(vec_future.begin(),vec_future.end(),[](std::future<std::string> &fu){

        /*std::cout << */fu.get();
    });
    mngr->DisplayResult(21);
#else
    std::unordered_map<std::string,size_t> master_table;
    std::for_each(vec_future.begin(),vec_future.end(),[&master_table](std::future<std::unordered_map<std::string,size_t>> &fu){

        MergeTables(master_table,fu.get());
        //delete the future here just incase accessing it will assert
    });
    DisplayResults(master_table,21);
#endif
    return 0;
}
