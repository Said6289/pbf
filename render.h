struct opengl {
    GLuint ShaderProgram;
    GLuint ComputeShaderProgram;
    GLuint VAO;
    GLuint VBO;

    GLuint Transform;
    GLuint Metaballs;
    GLuint XScale;
    GLuint YScale;

    GLuint FieldFramebuffer;
    GLuint FieldTexture;
    GLuint HashGridTexture;

    float *Vertices;
    int VertexCount;

    int GridW;
    int GridH;
    float *Field;
    float *HashGridData;
};

struct field_eval_work {
    hash_grid HashGrid;
    float *HashGridData;
    float CellW;
    float CellH;
    int GridW;
    float *Field;
    particle *Particles;

    int XStart;
    int YStart;
    int XEnd;
    int YEnd;
};

static void EvaluateFieldTile(void *Data);
static void * MarchingSquaresThread(void *Data);
