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
