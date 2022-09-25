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
#include "TimerWheel.h"
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <sys/epoll.h>
#include <cassert>

static const int BUFSIZE = 2048;
static const int TIMEOUT = 8;

int recvn( int fd, std::string& str );
int sendn( int fd, std::string& str );

#ifdef TEST
    TcpConnection::TcpConnection(  EventLoop* loop, int fd )
    : m_loop( loop ),
      m_fd( fd ),
      m_ch( new Channel( m_loop, m_fd )),
      m_timer( nullptr ),
      m_co ( m_loop->getCoroutineInstanceInCurrentLoop() )
{
    m_ch->setReadCb( std::bind( &TcpConnection::handleRead, this ) );
    m_ch->setWriteCb( std::bind( &TcpConnection::handleWrite, this ) );
    m_ch->setErrorCb( std::bind( &TcpConnection::handleError, this ) );
    m_ch->setCloseCb( std::bind( &TcpConnection::handleClose, this ) );

    m_input.clear();
    m_output.clear();
    m_inIdx = 0;
}
#elif
TcpConnection::TcpConnection(  EventLoop* loop, int fd )
    : m_loop( loop ),
      m_fd( fd ),
      m_ch( new Channel( m_loop, m_fd )),
      m_timer( new Timer( TIMEOUT, std::bind( &TcpConnection::timerFunc, this ) )),
      m_co ( m_loop->getCoroutineInstanceInCurrentLoop() )s
{
    m_ch->setReadCb( std::bind( &TcpConnection::handleRead, this ) );
    m_ch->setWriteCb( std::bind( &TcpConnection::handleWrite, this ) );
    m_ch->setErrorCb( std::bind( &TcpConnection::handleError, this ) );
    m_ch->setCloseCb( std::bind( &TcpConnection::handleClose, this ) );

    m_input.clear();
    m_output.clear();
    m_inIdx = 0;
}
#endif

TcpConnection::~TcpConnection()
{
    assert( m_isDisConn );
    if ( m_ch )
    {
        m_ch->remove();
        // delete m_ch;
        // m_ch = nullptr;
        Util::Delete<Channel>( m_ch );
    }
    ::close( m_fd );
/* timer不在时间轮中*/
    // if ( m_timer )
    // {
    //     // assert( !m_timer->next && !m_timer->prev );
    //     delete m_timer;
    // }
    Util::Delete<Timer>( m_timer );
}

void TcpConnection::addChannelToLoop()
{
/* 进行任务分发时在main eventloop所在线程，因此需要跨线程*/
    // m_ch->enableReading();
    if (m_timer)
    {
        m_timer->addInToWheel();
    }
    m_loop->runInLoop([=]()
    {
         m_ch->enableReading();
    } );
}

void TcpConnection::send( const std::string& str )
{
#ifdef DEBUG
    std::cout << str << std::endl;
#endif
    assert( m_isDisConn == false );
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

    if ( m_ch->getEvents() & EPOLLOUT )
    {   
        m_ch->disableWriting();
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
    m_isDisConn = true;
    m_ch->disableAll();
        
    TcpConnectionSP tcsp( shared_from_this() );
    m_closeCb( shared_from_this() );                                      
}

void TcpConnection::handleRead()
{
    if ( m_timer )
    {
          m_timer->adjustTimer();
    }
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
     if ( m_timer )
    {
          m_timer->adjustTimer();
    }
    std::cout << m_output << std::endl;
    int res = sendn( m_fd, m_output );
    if ( res > 0 )
    {
        if ( !m_output.empty() )
        {
            m_ch->enableWriting();
        }
    /* 可能有读入的数据需要处理（解析或数据不完整需要继续读取）*/    
        else if ( !m_closing )  
        {
            shutdown();
        }
        else 
        {
            handleClose();
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
    int err = SocketUtil::Socket::getSocketError( m_fd );
    std::cerr << "the connetion with fd : " << m_fd << " raise an error : " << err << std::endl;
    forceClose();
}

/**
 * @brief 对端使用close或shutdown时，都会发送FIN；服务器端，保证数据发送完成或解析完成后彻底断开连接
*/
void TcpConnection::handleClose()
{
    if ( m_isDisConn )
    {
        return ;
    }

    m_loop->assertInCurrentThread();

/* 接收到对端FIN后，读入数据未解析或未解析完成*/
    if ( !m_output.empty() ||  !m_input.empty() || !m_parseFin )
    {
        m_closing = true;
        // if ( m_ch->getEvents() & EPOLLOUT )
        // {
        //     m_ch->disableWriting();
        //     shutdown();
        // }
    /* 可能出现接收到数据后，立即接受到FIN标志，仍然要对这部分数据进行处理*/
        if ( !m_input.empty() )
        {
            m_msgCb( shared_from_this() );
        }
    } 
    else
    {
        m_isDisConn = true;
        m_ch->disableAll();
        m_timer->delFromWheel();
    /// @note 在断开连接处理connection时，仍在handleevent函数内，增加引用计数是为了防止tcpconnection提前析构，出现sgement fault    
        TcpConnectionSP tcsp( shared_from_this() );
        m_closeCb( shared_from_this() );
    } 
}

void TcpConnection::timerFunc()
{
    if ( m_timer == nullptr )
    {
        return;
    }
    else if ( m_timer->timer_type == Timer::TIMER_ONCE )
    {
        // std::cout << __func__ << " " << tcsp->getFd() << std::endl;
        forceClose();
    }
    else
    {
        m_timer->adjustTimer();
    }
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
