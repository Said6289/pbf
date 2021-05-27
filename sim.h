#define MAX_THREAD_COUNT 64

#define PARTICLE_RADIUS 0.025f
#define PARTICLE_MASS 1.0f
#define PARTICLES_PER_AXIS 75
#define dt 0.008f

#define REST_DENSITY 1000.0f
#define RELAXATION 300.0f

#define H (PARTICLE_RADIUS * 6.0f)
#define H2 (H*H)
#define H6 (H*H*H*H*H*H)
#define H9 (H*H*H*H*H*H*H*H*H)

#define MAX_NEIGHBORS 512

#define WORLD_WIDTH 10.0f
#define WORLD_HEIGHT 10.0f

struct hash_grid_cell {
    int x, y;
    int *Data;
};

struct hash_grid {
    int Width;
    int Height;
    int CellCount;

    v2 WorldP;
    float CellDim;

    int ElementsPerCell;

    int *ElementCounts;
    int *Elements;
};

struct particle {
    v2 P0;
    v2 P;
    v2 V;
    float Density;
    float Pressure;

    int NeighborCount;
    int *Neighbors;
};

struct sim {
    int ParticleCount;
    particle *Particles;

    hash_grid HashGrid;
    v2 Gravity;
};
