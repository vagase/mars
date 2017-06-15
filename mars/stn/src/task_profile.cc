// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.

//
//  task_profile.c
//  stn
//
//  Created by yerungui on 16/3/28.
//  Copyright © 2016年 Tencent. All rights reserved.
//

#include "mars/comm/xlogger/xlogger.h"
#include "mars/comm/platform_comm.h"
#include "mars/stn/task_profile.h"

#include "dynamic_timeout.h"

namespace mars {
namespace stn {

void __SetLastFailedStatus(std::list<TaskProfile>::iterator _it){
    if (_it->remain_retry_count > 0) {
        _it->last_failed_dyntime_status = _it->current_dyntime_status;
    }
}

 uint64_t __ReadWriteTimeout(uint64_t  _first_pkg_timeout) {
    uint64_t rate = (kMobile != getNetInfo()) ? kWifiMinRate : kGPRSMinRate;
    return  _first_pkg_timeout + 1000 * kMaxRecvLen / rate;
}

/**
 * 好大：首包超时和包包超时的关系，微信团队原话：https://mp.weixin.qq.com/s/PnICVDyVuMSyvpvTrdEpSQ
 *
 * 首包超时缩短了发现问题的周期，但是我们发现如果首个数据分段按时到达，而后续数据包丢失的情况下，仍然要等待整个读写超时才能发现问题。为此我们引入了包包超时，即两个数据分段之间的超时时间。因为包包超时在首包超时之后，这个阶段已经确认服务器收到了请求，且完成了请求的处理，因此不需要计算等待耗时、请求传输耗时、服务器处理耗时，只需要估算网络的 RTT。
 */

/**
 * 好大：计算首包超时的时间，整体而言，如果知道网络状况好，那么首包超时时间应该比不知道网络状况（或者网络状况差）的时候要短。
 * _init_first_pkg_timeout 就是 task->server_process_cost，默认值是 -1； _init_first_pkg_timeout 为 0，表示采用固定的首包超时时间。
 * _sendlen 表示这个包的大小
 * _send_count 表示 LongLinkTaskManager、ShortLinkTaskManager 队列中这个包之前，已经发送但是仍然还没有返回的包数量。
 *
 * 首包超时时间不同策略对比：
 *
 *                       1K Package                                           10K Package
 *
 *    ┌───────────────┬───────────────┬───────────────┐     ┌───────────────┬───────────────┬───────────────┐
 *    │               │     WIFI      │     4G/3G     │     │               │     WIFI      │     4G/3G     │
 *    ├───────────────┼───────────────┼───────────────┤     ├───────────────┼───────────────┼───────────────┤
 *    │   EXCELLENT   │      7s       │      10S      │     │   EXCELLENT   │      7s       │      10S      │
 *    ├───────────────┼───────────────┼───────────────┤     ├───────────────┼───────────────┼───────────────┤
 *    │  NORMAL/BAD   │    0.1+12S    │    0.3+15S    │     │  NORMAL/BAD   │     1+12S     │     3+15S     │
 *    └───────────────┴───────────────┴───────────────┘     └───────────────┴───────────────┴───────────────┘
 *
 */
uint64_t  __FirstPkgTimeout(int64_t  _init_first_pkg_timeout, size_t _sendlen, int _send_count, int _dynamictimeout_status) {
    xassert2(3600 * 1000 >= _init_first_pkg_timeout, TSF"server_cost:%_ ", _init_first_pkg_timeout);
    
    uint64_t ret = 0;

    // 好大：kWifiTaskDelay： 1.5s；kGPRSTaskDelay：3s
    uint64_t task_delay = (kMobile != getNetInfo()) ? kWifiTaskDelay : kGPRSTaskDelay;

    /**
     * 好大：微信团队原话：https://mp.weixin.qq.com/s/PnICVDyVuMSyvpvTrdEpSQ
     * 进入Exc状态后，就缩短信令收发的预期，即减小首包超时时间，这样做的原因是我们认为用户的网络状况好，可以设置较短的超时时间，当遇到网络波动时预期它能够快速恢复，所以可以尽快超时然后进行重试，从而改善用户体验。
     */

    /**
     * 好大：已知网络状态很好，且task采用默认固定首包超时策略。这个超时比一般采用的默认超时 kBaseFirstPackageWifiTimeout／kBaseFirstPackageGPRSTimeout 要短
     */
    if (_dynamictimeout_status == kExcellent && _init_first_pkg_timeout == 0) {
        // 好大：这个策略也太粗糙点了吧，_sendlen 都没考虑不管数据包大小一视同仁。
        ret = (kMobile != getNetInfo()) ? kDynTimeFirstPackageWifiTimeout /* 7s */ : kDynTimeFirstPackageGPRSTimeout /* 10s */;
        ret += _send_count * task_delay;
    }
    // 好大：动态超时还在计算中或者处在质量不是 excellent 的网络下，根据："网络类型" + "数据包大小" 来计算一个相对""
    else{
        // 好大：kWifiMinRate：10k/s；kGPRSMinRate：3K/s
        uint64_t rate = (kMobile != getNetInfo()) ? kWifiMinRate : kGPRSMinRate;
        // 好大：kBaseFirstPackageWifiTimeout：12s；kBaseFirstPackageGPRSTimeout：15s
        uint64_t base_rw_timeout = (kMobile != getNetInfo()) ? kBaseFirstPackageWifiTimeout : kBaseFirstPackageGPRSTimeout;
        // 好大：kMaxFirstPackageWifiTimeout：25s；kMaxFirstPackageGPRSTimeout：35s
        uint64_t max_rw_timeout = (kMobile != getNetInfo()) ? kMaxFirstPackageWifiTimeout : kMaxFirstPackageGPRSTimeout;

        // 好大：设置了首包超时时间: "首包超时时间 + 包大小／最低网速"
        if (0 < _init_first_pkg_timeout) {
            ret = _init_first_pkg_timeout + 1000 * _sendlen / rate;
        }
        // 好大：没有设置首包超时时间："默认首包超时时间 + 包大小／最低网速"
        else {
            ret =     base_rw_timeout + 1000 * _sendlen / rate;
            ret = ret < max_rw_timeout ? ret : max_rw_timeout;
        }
        
        ret += _send_count * task_delay;
    }
    
    return ret;
}

bool __CompareTask(const TaskProfile& _first, const TaskProfile& _second) {
    return _first.task.priority < _second.task.priority;
}

}}
