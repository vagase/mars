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
 * netsource_timercheck.h
 *
 *  Created on: 2013-5-16
 *      Author: yanguoyue
 */

#ifndef STN_SRC_NETSOURCE_TIMERCHECK_H_
#define STN_SRC_NETSOURCE_TIMERCHECK_H_

#include "boost/signals2.hpp"

#include "mars/comm/thread/thread.h"
#include "mars/baseevent/active_logic.h"
#include "mars/comm/socket/socketselect.h"
#include "mars/comm/messagequeue/message_queue_utils.h"
#include "mars/comm/messagequeue/message_queue.h"

#include "net_source.h"

class CommFrequencyLimit;

namespace mars {
    namespace stn {
        
class LongLink;

/**
 * 好大：NetSourceTimerCheck 工作原理如下：
 * 当 App active 的时候，启动一个每"2分半钟"触发一次的计时器，并对 longlink connect profile 中 host 进行 speed test；当非 active 的时候就将计时器停止掉。
 * 但这个 test 正常情况下并不进行，只有如下几个条件才做：
 * 1. IPSourceType 是 kIPSourceProxy 或 kIPSourceBackup，正常情况的 kIPSourceDebug，kIPSourceDNS，kIPSourceNewDns 都不检查
 * 2. 当通过 NEWDNS、DNS 获得的 IP 地址列表，不包含当前的 IP （proxy、backup）
 *
 * 为什么要设置这样的规则？因为 NetSourceTimerCheck 的目的是为了将正在使用 proxy、backup IP的长连接，尝试切换回 NEWDNS、DNS 得到的正常 IP 的链路。
 * 因为大部分情况下，proxy、backup 只是用来做保底的，NEWDNS、DNS 得到的 IP 质量应该比他们高，应该优先选择。所以当长连接在使用保底IP（不管保底IP是否能够连接上），
 * 如果通过 NetSourceTimerCheck 检测到了可以连接的 NEWDNS DNS IP 的时候，长连接应该立马切换回来。这也是为什么 fun_time_check_suc_ 是让长连接断开重连的原因所在。
 *
 * 当检测到状态为 success 的时候，会做两件事情：
 * 1. 将可以连接成功给 ip 移除被屏蔽状态
 * 2. 调用 fun_time_check_suc_，即 longlink_task_manager_->LongLinkChannel().Disconnect(LongLink::kTimeCheckSucc);
 */
class NetSourceTimerCheck {
  public:
    NetSourceTimerCheck(NetSource* _net_source, ActiveLogic& _active_logic, LongLink& _longlink, MessageQueue::MessageQueue_t  _messagequeue_id);
    ~NetSourceTimerCheck();

    /**
     * 好大：如下两种情况会调用 CancelConnect
     * 1. 网络状况发生变化的时候 NetCore::OnNetworkChange
     * 2. 手动调用 RedoTasks 的时候 NetCore::RedoTasks
     */
    void CancelConnect();

  public:

    // 好大：会调用 longlink_task_manager_->LongLinkChannel().Disconnect(LongLink::kTimeCheckSucc);
    boost::function<void ()> fun_time_check_suc_;

  private:
    void __Run(const std::string& _host);
    bool __TryConnnect(const std::string& _host);
    void __OnActiveChanged(bool _is_active);
    void __StartCheck();
    void __Check();
    void __StopCheck();

  private:
    Thread thread_;
    boost::signals2::scoped_connection active_connection_;
    NetSource* net_source_;
    SocketSelectBreaker breaker_;
    SocketSelect seletor_;
    CommFrequencyLimit* frequency_limit_;
    LongLink& longlink_;

    MessageQueue::ScopeRegister asyncreg_;
    MessageQueue::MessagePost_t asyncpost_;
    NetSource::DnsUtil dns_util_;
};
        
    }
}


#endif // STN_SRC_NETSOURCE_TIMERCHECK_H_
