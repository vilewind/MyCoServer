/*=============================================================================
#
# Author: vilewind - luochengx2019@163.com
#
# Last modified: 2022-09-09 05:53
#
# Filename: Util.cpp
#
# Description: 
#
=============================================================================*/

#include "Util.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstring>

using namespace Util;
using namespace SocketUtil;

Addr::Addr(const char* ip, uint16_t port) 
{
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
}

Socket::Socket(int fd) : m_fd(fd) {}
Socket::~Socket() { Util::ERRIF(__func__, 0, ::close, m_fd); }

void Socket::bind(const Addr& addr) {
    ERRIF(__func__, 0, ::bind, m_fd, (struct sockaddr*)&addr.addr, addr.len);
}

void Socket::listen() {
    ERRIF(__func__, 0, ::listen, m_fd, 1024);
}

int Socket::accept(Addr& addr) {
    return ERRIF(__func__, 0, ::accept, m_fd, (struct sockaddr*)&addr.addr, &addr.len);
}

int Socket::accept(int serv_fd, Addr& addr) {
     return ERRIF(__func__, 0, ::accept, serv_fd, (struct sockaddr*)&addr.addr, &addr.len);

}

void Socket::shutdownWrite(bool flag) {
    if (flag) {
        halfClose(m_fd, SHUT_WR);
	}
}

void Socket::setReuseAddr(bool flag) {
    if (flag) {
        int op = 1;
        ERRIF(__func__, 0, ::setsockopt, m_fd, SOL_SOCKET, SO_REUSEADDR, &op, static_cast<socklen_t>(sizeof op));
    }
}

void Socket::setReusePort(bool flag) {
    if (flag) {
        int op = 1;
        ERRIF(__func__, 0, ::setsockopt, m_fd, SOL_SOCKET, SO_REUSEPORT, &op, static_cast<socklen_t>(sizeof op));
    }
}

void Socket::keepAlive(bool flag) {
    if (flag) {
        int op = 1;
        ERRIF(__func__, 0, ::setsockopt, m_fd, SOL_SOCKET, SO_KEEPALIVE, &op, static_cast<socklen_t>(sizeof op));
    }
}

int Socket::createNoblockSocket() {
    return ERRIF(__func__, 0, ::socket, AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
}

void Socket::halfClose(int fd, int op) {
    const char* err = nullptr;
    if (op == SHUT_RD) {
        err = "shutdown read";
    } else if (op == SHUT_WR) {
        err = "shutdown write";
    } else {
        err = "shutdown read and write";
    }
    ERRIF(err, 0, ::shutdown, fd, op);
}

void Socket::setNonblock(int fd) {
	int flag = fcntl(fd, F_GETFL);
	int new_flag = flag | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_flag);
}

int Socket::getSocketError( int fd )
{
    int opt = 0;
    socklen_t len = sizeof opt;
    if ( ::getsockopt( fd, SOL_SOCKET, SO_ERROR, &opt, &len ) )
    {
        return errno;
    }
    else
    {
        return opt;
    }
}


const static int BUFSIZE = 1024;
int Socket::recvn( int fd, std::string&& str )
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

int Socket::sendn( int fd , std::string&& str )
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
                if ( sum == len )
                {
                    str.clear();
                    buf = nullptr;
                }
                else
                {
                    str = str.substr( sum, len - sum + 1);
                }
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
