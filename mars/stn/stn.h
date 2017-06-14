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
 ============================================================================
 Name       : stn.h
 Author     : yerungui
 Created on : 2012-7-18
 ============================================================================
 */

#ifndef NETWORK_SRC_NET_COMM_H_
#define NETWORK_SRC_NET_COMM_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "mars/comm/autobuffer.h"

namespace mars{
    namespace stn{

struct TaskProfile;
struct DnsProfile;

struct Task {
public:
    //channel type
    static const int kChannelShort = 0x1;
    static const int kChannelLong = 0x2;
    static const int kChannelBoth = 0x3;
    
    static const int kChannelNormalStrategy = 0;
    static const int kChannelFastStrategy = 1;
    static const int kChannelDisasterRecoveryStategy = 2;
    
    static const int kTaskPriorityHighest = 0;
    static const int kTaskPriority0 = 0;
    static const int kTaskPriority1 = 1;
    static const int kTaskPriority2 = 2;
    static const int kTaskPriority3 = 3;
    static const int kTaskPriorityNormal = 3;
    static const int kTaskPriority4 = 4;
    static const int kTaskPriority5 = 5;
    static const int kTaskPriorityLowest = 5;
    
    static const uint32_t kInvalidTaskID = 0;
    static const uint32_t kNoopTaskID = 0xFFFFFFFF;
    static const uint32_t kLongLinkIdentifyCheckerTaskID = 0xFFFFFFFE;
    static const uint32_t kSignallingKeeperTaskID = 0xFFFFFFFD;
    
    
    Task();

    //require
    uint32_t       taskid;
    uint32_t       cmdid;
    int32_t        channel_select;
    std::string    cgi;    // user

    //optional
    bool    send_only;  // user
    bool    need_authed;  // user
    bool    limit_flow;  // user
    bool    limit_frequency;  // user
    
    bool        network_status_sensitive;  // user 好大：对网络状况敏感，只会在网路状况好的时候发送。比如不支持在网络不稳定的时候，待网络连接上的时候再发送。
    int32_t     channel_strategy;
    int32_t     priority;  // user
    
    int32_t     retry_count;  // user
    int32_t     server_process_cost;  // user   好大：这个是有应用层提供的关于这个 task 预估的服务器需要处理的时间
    int32_t     total_timetout;  // user ms
    
    void*       user_context;  // user
    std::string report_arg;  // user for cgi report
    
    std::vector<std::string> shortlink_host_list;	// 好大：为什么长连接 host 可以在 source 里面设置，而短连接 host 却要在 task 里面设置？
};

enum TaskFailHandleType {
	kTaskFailHandleNormal = 0,
	kTaskFailHandleNoError = 0,         	// 好大：task 没毛病，Buf2Resp 解码包没有问题
	kTaskFailHandleDefault = -1,
	kTaskFailHandleSessionTimeout = -13,	// 好大：Buf2Resp 解码包的返回值，是逻辑层觉得这个包已经 timeout 了。
	kTaskFailHandleTaskEnd = -14,
	kTaskFailHandleTaskTimeout = -15
};
        
//error type
enum ErrCmdType {
	kEctOK = 0,
	kEctFalse = 1,
	kEctDial = 2,
	kEctDns = 3,
	kEctSocket = 4,
	kEctHttp = 5,
	kEctNetMsgXP = 6,
	kEctEnDecode = 7,
	kEctServer = 8,
	kEctLocal = 9,
    kEctCanceled = 10,
};

//error code
enum {
	kEctLocalTaskTimeout = -1,
    kEctLocalTaskRetry = -2,
	kEctLocalStartTaskFail = -3,        // 好大：调用 StartTask 出错，long_task_manager or short_task_manager
	kEctLocalAntiAvalanche = -4,
	kEctLocalChannelSelect = -5,
	kEctLocalNoNet = -6,
    kEctLocalCancel = -7,
    kEctLocalClear = -8,
    kEctLocalReset = -9,
	kEctLocalTaskParam = -12,
	kEctLocalCgiFrequcencyLimit = -13,
    
};

// -600 ~ -500
enum {
    kEctLongFirstPkgTimeout = -500,		// 好大：首包超时
    kEctLongPkgPkgTimeout = -501,		// 好大：包包超时
    kEctLongReadWriteTimeout = -502,	// 好大：读写超时
    kEctLongTaskTimeout = -503,			// 好大：任务超时
};

// -600 ~ -500
enum {
    kEctHttpFirstPkgTimeout = -500,
    kEctHttpPkgPkgTimeout = -501,
    kEctHttpReadWriteTimeout = -502,
    kEctHttpTaskTimeout = -503,
};

// -20000 ~ -10000
enum {
    kEctSocketNetworkChange = -10086,
    kEctSocketMakeSocketPrepared = -10087,
    kEctSocketWritenWithNonBlock = -10088,
    kEctSocketReadOnce = -10089,
    kEctSocketShutdown = -10090,
    kEctSocketRecvErr = -10091,
    kEctSocketSendErr = -10092,

    kEctHttpSplitHttpHeadAndBody = -10194,
    kEctHttpParseStatusLine = -10195,

    kEctNetMsgXPHandleBufferErr = -10504,

    kEctDnsMakeSocketPrepared = -10606,			// 好大： DNS 解释失败，导致建立连接的时候 IP 地址列表为空
};

enum NetStatus {
    kNetworkUnkown = -1,
    kNetworkUnavailable = 0,
    kGateWayFailed = 1,
    kServerFailed = 2,
    kConnecting = 3,
    kConnected = 4,
    kServerDown = 5
};

enum IdentifyMode {
    kCheckNow = 0,
    kCheckNext,
    kCheckNever
};

// 好大：这个类型就说明了 DNS host -> IP 的来源
enum IPSourceType {
    kIPSourceNULL = 0,
    kIPSourceDebug,			// 设置了 Debug IP 地址映射
    kIPSourceDNS,			// 标准 DNS 解析
    kIPSourceNewDns,		// new dns: 有应用层提供的 DNS 解析，比如 HTTPDNS
    kIPSourceProxy,			// 走系统代理
    kIPSourceBackup,
};

const char* const IPSourceTypeString[] = {
    "NullIP",
    "DebugIP",
    "DNSIP",
    "NewDNSIP",
    "ProxyIP",
    "BackupIP",
};

struct IPPortItem {
    std::string		str_ip;
    uint16_t 		port;
    IPSourceType 	source_type;
    std::string 	str_host;
};
        
extern bool MakesureAuthed();

//流量统计
extern void TrafficData(ssize_t _send, ssize_t _recv);
        
//底层询问上层该host对应的ip列表
extern std::vector<std::string> OnNewDns(const std::string& host);
//网络层收到push消息回调
extern void OnPush(int32_t cmdid, const AutoBuffer& msgpayload);
//底层获取task要发送的数据
extern bool Req2Buf(int32_t taskid,  void* const user_context, AutoBuffer& outbuffer, int& error_code, const int channel_select);
//底层回包返回给上层解析
extern int Buf2Resp(int32_t taskid, void* const user_context, const AutoBuffer& inbuffer, int& error_code, const int channel_select);
//任务执行结束
extern int  OnTaskEnd(int32_t taskid, void* const user_context, int error_type, int error_code);

//上报网络连接状态
// 好大：status: 综合长短连下的网络状态；longlink_status : 长连接的状态，值的范围和status相同。
extern void ReportConnectStatus(int status, int longlink_status);
//长连信令校验 ECHECK_NOW = 0, ECHECK_NEVER = 1, ECHECK_NEXT = 2
// 好大：要求上层生成长链接数据校验包,在长链接连接上之后使用,用于验证客户端身份和同步消息。
// 好大：identify_buffer : 校验包数据内容。
// 好大：buffer_hash : 校验包的 hash。
// 好大：cmdid : 数据校验的 cmd id。
extern int  GetLonglinkIdentifyCheckBuffer(AutoBuffer& identify_buffer, AutoBuffer& buffer_hash, int32_t& cmdid);
//长连信令校验回包
extern bool OnLonglinkIdentifyResponse(const AutoBuffer& response_buffer, const AutoBuffer& identify_buffer_hash);

// 好大：请求上层发起 sync 请求。
extern void RequestSync();
//验证是否已登录
extern bool IsLogoned();

//底层询问上层http网络检查的域名列表
extern void RequestNetCheckShortLinkHosts(std::vector<std::string>& _hostlist);
//底层向上层上报cgi执行结果
extern void ReportTaskProfile(const TaskProfile& _task_profile);
//底层通知上层cgi命中限制
extern void ReportTaskLimited(int _check_type, const Task& _task, unsigned int& _param);
//底层上报域名dns结果
// 好大：每次 DNS 解析的时候都会调用这个方法，默认这个方法啥也不干
extern void ReportDnsProfile(const DnsProfile& _dns_profile);
        
}}
#endif // NETWORK_SRC_NET_COMM_H_
