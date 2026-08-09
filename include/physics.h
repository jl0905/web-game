#ifndef PHYSICS_H
#define PHYSICS_H

#define PHYS_MAX_DIM 4

typedef struct {
    float accel;
    float max_speed;
    float friction;
} PhysicsParams;

void integrate(float dt, const float *input, float *pos, float *vel, int dim,
               const PhysicsParams *params);
void collide_bounds(float *pos, float *vel, int dim, const float *lo, const float *hi);

#endif
