#ifndef __GEMCAPS_MAIN__
#define __GEMCAPS_MAIN__

#include <iostream>
// #define GEMCAPS_DEBUG

#define INFO(x) std::cout << "INFO " << x << std::endl
#define WARN(x) std::cout << "WARNING " << x << std::endl
#define ERROR(x) std::cerr << "ERROR " << x << std::endl

#ifdef GEMCAPS_DEBUG

#define DEBUG(x) std::cout << "DEBUG " << x << std::endl

#else

#define DEBUG(x)

#endif

#endif