#ifndef ACTION_MANAGER_H
#define ACTION_MANAGER_H

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


class FileAction
{
public:
    virtual FileAction* SetFileName(std::string file_name){

        mFileName = file_name;
    };
    virtual void execute() = 0;
    virtual ~FileAction() { }
protected:
    std::string mFileName;
};

class FindUniqueWords : public FileAction{

public:
    FileAction* SetFileName(std::string file_name)override{

        mFileName = file_name;
        //validate file if its available on disk already
        return this;
    }

    void execute()override{

        /*
         * Rationale:
         *
         * when file is very large loading the entire file will run-out process memory
         * So file is memory-mapped so loading is done page by page and pages are cached by OS
         * mapped file is further partitioned into chunks propotional to available cores on executing system
         * as manay thread as cores are spawned to independently parse the partioned chunks
         * and update the parsed results to master static data structure
         *
         * memory mapped design instead of ifstream(reading the entire file from disk) improved the runtime by 96%
         * lock free coding and partioning the data to chunks (instead of single threaded) improved the runtime by 50%
         *
        */
        if(mFileName.empty())
            throw std::bad_exception();

        std::string input_file_name = mFileName;
        uint number_of_cores_available = std::thread::hardware_concurrency();
        std::size_t individual_buffer_size = 0;
        std::vector<std::future<std::string>> vec_future;

#ifdef USING_BOOST_IPC
        boost::interprocess::file_mapping input_file_mapped(input_file_name.c_str(),boost::interprocess::read_only);
        boost::interprocess::mapped_region mapped_region(input_file_mapped, boost::interprocess::read_only);

        try{
            const char* start_address = reinterpret_cast<const char*>(mapped_region.get_address());
            std::size_t size  = mapped_region.get_size();
            individual_buffer_size = (size / number_of_cores_available);

            for(int i=1;i <= number_of_cores_available;i++,start_address+=individual_buffer_size){

                std::string str;
                str.assign(start_address,individual_buffer_size);

                if(!str.empty()){

                    auto func = [=](std::string input_buffer){

                        FrequencyTableMngr mngr;
                        return mngr.UpdateFrequency(input_buffer);
                    };
                    std::packaged_task<std::string(std::string)> task(func);
                    vec_future.push_back(std::move(task.get_future()));
                    std::thread(std::move(task),std::move(std::string(&str[0]))).detach();
                    str.resize(0);
                }
                if(i == number_of_cores_available){

                    individual_buffer_size = size - (i*individual_buffer_size);
                }
            }
        }catch(std::exception &exp){

            boost::interprocess::file_mapping::remove(input_file_name.c_str());
        }
#else
        std::ifstream input_file_stream(input_file_name, std::ios::in|std::ios::ate);
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

#ifndef USING_LOCK_FREE_CODE
                        std::shared_ptr<FrequencyTableMngr> mngr = FrequencyTableMngr::globalInstance();
#else
                        std::unique_ptr<FrequencyTableMngr> mngr = std::make_unique<FrequencyTableMngr>();
#endif
                        return mngr->UpdateFrequency(input_buffer);
                    };
                    std::packaged_task<std::string(std::string)> task(func);
                    vec_future.push_back(task.get_future());
                    std::thread(std::move(task),std::move(std::string(&str[0]))).detach();
                    str.resize(0);
                }
                if(i == number_of_cores_available){

                    //remaining buffer size will be bigger due to round-off in previous steps
                    individual_buffer_size = size - input_file_stream.tellg();
                }
            }
        }
#endif

        std::for_each(vec_future.begin(),vec_future.end(),[](std::future<std::string> &fu){

            //wait until all detached threads complete execution
            /*std::cout << */fu.get();
        });
    }
};

#endif //ACTION_MANAGER_H
