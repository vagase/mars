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
//  stn_logic.h
//  network
//
//  Created by yanguoyue on 16/2/18.
//  Copyright © 2016年 Tencent. All rights reserved.
//

#ifndef MARS_STN_LOGIC_H_
#define MARS_STN_LOGIC_H_

#include <stdint.h>
#include <string>
#include <map>
#include <vector>

#include "mars/comm/autobuffer.h"
#include "mars/stn/stn.h"

namespace mars{
namespace stn{
    //callback interface
    class Callback
    {
    public:
    	virtual ~Callback() {}
        virtual bool MakesureAuthed() = 0;
        
        //流量统计
        virtual void TrafficData(ssize_t _send, ssize_t _recv);
        
        //底层询问上层该host对应的ip列表
        virtual std::vector<std::string> OnNewDns(const std::string& host);
        //网络层收到push消息回调
        virtual void OnPush(int32_t cmdid, const AutoBuffer& msgpayload) = 0;
        //底层获取task要发送的数据
        virtual bool Req2Buf(int32_t taskid, void* const user_context, AutoBuffer& outbuffer, int& error_code, const int channel_select) = 0;
        //底层回包返回给上层解析
        virtual int Buf2Resp(int32_t taskid, void* const user_context, const AutoBuffer& inbuffer, int& error_code, const int channel_select) = 0;
        //任务执行结束
        virtual int  OnTaskEnd(int32_t taskid, void* const user_context, int error_type, int error_code) = 0;

        //上报网络连接状态
        virtual void ReportConnectStatus(int status, int longlink_status) = 0;
        //长连信令校验 ECHECK_NOW = 0, ECHECK_NEXT = 1, ECHECK_NEVER = 2
        virtual int  GetLonglinkIdentifyCheckBuffer(AutoBuffer& identify_buffer, AutoBuffer& buffer_hash, int32_t& cmdid) = 0;
        //长连信令校验回包
        virtual bool OnLonglinkIdentifyResponse(const AutoBuffer& response_buffer, const AutoBuffer& identify_buffer_hash) = 0;
        
        // 好大：请求上层发起 sync 请求。
        virtual void RequestSync() = 0;
        
        //验证是否已登录
        virtual bool IsLogoned() = 0;
    };

    void SetCallback(Callback* const callback);
    

    // 好大：1. 为什么长连接只能有一个 host 而非 host list？2. 为什么短连接不能在这里设置好，而是 send 的时候，每次在 task 里面提供？
    void SetLonglinkSvrAddr(const std::string& host, const std::vector<uint16_t> ports);
    void SetShortlinkSvrAddr(const uint16_t port);
    

    // 'host' will be ignored when 'debugip' is not empty.
    void SetLonglinkSvrAddr(const std::string& host, const std::vector<uint16_t> ports, const std::string& debugip);
    
    // 'task.host' will be ignored when 'debugip' is not empty.
    void SetShortlinkSvrAddr(const uint16_t port, const std::string& debugip);
    
    // setting debug ip address for the corresponding host
    void SetDebugIP(const std::string& host, const std::string& ip);
    
    // setting backup iplist for the corresponding host
    // if debugip is not empty, iplist will be ignored.
    // iplist will be used when newdns/dns ip is not available.
    void SetBackupIPs(const std::string& host, const std::vector<std::string>& iplist);
    

    // async function.
    void StartTask(const Task& task);
    
    // sync function
    void StopTask(int32_t taskid);
    
    // check whether task's list has the task or not.
    bool HasTask(int32_t taskid);

    // reconnect longlink and redo all task
    // when you change svr ip, you must call this function.
    void RedoTasks();
    
    // stop and clear all task
    void ClearTasks();
    
    // the same as ClearTasks(), but also reinitialize network.
    void Reset();
    
    //setting signalling's parameters.
    //if you did not call this function, stn will use default value: period:  5s, keeptime: 20s
    void SetSignallingStrategy(long period, long keeptime);

    /*
     * 好大：已经有长连接心跳了，为什么还要 keep signalling 呢？
     *
     * 官方解答：信令包，为了维持手机网卡的活跃态以及用来长时间霸占基站的信令进而提高发送数据的速度，具体细节可谷歌 "手机 RRC"。 该功能可选。https://github.com/Tencent/mars/wiki/Mars-%E5%B8%B8%E7%94%A8%E6%9C%AF%E8%AF%AD
     * 信令保活好理解，就是防止基站以及 NAT 设备地址转换表失效，导致 TCP 连接失效。
     * RRC (Radio Resource Control) 无限资源控制器，因为 RRC 比较耗电（比 WIFI 耗电），所以为了省电当没有流量的时候就会将 RRC 设置为空闲状态。当再次发送流量当时候，RRC 从闲置到激活有一个延迟，3G 200-2500ms，4G 50-100ms。http://www.hello-code.com/blog/android/201603/5977.html
     * 所以这里 KeepSignalling 是为了同时信令保活和保持RRC活跃。
     *
     * 现在回来回答最初的问题：为什么已经有长连接心跳，还需要 keep signalling 呢？
     * 如果 keep signalling 是走长连接，确实没太大差别，出了保活RRC；但如果没有长连接或者长连接不太好，那么也只能通过 UDP 发送保活心跳了。
     * 感觉 keep signalling 是长连接心跳的一种补充。
     */
    // used to keep longlink active
    // keep signnaling once 'period' and last 'keeptime'
    void KeepSignalling();
    

    void StopSignalling();
    
    // connect quickly if longlink is not connected.
    void MakesureLonglinkConnected();
    
    bool LongLinkIsConnected();

    // noop is used to keep longlink conected
    // get noop taskid
    uint32_t getNoopTaskID();
}}

#endif /* MARS_STN_LOGIC_H_ */
