// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.


#ifndef __UNIX_SOCKET_H_
#define  __UNIX_SOCKET_H_

#if (defined(WP8) || defined(WIN32))
#include <stdint.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <net/if.h>
#include <unistd.h>

#endif

#ifdef _WIN32

#include "winsock2.h"

#define SOCKET_ERRNO(error) WSA##error

#define socket_close closesocket

#define socket_errno WSAGetLastError()
#define socket_strerror gai_strerror


#ifdef __cplusplus
extern "C" {
#endif

const char*  socket_inet_ntop(int af, const void* src, char* dst, unsigned int size);
int          socket_inet_pton(int af, const char* src, void* dst);

#ifdef __cplusplus
}
#endif

#define send(fd, buf, len, flag) send(fd, (const char *)buf, len, flag)
#define recv(fd, buf, len, flag) recv(fd, (char *)buf, len, flag)
#define bzero(addr, size) memset(addr, 0, size)
#define getsockopt(s, level, optname, optval, optlen) getsockopt(s, level, optname, (char*)optval, optlen)

#define IS_NOBLOCK_CONNECT_ERRNO(err) ((err) == SOCKET_ERRNO(EWOULDBLOCK))

#define IS_NOBLOCK_SEND_ERRNO(err) IS_NOBLOCK_WRITE_ERRNO(err)
#define IS_NOBLOCK_RECV_ERRNO(err) IS_NOBLOCK_READ_ERRNO(err)

#define IS_NOBLOCK_WRITE_ERRNO(err) ((err) == SOCKET_ERRNO(EWOULDBLOCK))
#define IS_NOBLOCK_READ_ERRNO(err)  ((err) == SOCKET_ERRNO(EWOULDBLOCK))


typedef uint32_t in_addr_t;
typedef SSIZE_T ssize_t;
typedef int socklen_t;
typedef unsigned short in_port_t;

#else

#define SOCKET_ERRNO(error) error

#define socket_errno errno
#define socket_strerror strerror

#define socket_close close

// 好大：addr -> string. now get it back and print itinet_ntop(AF_INET, &(sa.sin_addr), str, INET_ADDRSTRLEN);
#define socket_inet_ntop inet_ntop
// 好大：string -> addr. store this IP address in sa: inet_pton(AF_INET, "192.0.2.33", &(sa.sin_addr));
#define socket_inet_pton inet_pton

#define SOCKET int
#define INVALID_SOCKET -1

#define IS_NOBLOCK_CONNECT_ERRNO(err) ((err) == SOCKET_ERRNO(EINPROGRESS))

#define IS_NOBLOCK_SEND_ERRNO(err) IS_NOBLOCK_WRITE_ERRNO(err)
#define IS_NOBLOCK_RECV_ERRNO(err) IS_NOBLOCK_READ_ERRNO(err)

#define IS_NOBLOCK_WRITE_ERRNO(err) ((err) == SOCKET_ERRNO(EAGAIN) || (err) == SOCKET_ERRNO(EWOULDBLOCK))
#define IS_NOBLOCK_READ_ERRNO(err)  ((err) == SOCKET_ERRNO(EAGAIN) || (err) == SOCKET_ERRNO(EWOULDBLOCK))

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 好大：非阻塞模式
 */
int socket_set_nobio(SOCKET fd);

/*
 * 好大：设置 tcp 包 maximum segment size
 */
int socket_set_tcp_mss(SOCKET sockfd, int size);
int socket_get_tcp_mss(SOCKET sockfd, int* size);

/**
 * 好大：写网络程序躲不开运营商。印象比较深刻的某地的用户反馈连接 WiFi 时，微信不可用，后来 tcpdump 发现，当包的大小超过一定大小后就发不出去。解决方案：在 WiFi 网络下强制把 MSS 改为1400(代码见unix_socket.cc)。
 */
int socket_fix_tcp_mss(SOCKET sockfd);    // make mss=mss-40

/**
 * 好大：TCP_NODELAY - used to disable nagle's algorithm to improve tcp/ip networks and decrease the number of packets by waiting until an acknowledgment of previous sent data is received to send the accumulated packets.
 */
int socket_disable_nagle(SOCKET sock, int nagle);
int socket_error(SOCKET sock);

/**
 * 好大：
 * 1. 这个套接字选项通知内核，如果端口忙，但TCP状态位于 TIME_WAIT ，可以重用端口。如果端口忙，而TCP状态位于其他状态，重用端口时依旧得到一个错误信息， 指明"地址已经使用中"。如果你的服务程序停止后想立即重启，而新套接字依旧使用同一端口，此时 SO_REUSEADDR 选项非常有用。
 * 2. 允许在同一端口上启动同一服务器的多个实例，只要每个实例捆绑一个不同的本地IP地址即可。对于TCP，我们根本不可能启动捆绑相同IP地址和相同端口号的多个服务器。
 */
int socket_reuseaddr(SOCKET sock, int optval);

/**
 * 好大：Get number of bytes currently in send socket buffer
 */
int socket_get_nwrite(SOCKET _sock, int* _nwriteLen);

/**
 * 好大：APPLE: get 1st-packet byte count
 */
int socket_get_nread(SOCKET _sock, int* _nreadLen);
int socket_nwrite(SOCKET _sock);
int socket_nread(SOCKET _sock);
/*
 https://msdn.microsoft.com/zh-cn/library/windows/desktop/bb513665(v=vs.85).aspx
 Dual-Stack Sockets for IPv6 Winsock Applications
 By default, an IPv6 socket created on Windows Vista and later only operates over the IPv6 protocol. In order to make an IPv6 socket into a dual-stack socket, the setsockopt function must be called with the IPV6_V6ONLY socket option to set this value to zero before the socket is bound to an IP address. When the IPV6_V6ONLY socket option is set to zero, a socket created for the AF_INET6 address family can be used to send and receive packets to and from an IPv6 address or an IPv4 mapped address.
 */
int socket_ipv6only(SOCKET _sock, int _only);

int socket_isnonetwork(int error);

#ifdef __cplusplus
}
#endif
#endif
