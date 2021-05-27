#include "linalg.h"

#define PARTICLE_RADIUS 0.025f
#define PARTICLES_PER_AXIS 75
#define dt 0.008f

#define H (PARTICLE_RADIUS * 6.0f)
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

static void
Clear(hash_grid Grid)
{
    for (int i = 0; i < Grid.CellCount; ++i) {
        Grid.ElementCounts[i] = 0;
    }
}

static int
GetCellIndex(hash_grid Grid, hash_grid_cell Cell)
{
    int Index = Cell.y * Grid.Width + Cell.x;
    return Index;
}

static hash_grid_cell
GetCell(hash_grid Grid, int X, int Y)
{
    hash_grid_cell Cell = {};
    Cell.x = X;
    Cell.y = Y;
    int CellIndex = GetCellIndex(Grid, Cell);
    Cell.Data = &Grid.Elements[CellIndex * Grid.ElementsPerCell];
    return Cell;
}

static hash_grid_cell
GetCell(hash_grid Grid, v2 P)
{
    P = P - Grid.WorldP;

    int X = (int)(P.x / Grid.CellDim);
    int Y = (int)(P.y / Grid.CellDim);

    if (X < 0) X = 0;
    else if (X > Grid.Width - 1) X = Grid.Width - 1;

    if (Y < 0) Y = 0;
    else if (Y > Grid.Height - 1) Y = Grid.Height - 1;

    hash_grid_cell Cell = GetCell(Grid, X, Y);
    return Cell;
}

static bool
IsWithinBounds(hash_grid Grid, int X, int Y)
{
   bool IsInvalid = X < 0 || X >= Grid.Width || Y < 0 || Y >= Grid.Height;
   return !IsInvalid;
}

static int
GetElementCount(hash_grid Grid, hash_grid_cell Cell)
{
    int CellIndex = GetCellIndex(Grid, Cell);
    int Count = Grid.ElementCounts[CellIndex];
    return Count;
}

static void
AddElement(hash_grid Grid, v2 P, int Element)
{
    hash_grid_cell Cell = GetCell(Grid, P);
    int CellIndex = GetCellIndex(Grid, Cell);
    assert(Grid.ElementCounts[CellIndex] < Grid.ElementsPerCell);
    Cell.Data[Grid.ElementCounts[CellIndex]++] = Element;
}

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

static void
Simulate(sim *Sim)
{
    const float PARTiCLE_MASS = 1.0f;
    const float REST_DENSITY = 1000.0f; 
    const float RELAXATION = 300.0f;

    const float H2 = H*H;
    const float H6 = H*H*H*H*H*H;
    const float H9 = H*H*H*H*H*H*H*H*H;

    hash_grid HashGrid = Sim->HashGrid;
    int ParticleCount = Sim->ParticleCount;
    particle *Particles = Sim->Particles;

    Clear(HashGrid);
    for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex) {
        particle *P = &Particles[ParticleIndex];
        AddElement(HashGrid, P->P, ParticleIndex);

        P->P0 = P->P;
        P->V += Sim->Gravity * dt;
        P->P += P->V * dt;
    }

    for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex) {
        particle *Particle = &Particles[ParticleIndex];
        Particle->NeighborCount = 0;

        hash_grid_cell CenterCell = GetCell(HashGrid, Particle->P);
        for (int Row = 0; Row < 3; ++Row) {
            for (int Col = 0; Col < 3; ++Col) {
                int CellX = CenterCell.x - 1 + Col;
                int CellY = CenterCell.y - 1 + Row;
                if (!IsWithinBounds(HashGrid, CellX, CellY)) {
                    continue;
                }

                hash_grid_cell Cell = GetCell(HashGrid, CellX, CellY);
                int ElementCount = GetElementCount(HashGrid, Cell);

                for (int ElementIndex = 0;
                     ElementIndex < ElementCount;
                     ++ElementIndex)
                {
                    int OtherIndex = Cell.Data[ElementIndex];
                    particle Other = Particles[OtherIndex];

                    if (LengthSq(Other.P - Particle->P) < H2) {
                        assert(Particle->NeighborCount < MAX_NEIGHBORS);
                        Particle->Neighbors[Particle->NeighborCount++] = OtherIndex;
                    }
                }
            }
        }
    }

    for (int i = 0; i < ParticleCount; ++i) {
        particle *P = Particles + i;

        P->Density = 0;

        for (int ni = 0; ni < P->NeighborCount; ++ni) {
            particle *N = Particles + P->Neighbors[ni];

            float R2 = LengthSq(P->P - N->P);
            if (R2 < H2) {
                float A = H2 - R2;
                P->Density += PARTiCLE_MASS * (315.0f / (64.0f * (float)M_PI * H9)) * A * A * A;
            }
        }
    }

    for (int i = 0; i < ParticleCount; ++i) {
        particle *P = Particles + i;

        float SquaredGradSum = 0;
        v2 GradientOfI = {};

        for (int ni = 0; ni < P->NeighborCount; ++ni) {
            int j = P->Neighbors[ni];
            if (i == j) continue;

            particle *N = Particles + j;

            v2 R = P->P - N->P;
            float RLen = Length(R);

            if (RLen > 0 && RLen < H) {
                float A = H - RLen;
                A = (-45.0f / ((float)M_PI * H6)) * A * A;
                A /= RLen;
                v2 Gradient = A * R;

                Gradient *= (1.0f / REST_DENSITY);

                SquaredGradSum += Dot(Gradient, Gradient);
                GradientOfI += Gradient;
            }
        }

        float LambdaDenom = SquaredGradSum + Dot(GradientOfI, GradientOfI) + RELAXATION;
        P->Pressure = -(P->Density / REST_DENSITY - 1) / LambdaDenom;
    }

    for (int i = 0; i < ParticleCount; ++i) {
        particle *P = Particles + i;

        v2 DeltaP = {};

        for (int ni = 0; ni < P->NeighborCount; ++ni) {
            int j = P->Neighbors[ni];
            if (i == j) continue;

            particle *N = Particles + j;

            v2 R = P->P - N->P;
            float RLen = Length(R);

            if (RLen > 0 && RLen < H) {
                float A = H - RLen;
                A = (-45.0f / ((float)M_PI * H6)) * A * A;
                A /= RLen;
                v2 Gradient = A * R;

                DeltaP += (N->Pressure + P->Pressure) * Gradient;
            }
        }

        DeltaP *= (1.0f / REST_DENSITY);
        P->P += DeltaP;

        P->V = (P->P - P->P0) * (1.0f / dt);

        float Elasticity = 0.1f;

        if (P->P.x < -WORLD_WIDTH * 0.5f + PARTICLE_RADIUS) {
            P->P.x = -WORLD_WIDTH * 0.5f + PARTICLE_RADIUS;
            P->V.x = -P->V.x * Elasticity;
        } else if (P->P.x > WORLD_WIDTH * 0.5f - PARTICLE_RADIUS) {
            P->P.x = WORLD_WIDTH * 0.5f - PARTICLE_RADIUS;
            P->V.x = -P->V.x * Elasticity;
        }

        if (P->P.y < -WORLD_HEIGHT * 0.5f + PARTICLE_RADIUS) {
            P->P.y = -WORLD_HEIGHT * 0.5f + PARTICLE_RADIUS;
            P->V.y = -P->V.y * Elasticity;
        } else if (P->P.y > WORLD_HEIGHT * 0.5f - PARTICLE_RADIUS) {
            P->P.y = WORLD_HEIGHT * 0.5f - PARTICLE_RADIUS;
            P->V.y = -P->V.y * Elasticity;
        }
    }
}

static void
InitSim(sim *Sim)
{
    int ParticleCount = PARTICLES_PER_AXIS * PARTICLES_PER_AXIS;
    particle *Particles = (particle *)malloc(ParticleCount * sizeof(particle));

    float Gap = 0.00f;
    float Spacing = Gap + PARTICLE_RADIUS;
    for (int i = 0; i < ParticleCount; ++i) {
        float x = (i % PARTICLES_PER_AXIS) * Spacing;
        float y = (i / PARTICLES_PER_AXIS) * Spacing;

        x -= Spacing * PARTICLES_PER_AXIS * 0.5f;
        y -= Spacing * PARTICLES_PER_AXIS * 0.5f;

        Particles[i].P = V2(x, y);

        float V = 4.0f;
        Particles[i].V = V2(
                V * ((float)rand() / (float)RAND_MAX - 0.5f),
                V * ((float)rand() / (float)RAND_MAX - 0.5f));
        Particles[i].V = V2(0);

        Particles[i].Neighbors = (int *)malloc(MAX_NEIGHBORS * sizeof(int));
        Particles[i].NeighborCount = 0;
    }

    Sim->ParticleCount = ParticleCount;
    Sim->Particles = Particles;

    hash_grid Grid = {};
    Grid.WorldP = V2(WORLD_WIDTH, WORLD_HEIGHT) * -0.5f;
    Grid.CellDim = H;
    Grid.Width = WORLD_WIDTH / Grid.CellDim;
    Grid.Height = WORLD_HEIGHT / Grid.CellDim;
    Grid.CellCount = Grid.Width * Grid.Height;
    Grid.ElementsPerCell = MAX_NEIGHBORS;
    Grid.Elements = (int *)calloc(Grid.CellCount * Grid.ElementsPerCell, sizeof(int));
    Grid.ElementCounts = (int *)calloc(Grid.CellCount, sizeof(int));

    Sim->HashGrid = Grid;

    Sim->Gravity = V2(0, -9.81f);
}
