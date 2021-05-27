#include <unistd.h>
#include <pthread.h>

#define MAX_THREAD_COUNT 64

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

    pthread_t Threads[MAX_THREAD_COUNT - 1];
};

struct worker_thread_data {
    sim *Sim;
    opengl *OpenGL;
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

static work_queue GlobalWorkQueue;
