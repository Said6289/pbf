static const char *VertexShaderCode = R"glsl(
    #version 310 es

    precision mediump float;

    layout(location = 0) in vec2 position;
    layout(location = 1) in vec2 uv;

    void main()
    {
        gl_Position = vec4(position.x, position.y, 0.0, 1.0);
    }
)glsl";

static const char *FragmentShaderCode = R"glsl(
    #version 310 es

    precision mediump float;

    out vec4 FragColor;

    void main()
    {
        FragColor = vec4(0.0, 1.0, 1.0, 1.0);
    }
)glsl";

static const char *TextVertShaderCode = R"glsl(
    #version 310 es

    precision mediump float;

    layout(location = 0) in vec2 in_Position;
    layout(location = 1) in vec2 in_UV;

    uniform vec2 Resolution;

    smooth out vec2 UV;

    void main()
    {
        UV = in_UV;
        vec2 P = 2.0 * (in_Position / Resolution) - 1.0;
        P.y = -P.y;
        gl_Position = vec4(P, 0.0, 1.0);
    }
)glsl";

static const char *TextFragShaderCode = R"glsl(
    #version 310 es

    precision mediump float;

    smooth in vec2 UV;
    out vec4 FragColor;

    uniform sampler2D Font;

    void main()
    {
        vec4 Alpha = texture(Font, UV);
        FragColor = vec4(0.0, 1.0, 1.0, Alpha.r);
    }
)glsl";

static const char *TextureVertShaderCode = R"glsl(
    #version 310 es

    precision mediump float;

    layout(location = 0) in vec2 in_Position;
    layout(location = 1) in vec2 in_UV;

    smooth out vec2 UV;

    void main()
    {
        UV = in_UV;
        gl_Position = vec4(in_Position, 0.0, 1.0);
    }
)glsl";

static const char *TextureFragShaderCode = R"glsl(
    #version 310 es

    precision mediump float;

    smooth in vec2 UV;
    out vec4 FragColor;

    uniform sampler2D Texture;

    void main()
    {
        float Field = texture(Texture, UV).r;

        float Threshold = 0.2;
        float D = 0.05;

        float Edge0 = Threshold - D;
        float Edge1 = Threshold + D;
        FragColor = vec4(0.0, vec2(smoothstep(Edge0, Edge1, Field)), 1.0);
    }
)glsl";

static const char *ComputeShaderCode = R"glsl(
    #version 310 es

    precision mediump image2D;
    precision mediump image3D;

    layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

    layout(r32f, binding = 0) uniform writeonly image2D Field;
    layout(rgba32f, binding = 1) uniform readonly image2D HashGrid;

    void main()
    {
        uvec2 Pixel = gl_WorkGroupID.xy;
        uvec2 GridSize = gl_NumWorkGroups.xy;

        // TODO(said): Add #defines or here
        float WORLD_WIDTH = 10.0;
        float WORLD_HEIGHT = 10.0;
        vec2 WorldSize = vec2(WORLD_WIDTH, WORLD_HEIGHT);
        float ParticleRadius = 0.0125;
        float H = ParticleRadius * 12.0;
        ivec2 HashGridSize = ivec2(WorldSize / H);
        vec2 CellSize = WorldSize / vec2(GridSize);

        vec2 P = -0.5f * WorldSize + vec2(Pixel) * CellSize;

        float FieldValue = 0.0;

        ivec2 CenterCell = ivec2((P + 0.5 * WorldSize) / H);

        for (int Row = 0; Row < 3; ++Row) {
            for (int Col = 0; Col < 3; ++Col) {
                ivec2 Cell = CenterCell - ivec2(1) + ivec2(Col, Row);

                bool IsInvalid = Cell.x < 0 || Cell.x >= HashGridSize.x || Cell.y < 0 || Cell.y >= HashGridSize.y;
                if (IsInvalid) {
                    continue;
                }

                for (int i = 0; i < 64; ++i) {
                    vec4 ParticleP = imageLoad(HashGrid, ivec2(Cell.x + Cell.y * HashGridSize.x, i));
                    if (ParticleP.a < 1.0) {
                        break;
                    }

                    vec2 D = ParticleP.xy - P;
                    float R = ParticleP.z;
                    float DistSq = dot(D, D);
                    FieldValue += (R * R) / DistSq;
                }
            }
        }

        imageStore(Field, ivec2(Pixel), vec4(FieldValue, 0, 0, 0));
    }
)glsl";

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

static font_info
InitFontTexture(opengl *OpenGL)
{
    font_info Result = {};

    char *FilePath = "font.ttf";
    FILE *File = fopen(FilePath, "rb");
    if (!File) {
        return Result;
    }

    fseek(File, 0, SEEK_END);
    size_t FileSize = ftell(File);
    fseek(File, 0, SEEK_SET);

    uint8_t *FileBuffer = (uint8_t *)malloc(FileSize * sizeof(uint8_t));
    fread(FileBuffer, 1, FileSize, File);

    fclose(File);

    int StartChar = 0x20;
    int NumChars = 0x7E - 0x20;

    Result.PixelHeight = 16;
    Result.Width = 256;
    Result.Height = 256;
    Result.FontBitmap = (uint8_t *)malloc(Result.Width * Result.Height * sizeof(uint8_t));
    Result.Chars = (stbtt_bakedchar *)malloc(NumChars * sizeof(stbtt_bakedchar));

    int Rows = stbtt_BakeFontBitmap(
            FileBuffer, 0, Result.PixelHeight, 
            Result.FontBitmap, Result.Width, Result.Height,
            0x20, 0x7E - 0x20,
            Result.Chars);

    float Descent, LineGap;
    stbtt_GetScaledFontVMetrics(FileBuffer, 0, Result.PixelHeight, &Result.Ascent, &Descent, &LineGap);

    assert(Rows > 0);

    free(FileBuffer);

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &OpenGL->FontTexture);
    glBindTexture(GL_TEXTURE_2D, OpenGL->FontTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, Result.Width, Result.Height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, Result.FontBitmap);

    return Result;
}

static GLuint
CompileShader(GLuint ShaderType, const char *Code)
{
    GLuint Shader = glCreateShader(ShaderType);

    GLint CodeSize = strlen(Code);
    glShaderSource(Shader, 1, &Code, &CodeSize);
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

static void
InitializeOpenGL(opengl *OpenGL, hash_grid HashGrid, int ParticleCount, particle *Particles)
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);

    GLuint VertexShader = CompileShader(GL_VERTEX_SHADER, VertexShaderCode);
    GLuint FragmentShader = CompileShader(GL_FRAGMENT_SHADER, FragmentShaderCode);
    OpenGL->ShaderProgram = glCreateProgram();
    glAttachShader(OpenGL->ShaderProgram, VertexShader);
    glAttachShader(OpenGL->ShaderProgram, FragmentShader);
    LinkProgram(OpenGL->ShaderProgram);

    GLuint TextureVertShader = CompileShader(GL_VERTEX_SHADER, TextureVertShaderCode);
    GLuint TextureFragShader = CompileShader(GL_FRAGMENT_SHADER, TextureFragShaderCode);
    OpenGL->TextureProgram = glCreateProgram();
    glAttachShader(OpenGL->TextureProgram, TextureVertShader);
    glAttachShader(OpenGL->TextureProgram, TextureFragShader);
    LinkProgram(OpenGL->TextureProgram);

    GLuint VBO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    OpenGL->VBO = VBO;

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const GLvoid *)offsetof(vertex, P));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (const GLvoid *)offsetof(vertex, UV));
    glEnableVertexAttribArray(1);

    OpenGL->VertexCapacity = 6 * 8192;
    OpenGL->VertexSize = 0;
    OpenGL->Vertices = (vertex *)malloc(sizeof(vertex) * OpenGL->VertexCapacity);

    OpenGL->GridW = 200;
    OpenGL->GridH = 200;
    OpenGL->Field = (float *)malloc((OpenGL->GridW + 1) * (OpenGL->GridH + 1) * sizeof(float));
    OpenGL->HashGridData = (float *)malloc(HashGrid.Width * HashGrid.Height * HashGrid.ElementsPerCell * 4 * sizeof(float));

    GLuint ComputeShader = CompileShader(GL_COMPUTE_SHADER, ComputeShaderCode);
    OpenGL->ComputeShaderProgram = glCreateProgram();
    glAttachShader(OpenGL->ComputeShaderProgram, ComputeShader);
    LinkProgram(OpenGL->ComputeShaderProgram);

    GLuint TextVertShader = CompileShader(GL_VERTEX_SHADER, TextVertShaderCode);
    GLuint TextFragShader = CompileShader(GL_FRAGMENT_SHADER, TextFragShaderCode);
    OpenGL->TextProgram = glCreateProgram();
    glAttachShader(OpenGL->TextProgram, TextVertShader);
    glAttachShader(OpenGL->TextProgram, TextFragShader);
    LinkProgram(OpenGL->TextProgram);

    OpenGL->ResolutionUniform = glGetUniformLocation(OpenGL->TextProgram, "Resolution");

    glGenTextures(1, &OpenGL->HashGridTexture);
    glBindTexture(GL_TEXTURE_2D, OpenGL->HashGridTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, HashGrid.Width * HashGrid.Height, HashGrid.ElementsPerCell);
    glBindImageTexture(1, OpenGL->HashGridTexture, 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

    glGenTextures(1, &OpenGL->FieldTexture);
    glBindTexture(GL_TEXTURE_2D, OpenGL->FieldTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R32F, OpenGL->GridW + 1, OpenGL->GridH + 1);
    glBindImageTexture(0, OpenGL->FieldTexture, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_R32F);

    glGenFramebuffers(1, &OpenGL->FieldFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, OpenGL->FieldFramebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OpenGL->FieldTexture, 0);

    OpenGL->Font = InitFontTexture(OpenGL);

    AssertGLError();
}

static void
PushVertex(opengl *OpenGL, v2 P, v2 UV = V2(0, 0))
{
    bool HasSpace = OpenGL->VertexSize < OpenGL->VertexCapacity;
    //assert(HasSpace);
    if (HasSpace) {
        OpenGL->Vertices[OpenGL->VertexSize].P = P;
        OpenGL->Vertices[OpenGL->VertexSize].UV = UV;
        ++OpenGL->VertexSize;
    }
}

static void
PushLine(opengl *OpenGL, v2 P0, v2 P1)
{
    P0.x = 2 * P0.x / WORLD_WIDTH;
    P0.y = 2 * P0.y / WORLD_HEIGHT;

    P1.x = 2 * P1.x / WORLD_WIDTH;
    P1.y = 2 * P1.y / WORLD_HEIGHT;

    PushVertex(OpenGL, P0);
    PushVertex(OpenGL, P1);
}

static void
PushQuad(opengl *OpenGL, v2 MinCorner, v2 MaxCorner, v2 MinUV, v2 MaxUV)
{
    v2 P0 = V2(MinCorner.x, MinCorner.y);
    v2 P1 = V2(MaxCorner.x, MinCorner.y);
    v2 P2 = V2(MaxCorner.x, MaxCorner.y);
    v2 P3 = V2(MinCorner.x, MaxCorner.y);

    PushVertex(OpenGL, P0, V2(MinUV.x, MinUV.y));
    PushVertex(OpenGL, P1, V2(MaxUV.x, MinUV.y));
    PushVertex(OpenGL, P2, V2(MaxUV.x, MaxUV.y));

    PushVertex(OpenGL, P2, V2(MaxUV.x, MaxUV.y));
    PushVertex(OpenGL, P3, V2(MinUV.x, MaxUV.y));
    PushVertex(OpenGL, P0, V2(MinUV.x, MinUV.y));
}

static void
PushText(opengl *OpenGL, v2 P, char *Text)
{
    for (char *C = Text; *C; ++C) {
        stbtt_aligned_quad Quad;
        stbtt_GetBakedQuad(
                OpenGL->Font.Chars, OpenGL->Font.Width, OpenGL->Font.Height,
                (int)(*C - 0x20),
                &P.x, &P.y, &Quad,
                1);
        PushQuad(
                OpenGL,
                V2(Quad.x0, Quad.y0 + OpenGL->Font.Ascent), V2(Quad.x1, Quad.y1 + OpenGL->Font.Ascent),
                V2(Quad.s0, Quad.t0), V2(Quad.s1, Quad.t1));
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

static int
GetHashGridDataIndex(hash_grid HashGrid, int x, int y, int i)
{
    int Index = (x + y * HashGrid.Width) + i * HashGrid.Width * HashGrid.Height;
    return 4 * Index;
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
        for (int YIndex = YStart; YIndex < YEnd; ++YIndex) {
            float FieldValue = 0.0f;
            {
                v2 P = -0.5f * V2(WorldW, WorldH) + V2(XIndex, YIndex) * V2(CellW, CellH);

                v2 CenterCell = (P + 0.5f * V2(WorldW, WorldH)) * (1.0f / H);
                int CenterCellX = CenterCell.x;
                int CenterCellY = CenterCell.y;

                for (int Row = 0; Row < 3; ++Row) {
                    for (int Col = 0; Col < 3; ++Col) {
                        int CellX = CenterCellX - 1 + Col;
                        int CellY = CenterCellY - 1 + Row;

                        bool IsInvalid = CellX < 0 || CellX >= HashGrid.Width || CellY < 0 || CellY >= HashGrid.Height;
                        if (IsInvalid) {
                            continue;
                        }


                        for (int i = 0; i < HashGrid.ElementsPerCell; ++i)
                        {
                            int Index = GetHashGridDataIndex(HashGrid, CellX, CellY, i);
                            if (Work->HashGridData[Index + 3] < 1) {
                                break;
                            }

                            float MX = Work->HashGridData[Index + 0];
                            float MY = Work->HashGridData[Index + 1];
                            float MR = Work->HashGridData[Index + 2];

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
CPUEvaluateField(sim *Sim, opengl *OpenGL)
{
    hash_grid HashGrid = Sim->HashGrid;
    int ParticleCount = Sim->ParticleCount;
    particle *Particles = Sim->Particles;

    float WorldW = WORLD_WIDTH;
    float WorldH = WORLD_HEIGHT;

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
            Work->HashGridData = OpenGL->HashGridData;
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
}

static void
GPUEvaluateField(sim *Sim, opengl *OpenGL)
{
    hash_grid HashGrid = Sim->HashGrid;

    int GridW = OpenGL->GridW;
    int GridH = OpenGL->GridH;

    glBindTexture(GL_TEXTURE_2D, OpenGL->HashGridTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, HashGrid.Width * HashGrid.Height, HashGrid.ElementsPerCell, GL_RGBA, GL_FLOAT, OpenGL->HashGridData);

    glUseProgram(OpenGL->ComputeShaderProgram);
    glDispatchCompute(GridW + 1, GridH + 1, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, OpenGL->FieldFramebuffer);
    glReadPixels(0, 0, GridW + 1, GridH + 1, GL_RED, GL_FLOAT, OpenGL->Field);
}

static void
DumpField(opengl *OpenGL, const char *FileName)
{
    FILE *File = fopen(FileName, "w");
    fprintf(File, "P2\n");
    fprintf(File, "%u %u\n", OpenGL->GridW+1, OpenGL->GridH+1);
    fprintf(File, "255\n");
    for (int i = 0; i < (OpenGL->GridW+1)*(OpenGL->GridH+1); ++i) {
        uint32_t F = OpenGL->Field[i] * 255.0f;
        fprintf(File, "%u\n", F);
    }
    fclose(File);
}

static void
RenderMarchingSquares(opengl *OpenGL, sim *Sim, bool RenderContour)
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

    for (int y = 0; y < HashGrid.Height; ++y) {
        for (int x = 0; x < HashGrid.Width; ++x) {

            hash_grid_cell Cell = GetCell(HashGrid, x, y);
            int ElementCount = GetElementCount(HashGrid, Cell);

            for (int ElementIndex = 0;
                 ElementIndex < ElementCount;
                 ++ElementIndex)
            {
                particle Particle = Particles[Cell.Data[ElementIndex]];

                int Index = GetHashGridDataIndex(HashGrid, x, y, ElementIndex);

                OpenGL->HashGridData[Index + 0] = Particle.P.x;
                OpenGL->HashGridData[Index + 1] = Particle.P.y;
                OpenGL->HashGridData[Index + 2] = PARTICLE_RADIUS;
                OpenGL->HashGridData[Index + 3] = 1;
            }
            if (ElementCount < HashGrid.ElementsPerCell) {
                int Index = GetHashGridDataIndex(HashGrid, x, y, ElementCount);
                OpenGL->HashGridData[Index + 3] = 0;
            }
        }
    }

    float Threshold = 0.2f;

    int GridW = OpenGL->GridW;
    int GridH = OpenGL->GridH;
    float CellW = WorldW / GridW;
    float CellH = WorldH / GridH;
    float *Field = OpenGL->Field;

    TIMER_START(Timer_RenderFieldEval);
    CPUEvaluateField(Sim, OpenGL);
    TIMER_END(Timer_RenderFieldEval);

    if (RenderContour) {
        OpenGL->VertexSize = 0;

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
#define LINE PushLine(OpenGL, P0, P1)

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
        glBufferData(GL_ARRAY_BUFFER, OpenGL->VertexSize * sizeof(vertex), OpenGL->Vertices, GL_STREAM_DRAW);

        glUseProgram(OpenGL->ShaderProgram);
        glDrawArrays(GL_LINES, 0, OpenGL->VertexSize);
    } else {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, OpenGL->FieldTexture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GridW + 1, GridH + 1, GL_RED, GL_FLOAT, OpenGL->Field);

        vertex Verts[6];

        Verts[0].P = V2(-1, -1);
        Verts[0].UV = V2(0, 0);

        Verts[1].P = V2(1, -1);
        Verts[1].UV = V2(1, 0);

        Verts[2].P = V2(1, 1);
        Verts[2].UV = V2(1, 1);

        Verts[3].P = V2(1, 1);
        Verts[3].UV = V2(1, 1);

        Verts[4].P = V2(-1, 1);
        Verts[4].UV = V2(0, 1);

        Verts[5].P = V2(-1, -1);
        Verts[5].UV = V2(0, 0);

        glBindBuffer(GL_ARRAY_BUFFER, OpenGL->VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(Verts), Verts, GL_STREAM_DRAW);

        glUseProgram(OpenGL->TextureProgram);
        glDrawArrays(GL_TRIANGLES, 0, ArrayCount(Verts));
    }
}

static void
Render(sim *Sim, opengl *OpenGL, float Width, float Height, bool RenderContour)
{
    glViewport(0, 0, Width, Height);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
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

    RenderMarchingSquares(OpenGL, Sim, RenderContour);

    OpenGL->VertexSize = 0;

    char Buffer[64];
    float PenY = 0;

    sprintf(Buffer, "Sim: %.1f ms", TIMER_GET_MS(Timer_Sim));
    PushText(OpenGL, V2(0, PenY), Buffer);
    PenY += OpenGL->Font.PixelHeight;

    sprintf(Buffer, "RenderFieldEval: %.1f ms", TIMER_GET_MS(Timer_RenderFieldEval));
    PushText(OpenGL, V2(0, PenY), Buffer);
    PenY += OpenGL->Font.PixelHeight;
    PenY += OpenGL->Font.PixelHeight;

    PushText(OpenGL, V2(0, PenY), "Press F to switch rendering mode");
    PenY += OpenGL->Font.PixelHeight;

    glBindBuffer(GL_ARRAY_BUFFER, OpenGL->VBO);
    glBufferData(GL_ARRAY_BUFFER, OpenGL->VertexSize * sizeof(vertex), OpenGL->Vertices, GL_STREAM_DRAW);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, OpenGL->FontTexture);

    glUseProgram(OpenGL->TextProgram);
    glUniform2f(OpenGL->ResolutionUniform, Width, Height);

    glDrawArrays(GL_TRIANGLES, 0, OpenGL->VertexSize);
}
