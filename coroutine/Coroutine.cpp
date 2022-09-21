#include "Coroutine.h"
#include "coctx.h"
#include <cstring>
#include <memory>
#include <cassert>
#include <cstdlib>
#include <thread>
#include <vector>
#include <iostream>

// #define DEBUF
// #define TEST

// static thread_local Coroutine::CoroutineSptr t_mainCoroutine = nullptr;
// static thread_local Coroutine::CoroutineSptr t_curCoroutine = nullptr;
// static thread_local char shared_stack[1024 * 512];
// static thread_local std::vector<Coroutine::CoroutineSptr> t_coroutinePool;
static thread_local CoPool t_coPool;

void CoFunc(Coroutine *co) {
    #ifdef DEBUF
        std::cout << __func__ << " coid : " << co->co_id_ << std::endl; 
    #endif
    Coroutine::Callback cb = co->getCallback();
    if (cb) 
        cb();
    // Coroutine::Yield();
    ///@brief 重置相关标志，用以执行其他任务

    co->is_execFunc_ = false;
    co->status_ = Coroutine::CO_READY;
    co->is_used_ = false;
    #ifdef DEBUF
    std::cout << __func__ << " coid : " << co->co_id_ << " finished" << std::endl; 
    #endif
    Coroutine::Yield();
}

CoPool::CoPool()
{
    if (main_coroutine == nullptr) {
        main_coroutine = new Coroutine();
        cur_coroutine = main_coroutine;
        shared_stack = new char[SSIZE]();
    } 
    else 
    {
        std::cerr << "copool create error";
        ::exit(EXIT_FAILURE);
    }
}

CoPool::~CoPool()
{
    if (main_coroutine)
	{
		if (cur_coroutine != main_coroutine)
		{
			delete cur_coroutine;
			cur_coroutine = nullptr;
		}
        delete main_coroutine;
		main_coroutine = nullptr;
	}
    if (shared_stack) 
	{
        delete shared_stack;
		shared_stack = nullptr;
	}
	for (auto it = coroutine_pool.begin(); it != coroutine_pool.end(); )
    {
        delete *it++;
    }
    // std::cout << __func__ << std::endl;
}

Coroutine::Coroutine()
    : stack_sp_(nullptr),
      cap_(0),
      size_(0),
      is_used_(false),
      status_(CO_READY),
      is_execFunc_(false)
{
    memset(&ctx_, 0, sizeof ctx_);
    /* if (t_mainCoroutine == nullptr) { */
    /*     t_mainCoroutine = this; */
    /*     t_curCoroutine = t_mainCoroutine; */
    /* } */
}

Coroutine::~Coroutine() {
    if (stack_sp_) 
        free(stack_sp_);
    status_ = CO_DEAD;
}

void Coroutine::stackCopy(char* top) {
    char dummy = 0;
    assert(top - &dummy <= t_coPool.SSIZE);
    if (cap_ < top - &dummy) {
        if (stack_sp_ != t_coPool.shared_stack)
            delete stack_sp_;
        stack_sp_ = nullptr;
        cap_ = top - &dummy;
        stack_sp_ = reinterpret_cast<char*>(malloc(cap_));
    }

    size_ = top - &dummy;
    memcpy(stack_sp_, &dummy, size_);
}

void Coroutine::coroutineMake() {
    stack_sp_ = t_coPool.shared_stack;
    size_ = t_coPool.SSIZE;
    char* top = stack_sp_ + size_;
    top = reinterpret_cast<char *>((reinterpret_cast<unsigned long>(top)) & -16LL);

    memset(&ctx_, 0, sizeof ctx_);
    ctx_.regs[kRSP] = top;
    ctx_.regs[kRBP] = top;
    ctx_.regs[kRETAddr] = reinterpret_cast<char*>(CoFunc);
    ctx_.regs[kRDI] = reinterpret_cast<char*>(this);
}

bool Coroutine::isMainCoroutine() {
    if (t_coPool.main_coroutine == nullptr || t_coPool.main_coroutine == t_coPool.cur_coroutine)
        return true;
    return false;
}

void Coroutine::setCallback(Callback cb) {
    if (is_execFunc_)
    {
        std::cerr << "current coroutine already has task to exec";
        return;
    }
    cb_ = cb;
    is_execFunc_ = true;
}

Coroutine* Coroutine::getMainCoroutine() {
    /*if (t_mainCoroutine == nullptr) {
        t_mainCoroutine = std::make_shared<Coroutine>();
        t_curCoroutine = t_mainCoroutine;
    }
	*/
    return t_coPool.main_coroutine;
}

Coroutine* Coroutine::getCurCoroutine() {
    /*if (t_curCoroutine == nullptr)
        getMainCoroutine();
    */
	return t_coPool.cur_coroutine;
}

Coroutine* Coroutine::getInstanceCoroutine() {
    int cur = 0;
    for (; cur < t_coPool.coroutine_pool.size() && !t_coPool.coroutine_pool.empty(); ++cur) {
        if (t_coPool.coroutine_pool[cur] != nullptr && t_coPool.coroutine_pool[cur]->is_used_ == false && t_coPool.coroutine_pool[cur]->status_ == CO_READY) 
            break;
    }
    
    if (cur >= t_coPool.coroutine_pool.size()) {
        for (int i = cur; i < cur + 16; ++i) {
            t_coPool.coroutine_pool.emplace_back(new Coroutine());
        }
    }
    // std::cout << "get instance coroutine" << std::endl;
    t_coPool.coroutine_pool[cur]->co_id_ = t_coPool.coroutine_pool[cur]->co_id_ == -1 ?  cur : t_coPool.coroutine_pool[cur]->co_id_;
    t_coPool.coroutine_pool[cur]->is_used_ = true;
    return t_coPool.coroutine_pool[cur];
}

void Coroutine::Resume(Coroutine* co) {
    if (co == nullptr || co->is_used_ == false || co->is_execFunc_ == false) {
        /* target coroutine is nullptr*/
        return;
    }

    if (!isMainCoroutine()) {
        /* current coroutine is not main coroutine without the ability to resume sub-couroutine*/
        return;
    }

    if (co->status_ == CO_READY)
        co->coroutineMake();
    else if (co->status_ == CO_SUSPEND) {
        memcpy(t_coPool.shared_stack + t_coPool.SSIZE - co->size_, co->stack_sp_, co->size_);
    } 
    else
    {
        assert(0);
    }
    co->status_ = CO_RUNNING;
    t_coPool.cur_coroutine = co;
    coctx_swap(&t_coPool.main_coroutine->ctx_, &co->ctx_);
}

void Coroutine::Yield() {
    if (isMainCoroutine()) {
        /* main coroutine cannot Yield*/
        return;
    }

    Coroutine* co = t_coPool.cur_coroutine;
    /// @bug 处理完任务后，为了协程重用，设置了初始化了co的标志，导致无法yield执行main程序
    // if (co == nullptr || co->is_used_ == false || co->is_execFunc_ == false) {
    if (co == nullptr) {
        /* target coroutine is nullptr*/
        return;
    }
    t_coPool.cur_coroutine = t_coPool.main_coroutine;
    co->status_ = CO_SUSPEND;
    co->stackCopy(t_coPool.shared_stack + t_coPool.SSIZE);
    #ifdef DEBUF
    std::cout << " coid : " << co->co_id_ << std::endl;
    #endif
    coctx_swap(&co->ctx_, &t_coPool.main_coroutine->ctx_);
    #ifdef DEBUF
    std::cout << __func__ << " after coctx_swap" << std::endl;
    #endif
}

CoPool* CoPool::getCoPool()
{
    return &t_coPool;
}


#ifdef TEST

#include <iostream>
#include <thread>
#include <string>

int g = 1000;

void foo(int a)
{
    for (int i = a; i < g + 5; ++i)
    {
        std::cout << __func__ << " " << i << std::endl;
        Coroutine::Yield();
    }
}

/** @TODO 输出参数并未出现变化*/
int main()
{
    Coroutine* c1 = Coroutine::getInstanceCoroutine();
    Coroutine* c2 = Coroutine::getInstanceCoroutine();
    
    c1->setCallback(std::bind(foo, 0));
    c2->setCallback(std::bind(foo, 5));

    while(c1->is_execFunc_ && c2->is_execFunc_)
    {
        Coroutine::Resume(c1);
        Coroutine::Resume(c2);
    }
    return 0;
}

#endif