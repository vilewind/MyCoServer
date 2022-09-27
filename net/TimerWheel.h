/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-19 14:00
#
# Filename: TimerWheel.h
#
# Description: 使用timerfd将定时器转化为epoll上的IO事件，保证one timerwheel per eventloop。
	定时器的增删以及tick的运行都在对应的IO线程中，也不会存在同步问题。		
#
=============================================================================*/
#ifndef __TIMERWHEEL__
#define __TIMERWHEEL__

#include <time.h>
#include <memory>
#include <functional>
#include <condition_variable>
#include <vector>
#include <mutex>
#include <atomic>
#include <memory>

class Channel;
class EventLoop;

struct Timer 
{
	using TimerCallback = std::function<void()>;
	enum TimerType
	{
		TIMER_ONCE = 0,
		TIMER_CYCLE
	};

/*= data*/
	time_t timeout { 8 };					//超时时间
	int rotation { 0 };						//剩余转数
	int time_slot { 0 };					//时间轮中所处的槽
	std::atomic<bool> inSlot { false };
	
	Timer* prev { nullptr };
	Timer* next {nullptr };

	TimerType timer_type;
	TimerCallback timer_cb;

/*= func*/
	Timer( time_t timeout, const TimerCallback&, TimerType tt = TIMER_ONCE );
	~Timer();	

	void addTimer();

	void delTimer();

	void adjustTimer( TimerType tt = TIMER_ONCE );

	void setTimerFunc( const TimerCallback& tc ) { timer_cb = tc; }
};

class TimerWheel
{
public:
	TimerWheel( EventLoop* loop );
    ~TimerWheel();

    void start();

    static TimerWheel* getTimerWheel();

    // static void setEventLoopInCurrentThread( EventLoop* loop );

    void addTimerIntoWheel( Timer* timer );

    void delTimerFromWheel( Timer* timer );

	void adjustTimerInWheel( Timer* timer );

    void tick();
private:
/*= func*/
    void calculateTimer( Timer* timer );    
/*= data*/
    const static int INTERVAL = 1;
	const static int SLOTS = 60;

    EventLoop* m_loop;
    int m_timerfd { -1 };						
    std::unique_ptr<Channel> m_channel;

    int m_currentSlot { 0 };

    std::vector<Timer*> m_timerWheel;
};

#endif