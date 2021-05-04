#ifndef __GEMCAPS_MAIN__
#define __GEMCAPS_MAIN__

#include <iostream>
#define GEMCAPS_DEBUG

#define LOG_INFO(x) std::cout << "INFO " << x << std::endl
#define LOG_WARN(x) std::cout << "WARNING " << x << std::endl
#define LOG_ERROR(x) std::cerr << "ERROR " << x << std::endl

#ifdef GEMCAPS_DEBUG

#define LOG_DEBUG(x) std::cout << "DEBUG " << x << std::endl

#else

#define DEBUG(x)

#endif

#endif