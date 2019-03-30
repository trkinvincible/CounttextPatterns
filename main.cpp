
#include "action_manager.h"
#ifdef USING_LOCK_FREE_CODE
std::mutex FrequencyTableMngr::mFreqTableGuard;
std::unordered_map<std::string,size_t> FrequencyTableMngr::mPatterFequencyTable;
#endif
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
#ifdef USING_LOCK_FREE_CODE
    FrequencyTableMngr::DisplayResults(std::atoi(argv[2])+1);
#else
    FrequencyTableMngr::globalInstance()->DisplayResult(std::atoi(argv[2])+1);
#endif
    auto end = std::chrono::high_resolution_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::microseconds>(end-start);
    std::cout << "total runtime : " << diff.count() << "ms." << std::endl;


    return 0;
}
