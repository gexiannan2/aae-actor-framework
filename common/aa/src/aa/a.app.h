#include <string>
#include <mutex>

#include "aapp.h"

namespace aapp
{
    class mdata
    {
    public:
        std::mutex mtx_;
    };
}