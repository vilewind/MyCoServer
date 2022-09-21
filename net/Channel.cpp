/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 08:22
#
# Filename: Channel.cpp
#
# Description: 
#
=============================================================================*/
#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <cassert>

Channel::Channel( EventLoop* loop, int fd )
	: m_loop( loop ),
	  m_fd( fd ),
	  m_events( 0 )
{

}

Channel::~Channel() 
{
	
}

void Channel::enableReading() 
{
	m_events |= EPOLLIN | EPOLLET;
	update();
}

void Channel::disableReading()
{
	m_events &= ~EPOLLIN;
	update();
}

void Channel::enableWriting() {
	m_events |= EPOLLOUT | EPOLLET;
	update();
}

void Channel::disableWriting() 
{
	m_events &= ~EPOLLOUT;
	update();
}

void Channel::disableAll() 
{
	m_events = 0;
	update();
}

void Channel::remove() 
{
	if ( !m_inEpoll )
	{
		return ;
	}
	m_loop->removeChannel(this);
}

/** @brief 根据事件类型执行相应的回调 
 *  @param EPOLLPRI 文件描述符有紧急可读数据（带外数据）
 *  @param EPOLLERR 文件描述符发生错误
 *  @param EPOLLHUP 文件描述副被挂断
 * */
void Channel::handleEvent() {
	std::cout << __func__ << " : " << m_fd << std::endl;
	if (m_events & (EPOLLIN | EPOLLPRI) ) {
		if (m_RC)
			m_RC();
	}
	else if (m_events & EPOLLOUT) {
		if (m_WC)
			m_WC();
	}
	else if ( m_events & EPOLLRDHUP )
	{
		if (m_CC)
			m_CC();
	}
	else if ( m_events & EPOLLERR )
	{
		if ( m_EC )
			m_EC();
	}
}

void Channel::update() {
	m_loop->updateChannel(this);
}
