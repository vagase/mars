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
 * net_core.h
 *
 *  Created on: 2012-7-18
 *      Author: yerungui
 */

#ifndef STN_SRC_NET_CORE_H_
#define STN_SRC_NET_CORE_H_

#include "mars/comm/autobuffer.h"

#include "mars/comm/thread/mutex.h"
#include "mars/comm/singleton.h"
#include "mars/comm/messagequeue/message_queue.h"
#include "mars/comm/messagequeue/message_queue_utils.h"
#include "mars/stn/config.h"
#include "mars/stn/stn.h"

#include "netsource_timercheck.h"
#include "net_check_logic.h"

#ifdef USE_LONG_LINK
#include "longlink.h"
#endif

class NetSourceTimerCheck;

namespace mars {
    namespace stn {

class NetSource;
    
class ShortLinkTaskManager;
        
#ifdef USE_LONG_LINK
class LongLinkTaskManager;
class TimingSync;
class ZombieTaskManager;
#endif
        
class SignallingKeeper;
class NetCheckLogic;
class DynamicTimeout;
class AntiAvalanche;

enum {
    kCallFromLong,
    kCallFromShort,
    kCallFromZombie,
};

class NetCore {
  public:
    SINGLETON_INTRUSIVE(NetCore, new NetCore, NetCore::__Release);

  public:
    boost::function<void (Task& _task)> task_process_hook_;                                                                         // 好大：StartTask 时候的 hook 函数，仅仅是 hook 不对流程发生副作用
    boost::function<int (int _from, ErrCmdType _err_type, int _err_code, int _fail_handle, const Task& _task)> task_callback_hook_; // 好大：任务回调时候的 hook 函数，发生副作用。如果返回值是 0，则会不执行后面的逻辑。
    boost::signals2::signal<void (uint32_t _cmdid, const AutoBuffer& _buffer)> push_preprocess_signal_;                             // 好大：有 push 消息时候的预处理

  public:
    void    StartTask(const Task& _task);
    void    StopTask(uint32_t _taskid);
    bool    HasTask(uint32_t _taskid) const;
    void    ClearTasks();
    void    RedoTasks();

    void    MakeSureLongLinkConnect();
    bool    LongLinkIsConnected();
    void    OnNetworkChange();

    void	KeepSignal();
    void	StopSignal();

#ifdef USE_LONG_LINK
    LongLinkTaskManager& GetLongLinkTaskManager() {return *longlink_task_manager_;}
#endif

  private:
    NetCore();
    virtual ~NetCore();
    static void __Release(NetCore* _instance);
    
  private:
    int     __CallBack(int _from, ErrCmdType _err_type, int _err_code, int _fail_handle, const Task& _task, unsigned int _taskcosttime);
    void    __OnShortLinkNetworkError(int _line, ErrCmdType _err_type, int _err_code, const std::string& _ip, const std::string& _host, uint16_t _port);
    void    __OnSessionTimeout(int _err_code, uint32_t _src_taskid);

    void    __OnShortLinkResponse(int _status_code);

#ifdef USE_LONG_LINK
    void    __OnPush(uint32_t _cmdid, uint32_t _taskid, const AutoBuffer& _buf);
    void    __OnLongLinkNetworkError(int _line, ErrCmdType _err_type, int _err_code, const std::string& _ip, uint16_t _port);
    void    __OnLongLinkConnStatusChange(LongLink::TLongLinkStatus _status);
    void    __ResetLongLink();
#endif
    
    void    __ConnStatusCallBack();
    void    __OnTimerCheckSuc();

  private:
    NetCore(const NetCore&);
    NetCore& operator=(const NetCore&);

  private:
    MessageQueue::MessageQueueCreater   messagequeue_creater_;
    MessageQueue::ScopeRegister         asyncreg_;
    NetSource*                          net_source_;            // 好大：包括地址源，host, IP X PORT
    NetCheckLogic*                      netcheck_logic_;        // 好大：在长连接、短连接与服务器 send/recv 的时候，不断本地更新交互结果。在判断需要检查网络状况的时候，获取网络通信质量的各项指标：RTT，DNS time，error rate 等。
    AntiAvalanche*                      anti_avalanche_;        // 好大：从"频率"和"流量"两个方面，进行雪崩检测，说白了为了控制客户端不要发送太猛。如果不加控制，某个机制（可能是bug）一旦触发，使得所有客户端在某个点上一起猛发，服务端就爆炸了。
    
    DynamicTimeout*                     dynamic_timeout_;       // 好大：动态超时计算，比如首包超时时间
    ShortLinkTaskManager*               shortlink_task_manager_;
    int                                 shortlink_error_count_;

#ifdef USE_LONG_LINK
    ZombieTaskManager*                  zombie_task_manager_;   // 好大：ZombieTaskManager 感觉是当任务 retry 之后，而且 timeout 还没有耗尽，对任务进行最后补救重试。增大 timeout 比较长、优先级比价低，不太敏感的数据获取成功概率
    LongLinkTaskManager*                longlink_task_manager_;
    SignallingKeeper*                   signalling_keeper_;     // 好大：可以应用层调用 KeepSignalling／StopSignalling 手动控制，再一段时间内定时给服务器发送数据包，command id 为 signal_keep_cmdid() 也是由应用层控制。
    NetSourceTimerCheck*                netsource_timercheck_;  // 好大：用来检查长连接 host 对应的 IP 发生变化情况先，需要重新建立长连接。
    TimingSync*                         timing_sync_;           // 好大：在长连接没有建立成功之前，启动定时发送 syncRequest 的 timer，目的是为了如果长连接一直没建立好，起码还能手动 sync 一下（比如通过短连接）。
#endif

    bool                                shortlink_try_flag_;

};
        
}}

#endif // STN_SRC_NET_CORE_H_
