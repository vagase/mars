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
 * longlink_connect_monitor.cc
 *
 *  Created on: 2014-2-26
 *      Author: yerungui
 */

#include "longlink_connect_monitor.h"

#include "boost/bind.hpp"

#include "mars/app/app.h"
#include "mars/baseevent/active_logic.h"
#include "mars/comm/thread/lock.h"
#include "mars/comm/xlogger/xlogger.h"
#include "mars/comm/time_utils.h"
#include "mars/comm/socket/unix_socket.h"
#include "mars/comm/platform_comm.h"
#include "mars/sdt/src/checkimpl/dnsquery.h"
#include "mars/stn/config.h"

#include "longlink_speed_test.h"
#include "net_source.h"

using namespace mars::stn;
using namespace mars::app;

static const unsigned int kTimeCheckPeriod = 10 * 1000;     // 10s
static const unsigned int kStartCheckPeriod = 3 * 1000;     // 3s

static const unsigned long kNoNetSaltRate = 3;
static const unsigned long kNoNetSaltRise = 600;
static const unsigned long kNoAccountInfoSaltRate = 2;
static const unsigned long kNoAccountInfoSaltRise = 300;

static const unsigned long kNoAccountInfoInactiveInterval = (7 * 24 * 60 * 60);  // s

enum {
    kTaskConnect,           // 好大：手动调用 MakeSureConnected
    kLongLinkConnect,       // 好大：__OnSignalForeground，__OnSignalActive， __OnLongLinkStatuChanged
    kNetworkChangeConnect,  // 好大：NetworkChange
};

enum {
    kForgroundOneMinute,    // 好大：1 分钟内的 foreground active
    kForgroundTenMinute,    // 好大：1-10分钟的 foreground active
    kForgroundActive,       // 好大：10 分钟以上的 foreground active
    kBackgroundActive,      // 好大：_activeLogic.IsActive() && !_activeLogic.IsForeground()
    kInactive,              // 好大：!_activeLogic.IsActive()
};

/*
 * 好大：
 *                          前台1分钟   前台10分钟  前台超过10分钟    后台活跃    后台不活跃
 * kTaskConnect             5 秒       10 秒      20 秒           30秒        5 分钟
 * kLongLinkConnect         15 秒      30 秒      4 分钟           5 分钟      10 分钟
 * kNetworkChangeConnect：   立刻执行
 */
static unsigned long const sg_interval[][5]  = {
    {5,  10, 20,  30,  300},
    {15, 30, 240, 300, 600},
    {0,  0,  0,   0,   0},
};

static int __CurActiveState(const ActiveLogic& _activeLogic) {
    if (!_activeLogic.IsActive()) return kInactive;

    if (!_activeLogic.IsForeground()) return kBackgroundActive;

    if (10 * 60 * 1000 <= ::gettickcount() - _activeLogic.LastForegroundChangeTime()) return kForgroundActive;

    if (60 * 1000 <= ::gettickcount() - _activeLogic.LastForegroundChangeTime()) return kForgroundTenMinute;

    return kForgroundOneMinute;
}

/*
 * 好大：根据类型（任务，长连接，网络变化）+ 状态（前台1分钟，前台10分钟，前台超过10分钟，后台活跃，后台不活跃）  
 */
static unsigned long __Interval(int _type, const ActiveLogic& _activelogic) {
    unsigned long interval = sg_interval[_type][__CurActiveState(_activelogic)];

    if (kLongLinkConnect != _type) return interval;
    
    // 好大：针对长连接，后台不活跃 或者 前台活跃超过 10 分钟
    if (__CurActiveState(_activelogic) == kInactive || __CurActiveState(_activelogic) == kForgroundActive) {  // now - LastForegroundChangeTime>10min
        // 好大：不活跃且没登录：一周
        if (!_activelogic.IsActive() && GetAccountInfo().username.empty()) {
            interval = kNoAccountInfoInactiveInterval;
            xwarn2(TSF"no account info and inactive, interval:%_", interval);

        }
        // 好大：没有网络：3倍 + 10 分钟
        else if (kNoNet == getNetInfo()) {
            interval = interval * kNoNetSaltRate + kNoNetSaltRise;
            xinfo2(TSF"no net, interval:%0", interval);

        }
        // 好大：活跃有网络 但是没有登录：2倍 + 5分钟 
        else if (GetAccountInfo().username.empty()) {
            interval = interval * kNoAccountInfoSaltRate + kNoAccountInfoSaltRise;
            xinfo2(TSF"no account info, interval:%0", interval);

        } else {
            // default value
        }
    }

    return interval;
}


LongLinkConnectMonitor::LongLinkConnectMonitor(ActiveLogic& _activelogic, LongLink& _longlink, MessageQueue::MessageQueue_t _id)
    : activelogic_(_activelogic), longlink_(_longlink), alarm_(boost::bind(&LongLinkConnectMonitor::__OnAlarm, this), _id)
    , status_(LongLink::kDisConnected)
    , last_connect_time_(0)
    , last_connect_net_type_(kNoNet)
    , thread_(boost::bind(&LongLinkConnectMonitor::__Run, this), XLOGGER_TAG"::con_mon")
    , conti_suc_count_(0)
    , isstart_(false) {
    activelogic_.SignalActive.connect(boost::bind(&LongLinkConnectMonitor::__OnSignalActive, this, _1));
    activelogic_.SignalForeground.connect(boost::bind(&LongLinkConnectMonitor::__OnSignalForeground, this, _1));
    longlink_.SignalConnection.connect(boost::bind(&LongLinkConnectMonitor::__OnLongLinkStatuChanged, this, _1));
}

LongLinkConnectMonitor::~LongLinkConnectMonitor() {
#ifdef __APPLE__
    __StopTimer();
#endif
    longlink_.SignalConnection.disconnect(boost::bind(&LongLinkConnectMonitor::__OnLongLinkStatuChanged, this, _1));
    activelogic_.SignalForeground.disconnect(boost::bind(&LongLinkConnectMonitor::__OnSignalForeground, this, _1));
    activelogic_.SignalActive.disconnect(boost::bind(&LongLinkConnectMonitor::__OnSignalActive, this, _1));
}

bool LongLinkConnectMonitor::MakeSureConnected() {
    __IntervalConnect(kTaskConnect);
    return LongLink::kConnected == longlink_.ConnectStatus();
}

/**
 * 好大：只有在真正立即断开并重新连接的时候才返回 true
 */
bool LongLinkConnectMonitor::NetworkChange() {
    xdebug_function();

    /**
     * 好大：为什么只有Apple平台才在网络从 mobile 切换至非 mobile 时，通过 timer 的形式去重连？
     * 是因为OS reachability 告诉网络状态发生的时候，Wi-Fi 实际上并没有准备好？需要等 2-3秒后才能使用？
     *
     *******************************************
     ***** 通过微信iOS版本实际测试得到的结果是 *****：
     *******************************************
     *
     * 1. 从wifi切换至 mobile 网络，立即重连；
     * 2. 如果在频繁切换网络，导致 mobile 连接状态不够 10秒，那么从mobile切换至wifi会立即重连；
     * 3. 如果 mobile 网络连接超过10秒，切换至 wifi，会在大概22秒之后进行重连
     *
     * 综上，有几个结论：
     * 1. 即使在wifi情况下，仍然可以使用 Mobile 网络，Mobile 网络并不会因为连接了WIFI就立即断开，可以延迟重连
     * （请注意，即使这个时候连接了一个不能访问互联网但WIFI，收发信息都是没有问题的！过度如此平滑，可见功夫！）；
     * 2. WIFI 切换到4G后，WIFI肯定就不能再使用，需要立即重连；
     * 3. 为什么从mobile网络切换至wifi，只有苹果采取延迟连接重连的策略？因为 WIFI 环境比 mobile 质量上更负责，认为mobile其实稳定性更可靠，
     * 在对 WIFI进行了充分的测试之后，才连接Wi-Fi，所以这是理论上最优的一种策略。但是为什么只有苹果设备这么做？
     * 因为安卓设备千奇百怪，所以各种厂商优化层出不穷，是不是有可能在wifi连接上之后，就立马关闭了mobile网络？那么与其这样还不如不延迟。
     * 但苹果设备表现都相对一致，统一采取这样都策略没什么问题（这或许是和 MultipleTCP 有关？）。
     */
#ifdef __APPLE__
    __StopTimer();

    do {
        if (LongLink::kConnected != status_ || (::gettickcount() - last_connect_time_) <= 10 * 1000) break;

        if (kMobile != last_connect_net_type_) break;

        int netifo = getNetInfo();

        if (kNoNet == netifo) break;

        /**
         * 好大：满足这些条件才会 startTimer：
         * 1. 当网络状态发生变化是，长连接必须处于连接状态
         * 2. 长连接处于移动网络
         * 3. 长连接维持了 10 秒钟以上
         *
         * 综上，长连接处于移动网络下的连接状态，然后切换到其他非移动网络时启动计时器；否值直接断开并重连长连接。
         */
        if (__StartTimer()) return false;
    } while (false);

#endif

    // 好大：如果能启动计时器，直接断开并重连长连接
    // 好大：1.首先断开连接
    longlink_.Disconnect(LongLink::kNetworkChange);
    // 好大：2.然后立即重连
    return 0 == __IntervalConnect(kNetworkChangeConnect);
}

// 好大：如果长连接没有建立 or 连接断开的情况下，到达间隔的情况下自动重连；如果没到达间隔，等到达间隔后再重试
unsigned long LongLinkConnectMonitor::__IntervalConnect(int _type) {
    if (LongLink::kConnecting == longlink_.ConnectStatus() || LongLink::kConnected == longlink_.ConnectStatus()) return 0;

    unsigned long interval =  __Interval(_type, activelogic_) * 1000;
    unsigned long posttime = gettickcount() - longlink_.Profile().dns_time;

    // 好大：计算的是从获得 DNS 时间到现在的 interval，当到达一定 interval 的时候就检查一次长连接状态。
    if (posttime >= interval) {
        bool newone = false;
        bool ret = longlink_.MakeSureConnected(&newone);
        xinfo2(TSF"made interval connect interval:%0, posttime:%_, newone:%_, connectstatus:%_", interval, posttime, newone, longlink_.ConnectStatus());
        return (ret || newone) ? 0 : 0;

    } else {
        return interval - posttime;
    }
}

/**
 * 好大：长连接若没处于连接或正在连接状态，保证自 dns 获取以来一定间隔后自动重连。
 */
unsigned long  LongLinkConnectMonitor::__AutoIntervalConnect() {
    alarm_.Cancel();
    unsigned long remain = __IntervalConnect(kLongLinkConnect);

    if (0 == remain) return remain;

    xinfo2(TSF"start auto connect after:%0", remain);
    alarm_.Start((int)remain);
    return remain;
}

void LongLinkConnectMonitor::__OnSignalForeground(bool _isForeground) {
    // 好大：因为安卓有独立的 service 来做消息推送，所以当App进入前后台对于安卓并不影响，只有iOS需要做这些考虑。
#ifdef __APPLE__

    if (_isForeground) {
        /**
         * 好大：App回到前台，发现有长连接虽然处于连接状态，但是已经有4分半钟以上时间没有收到任何数据了，就重置长连接。
         * 因为这个间隔大于心跳间隔了，任何长连接大于心跳间隔没有心跳、数据都应该重连。因为这个长连接处于"悬空状态"了，指不定服务端都已经将 socket close 了。
         */
        if ((longlink_.ConnectStatus() == LongLink::kConnected) &&
                (tickcount_t().gettickcount() - longlink_.GetLastRecvTime() > tickcountdiff_t(4.5 * 60 * 1000))) {
            xwarn2(TSF"sock long time no send data, close it");
            __ReConnect();
        }
    }

#endif

    /**
     * 好大：即使是 App 进入后台，也尝试长连接重连。所以 App 进入后台的时候，并没有关闭长连接的动作。
     * #iOS：那么App进入后台之后，socket 仍然处于 open状态，这个时候服务端狂给客户端写数据，客户端网卡能收到，但是App应用层已经不读取了，这个时候会怎样？
     */
    __AutoIntervalConnect();
}

void LongLinkConnectMonitor::__OnSignalActive(bool _isactive) {
    __AutoIntervalConnect();
}

void LongLinkConnectMonitor::__OnLongLinkStatuChanged(LongLink::TLongLinkStatus _status) {
    alarm_.Cancel();

    if (LongLink::kConnectFailed == _status || LongLink::kDisConnected == _status) {
        alarm_.Start(500);
    } else if (LongLink::kConnected == _status) {
        xinfo2(TSF"cancel auto connect");
    }

    status_ = _status;
    last_connect_time_ = ::gettickcount();
    last_connect_net_type_ = ::getNetInfo();
}

void LongLinkConnectMonitor::__OnAlarm() {
    __AutoIntervalConnect();
}

#ifdef __APPLE__
bool LongLinkConnectMonitor::__StartTimer() {
    xdebug_function();

    conti_suc_count_ = 0;

    ScopedLock lock(testmutex_);
    isstart_ = true;

    if (thread_.isruning()) {
        return true;
    }

    // 好大：等待 3 秒，然后每 10 秒钟触发一次
    int ret = thread_.start_periodic(kStartCheckPeriod, kTimeCheckPeriod);
    return 0 == ret;
}


bool LongLinkConnectMonitor::__StopTimer() {
    xdebug_function();

    ScopedLock lock(testmutex_);

    if (!isstart_) return true;

    isstart_ = false;

    if (!thread_.isruning()) {
        return true;
    }

    thread_.cancel_periodic();


    thread_.join();
    return true;
}
#endif


void LongLinkConnectMonitor::__Run() {
    int netifo = getNetInfo();

    /**
     * 好大：必须满足如下条件才执行计时器，否则就停止计时器：
     * 1. 长连接处于连接状态；
     * 2. last_connect_time_（最近一次长连接状态发生变化的时间）必须大于 12 秒（比起之前的 10秒额外加了2秒，因为timer的delay是3秒）；
     * 3. last_connect_net_type_（最近一次长连接状态发生变化的网路状态） 必须是 kMobile；
     * 4. 现在必须不能是 kMobile；
     *
     * 综上，长连接处于移动网络下的连接状，且连接成功已经超过 12 秒，但是现在的网路不是移动网络。
     */
    if (LongLink::kConnected != status_ || (::gettickcount() - last_connect_time_) <= 12 * 1000
            || kMobile != last_connect_net_type_ || kMobile == netifo) {
        thread_.cancel_periodic();
        return;
    }

    // 好大：发送标准 DNS 协议包，其实并不关心 DNS 查询结果。这个是用来测试网络是否通常的
    struct socket_ipinfo_t dummyIpInfo;
    int ret = socket_gethostbyname(NetSource::GetLongLinkHosts().front().c_str(), &dummyIpInfo, 0, NULL);

    if (ret == 0) {
        ++conti_suc_count_;
    } else {
        conti_suc_count_ = 0;
    }

    // 好大：只有 WIFI 在保证 3 次以上的网络测试都通常的情况下，才开始重连，这个大概需要 30 多秒以上？
    if (conti_suc_count_ >= 3) {
        __ReConnect();
        thread_.cancel_periodic();
    }
}

void LongLinkConnectMonitor::__ReConnect() {
    xinfo_function();
    xassert2(fun_longlink_reset_);
    fun_longlink_reset_();
}


