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
 * timing_sync.h
 *
 *  Created on: 2012-10-19
 *      Author: zhouzhijie
 *  Copyright (c) 2013-2015 Tencent. All rights reserved.
 *
 */

#ifndef STN_SRC_TIMING_SYNC_H_
#define STN_SRC_TIMING_SYNC_H_

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "mars/baseevent/active_logic.h"
#include "mars/comm/alarm.h"
#include "mars/comm/thread/mutex.h"

#include "longlink.h"

namespace mars {
    namespace stn {

/**
 * 好大：TimingSync 最终调用的是 RequestSync，由应用层来发送同步请求（短链）。具体机制如下：
 *
 * 1. App Active 变化，网络状态变化，长连接断开的时候，开启 sync timer；
 * 2. 长连接连接上的时候，停止 sync timer
 *
 * TimingSync 的目的就是为了当长连接断开的时候，客户端不能和服务器同步状态，这里就可以通过 TimingSync 提供一种保底的同步时机。
 */
class TimingSync {
  public:
    TimingSync(ActiveLogic& _active_logic);
    ~TimingSync();

    void OnLongLinkStatuChanged(LongLink::TLongLinkStatus _status);
    void OnActiveChanged(bool _is_actived);
    void OnNetworkChange();

  private:
    void __OnAlarm();

  private:
    Alarm alarm_;

    ActiveLogic& active_logic_;
    boost::signals2::scoped_connection timing_sync_active_connection_;
};

    }
}

#endif // STN_SRC_TIMING_SYNC_H_
