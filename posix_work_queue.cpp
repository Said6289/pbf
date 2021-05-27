#include <unistd.h>
#include <pthread.h>

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

pthread_t GlobalThreadHandles[MAX_THREAD_COUNT - 1];
static work_queue GlobalWorkQueue;

static bool
RunWorkEntry(work_queue *Queue)
{
    bool DidWork = false;
    
    pthread_mutex_lock(&Queue->Mutex);
    if (Queue->Index < Queue->Size) {
        work_queue_entry Entry = Queue->Works[Queue->Index];
        Queue->Index = Queue->Index + 1;
        pthread_mutex_unlock(&Queue->Mutex);

        Entry.Proc(Entry.Data);

        pthread_mutex_lock(&Queue->Mutex);
        Queue->DoneCount = Queue->DoneCount + 1;
        pthread_mutex_unlock(&Queue->Mutex);

        DidWork = true;
    } else {
        pthread_mutex_unlock(&Queue->Mutex);
    }

    return DidWork;
}

static void *
WorkerThreadProc(void *Data)
{
    work_queue *Queue = &GlobalWorkQueue;
    
    while (true) {
        bool DidWork = RunWorkEntry(Queue);
        if (!DidWork) {
            pthread_mutex_lock(&Queue->Mutex);
            pthread_cond_wait(&Queue->Cond, &Queue->Mutex);
            pthread_mutex_unlock(&Queue->Mutex);
        }
    }
}

static void
InitQueue(work_queue *Queue)
{
    pthread_mutex_init(&Queue->Mutex, 0);
    pthread_cond_init(&Queue->Cond, 0);
    Queue->Works = (work_queue_entry *)malloc(512 * sizeof(work_queue_entry));
    Queue->Size = 0;
    Queue->DoneCount = 0;
    Queue->Index = 0;

    int WorkerThreads = sysconf(_SC_NPROCESSORS_CONF) - 1;
    if (WorkerThreads > MAX_THREAD_COUNT - 1) {
        WorkerThreads = MAX_THREAD_COUNT - 1;
    }
    printf("Spawning %d worker threads...\n", WorkerThreads);
    for (int i = 0; i < WorkerThreads; ++i) {
        pthread_create(&GlobalThreadHandles[i], 0, WorkerThreadProc, 0);
    }
}

static void
ResetQueue(work_queue *Queue)
{
    Queue->Index = 0;
    Queue->Size = 0;
    Queue->DoneCount = 0;
}

static void
AddEntry(work_queue *Queue, void *Work, work_queue_proc Proc)
{
    assert(Queue->Size < 512);
    work_queue_entry *Entry = Queue->Works + Queue->Size++;
    Entry->Data = Work;
    Entry->Proc = Proc;
}

static void
FinishWork(work_queue *Queue)
{
    pthread_cond_broadcast(&Queue->Cond);
    while (true) {
        bool DidWork = RunWorkEntry(Queue);
        if (!DidWork) {
            while (Queue->DoneCount != Queue->Size);
            break;
        }
    }
}
