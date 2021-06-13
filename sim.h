#define MAX_THREAD_COUNT 64

#define PARTICLE_RADIUS 0.0125f
#define PARTICLE_MASS 1.0f
#define PARTICLES_PER_AXIS 90
#define dt 0.008f

#define REST_DENSITY 1000.0f
#define RELAXATION 300.0f

#define H (PARTICLE_RADIUS * 16.0f)
#define H2 (H*H)
#define H6 (H*H*H*H*H*H)
#define H9 (H*H*H*H*H*H*H*H*H)

#define MAX_NEIGHBORS 128

#define WORLD_WIDTH 10.0f
#define WORLD_HEIGHT 10.0f

struct hash_grid_cell {
    int x;
    int y;
    int Index;
    int ParticleIndex;
};

struct hash_grid {
    v2 WorldP;
    float CellDim;

    int Width;
    int Height;
    int CellCount;

    int *CellStart;
};

struct particle {
    v2 P0;
    v2 P;
    v2 V;
    float Density;
    float Pressure;

    int CellIndex;
    int NeighborCount;
    int *Neighbors;
};

struct sim {
    float Time;

    int ParticleCount;
    particle *Particles;

    hash_grid HashGrid;
    v2 Gravity;

    bool Pulling;
    v2 PullPoint;
};
