#ifndef __COROUTINE_H__
#define __COROUTINE_H__
#include "coctx.h"
#include <cstddef>
#include <functional>
#include <memory>

class Coroutine
{
public:
    using Callback = std::function<void()>;
    // using CoroutineSptr = std::shared_ptr<Coroutine>;
    
    enum Status
    {
        CO_READY = 0,
        CO_SUSPEND,
        CO_RUNNING,
        CO_DEAD
    };

public:
	Coroutine();
	~Coroutine();
    
    void setCallback(Callback);
    Callback getCallback() const { return cb_; }
    static void Resume(Coroutine*);
    static void Yield();

    static bool isMainCoroutine();

    static Coroutine* getMainCoroutine();
    static Coroutine* getCurCoroutine();
    static Coroutine* getInstanceCoroutine();
private:
    void coroutineMake();
    void stackCopy(char* top);
public:
    coctx_t ctx_;
    char* stack_sp_;
    ptrdiff_t cap_;
    ptrdiff_t size_;
	bool is_used_;
    Status status_;
    bool is_execFunc_;
    int co_id_{-1};
private:    
    Callback cb_;
};

struct CoPool
{
    Coroutine* main_coroutine{nullptr};
    Coroutine* cur_coroutine{nullptr};
    char* shared_stack{nullptr};
    std::vector<Coroutine*> coroutine_pool;
    const static int SSIZE = 1024*512;

    static CoPool* getCoPool();

    CoPool();
    ~CoPool();
};

#endif

