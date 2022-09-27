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

static thread_local TimerWheel* t_tw = nullptr;

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
    assert( inSlot == false );
    // delFromWheel();
    // next = nullptr;
    // prev= nullptr;
}

void Timer::addTimer()
{
    if ( t_tw == nullptr )
    {
        std::cerr << " t_tw is nullptr " << std::endl;
        return ;
    }

    t_tw->addTimerIntoWheel( this );
}

void Timer::delTimer()
{
    if ( t_tw == nullptr )
    {
        std::cerr << " t_tw is nullptr " << std::endl;
        return ;
    }

    t_tw->delTimerFromWheel( this );
}

void Timer::adjustTimer( TimerType tt )
{
    if ( t_tw == nullptr )
    {
        std::cerr << " t_tw is nullptr " << std::endl;
        return ;
    }

    timer_type = tt;
    t_tw->adjustTimerInWheel( this );
}

TimerWheel::TimerWheel( EventLoop* loop )
    : m_loop( loop ),
      m_timerfd( createTimerFd() ),
      m_channel( std::make_unique<Channel>( m_loop, m_timerfd ) )
{
    if ( t_tw == nullptr )
    {
        m_timerWheel.resize( SLOTS );
        setTimerFd( m_timerfd, INTERVAL );
        m_channel->setReadCb( std::bind( &TimerWheel::tick ,this ) );

        t_tw = this;
    }
    else 
    {
        std::cerr << " there keeps another timerwheel in current thread " << std::endl;
        ::exit( EXIT_FAILURE) ; 
    }
}

TimerWheel::~TimerWheel()
{
    m_channel->disableAll();
    m_channel->remove();
    ::close( m_timerfd );
}

void TimerWheel::start()
{
    m_channel->enableReading();
}

void TimerWheel::tick()
{
    uint64_t opt = 0;
    Util::ERRIF( __func__, sizeof opt, ::read, m_channel->getFd(), reinterpret_cast<char*>( &opt ), sizeof opt);

    Timer* timer = m_timerWheel[m_currentSlot];

    while( timer != nullptr )
    {
        if ( timer->rotation > 0 )
        {
            --timer->rotation;
        }
        else 
        {
            Timer* tmp = timer->next;

            if ( timer->timer_cb )
            {
                timer->timer_cb();
            }

            timer = tmp;
        }
    }

    m_currentSlot = ( ++m_currentSlot ) % SLOTS;
}

void TimerWheel::addTimerIntoWheel( Timer* timer )
{
    if ( timer == nullptr )
    {
        std::cerr << " the timer is nullptr " << std::endl;
        return;
    }

    if ( timer->inSlot == true )
    {
        adjustTimerInWheel( timer );
        return;
    }

    calculateTimer( std::move( timer ) );

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

    timer->inSlot = true;
}

void TimerWheel::adjustTimerInWheel( Timer* timer )
{
    if ( timer == nullptr )
    {
        std::cerr << " the timer is nullptr " << std::endl;
        return;
    }

    if ( timer->inSlot == false )
    {
        addTimerIntoWheel( timer );
        return;
    }

    delTimerFromWheel( timer );
    calculateTimer( timer );
    addTimerIntoWheel( timer );
}

void TimerWheel::delTimerFromWheel( Timer* timer )
{
    if ( timer == nullptr || timer->inSlot == false )
    {
        std::cerr << " the timer is nullptr or not in wheel " << std::endl;
        return;
    }

    int slot = timer->time_slot;
    if ( timer == m_timerWheel[slot] )
    {
        m_timerWheel[slot] = timer->next;
        if ( timer->next != nullptr )
        {   
            timer->next->prev = nullptr;
        }
    }
/// @note timer不是所处槽的首节点，同时timer的前一个节点为空时，直接进行脱离处理   
    else if ( timer->prev != nullptr )
    {
        // if ( timer->prev == nullptr )
        // {
        //     return;
        // }

        timer->prev->next = timer->next;
        if ( timer->next != nullptr )
        {
            timer->next->prev = timer->prev;
        }
    }

    timer->prev = timer->next = nullptr;
    timer->inSlot = false;
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