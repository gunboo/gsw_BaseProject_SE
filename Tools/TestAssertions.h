#pragma once

// Tests remain active in optimized builds without changing the game's flags.
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
