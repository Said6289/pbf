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

typedef void (*work_queue_proc) (void *Data);

struct work_queue_entry {
    void *Data;
    work_queue_proc Proc;
};

struct work_queue {
    work_queue_entry *Works;
    int Size;
    volatile int Index;
    volatile int DoneCount;

    pthread_mutex_t Mutex;
    pthread_cond_t Cond;
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
static void ResetQueue(work_queue *Queue);
static void AddEntry(work_queue *Queue, void *Work, work_queue_proc Proc);
static void FinishWork(work_queue *Queue);

static work_queue GlobalWorkQueue;
