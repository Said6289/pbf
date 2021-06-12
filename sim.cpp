static int
GetCellIndex(hash_grid Grid, int x, int y)
{
    int Index = x + y * Grid.Width;
    return Index;
}

static int
GetCellIndex(hash_grid Grid, v2 P)
{
    P = P - Grid.WorldP;

    int x = Clamp(0, (int)(P.x / Grid.CellDim), Grid.Width - 1);
    int y = Clamp(0, (int)(P.y / Grid.CellDim), Grid.Height - 1);

    int Index = x + y * Grid.Width;
    assert(Index >= 0 && Index < Grid.CellCount);

    return Index;
}

static hash_grid_cell
GetCell(hash_grid Grid, int x, int y)
{
    int CellIndex = GetCellIndex(Grid, x, y);

    hash_grid_cell Cell = {};
    Cell.x = x;
    Cell.y = y;
    Cell.Index = CellIndex;
    Cell.ParticleIndex = Grid.CellStart[CellIndex];

    return Cell;
}

static hash_grid_cell
GetCell(hash_grid Grid, v2 P)
{
    P = P - Grid.WorldP;

    int x = Clamp(0, (int)(P.x / Grid.CellDim), Grid.Width - 1);
    int y = Clamp(0, (int)(P.y / Grid.CellDim), Grid.Height - 1);

    hash_grid_cell Cell = GetCell(Grid, x, y);
    return Cell;
}

static bool
IsWithinBounds(hash_grid Grid, int x, int y)
{
   bool IsInvalid = x < 0 || x >= Grid.Width || y < 0 || y >= Grid.Height;
   return !IsInvalid;
}

static int
ParticleCellIndexCompare(const void *A, const void *B)
{
    particle *ParticleA = (particle *)A;
    particle *ParticleB = (particle *)B;
    return ParticleA->CellIndex - ParticleB->CellIndex;
}

static void
ConstructSortedGrid(int ParticleCount, particle *Particles, hash_grid HashGrid)
{
    // TODO(said): Implement my own sort, since the loop
    // can probably be merged with the sort?
    qsort(Particles, ParticleCount, sizeof(particle), ParticleCellIndexCompare);

    for (int i = 0; i < HashGrid.CellCount; ++i) {
        HashGrid.CellStart[i] = -1;
    }

    int CurrentCellIndex = -1;
    for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex) {
        particle Particle = Particles[ParticleIndex];

        if (Particle.CellIndex != CurrentCellIndex) {
            CurrentCellIndex = Particle.CellIndex;
            HashGrid.CellStart[CurrentCellIndex] = ParticleIndex;
        }
    }
}

struct lambda_work {
    int ParticleIndex;
    int ParticleEnd;

    particle *Particles;
};

static void
ComputeLambda(void *Data)
{
    lambda_work *Work = (lambda_work *)Data;

    int ParticleIndex = Work->ParticleIndex;
    int ParticleEnd = Work->ParticleEnd;
    particle *Particles = Work->Particles;

    for (int i = ParticleIndex; i < ParticleEnd; ++i) {
        particle *P = Particles + i;

        P->Density = 0;
        float SquaredGradSum = 0;
        v2 GradientOfI = {};

        for (int ni = 0; ni < P->NeighborCount; ++ni) {
            int j = P->Neighbors[ni];
            particle *N = Particles + j;

            float R2 = LengthSq(P->P - N->P);
            if (R2 < H2) {
                float A = H2 - R2;
                P->Density += PARTICLE_MASS * (315.0f / (64.0f * (float)M_PI * H9)) * A * A * A;
            }

            if (i != j) {
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
        }

        float LambdaDenom = SquaredGradSum + Dot(GradientOfI, GradientOfI) + RELAXATION;
        P->Pressure = -(P->Density / REST_DENSITY - 1) / LambdaDenom;
    }
}

static void
Simulate(sim *Sim)
{
    hash_grid HashGrid = Sim->HashGrid;
    int ParticleCount = Sim->ParticleCount;
    particle *Particles = Sim->Particles;

    for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex) {
        particle *P = &Particles[ParticleIndex];

        P->P0 = P->P;

        if (Sim->Pulling) {
            P->V += (Sim->PullPoint - P->P) * 3.0f * dt;
        }
        P->V += Sim->Gravity * dt;

        { // NOTE(said): Waves
            v2 WaveP = V2(fmodf(2.0f * Sim->Time, WORLD_WIDTH), 0);
            WaveP.x -= WORLD_WIDTH * 0.5f;

            float D = P->P.x - WaveP.x;
            D /= 0.125f * WORLD_WIDTH;

            if (D > 0 && D < 1) {
                P->V.x += 10.0f * dt;
            }
        }

        P->P += P->V * dt;

        P->CellIndex = GetCellIndex(HashGrid, P->P);
    }

    ConstructSortedGrid(ParticleCount, Particles, HashGrid);

#define BRUTE_FORCE 0
    for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex) {
        particle *Particle = &Particles[ParticleIndex];
        Particle->NeighborCount = 0;

#if !BRUTE_FORCE
        hash_grid_cell CenterCell = GetCell(HashGrid, Particle->P);
        for (int Row = 0; Row < 3; ++Row) {
            for (int Col = 0; Col < 3; ++Col) {
                int CellX = CenterCell.x - 1 + Col;
                int CellY = CenterCell.y - 1 + Row;
                if (!IsWithinBounds(HashGrid, CellX, CellY)) {
                    continue;
                }

                int CellIndex = GetCellIndex(HashGrid, CellX, CellY);

                for (int OtherIndex = HashGrid.CellStart[CellIndex];
                     OtherIndex != -1 && CellIndex == Particles[OtherIndex].CellIndex;
                     ++OtherIndex)
                {
                    particle Other = Particles[OtherIndex];

                    if (LengthSq(Other.P - Particle->P) < H2) {
                        assert(Particle->NeighborCount < MAX_NEIGHBORS);
                        Particle->Neighbors[Particle->NeighborCount++] = OtherIndex;
                    }
                }
            }
        }
#endif

#if BRUTE_FORCE 
        for (int OtherIndex = 0; OtherIndex < ParticleCount; ++OtherIndex) {
            if (LengthSq(Particles[OtherIndex].P - Particle->P) < H2) {
                assert(Particle->NeighborCount < MAX_NEIGHBORS);
                Particle->Neighbors[Particle->NeighborCount++] = OtherIndex;
            }
        }
#endif
    }

#undef BRUTE_FORCE

    work_queue *Queue = &GlobalWorkQueue;
    ResetQueue(Queue);

    lambda_work Works[512];
    int WorkCount = 0;

    int TileSize = 64;
    int TileCount = (ParticleCount + TileSize - 1) / TileSize;

    for (int i = 0; i < TileCount; ++i) {
        assert(WorkCount < 512);
        lambda_work *Work = Works + WorkCount++;

        Work->ParticleIndex = i * TileSize;
        Work->ParticleEnd = Work->ParticleIndex + TileSize;
        if (Work->ParticleEnd > ParticleCount) {
            Work->ParticleEnd = ParticleCount;
        }
        Work->Particles = Particles;

        AddEntry(Queue, Work, ComputeLambda);
    }

    FinishWork(Queue);

    for (int i = 0; i < ParticleCount; ++i) {
        particle *P = Particles + i;

        v2 DeltaP = {};

        for (int ni = 0; ni < P->NeighborCount; ++ni) {
            int j = P->Neighbors[ni];
            if (i == j) continue;

            particle *N = Particles + j;

            v2 R = P->P - N->P;
            float RLen = Length(R);
            float R2 = LengthSq(R);

            if (RLen > 0 && RLen < H) {
                float A = H - RLen;
                A = (-45.0f / ((float)M_PI * H6)) * A * A;
                A /= RLen;
                v2 Gradient = A * R;

                const float k = 0.1f;
                A = H2 - R2;
                float Scorr = 0;
                if (A > 0) {
                    float Numerator = (315.0f / (64.0f * (float)M_PI * H9)) * A * A * A;
                    A = 0.3f * H;
                    float Denomerator = (315.0f / (64.0f * (float)M_PI * H9)) * A * A * A;
                    Scorr = Numerator / Denomerator;
                }

                Scorr *= Scorr;
                Scorr *= Scorr;
                Scorr *= -k;

                DeltaP += (N->Pressure + P->Pressure + Scorr) * Gradient;
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

    Sim->Time += dt;
}

static void
InitSim(sim *Sim)
{
    int ParticleCount = PARTICLES_PER_AXIS * PARTICLES_PER_AXIS;
    particle *Particles = (particle *)malloc(ParticleCount * sizeof(particle));

    float Gap = 0.1f;
    float Spacing = Gap + 2.0 * PARTICLE_RADIUS;
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

    printf("Simulating %d particles...\n", Sim->ParticleCount);

    hash_grid Grid = {};

    Grid.WorldP = V2(WORLD_WIDTH, WORLD_HEIGHT) * -0.5f;
    Grid.CellDim = H;

    Grid.Width = WORLD_WIDTH / Grid.CellDim;
    Grid.Height = WORLD_HEIGHT / Grid.CellDim;
    Grid.CellCount = Grid.Width * Grid.Height;
    Grid.CellStart = (int *)calloc(Grid.CellCount, sizeof(int));

    Sim->HashGrid = Grid;

    Sim->Gravity = V2(0, -9.81f);
}
