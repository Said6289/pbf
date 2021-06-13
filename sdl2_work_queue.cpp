struct work_queue_entry {
    void *Data;
    work_queue_proc Proc;
};

struct work_queue {
    work_queue_entry *Works;
    int Size;
    volatile int Index;
    volatile int DoneCount;

    SDL_mutex *Mutex;
    SDL_cond *Cond;
};

SDL_Thread *GlobalThreadHandles[MAX_THREAD_COUNT - 1];
static work_queue GlobalWorkQueue;

static bool
RunWorkEntry(work_queue *Queue)
{
    bool DidWork = false;

    SDL_LockMutex(Queue->Mutex);
    if (Queue->Index < Queue->Size) {
        work_queue_entry Entry = Queue->Works[Queue->Index];
        Queue->Index = Queue->Index + 1;
        SDL_UnlockMutex(Queue->Mutex);

        Entry.Proc(Entry.Data);

        SDL_LockMutex(Queue->Mutex);
        Queue->DoneCount = Queue->DoneCount + 1;
        SDL_UnlockMutex(Queue->Mutex);

        DidWork = true;
    } else {
        SDL_UnlockMutex(Queue->Mutex);
    }

    return DidWork;
}

static int
WorkerThreadProc(void *Data)
{
    work_queue *Queue = &GlobalWorkQueue;

    while (true) {
        bool DidWork = RunWorkEntry(Queue);
        if (!DidWork) {
            SDL_LockMutex(Queue->Mutex);
            SDL_CondWait(Queue->Cond, Queue->Mutex);
            SDL_UnlockMutex(Queue->Mutex);
        }
    }

    return 0;
}

static void
InitQueue(work_queue *Queue)
{
    Queue->Mutex = SDL_CreateMutex();
    Queue->Cond = SDL_CreateCond();
    Queue->Works = (work_queue_entry *)malloc(512 * sizeof(work_queue_entry));
    Queue->Size = 0;
    Queue->DoneCount = 0;
    Queue->Index = 0;

    int WorkerThreads = SDL_GetCPUCount() - 1;
    if (WorkerThreads > MAX_THREAD_COUNT - 1) {
        WorkerThreads = MAX_THREAD_COUNT - 1;
    }
    printf("Spawning %d worker threads...\n", WorkerThreads);
    for (int i = 0; i < WorkerThreads; ++i) {
        GlobalThreadHandles[i] = SDL_CreateThread(WorkerThreadProc, "WorkerThread", 0);
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
    SDL_CondBroadcast(Queue->Cond);
    while (true) {
        bool DidWork = RunWorkEntry(Queue);
        if (!DidWork) {
            while (Queue->DoneCount != Queue->Size);
            break;
        }
    }
}
