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
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>

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