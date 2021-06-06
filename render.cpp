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

static const char *ComputeShaderCode = R"glsl(
    #version 310 es

    precision mediump image2D;

    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

    layout(r32f, binding = 0) uniform image2D Field;

    void main()
    {
        uvec2 Pixel = gl_WorkGroupID.xy;
        uvec2 GridSize = gl_NumWorkGroups.xy;

        // TODO(said): Add #defines here
        float WORLD_WIDTH = 10.0;
        float WORLD_HEIGHT = 10.0;
        vec2 WorldSize = vec2(WORLD_WIDTH, WORLD_HEIGHT);
        float ParticleRadius = 0.0125;
        float H = ParticleRadius * 12.0;
        ivec2 HashGridSize = ivec2(WorldSize / H);

        vec2 P = (vec2(Pixel) / vec2(GridSize - uvec2(1)) - 0.5) * WorldSize;

        float FieldValue = 0.0;

        ivec2 CenterCell = ivec2((P + 0.5 * WorldSize) / H);

        for (int Row = 0; Row < 3; ++Row) {
            for (int Col = 0; Col < 3; ++Col) {
                ivec2 Cell = CenterCell - ivec2(1) + ivec2(Col, Row);

                bool IsInvalid = Cell.x < 0 || Cell.x >= HashGridSize.x || Cell.y < 0 || Cell.y >= HashGridSize.y;
                if (IsInvalid) {
                    continue;
                }

                // int ElementCount = GetElementCount(HashGrid, Cell);
                int ElementCount = 1;
                for (int ElementIndex = 0;
                     ElementIndex < ElementCount;
                     ++ElementIndex)
                {
                    vec2 ParticleP = vec2(Cell) * H - 0.5f * WorldSize;
                    vec2 R = ParticleP - P;
                    float Radius = ParticleRadius;
                    FieldValue += (Radius * Radius) / dot(R, R);
                }
            }
        }

        imageStore(Field, ivec2(Pixel), vec4(FieldValue, 0, 0, 0));
    }
)glsl";

static GLuint
CompileShader(GLuint ShaderType, const char *Code)
{
    GLuint Shader = glCreateShader(ShaderType);
    glShaderSource(Shader, 1, &Code, 0);
    glCompileShader(Shader);

    GLint status = 0;
    glGetShaderiv(Shader, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        char Buffer[4096];
        int BufferSize = 0;
        glGetShaderInfoLog(Shader, ArrayCount(Buffer) - 1, &BufferSize, Buffer);
        Buffer[BufferSize] = 0;

        printf("Error compiling shader:\n");
        printf("%s\n", Buffer);
    }

    return Shader;
}

static void
LinkProgram(GLuint Program)
{
    glLinkProgram(Program);

    GLint status = 0;
    glGetProgramiv(Program, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        char Buffer[4096];
        int BufferSize = 0;
        glGetProgramInfoLog(Program, ArrayCount(Buffer) - 1, &BufferSize, Buffer);
        Buffer[BufferSize] = 0;

        printf("Error linking program:\n");
        printf("%s\n", Buffer);
    }
}

#define AssertGLError() AssertGLError_(__FILE__, __LINE__)

static void
AssertGLError_(char *File, int Line)
{
    GLenum Code = glGetError();
    const char *CodeName = 0;
    switch (Code) {
        case GL_NO_ERROR: CodeName = "GL_NO_ERROR"; break;
        case GL_INVALID_ENUM: CodeName = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: CodeName = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: CodeName = "GL_INVALID_OPERATION"; break;
        case GL_STACK_OVERFLOW: CodeName = "GL_INVALID_OVERFLOW"; break;
        case GL_STACK_UNDERFLOW: CodeName = "GL_INVALID_UNDERFLOW"; break;
        case GL_OUT_OF_MEMORY: CodeName = "GL_OUT_OF_MEMORY"; break;
        default: CodeName = "UNKNOWN"; break;
    }
    if (Code != GL_NO_ERROR) {
        printf("OpenGL error %s:%d %s\n", File, Line, CodeName);
        assert(!"OpenGL error encountered");
    }
}

static void
InitializeOpenGL(opengl *OpenGL, hash_grid HashGrid, int ParticleCount, particle *Particles)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    GLuint VertexShader = CompileShader(GL_VERTEX_SHADER, VertexShaderCode);
    GLuint FragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShaderCode);

    GLuint ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, VertexShader);
    glAttachShader(ShaderProgram, FragmentShader);
    LinkProgram(ShaderProgram);

    OpenGL->ShaderProgram = ShaderProgram;
    glUseProgram(ShaderProgram);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    OpenGL->VBO = VBO;

    GLint PosAttrib = glGetAttribLocation(ShaderProgram, "position");
    glVertexAttribPointer(PosAttrib, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
    glEnableVertexAttribArray(PosAttrib);

    OpenGL->VertexCount = 8 * 8192;
    OpenGL->Vertices = (float *)malloc(2 * sizeof(float) * OpenGL->VertexCount);

    OpenGL->GridW = 200;
    OpenGL->GridH = 200;
    OpenGL->Field = (float *)malloc((OpenGL->GridW + 1) * (OpenGL->GridH + 1) * sizeof(float));

    GLuint ComputeShader = CompileShader(GL_COMPUTE_SHADER, ComputeShaderCode);
    OpenGL->ComputeShaderProgram = glCreateProgram();
    glAttachShader(OpenGL->ComputeShaderProgram, ComputeShader);
    LinkProgram(OpenGL->ComputeShaderProgram);

    glGenTextures(1, &OpenGL->FieldTexture);
    glBindTexture(GL_TEXTURE_2D, OpenGL->FieldTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, OpenGL->GridW + 1, OpenGL->GridH + 1);
    glBindImageTexture(0, OpenGL->FieldTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glGenFramebuffers(1, &OpenGL->FieldFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, OpenGL->FieldFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OpenGL->FieldTexture, 0);

    AssertGLError();
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

    float Threshold = 1.0f;

    int GridW = OpenGL->GridW;
    int GridH = OpenGL->GridH;
    float CellW = WorldW / GridW;
    float CellH = WorldH / GridH;
    float *Field = OpenGL->Field;

    glUseProgram(OpenGL->ComputeShaderProgram);
    glDispatchCompute(GridW + 1, GridH + 1, 1);
    // TODO(said): Barrier?

    glBindFramebuffer(GL_READ_FRAMEBUFFER, OpenGL->FieldFramebuffer);
    glReadPixels(0, 0, GridW + 1, GridH + 1, GL_RED, GL_FLOAT, OpenGL->Field);

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
