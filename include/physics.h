#ifndef PHYSICS_H
#define PHYSICS_H

#define PHYS_MAX_DIM 4

typedef struct {
    float accel;
    float max_speed;
    float friction;
} PhysicsParams;

typedef struct {
    float x;
    float y;
    float w;
    float h;
} RectF;

void integrate(float dt, const float *input, float *pos, float *vel, int dim,
               const PhysicsParams *params);
void collide_bounds(float *pos, float *vel, int dim, const float *lo, const float *hi);

// Push a `size`x`size` square (top-left at pos) out of `solid` if they overlap,
// zeroing the velocity component along the axis of penetration. Returns 1 if
// the square had to be moved.
int collide_solid(float pos[2], float vel[2], float size, const RectF *solid);

#endif
