/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 09:19
#
# Filename: Server.cpp
#
# Description: 
#
=============================================================================*/

#include "TcpServer.h"
#include <functional>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <sys/epoll.h>
#include <cassert>

const static int MAX_CONN = 1000;

TcpServer::TcpServer( EventLoop* loop, const int threadNum, const char* ip, const uint16_t port )
    : m_loop( loop ),
      m_addr( new SocketUtil::Addr( ip, port ) ),
      m_sock( new SocketUtil::Socket( SocketUtil::Socket::createNoblockSocket() ) ),
      m_acceptor( new Channel( m_loop, m_sock->getFd() ) ),
      m_ioPool( new IOThreadPool( threadNum ))
{
    m_sock->setReuseAddr();
    m_sock->bind( *m_addr );
    m_sock->listen();

    m_acceptor->setReadCb( std::bind( &TcpServer::acceptNewConn, this ) );
    m_acceptor->setErrorCb( std::bind( &TcpServer::dealErroOnConn, this ) );
}

TcpServer::~TcpServer()
{
    m_acceptor->disableAll();
    m_acceptor->remove();
    delete m_acceptor;
    delete m_addr;
    delete m_sock;
    delete m_acceptor;
    delete m_ioPool;
}

/// @brief 在开启tcpserver前，需要设置应用层需要的回调函数，因此不可以直接在构造函数中enablereading
void TcpServer::start()
{
  m_acceptor->enableReading();
}

void TcpServer::acceptNewConn()
{
    SocketUtil::Addr addr;
    ::memset( &addr.addr, 0, sizeof addr.addr );

    int fd = SocketUtil::Socket::accept( m_acceptor->getFd(), addr );
    SocketUtil::Socket::setNonblock( fd );

/* io线程池为空，那么使用主线程loop*/
    EventLoop* loop = m_ioPool->getEventLoop( fd );
    if ( loop == nullptr )
    {
        loop = m_loop;
    }

    TcpConnectionSP tcsp = std::make_shared<TcpConnection>( loop, fd );
    tcsp->setMsgCb( std::bind( &TcpServer::handleMsg, this, std::placeholders::_1 ) );
    tcsp->setCloseCb( m_closeCb );
    tcsp->setErrorCb( m_errorCb );
    tcsp->setDelConnCb( std::bind( &TcpServer::cleanConn, this, std::placeholders::_1 ) );
    /* add timer */
    {
        std::lock_guard<std::mutex> locker( m_mtx );
        m_tcpConns[fd] = tcsp;
    }

    tcsp->addChannelToLoop();
    m_acceptCb( tcsp );
}

void TcpServer::handleMsg( TcpConnectionSP tcsp )
{
    /* adjust timer */
    m_msgCb( tcsp );
}

void TcpServer::cleanConn( TcpConnectionSP tcsp )
{

    {
         std::lock_guard<std::mutex> locker( m_mtx );
         m_tcpConns.erase( tcsp->getFd() );
    }
}

void TcpServer::dealErroOnConn()
{
    std::cout << "the error on fd is " << SocketUtil::Socket::getSocketError( m_sock->getFd() ) << std::endl;
}