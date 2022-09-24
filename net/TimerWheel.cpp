/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-19 14:00
#
# Filename: TimerWheel.cpp
#
# Description: 
#
=============================================================================*/
#include "Util.h"
#include "TimerWheel.h"
#include "EventLoop.h"
#include "Channel.h"
#include <cassert>
#include <sys/timerfd.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstring>

static EventLoop* g_loop = nullptr;

int createTimerFd()
{
/// @a CLOCK_MONOTONIC 系统启动后不可改变的单调递增时钟
    return Util::ERRIF( __func__, 0 , ::timerfd_create, CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC );
}

void setTimerFd( int fd, double interval = 1. )
{
    struct itimerspec new_time;
    ::memset( &new_time, 0, sizeof new_time );

/* 计算超时时间*/
    struct timeval tv;
    // ::gettimeofday( &tv, nullptr );
    // int64_t microseconds = interval * 1000 * 1000 + tv.tv_sec * 1000 * 1000 + tv.tv_usec;

    struct timespec ts;
    ts.tv_sec = static_cast<time_t>( interval );
    ts.tv_nsec = static_cast<long>( 0 );

    new_time.it_value = ts;                                                                 //超时时间
    new_time.it_interval = { static_cast<time_t>( interval ), 0 };                                                        //超时后的下一次定时
    if ( ::timerfd_settime( fd, 0, &new_time, nullptr ) != 0 )
    {
        std::cerr << "timerfd settime " << std::endl;
        ::exit( EXIT_FAILURE ); 
    }
}

Timer::Timer( time_t timeout, const TimerCallback& tc, TimerType tt )
    : timeout ( timeout ),
      timer_cb ( tc ),
      timer_type ( tt )
{
    assert( timeout >= 0);
}

Timer::~Timer()
{
    // delFromWheel();
    next = nullptr;
    prev= nullptr;
}

void Timer::addInToWheel()
{
    TimerWheel::getTimerWheel()->addTimer( this );
}

void Timer::adjustTimer( TimerType tt )
{
    if ( errno != 0 )
    {
        std::cout << __func__ << " : errno : " << errno << std::endl;
    }
    timer_type = tt;
    TimerWheel::getTimerWheel()->adjustTimer( this );
}

void Timer::delFromWheel()
{
    TimerWheel::getTimerWheel()->removeTimer( this );
}

TimerWheel::TimerWheel( EventLoop* loop )
    : m_loop ( loop ),
      m_timerFd( createTimerFd() ),
      m_ch( new Channel( m_loop, m_timerFd ) )
{
    m_timerWheel.resize( SLOTS );
    setTimerFd( m_timerFd, m_interval );
    std::cout << __func__ << " : " << m_timerFd << std::endl;
    m_ch->setReadCb( std::bind( &TimerWheel::tick, this ) );
}

void TimerWheel::start()
{
    m_ch->enableReading();
}

TimerWheel::~TimerWheel()
{
    m_ch->disableAll();
    m_ch->remove();
    ::close( m_timerFd );
    delete m_ch;
}

void TimerWheel::setGlobalEvent( EventLoop* loop )
{
    g_loop = loop;
}

void TimerWheel::tick()
{
    // std::cout << __func__ << " slot : " << m_currentSlot << std::endl;
    uint64_t opt = 0;
    Util::ERRIF( __func__, sizeof opt, ::read, m_ch->getFd(), reinterpret_cast<char*>( &opt ), sizeof opt);
    std::lock_guard<std::mutex> locker( m_mtx );
    Timer* timer = m_timerWheel[m_currentSlot];

    while( timer != nullptr )
    {
        if ( timer->rotation > 0 )
        {
            --timer->rotation;
            timer = timer->next;
        }
        else 
        {
            Timer* tmp = timer->next;
            if (timer->timer_cb)
            {
                timer->timer_cb();
            }
            timer = tmp;

            // if ( tmp->timer_type == Timer::TimerType::TIMER_ONCE )
            // {
            //     removeTimerFromWheel( std::move ( tmp ) );
            // }
            // else 
            // {
            //     std::cout << "emm" << std::endl;
            //     adjustTimerInLoop( std::move ( tmp ) );
            // }
        }
    }
    m_currentSlot = ( ++m_currentSlot ) % SLOTS;
    setTimerFd( m_timerFd, m_interval );
    m_ch->enableReading();
}

/**
 * @brief 单例模式，同时必须保证第一次调用在TcpServer中，
 *        使得timerwheel绑定的eventloop时tcpserver拥有的执行accept的eventloop
*/
TimerWheel* TimerWheel::getTimerWheel() 
{
    static TimerWheel tw = TimerWheel( g_loop );
    return &tw;
}

void TimerWheel::addTimer( Timer* timer )
{
    if ( timer == nullptr )
    {
        return ;
    }
    if ( timer->inSlot )
    {
        adjustTimer( timer );
        return;
    }

    calculateTimer( std::move( timer ) );
    
    {
        std::lock_guard<std::mutex> locker( m_mtx );
        addTimerIntoWheel( std::move( timer ) );
        timer->inSlot = true;
    }
}

void TimerWheel::removeTimer( Timer* timer )
{
    if ( timer == nullptr || !timer->inSlot )
    {
        return ;
    }
    
    {
        std::lock_guard<std::mutex> locker( m_mtx );
        removeTimerFromWheel( std::move( timer ) );
        timer->inSlot = false;
    }
    // std::cout << __func__ << std::endl;
}

void TimerWheel::adjustTimer( Timer* timer )
{
    if ( timer == nullptr ||  !timer->inSlot )
    {
        return ;
    }

    {
        std::lock_guard<std::mutex> locker( m_mtx );
        adjustTimerInLoop( std::move( timer ) );
    }
}

void TimerWheel::addTimerIntoWheel( Timer* timer )
{
    int slot = timer->time_slot;

    if ( m_timerWheel[slot] != nullptr )
    {
        timer->next = m_timerWheel[slot];
        m_timerWheel[slot]->prev = timer;
        m_timerWheel[slot] = timer;
    }
    else
    {
        m_timerWheel[slot] = timer;
    }

    // std::cout << __func__ << " " << slot << std::endl;
}

void TimerWheel::removeTimerFromWheel( Timer* timer )
{
    int slot = timer->time_slot;
   
    if ( timer == m_timerWheel[slot] )
    {
        m_timerWheel[slot] = timer->next;
        if ( timer->next != nullptr )
        {   
            timer->next->prev = nullptr;
        }
    }
    else 
    {
        if ( timer->prev == nullptr )
        {
            return;
        }

        timer->prev->next = timer->next;
        if ( timer->next != nullptr )
        {
            timer->next->prev = timer->prev;
        }
    }

    timer->prev = timer->next = nullptr;
}

void TimerWheel::adjustTimerInLoop( Timer* timer )
{
    removeTimerFromWheel( std::move( timer ) );
    calculateTimer( std::move( timer ) );
    addTimerIntoWheel( std::move( timer ) );
}

void TimerWheel::calculateTimer( Timer* timer )
{
    if ( timer == nullptr )
    {
        return ;
    }

    int tick = 0;
    int timeout = timer->timeout;
    if ( timeout < INTERVAL )
    {
        tick = 1;
    }
    else 
    {
        tick = timeout / INTERVAL;
    }

    timer->rotation = tick / SLOTS;
    timer->time_slot = ( m_currentSlot + tick ) % SLOTS;
}


