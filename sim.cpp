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

    work_queue *Queue = &GlobalWorkQueue;
    ResetQueue(Queue);

    lambda_work Works[256];
    int WorkCount = 0;

    int TileSize = 128;
    int TileCount = (ParticleCount + TileSize - 1) / TileSize;

    for (int i = 0; i < TileCount; ++i) {
        assert(WorkCount < 256);
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

    pthread_mutex_init(&GlobalWorkQueue.Mutex, 0);
    pthread_cond_init(&GlobalWorkQueue.Cond, 0);
    GlobalWorkQueue.Works = (work_queue_entry *)malloc(512 * sizeof(work_queue_entry));
    GlobalWorkQueue.Size = 0;
    GlobalWorkQueue.DoneCount = 0;
    GlobalWorkQueue.Index = 0;

    int WorkerThreads = sysconf(_SC_NPROCESSORS_CONF) - 1;
    if (WorkerThreads > MAX_THREAD_COUNT - 1) {
        WorkerThreads = MAX_THREAD_COUNT - 1;
    }
    printf("Spawning %d worker threads...\n", WorkerThreads);
    for (int i = 0; i < WorkerThreads; ++i) {
        pthread_create(&Sim->Threads[i], 0, MarchingSquaresThread, 0);
    }
}
