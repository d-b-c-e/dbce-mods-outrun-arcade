/***************************************************************************
    Triple screens (DBCE toolkit STD-015): Surround and separate monitors.

    A display at least 2.9x as wide as it is tall (NVIDIA Surround / AMD
    Eyefinity) or three or more equal-height monitors side by side count as
    a triple display. On separate monitors the game opens one borderless window
    across all of them instead of going fullscreen on the primary display, so
    no display mode is ever changed.
***************************************************************************/

#pragma once

#include <SDL.h>

namespace span
{
    // Pure: union of the display rectangles when they form a separate-monitor triple
    // (3+ displays, equal heights, contiguous in a row, union >= 2.9:1, no Surround display).
    bool separate_triple(const SDL_Rect* displays, int count, SDL_Rect& out);

    // Queries SDL (video must be initialised).
    bool separate_monitors(SDL_Rect& out);
    bool surround();
    inline bool triple_display() { SDL_Rect r; return surround() || separate_monitors(r); }
}