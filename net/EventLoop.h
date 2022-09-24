/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 08:13
#
# Filename: EventLoop.h
#
# Description: 
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
private:
/*= func*/
	/* 跨线程唤醒（读写）*/
	void wakeup();
	void wokeup();

	void execPendingTask();

/*= data*/
	std::thread::id m_tid;
	Epoller* m_epoller;
	int m_efd;									//eventfd，用于跨线程唤醒eventloop
	Channel* m_waker;
	CoPool* m_cp;

	std::atomic<bool> m_looping { false };
	std::atomic<bool> m_stop { false };
	std::atomic<bool> m_pending { false };

	std::mutex m_mtx { };
	ChannelVec m_chs;
	TaskVec m_tasks;
};

#endif