// euclid.h - Euclidean rhythm track (Bresenham form of Bjorklund)
#pragma once

struct EuclidTrack
{
    int steps = 16;  // pattern length, 1..32
    int hits = 4;    // number of onsets, 0..steps
    int rotate = 0;  // rotation in steps

    bool hit(int step) const
    {
        if (steps <= 0 || hits <= 0)
            return false;
        int i = ((step + rotate) % steps + steps) % steps;
        // onset where floor(i*k/n) increases: evenly spread k hits over n steps
        return ((i * hits) % steps) < hits;
    }

    void set(int n, int k, int r)
    {
        steps = n < 1 ? 1 : (n > 32 ? 32 : n);
        hits = k < 0 ? 0 : (k > steps ? steps : k);
        rotate = r;
    }
};
