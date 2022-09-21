/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 08:49
#
# Filename: Epoller.cpp
#
# Description: 
#
=============================================================================*/
#include "Epoller.h"
#include "EventLoop.h"
#include "Channel.h"
#include <sys/epoll.h>
#include <unistd.h>
#include <cstring>

Epoller::Epoller( EventLoop* loop )
    : m_loop( loop ),
      m_epfd( Util::ERRIF( "create epoll fd", 0, epoll_create1, EPOLL_CLOEXEC ) )
{
    m_ev.reserve(1024);
}

Epoller::~Epoller() 
{
    Util::ERRIF("close epoll fd", 0, ::close, m_epfd);
}

void Epoller::updateChannel( Channel* ch ) 
{
    if (ch->getInEPoll()) {
       epollCtl( ch, EPOLL_CTL_MOD, "epoll mode" );
    } else {
        epollCtl( ch, EPOLL_CTL_ADD, "epoll add" );
        ch->setInEpoll(true);
    }
    std::cout << __func__ << " " << ch->getFd() << std::endl;
}

void Epoller::removeChannel( Channel* ch )
{
    std::cout << __func__ << " " << ch->getFd() << std::endl;
    if ( ch-> getInEPoll() )
    {
        epollCtl( ch, EPOLL_CTL_DEL, __func__ );
        ch->setInEpoll( false );
    }
    else 
    {
         std::cout << " the target with fd = " << ch->getFd() << " is not in epoll table" << std::endl;
    }
}

void Epoller::epollWait( ChannelVec& chs, int timeout ) {
    chs.clear();
    int nfds = Util::ERRIF("epoll wait", 0, ::epoll_wait, m_epfd, &*m_ev.begin(), m_ev.capacity(), timeout);
    if (nfds == 0) {
        std::cout << "nothing happend" << std::endl;
    } else {
        for (int i = 0; i < nfds; ++i) {
            epoll_event ee = m_ev[i];
            chs.push_back(static_cast<Channel*>(ee.data.ptr));
        }
    }
}

void Epoller::epollCtl( Channel* ch, int op, const char* opstr )
{
    struct epoll_event ee;
    memset(&ee, 0, sizeof ee);
    ee.events = ch->getEvents();
    ee.data.ptr = static_cast<void*>(ch);
    int fd = ch->getFd();

    Util::ERRIF( opstr , 0, epoll_ctl, m_epfd, op, fd, &ee);
}


