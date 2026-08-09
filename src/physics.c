#include "physics.h"

#include <math.h>

static float clampf(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void integrate(float dt, const float *input, float *pos, float *vel, int dim,
               const PhysicsParams *params)
{
    float len = 0.0f;
    for (int i = 0; i < dim; i++)
        len += input[i] * input[i];
    len = sqrtf(len);

    float scale = 1.0f;
    if (len > 1.0f) scale = 1.0f / len;

    float decay = 1.0f / (1.0f + params->friction * dt);

    for (int i = 0; i < dim; i++) {
        vel[i] += input[i] * scale * params->accel * dt;
        vel[i] *= decay;
        vel[i] = clampf(vel[i], -params->max_speed, params->max_speed);
        pos[i] += vel[i] * dt;
    }
}

void collide_bounds(float *pos, float *vel, int dim, const float *lo, const float *hi)
{
    for (int i = 0; i < dim; i++) {
        if (pos[i] < lo[i]) {
            pos[i] = lo[i];
            vel[i] = 0.0f;
        } else if (pos[i] > hi[i]) {
            pos[i] = hi[i];
            vel[i] = 0.0f;
        }
    }
}

int collide_solid(float pos[2], float vel[2], float size, const RectF *solid)
{
    float px = pos[0];
    float py = pos[1];

    // Touching edges do not count as overlap (same semantics as before).
    if (px >= solid->x + solid->w || px + size <= solid->x ||
        py >= solid->y + solid->h || py + size <= solid->y) {
        return 0;
    }

    // Push out along the smallest overlap.
    float overL = (px + size) - solid->x;
    float overR = (solid->x + solid->w) - px;
    float overT = (py + size) - solid->y;
    float overB = (solid->y + solid->h) - py;
    float m = fminf(fminf(overL, overR), fminf(overT, overB));

    if (m == overL) { pos[0] = solid->x - size; vel[0] = 0.0f; }
    else if (m == overR) { pos[0] = solid->x + solid->w; vel[0] = 0.0f; }
    else if (m == overT) { pos[1] = solid->y - size; vel[1] = 0.0f; }
    else { pos[1] = solid->y + solid->h; vel[1] = 0.0f; }
    return 1;
}
