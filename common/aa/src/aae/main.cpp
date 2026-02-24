
#include <string>
#include <span>
#include <iostream>

#include "aargs.h"
#include "alua.h"

#include "aapp.h"

int main(int argc, char* argv[])
{
    std::cout << "CMain >> 开始..." << std::endl;

    std::cout << "CMain >> Parse args..." << std::endl;
    aargs::getrunarger()->parse(argc, argv);
    
    std::cout << "CMain >> New lua state" << std::endl;
    alua::newstate(false);
    std::cout << "CMain >> Call LMain..." << std::endl;
    auto mainfile = aargs::getrunarger()->get("mainfile", "");
    if (mainfile.empty())
    {
        std::cout << "CMain >> 没有输入Lua-Main入口文件" << std::endl;
        return 1;
    }
    alua::loadfile(mainfile.c_str());
    
    std::cout << "CMain >> Close lua state" << std::endl;
    alua::closestate();

    std::cout << "CMain >> 结束" << std::endl;
    return 0;
}
