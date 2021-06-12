#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct font_info {
    stbtt_bakedchar *Chars;
    uint8_t *FontBitmap;
    uint16_t Width;
    uint16_t Height;
    float Ascent;
    float PixelHeight;
};

struct vertex {
    v2 P;
    v2 UV;
};

struct opengl {
    GLuint ShaderProgram;
    GLuint ComputeShaderProgram;
    GLuint TextProgram;
    GLuint TextureProgram;

    GLuint VAO;
    GLuint VBO;

    GLuint Transform;
    GLuint Metaballs;
    GLuint XScale;
    GLuint YScale;

    GLuint FieldFramebuffer;
    GLuint FieldTexture;
    GLuint HashGridTexture;
    GLuint FontTexture;
    GLuint ResolutionUniform;

    vertex *Vertices;
    uint16_t VertexCapacity;
    uint16_t VertexSize;

    int GridW;
    int GridH;
    float *Field;

    font_info Font;
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
