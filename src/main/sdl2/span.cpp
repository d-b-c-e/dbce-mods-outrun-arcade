/***************************************************************************
    Triple screens (DBCE toolkit STD-015). See span.hpp.
***************************************************************************/

#include "sdl2/span.hpp"

#include <algorithm>
#include <vector>

namespace span
{
    static const float TRIPLE_ASPECT = 2.9f;

    bool separate_triple(const SDL_Rect* d, int count, SDL_Rect& out)
    {
        if (count < 3) return false;
        int x0 = d[0].x, x1 = d[0].x + d[0].w, y0 = d[0].y, h = d[0].h;
        long long area = 0;
        for (int i = 0; i < count; i++)
        {
            if (d[i].h != h || d[i].y != y0) return false;           // one row, equal heights
            if (d[i].w >= d[i].h * TRIPLE_ASPECT) return false;     // a Surround display: not separate
            x0 = std::min(x0, d[i].x); x1 = std::max(x1, d[i].x + d[i].w);
            area += (long long) d[i].w;
        }
        if (area != (long long) (x1 - x0)) return false;            // contiguous, no gaps or overlaps
        if (x1 - x0 < h * TRIPLE_ASPECT) return false;
        out.x = x0; out.y = y0; out.w = x1 - x0; out.h = h;
        return true;
    }

    bool separate_monitors(SDL_Rect& out)
    {
        int n = SDL_GetNumVideoDisplays();
        if (n < 3) return false;
        std::vector<SDL_Rect> rects(n);
        for (int i = 0; i < n; i++) if (SDL_GetDisplayBounds(i, &rects[i]) != 0) return false;
        return separate_triple(rects.data(), n, out);
    }

    bool surround()
    {
        SDL_Rect r;
        if (SDL_GetDisplayBounds(0, &r) != 0) return false;
        return r.w >= r.h * TRIPLE_ASPECT;
    }
}