/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-19 14:00
#
# Filename: TimerWheel.h
#
# Description: 
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

	void addInToWheel();

	void delFromWheel();

	void adjustTimer( TimerType tt = TIMER_ONCE );

	void setTimerFunc( const TimerCallback& tc ) { timer_cb = tc; }
};

class TimerWheel
{
public:
    ~TimerWheel();

    static TimerWheel* getTimerWheel();

	void addTimer( Timer* timer );

	void removeTimer( Timer* timer );

	void adjustTimer( Timer* timer );

	void tick();

	void start();

	static void setGlobalEvent( EventLoop* );
    
private:
/*= func*/
	TimerWheel( EventLoop* );

    void calculateTimer( Timer* timer );

	void addTimerIntoWheel( Timer* timer );

	void removeTimerFromWheel( Timer* timer );

	void adjustTimerInLoop( Timer* timer );

/*= data*/	
	EventLoop* m_loop; 
	int m_timerFd;
	Channel* m_ch;
	int m_currentSlot { 0 };
	time_t m_interval { 1 };
	std::mutex m_mtx { };
	std::condition_variable m_cv { };

    std::vector<Timer*> m_timerWheel;
	const static int INTERVAL = 1;
	const static int SLOTS = 60;
};

#endif