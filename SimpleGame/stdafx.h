#pragma once

#include <stdio.h>
#include <cwchar>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "targetver.h"
#include <tchar.h>
#endif
