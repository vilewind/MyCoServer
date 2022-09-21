/*=============================================================================
#
# Author: vilewind - luochengx2018@163.com
#
# Last modified: 2022-09-05 11:51
#
# Filename: Util.h
#
# Description:基础工具类，包括socket的基础操作
#
=============================================================================*/
#ifndef __UTIL_H__
#define __UTIL_H__

#include <iostream>
#include <arpa/inet.h>

namespace Util
{
	/**
	 * @brief 实现不可复制类，但可移动
	*/
	class noncopyable
	{
	public:
		noncopyable() = default;
		~noncopyable() = default;
		
		noncopyable(const noncopyable&) = delete;
		noncopyable& operator=(const noncopyable) = delete;
	};

	/**
	 * @brief 错误提示
	 * @param err 错误信息
	 * @param flag 判断是否错误的标志
	 * @param func 执行的函数，如socket等
	 * @param args 执行函数的必要参数
	 * 
	 * @returns 执行函数的返回值
	*/
	template<typename Func, typename ...Args>
	auto ERRIF(const char* err, int flag, Func&& func, Args&& ...args)->decltype(func(args...)) {
		auto res = func(args...);
		if (res < flag) {
			std::cerr << err;
			exit(EXIT_FAILURE);
		} 
		return res;
	}
}

namespace SocketUtil
{
	struct Addr
	{
		struct sockaddr_in addr;
		socklen_t len { sizeof(addr)};
		Addr() = default;
		Addr(const char* ip, uint16_t port);
		~Addr() = default;
	};

	class Socket : Util::noncopyable
	{
	public:
		Socket(int fd);
		~Socket();

		void bind(const Addr& addr);

		void listen();

		int accept(Addr& addr);

		static int accept(int, Addr& adr);
		/**
		 * @brief 支持半关闭，服务器半关闭写
		*/
		void shutdownWrite(bool flag = true);
		/**
		 * @brief 支持地址重用
		*/
		void setReuseAddr(bool flag = true);
		/**
		 * @brief 支持端口重用
		*/
		void setReusePort(bool flag = true);
		/**
		 * @brief 支持keep-alive
		*/
		void keepAlive(bool flag = true);

		int getFd() const { return m_fd; }

		static int createNoblockSocket();

		static void halfClose(int fd, int op);

		static void setNonblock(int fd);

		static int getSocketError( int fd );
	private:	
		const int m_fd;
	};
}

#endif

