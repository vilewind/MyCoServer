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
#include "Channel.h"
#include "EventLoop.h"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/epoll.h>
#include <cassert>

static const int BUFSIZE = 2048;

int recvn( int fd, std::string& str );
int sendn( int fd, std::string& str );

TcpConnection::TcpConnection(  EventLoop* loop, int fd )
    : m_loop( loop ),
      m_fd( fd ),
      m_ch( new Channel( m_loop, m_fd ))
{
    m_ch->setReadCb( std::bind( &TcpConnection::handleRead, this ) );
    m_ch->setWriteCb( std::bind( &TcpConnection::handleWrite, this ) );
    m_ch->setErrorCb( std::bind( &TcpConnection::handleError, this ) );

    m_input.clear();
    m_output.clear();
    m_inIdx = 0;
}

TcpConnection::~TcpConnection()
{
    m_ch->disableAll();
    m_ch->remove();
    if ( m_ch )
    {
        delete m_ch;
    }
}

void TcpConnection::addChannelToLoop()
{
/* 进行任务分发时在main eventloop所在线程，因此需要跨线程*/
    // m_ch->enableReading();
    m_loop->runInLoop([=]()
    {
         m_ch->enableReading();
         std::cout << __func__ << std::endl;
    } );
}

void TcpConnection::send( const std::string& str )
{
#ifdef DEBUG
    std::cout << str << std::endl;
#endif
    m_output += str;
    m_loop->runInLoop( std::bind( &TcpConnection::sendInLoop, shared_from_this() ) );
}

void TcpConnection::sendInLoop()
{
    if (m_isDisConn)
    {
        return;
    }

    handleWrite();
}

void TcpConnection::shutdown()
{
    m_loop->runInLoop( std::bind( &TcpConnection::shutdownInLoop, shared_from_this() ) );
} 

/**
 * @brief 半关闭，当对端可能调用close，也可能调用shutdown
*/
void TcpConnection::shutdownInLoop()
{
    if (m_isDisConn )
    {
        return;
    }

    if ( m_ch->getEvents() & (!EPOLLOUT) )
    {
        SocketUtil::Socket::halfClose( m_fd, SHUT_WR );
    }
}

void TcpConnection::forceClose()
{
    m_loop->runInLoop( std::bind( &TcpConnection::forceCloseInLoop, shared_from_this() ) );
}

void TcpConnection::forceCloseInLoop()
{
    if ( m_isDisConn )
    {
        return ;
    }

    m_loop->runInLoop( std::bind( m_cleanConnCb, shared_from_this() ) );  //无法自己清理自己，在TcpServer中处理
    m_ch->disableAll();
    m_ch->remove();
    ::close( m_fd );
    m_isDisConn = true;

    m_closeCb( shared_from_this() );                                      //应用层close函数                                          
}

void TcpConnection::handleRead()
{
    int res = recvn( m_fd, m_input );
    if ( res > 0 )
    {
        m_msgCb( shared_from_this() );            //应用层处理数据
    }
    else if (res < 0 )
    {
        handleError();
    } 
    else 
    {
        handleClose();
    }
}

void TcpConnection::handleWrite()
{
    int res = sendn( m_fd, m_output );
    if ( res >= 0 )
    {
        if ( !m_output.empty() )
        {
            m_ch->enableWriting();
        }
        else 
        {
            m_ch->disableWriting();
            shutdown();
        }
    }
    else if ( res < 0 )
    {
        handleError();
    }
}

void TcpConnection::handleError()
{
    if ( m_isDisConn )
    {
        return;
    }
    m_loop->runInLoop( std::bind( m_cleanConnCb, shared_from_this() ) );
    m_ch->disableAll();
    m_ch->remove();
    ::close( m_fd );
    m_errorCb( shared_from_this() );
     m_isDisConn = true;
}

void TcpConnection::handleClose()
{
    if ( m_isDisConn )
    {
        return ;
    }

    m_loop->assertInCurrentThread();
    
    m_isDisConn = true;
    
    m_ch->disableAll();

/* 读入数据未解析或未解析完成*/
    if ( !m_input.empty() || !m_parseFin )
    {
    /* 可能出现接收到数据后，立即接受到FIN标志，仍然要对这部分数据进行处理*/
        if ( !m_input.empty() )
        {
            m_msgCb( shared_from_this() );
        }
    }
    m_closeCb( shared_from_this() );
}

int recvn( int fd, std::string& str )
{
    ssize_t n = 0;
    size_t sum = 0;
    char buf[BUFSIZE];

    while( 1 )
    {
        ::memset(&buf, 0, sizeof buf );
        n = ::read( fd, buf, BUFSIZE );

        if ( n < 0 )
        {
        /* 读缓冲区为空，非阻塞返回*/    
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                return sum;
            }
            else if ( errno == EINTR )
            {
                continue;
            }
            else 
            {
                std::cerr << __func__ << " with errno : " <<  errno << std::endl;
                return -1;
            }
        } 
        else if ( n == 0)
        {
            return 0;
        }
        else 
        {
            str.append( buf );
            sum += n;
        /* 若读到的数据规模不足BUFSIZE，说明读缓冲区暂时为空，立即返回*/    
            if ( n < BUFSIZE )
            {
                return sum;
            }
        }
    }
}

int sendn( int fd , std::string& str )
{
    ssize_t n = 0;
    size_t sum = 0;
    size_t len = str.size();
    char* buf = const_cast<char*>( str.c_str() );

    while( 1 )
    {
        n = ::write( fd, buf + sum, len - sum );
        if ( n < 0 )
        {
            /* ignore SIGPIPE*/
            if ( errno == EAGAIN || errno == EWOULDBLOCK )
            {
                return sum;
            }
            else if ( errno == EINTR )
            {
                continue;
            }
            else 
            {
                std::cerr << __func__ << " with errno : " <<  errno << std::endl;
                return -1; 
            }
        }
        else if ( n > 0 )
        {
            sum += n;
            if ( sum == len )
            {
                str.clear();
                buf = nullptr;
                return sum;
            }
        }
        else {
            return 0;
        }
    }
}
