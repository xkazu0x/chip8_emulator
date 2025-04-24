#include "excalibur_os_helper.h"
#include "excalibur_os_helper.cpp"

#if defined(OS_WINDOWS)
#include "excalibur_win32.h"
#include "excalibur_win32.cpp"
#else
#error no implementation for the current operating system
#endif
