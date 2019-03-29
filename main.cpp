
#include "action_manager.h"

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
//using boost yields 3.6 percent efficiency
std::mutex FrequencyTableMngr::mFreqTableGuard;
std::unordered_map<std::string,size_t> FrequencyTableMngr::mPatterFequencyTable;

/*
 * The program supports command pattern
 * class can support mutiple file actions - "class is closed for modification and open for extension"
*/
int main(int argc, char *argv[])
{
    auto start = std::chrono::high_resolution_clock::now();
    if(argc < 2){
        std::cout << "Error!! input file required: " << std::endl;
        std::cout << "Eg: ./groundlabs ./moby.txt 20" << std::endl;
        return 0;
    }

    try{

        std::unique_ptr<FileAction> action_mngr = std::make_unique<FindUniqueWords>();
        //Method chaining eliminates an extra variable for each intermediate step
        action_mngr->SetFileName(std::string("./")+std::string(argv[1]))->execute();
    }catch(std::exception &e){

        std::cout << "file name is not valid";
    }
    FrequencyTableMngr::DisplayResults(std::atoi(argv[2])+1);

    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "total runtime : " << diff.count() << "ms." << std::endl;


    return 0;
}
