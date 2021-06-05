static const char *VertexShaderCode = R"glsl(
    attribute vec2 position;
    void main()
    {
        gl_Position = vec4(position.x, position.y, 0.0, 1.0);
    }
)glsl";

static const char *FragmentShaderCode = R"glsl(
    void main()
    {
        gl_FragColor = vec4(0.0, 1.0, 1.0, 1.0);
    }
)glsl";

static void
InitializeOpenGL(opengl *OpenGL, hash_grid HashGrid, int ParticleCount, particle *Particles) {
    GLint status;

    GLuint VertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(VertexShader, 1, &VertexShaderCode, 0);
    glCompileShader(VertexShader);
    glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);

    GLuint FragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(FragmentShader, 1, &FragmentShaderCode, 0);
    glCompileShader(FragmentShader);
    glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &status);
    assert(status == GL_TRUE);

    GLuint ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, VertexShader);
    glAttachShader(ShaderProgram, FragmentShader);
    glLinkProgram(ShaderProgram);
    glGetProgramiv(ShaderProgram, GL_LINK_STATUS, &status);
    assert(status == GL_TRUE);

    OpenGL->ShaderProgram = ShaderProgram;
    glUseProgram(ShaderProgram);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    OpenGL->VBO = VBO;

    GLint PosAttrib = glGetAttribLocation(ShaderProgram, "position");
    glVertexAttribPointer(PosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
    glEnableVertexAttribArray(PosAttrib);

    OpenGL->VertexCount = 4 * 8192;
    OpenGL->Vertices = (float *)malloc(2 * sizeof(float) * OpenGL->VertexCount);

    OpenGL->GridW = 200;
    OpenGL->GridH = 200;
    OpenGL->Field = (float *)malloc((OpenGL->GridW + 1) * (OpenGL->GridH + 1) * sizeof(float));
}

static void
PushLine(opengl *OpenGL, int Index, v2 P0, v2 P1)
{
    if (Index * 2 <= OpenGL->VertexCount) {
        P0.x = 2 * P0.x / WORLD_WIDTH;
        P0.y = 2 * P0.y / WORLD_HEIGHT;

        P1.x = 2 * P1.x / WORLD_WIDTH;
        P1.y = 2 * P1.y / WORLD_HEIGHT;

        OpenGL->Vertices[2 * (Index * 2 + 0) + 0] = P0.x;
        OpenGL->Vertices[2 * (Index * 2 + 0) + 1] = P0.y;

        OpenGL->Vertices[2 * (Index * 2 + 1) + 0] = P1.x;
        OpenGL->Vertices[2 * (Index * 2 + 1) + 1] = P1.y;
    }
}

static v2
FieldLerp(v2 P0, float F0, v2 P1, float F1, float Threshold)
{
    assert(F0 >= Threshold && F1 < Threshold);

    float ResultX = (Threshold - F1) * (P1.x - P0.x) / (F1 - F0) + P1.x;
    float ResultY = (Threshold - F1) * (P1.y - P0.y) / (F1 - F0) + P1.y;

    v2 Result = {ResultX, ResultY};

    return Result;
}

static void
EvaluateFieldTile(void *Data)
{
    field_eval_work *Work = (field_eval_work *)Data;

    hash_grid HashGrid = Work->HashGrid;
    float CellW = Work->CellW;
    float CellH = Work->CellH;
    int GridW = Work->GridW;
    float *Field = Work->Field;
    particle *Particles = Work->Particles;

    int XStart = Work->XStart;
    int YStart = Work->YStart;
    int XEnd = Work->XEnd;
    int YEnd = Work->YEnd;

    float WorldW = WORLD_WIDTH;
    float WorldH = WORLD_HEIGHT;

    for (int XIndex = XStart; XIndex < XEnd; ++XIndex) {
        float X0 = -0.5f * WorldW + XIndex * CellW;

        for (int YIndex = YStart; YIndex < YEnd; ++YIndex) {
            float Y0 = -0.5f * WorldH + YIndex * CellH;

            float FieldValue = 0.0f;
            {
                v2 P = V2(X0, Y0);

                hash_grid_cell CenterCell = GetCell(HashGrid, P);

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
                            particle Particle = Particles[Cell.Data[ElementIndex]];

                            float MX = Particle.P.x;
                            float MY = Particle.P.y;
                            float MR = PARTICLE_RADIUS;

                            float dX = MX - P.x;
                            float dY = MY - P.y;
                            float R2 = dX*dX + dY*dY;

                            FieldValue += (MR * MR) / (R2);
                        }
                    }
                }
            }
            Field[XIndex + YIndex * (GridW + 1)] = FieldValue;
        }
    }
}

static void
RenderMarchingSquares(opengl *OpenGL, sim *Sim)
{
    hash_grid HashGrid = Sim->HashGrid;
    int ParticleCount = Sim->ParticleCount;
    particle *Particles = Sim->Particles;

    Clear(HashGrid);
    for (int ParticleIndex = 0; ParticleIndex < ParticleCount; ++ParticleIndex) {
        particle Particle = Particles[ParticleIndex];
        AddElement(HashGrid, Particle.P, ParticleIndex);
    }

    float WorldW = WORLD_WIDTH;
    float WorldH = WORLD_HEIGHT;

    float Threshold = 0.2f;

    int GridW = OpenGL->GridW;
    int GridH = OpenGL->GridH;
    float CellW = WorldW / GridW;
    float CellH = WorldH / GridH;
    float *Field = OpenGL->Field;

    int TileSize = 32;
    int TileCountX = ((GridW + 1) + TileSize - 1) / TileSize;
    int TileCountY = ((GridH + 1) + TileSize - 1) / TileSize;

    int TileCount = TileCountX * TileCountY;

    work_queue *Queue = &GlobalWorkQueue;
    ResetQueue(Queue);

    field_eval_work Works[128];
    int WorkCount = 0;

    for (int TileX = 0; TileX < TileCountX; ++TileX) {
        for (int TileY = 0; TileY < TileCountY; ++TileY) {
            assert(WorkCount < 128);
            field_eval_work *Work = Works + WorkCount++;

            Work->HashGrid = HashGrid;
            Work->CellW = CellW;
            Work->CellH = CellH;
            Work->GridW = GridW;
            Work->Field = Field;
            Work->Particles = Particles;

            Work->XStart = TileX * TileSize;
            Work->YStart = TileY * TileSize;

            Work->XEnd = Work->XStart + TileSize;
            Work->YEnd = Work->YStart + TileSize;

            if (Work->XEnd > (GridW + 1)) Work->XEnd = (GridW + 1);
            if (Work->YEnd > (GridH + 1)) Work->YEnd = (GridH + 1);

            AddEntry(Queue, Work, EvaluateFieldTile);
        }
    }

    FinishWork(Queue);

    int LineIndex = 0;

    float X0 = -0.5f * WorldW;

    for (int XIndex = 0; XIndex < GridW; ++XIndex) {

        float Y0 = -0.5f * WorldH;

        for (int YIndex = 0; YIndex < GridH; ++YIndex) {

            float F[4];
            v2 Corners[4];

            Corners[0] = {X0, Y0};
            Corners[1] = {X0 + CellW, Y0};
            Corners[2] = {X0 + CellW, Y0 + CellH};
            Corners[3] = {X0, Y0 + CellH};

            char FieldIndex = 0;
            for (int CornerIndex = 0; CornerIndex < 4; ++CornerIndex) {
                int Bit0 = (CornerIndex & 1);
                int Bit1 = (CornerIndex >> 1);

                int XOffset = (Bit0 + Bit1) & 1;
                int YOffset = Bit1;

                F[CornerIndex] = Field[(XIndex + XOffset) + (YIndex + YOffset) * (GridW + 1)];
                if (F[CornerIndex] >= Threshold) {
                    FieldIndex |= (1 << (3 - CornerIndex));
                }
            }

            assert(FieldIndex >= 0 && FieldIndex < 16);

            v2 P0 = {};
            v2 P1 = {};

#define L(I0, I1) FieldLerp(Corners[I0], F[I0], Corners[I1], F[I1], Threshold)
#define LINE PushLine(OpenGL, LineIndex++, P0, P1)

            switch (FieldIndex) {
                case 5: {
                    P0 = L(1, 0);
                    P1 = L(3, 0);
                    LINE;

                    P0 = L(1, 2);
                    P1 = L(3, 2);
                    LINE;
                } break;

                case 6: {
                    P0 = L(1, 0);
                    P1 = L(2, 3);
                    LINE;
                } break;

                case 7: {
                    P0 = L(1, 0);
                    P1 = L(3, 0);
                    LINE;
                } break;

                case 8: {
                    P0 = L(0, 1);
                    P1 = L(0, 3);
                    LINE;
                } break;

                case 9: {
                    P0 = L(0, 1);
                    P1 = L(3, 2);
                    LINE;
                } break;

                case 10: {
                    P0 = L(0, 1);
                    P1 = L(2, 1);
                    LINE;

                    P0 = L(2, 3);
                    P1 = L(0, 3);
                    LINE;
                } break;

                case 11: {
                    P0 = L(0, 1);
                    P1 = L(2, 1);
                    LINE;
                } break;

                case 4: {
                    P0 = L(1, 0);
                    P1 = L(1, 2);
                    LINE;
                } break;

                case 3: {
                    P0 = L(3, 0);
                    P1 = L(2, 1);
                    LINE;
                } break;

                case 12: {
                    P0 = L(0, 3);
                    P1 = L(1, 2);
                    LINE;
                } break;

                case 13: {
                    P0 = L(1, 2);
                    P1 = L(3, 2);
                    LINE;
                } break;

                case 2: {
                    P0 = L(2, 1);
                    P1 = L(2, 3);
                    LINE;
                } break;

                case 14: {
                    P0 = L(0, 3);
                    P1 = L(2, 3);
                    LINE;
                } break;

                case 1: {
                    P0 = L(3, 0);
                    P1 = L(3, 2);
                    LINE;
                } break;
            }

            Y0 += CellH;
        }

        X0 += CellW;
    }

    glBindBuffer(GL_ARRAY_BUFFER, OpenGL->VBO);
    glBufferData(GL_ARRAY_BUFFER, LineIndex * 2 * 2 * sizeof(float), OpenGL->Vertices, GL_STREAM_DRAW);

    glUseProgram(OpenGL->ShaderProgram);
    glDrawArrays(GL_LINES, 0, LineIndex * 2);
}

static void
Render(sim *Sim, opengl *OpenGL, float Width, float Height)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    float AspectRatio = (Width / Height);

    particle *Particles = Sim->Particles;

    float WorldW = WORLD_WIDTH;
    float WorldH = WORLD_HEIGHT;

    float WorldAspectRatio = WorldW / WorldH;

    float X = 0.0f;
    float Y = 0.0f;
    float GlW = 0.0f;
    float GlH = 0.0f;

    v2 GlUnitsPerMeter = {};

    if (WorldAspectRatio > AspectRatio)
    {
        GlW = 2.0f;
        GlH = ((GlW * (WorldH / WorldW)) * AspectRatio);

        X = -1.0f;

        float Empty = 2.0f - GlH;
        float HalfEmpty = Empty * 0.5f;

        Y = HalfEmpty - 1.0f;
    }
    else
    {
        GlH = 2.0f;
        GlW = ((GlH * (WorldW / WorldH)) / AspectRatio);

        Y = -1.0f;

        float Empty = 2.0f - GlW;
        float HalfEmpty = Empty * 0.5f;

        X = HalfEmpty - 1.0f;
    }

    X = 0.0f;
    Y = 0.0f;
    GlW = 2.0f;
    GlH = 2.0f;
    GlUnitsPerMeter.x = GlW / WorldW;
    GlUnitsPerMeter.y = GlH / WorldH;

    RenderMarchingSquares(OpenGL, Sim);
}
