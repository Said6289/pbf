struct work_queue;
typedef void (*work_queue_proc) (void *Data);

static void InitQueue(work_queue *Queue);
static void ResetQueue(work_queue *Queue);
static void AddEntry(work_queue *Queue, void *Work, work_queue_proc Proc);
static void FinishWork(work_queue *Queue);
