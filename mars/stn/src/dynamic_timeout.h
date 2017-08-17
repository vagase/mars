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
*  dynamic_timeout.h
*  network
*
*  Created by caoshaokun on 15/10/28.
*  Copyright © 2015 Tencent. All rights reserved.
*/

#ifndef STN_SRC_DYNAMIC_TIMEOUT_H_
#define STN_SRC_DYNAMIC_TIMEOUT_H_

#include <bitset>
#include <string>
#include <sstream>

// 三个状态表示：正在计算状态，很好的状态，很差的状态。
enum DynamicTimeoutStatus {
    kEValuating = 1,
    kExcellent,
    kBad
};

namespace mars {
    namespace stn {

// 好大：用于动态超时计算，比如首包超时时间
/**
 * 好大：通过分析"历史数据包"发送接受情况，从"网速快慢"和"网速稳定"性两个层面去判断当前网络是"优秀、普通、差"中的哪一种。
 * 然后用这个网络状况来作为计算超时的依据，参见 __FirstPkgTimeout 。
 *
 * 有一个问题："超时是越长越好还是越短越好"？
 * 答案是：对于稳定但是延迟高的网络，超时越长越好；对于不稳定但是延迟低的网络，超时越短越好。
 *
 * 那么这里的关键点就是需要测试当前网络质量，包括"是否稳定"，"延迟高低"。最全备的方法就是不断给服务器发送数据，实时检测当前连接的质量，但很明显实际上是不可能这样做的。
 * 所以 Mars 中就采用了 DynamicTimeout 来检测网络的状况，根据网络的不同状况采取不同的超时。DynamicTimeout 是相比理论上的一个简化模型的实现。
 */
class DynamicTimeout {
    
  public:
    DynamicTimeout();
    virtual ~DynamicTimeout();

    /**
     * 好大：当网络变化的时候，需要重置状态，因为 DynamicTimeoutStatus 这能基于某一种网络下的历史数据包分析，不然就不准确了。
     */
    void ResetStatus();

    // 好大：统计 CGI 任务的耗时，参数中 _cgi_uri 并没有什么实际用处，用来打 log 。
    void CgiTaskStatistic(std::string _cgi_uri, unsigned int _total_size, uint64_t _cost_time);
    
    int GetStatus();
    
  private:
    void __StatusSwitch(std::string _cgi_uri, int _task_status);
    
  private:
    int                     dyntime_status_;
    unsigned int            dyntime_continuous_good_count_;     // 好大：dyntime_status_ 处于 kEValuating时，连续 MeetExpectTag 的次数。
    unsigned long           dyntime_latest_bigpkg_goodtime_;  //ms 好大：dyntime_status_ 处于 kEValuating时，最近一次大包 MeetExpectTag 的时间戳
    std::bitset<10>         dyntime_failed_normal_count_;       // 好大：这个是用来标记正常的次数，也就是 MeetExpectTag 的个数，和 dyntime_status_ 无关。
    unsigned long           dyntime_fncount_latstmodify_time_;    //ms 好大：每隔 5 分钟重置 dyntime_failed_normal_count_；设置为 0 强制重置。
    size_t                  dyntime_fncount_pos_;
};
        
    }
}

#endif // STN_SRC_DYNAMIC_TIMEOUT_H_
