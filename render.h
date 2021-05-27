struct opengl {
    GLuint ShaderProgram;
    GLuint VAO;
    GLuint VBO;

    GLuint Transform;
    GLuint Metaballs;
    GLuint XScale;
    GLuint YScale;

    float *Vertices;
    int VertexCount;

    int GridW;
    int GridH;
    float *Field;
};

struct field_eval_work {
    hash_grid HashGrid;
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
