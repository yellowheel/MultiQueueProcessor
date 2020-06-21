#include <iostream>
#include "MultiQueueProcessor_test.h"
#include <set>
#include <random>
using namespace std;


int main()
{
    //test Example
    std::vector<std::string> qs {"a","b","c","e","f","g","h","i","j"};
    ProdTest<std::string,std::string>(10,30000,100,qs,5);
return 0;
}
