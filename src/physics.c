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
