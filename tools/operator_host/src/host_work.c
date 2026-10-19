/* Bounded deterministic host work queue. Delayed work never recurses immediately. */
#include <assert.h>
#include <zephyr/kernel.h>

static int64_t now;
static struct {
    struct k_work_delayable *work;
    int64_t due;
} pending[16];

int64_t k_uptime_get(void) { return now; }
void k_work_init_delayable(struct k_work_delayable *w, void (*handler)(struct k_work *)) {
    w->work.handler = handler;
}
int k_work_cancel_delayable(struct k_work_delayable *w) {
    for (int i = 0; i < 16; ++i)
        if (pending[i].work == w)
            pending[i].work = NULL;
    return 0;
}
int k_work_reschedule(struct k_work_delayable *w, int delay) {
    k_work_cancel_delayable(w);
    for (int i = 0; i < 16; ++i) {
        if (!pending[i].work) {
            pending[i].work = w;
            pending[i].due = now + delay;
            return 0;
        }
    }
    assert(!"host work queue capacity exceeded");
    return -1;
}
/* Zephyr semantics: scheduling already-pending work leaves its deadline unchanged. */
int k_work_schedule(struct k_work_delayable *w, int delay) {
    for (int i = 0; i < 16; ++i)
        if (pending[i].work == w)
            return 0;
    return k_work_reschedule(w, delay);
}
int k_work_schedule_for_queue(struct k_work_q *q, struct k_work_delayable *w, int delay) {
    (void)q;
    return k_work_schedule(w, delay);
}
int k_work_reschedule_for_queue(struct k_work_q *q, struct k_work_delayable *w, int delay) {
    (void)q;
    return k_work_reschedule(w, delay);
}
void host_work_advance(int ms) {
    now += ms;
    for (int pass = 0; pass < 256; ++pass) {
        struct k_work_delayable *ready = NULL;
        for (int i = 0; i < 16; ++i)
            if (pending[i].work && pending[i].due <= now) {
                ready = pending[i].work;
                pending[i].work = NULL;
                break;
            }
        if (!ready)
            return;
        if (ready->work.handler)
            ready->work.handler(&ready->work);
    }
    assert(!"host work queue did not quiesce");
}
