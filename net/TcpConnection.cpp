/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 10:58
#
# Filename: TcpConnection.cpp
#
# Description: 
#
=============================================================================*/
#include "TcpConnection.h"
#include "../coroutine/Coroutine.h"
#include "Channel.h"
#include "EventLoop.h"
#include "TimerWheel.h"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/epoll.h>
#include <cassert>

using namespace SocketUtil;

static const int TIMEOUT = 8;

TcpConnection::TcpConnection( EventLoop* loop, int fd )
    : m_loop( loop ),
      m_sock( std::make_unique<Socket>( fd ) ),
      m_ch( std::make_unique<Channel>( m_loop, fd ) ),
      m_co( m_loop->getCoroutineInstanceInCurrentLoop() )
{
    m_ch->setReadCb( std::bind( &TcpConnection::handleRead, this ) );
    m_ch->setWriteCb( std::bind( &TcpConnection::handleWrite, this ) );
    m_ch->setCloseCb( std::bind( &TcpConnection::handleClose, this ) );
    m_ch->setErrorCb( std::bind( &TcpConnection::handleError, this ) );
/// @bug 创建tcpconnection时，所处的IO线程为主线程，添加的时间轮不是当前线程中的时间轮
    // m_timer->addTimer();
}  

TcpConnection::~TcpConnection()
{
    assert( m_state == DisConnected );
    m_co->clear();
}

void TcpConnection::send( const std::string& str )
{
    if( m_state == Connected )
    {
        m_loop->runInLoop( [=]()
        {
            sendInLoop( str );  
        } );
    }
}

void TcpConnection::sendInLoop( const std::string& str )
{
    m_loop->assertInCurrentThread();

    m_output += str;
    handleWrite();
}

void TcpConnection::shutdown()
{
    if ( m_state == Connected )
    {
        m_state = DisCOnnecting;

        m_loop->runInLoop( std::bind( &TcpConnection::shutdownInLoop, this ) );
    }
}

void TcpConnection::shutdownInLoop()
{
    m_loop->assertInCurrentThread();
    if ( m_ch->getEvents() & EPOLLOUT )
    {
        return;
    }
    m_sock->halfClose( m_sock->getFd(), SHUT_WR );
}

void TcpConnection::forceClose()
{
    if ( m_state == Connected || m_state == DisCOnnecting )
    {
        m_state = DisCOnnecting;
        m_loop->runInLoop( std::bind( &TcpConnection::forceCloseInLoop, shared_from_this() ) );
    }
}

void TcpConnection::forceCloseInLoop()
{
    m_loop->assertInCurrentThread();
    if ( m_state == Connected || m_state == DisCOnnecting )
    {
        handleClose();
    }
}

void TcpConnection::establishConnection()
{
    m_loop->assertInCurrentThread();
    assert( m_state == Connecting );
    m_state = Connected;

    m_timer =  std::make_unique<Timer>( TIMEOUT, std::bind( &TcpConnection::timerFunc, this ) ); 
    m_timer->addTimer();

    m_ch->enableReading();
}

void TcpConnection::destroyConnection()
{
    m_loop->assertInCurrentThread();
    if ( m_state == Connected )
    {
        m_state = DisConnected;

        m_ch->disableAll();
    }

    m_ch->remove();
}

void TcpConnection::handleRead()
{
    m_loop->assertInCurrentThread();
    m_timer->adjustTimer();

    int res = Socket::recvn( m_sock->getFd(), std::move( m_input ) );
    if ( res > 0 )
    {
        if ( m_msgCb )
        {
            std::cout << __func__ << std::endl;
            m_msgCb( shared_from_this() );
        }
    }
    else if ( res == 0 )
    {
        handleClose();
    }
    else 
    {
        handleError();
    }
}

void TcpConnection::handleWrite() 
{
    m_loop->assertInCurrentThread();
    m_timer->adjustTimer();

    int res = Socket::sendn( m_sock->getFd(), std::move( m_output ) );
    if ( res > 0 )
    {
    /// @note 输出缓冲不为空或者应用层任务未完成前，应当保证服务器可写    
        if ( !m_output.empty() || !m_parseFin )
        {
            m_ch->enableWriting();
        }
        else if ( m_state == DisCOnnecting )
        {
            shutdownInLoop();
        }
    }
    else 
    {
        std::cerr << __func__ << " error " << std::endl;
    }
}

void TcpConnection::handleClose()
{
    m_loop->assertInCurrentThread();
    m_timer->delTimer();

    assert( m_state == Connected || m_state == DisCOnnecting ); 
    m_ch->disableAll();
    m_state = DisConnected;

    if ( m_closeCb )
    {
        TcpConnectionSP tcsp( shared_from_this() );
    /// @note 最后调用，保证在handleevent结束前tcpconnection不会被析构    
        m_closeCb( tcsp );
    }
}

void TcpConnection::handleError()
{
    int err = SocketUtil::Socket::getSocketError( m_ch->getFd() );
    std::cerr << "the connection with fd : " << m_ch->getFd() << " raise an error : " << err << std::endl;
}


void TcpConnection::timerFunc()
{
    m_loop->assertInCurrentThread();
    
    if ( m_timer->timer_type == Timer::TIMER_ONCE )
    {
        forceClose();
        // std::cout << __func__ << std::endl;
    }
    else
    {
        m_timer->adjustTimer();
    }
}
