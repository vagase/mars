// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.


/*
 * longlink_speed_test.h
 *
 *  Created on: 2013-5-13
 *      Author: yanguoyue
 */

#ifndef STN_SRC_LONGLINK_SPEED_TEST_H_
#define STN_SRC_LONGLINK_SPEED_TEST_H_

#include <string>
#include <vector>

#include "boost/shared_ptr.hpp"

#include "mars/comm/autobuffer.h"
#include "mars/comm/socket/socketselect.h"
#include "mars/comm/socket/unix_socket.h"

#include "net_source.h"

enum ELongLinkSpeedTestState {
    kLongLinkSpeedTestConnecting,
    kLongLinkSpeedTestReq,
    kLongLinkSpeedTestResp,
    kLongLinkSpeedTestOOB,
    kLongLinkSpeedTestSuc,
    kLongLinkSpeedTestFail,
};

namespace mars {
    namespace stn {

/**
 * 好大：LongLinkSpeedTestItem 工作原理：
 * 建立一个 socket 连接，然后给服务器发送一个心跳包（标准心跳包），然后服务器回一个正常的心跳包，然后将状态标记为 success。
 * 没有其他测试指标，就只有成功失败，当然还有 GetConnectTime 而已。
 * 如果服务器返回了的 command id 是 oob (out of band)，那么客户端立即再发送一个全新的心跳包过去。
 *
 * 那么什么时候标记为失败呢，有如下几种情况：
 * 1. socket send / recv 出错
 * 2. 解码服务器返回的心跳包时出错
 * 3. 服务器response 的 command id 不是 kCmdIdOutOfBand，longlink_noop_resp_cmdid
 * 4. 服务器 timeout 内没有返回值，timeout 一般是 10s
 *
 * LongLinkSpeedTestItem 作为基础元件，供给 LongLinkSpeedTest 、NetSourceTimerCheck 使用
 */
class LongLinkSpeedTestItem {
  public:
    LongLinkSpeedTestItem(const std::string& _ip, uint16_t _port);
    ~LongLinkSpeedTestItem();

    void HandleFDISSet(SocketSelect& _sel);
    void HandleSetFD(SocketSelect& _sel);

    int GetSocket();
    std::string GetIP();
    unsigned int GetPort();
    unsigned long GetConnectTime();
    int GetState();

    void CloseSocket();

  private:
    int __HandleSpeedTestReq();
    int __HandleSpeedTestResp();

  private:
    std::string ip_;
    unsigned int port_;
    SOCKET socket_;
    int state_;

    uint64_t before_connect_time_;
    uint64_t after_connect_time_;

    AutoBuffer req_ab_;
    AutoBuffer resp_ab_;
};

class LongLinkSpeedTest {
  public:
    LongLinkSpeedTest(const boost::shared_ptr<NetSource>& _netsource);
    ~LongLinkSpeedTest();

    bool GetFastestSocket(int& _fdSocket, std::string& _strIp, unsigned int& _port, IPSourceType& _type, unsigned long& _connectMillSec);

    boost::shared_ptr<NetSource> GetNetSource();
  private:
    boost::shared_ptr<NetSource> netsource_;
    SocketSelectBreaker breaker_;
    SocketSelect selector_;
};
        
    }
}


#endif // STN_SRC_LONGLINK_SPEED_TEST_H_
