/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 08:13
#
# Filename: EventLoop.h
#
# Description: 1、one eventloop per thread，负责每一个IO线程上的IO事件；
			   2、使用timerfd实现定时器，因此每个eventloop拥有一个timerwheel，
				  定时剔除不活跃连接（本项目采取主从reactor模式，因此设置了是否使用timerwheel的函数）
			   3、此外，eventloop拥有一个协程池，管理协程，并执行eventloop分发的任务，本项目主要为使用状态机解析http协议），
			      协程池初始协程数量为0，主reactor只有一个thread local copool对象，协程对其无影响
#
=============================================================================*/
#ifndef __EVENTLOOP_H__
#define __EVENTLOOP_H__

#include "Util.h"
#include "Epoller.h"
#include "Channel.h"
#include <atomic>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>

class CoPool;
class Coroutine;
class TimerWheel;

class EventLoop : Util::noncopyable
{
public:
	using ChannelVec = std::vector<Channel*>;
	using Task = std::function<void()>;
	using TaskVec = std::vector<Task>;

	EventLoop();
	~EventLoop();

	void loop();

	void quit();

	void updateChannel( Channel* );

	void removeChannel( Channel* );

	void runInLoop( Task&& task );

	void queueInLoop( Task&& task );

	static EventLoop* getEventLoopInCurrentThread();

	bool isInCurrentThread() const { return std::this_thread::get_id() == m_tid; }

	void assertInCurrentThread() 
	{
		if ( !isInCurrentThread() )
		{
			std::cerr << "the owner eventloop is not in current thread" << std::endl;
			::exit( EXIT_FAILURE );
		}
	}

	std::thread::id getTid() const { return m_tid; }
	int getEfd() const { return m_efd; }

	Coroutine* getCoroutineInstanceInCurrentLoop();

//// @brief 启用timerwheel	
	void enableTimer();
private:
/*= func*/
	/* 跨线程唤醒（读写）*/
	void wakeup();
	void wokeup();

	void execPendingTask();

/*= data*/
	std::thread::id m_tid;
	std::unique_ptr<Epoller> m_epoller;
	int m_efd;									//eventfd，用于跨线程唤醒eventloop
	std::unique_ptr<Channel> m_waker;
	CoPool* m_cp;
	std::unique_ptr<TimerWheel> m_tw;

	std::atomic<bool> m_looping { false };
	std::atomic<bool> m_stop { false };
	std::atomic<bool> m_pending { false };

	std::mutex m_mtx { };
	ChannelVec m_chs;
	TaskVec m_tasks;
};

#endif