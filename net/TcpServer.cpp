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

TcpServer::TcpServer( EventLoop* loop, const int threadNum, const char* ip, const uint16_t port )
    : m_loop( loop ),
      m_addr( std::make_unique<SocketUtil::Addr>( ip, port )  ),
      m_sock( std::make_unique<SocketUtil::Socket>( SocketUtil::Socket::createNoblockSocket() ) ),
      m_acceptor( std::make_unique<Channel>( m_loop, m_sock->getFd() ) ),
      m_IOTP( std::make_unique<IOThreadPool>( threadNum ) )
{
    m_sock->setReuseAddr();
    m_sock->bind( *m_addr );
    m_sock->listen();

    m_acceptor->setReadCb( std::bind( &TcpServer::acceptorFunc, this ) );

    if ( threadNum == 0 )
    {
        m_loop->enableTimer();
    }
}

TcpServer::~TcpServer()
{
    m_acceptor->disableAll();
    m_acceptor->remove();
}

void TcpServer::start()
{
    m_acceptor->enableReading( false );     //LT
}

void TcpServer::acceptorFunc()
{
     SocketUtil::Addr addr;
    ::memset( &addr.addr, 0, sizeof addr.addr );

    int fd = SocketUtil::Socket::accept( m_acceptor->getFd(), addr );
    SocketUtil::Socket::setNonblock( fd );

/* io线程池为空，那么使用主线程loop*/
    EventLoop* loop = m_IOTP->getEventLoop( fd );
    if ( loop == nullptr )
    {
        loop = m_loop;
    }

    TcpConnectionSP tcsp = std::make_shared<TcpConnection>( loop, fd );
    if ( tcsp == nullptr )
    {
        std::cout << "new connection establish failed for make share error " << std::endl;
        return;
    }

    if ( m_initConnCb )
    {
        m_initConnCb( tcsp );
    }
    
    m_conns[fd] = tcsp;
    tcsp->setMsgCb( m_msgCb );
    tcsp->setCloseCb( std::bind( &TcpServer::handleCleanConn, this, std::placeholders::_1 ) );

    loop->runInLoop( std::bind( &TcpConnection::establishConnection, tcsp ) );
}

void TcpServer::handleCleanConn( const TcpConnectionSP& tcsp )
{
    m_loop->runInLoop( [=]()
    {
        m_loop->assertInCurrentThread();

        m_conns.erase( tcsp->getFd() );
        EventLoop* loop = tcsp->getLoop();

        loop->queueInLoop( std::bind( &TcpConnection::destroyConnection, tcsp ) );

    } );
}

// #define TCPTEST 
#ifdef TCPTEST

void msgCallback( Channel* ch )
{
    char buf[1024];
    int res = -1;
    int fd = ch->getFd();
    ::memset(buf, 0, sizeof buf );
    res = ::read( fd, buf, sizeof buf );
    std::cout << buf << std::endl;
    if ( res > 0)
    {
        ::write( fd, buf, res );   
    }
    else if ( res == 0 )
    {
        ch->disableAll();
        ch->remove();
        ::close( fd );
    }
}

void TcpServer::acceptorFunc()
{
    SocketUtil::Addr addr;
    ::memset( &addr.addr, 0, sizeof addr.addr );

    int fd = SocketUtil::Socket::accept( m_acceptor->getFd(), addr );
    std::cout << fd << std::endl;
    SocketUtil::Socket::setNonblock( fd );
    Channel* ch = new Channel( m_loop, fd );
    ch->setReadCb( [=]()
    {
        msgCallback( ch );
    });
    ch->enableReading();
}

int main()
{
    EventLoop el;
    TcpServer ts( &el );
    ts.start();
    try
    {
        el.loop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    return 0;
}

#endif
