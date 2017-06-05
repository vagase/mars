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
//  config.h
//  stn
//
//  Created by yerungui on 16/2/26.
//  Copyright © 2016年 Tencent. All rights reserved.
//

#ifndef stn_config_h
#define stn_config_h

/**
 * nooping param
 */
//#define NOOP_CMDID_REQ 6
//#define NOOP_DUMMY_SEQ 1
//#define NOOP_CHECK_SEQ (0xFFFFFFFF)
//#define NOOP_CMDID_RESP 6

//
#define DNS_TIMEOUT  (1)// s

#define USE_LONG_LINK
//task attribute max value
#define DEF_TASK_TIME_OUT (60*1000)
#define DEF_TASK_RETRY_COUNT (1)
#define DEF_TASK_RETRY_INTERNAL (1 * 1000)

// Heartbeart Range
#define MinHeartInterval (4  * 60 * 1000 + 30 * 1000)   // 4.5 minute
#define MaxHeartInterval (14 * 60 * 1000 + 50 * 1000)   // 14 minute 50 seconds

// Heartbeat Step
#define HeartStep (60 * 1000)      // try to increse current heartbeat by HeartStep
#define SuccessStep  (20 * 1000)   // when finish compute choose successHeart-=SuccessStep as the stable heartbeat

#define MaxHeartFailCount (3)
#define BaseSuccCount (3)
#define NetStableTestCount (3)     //We think it's time to test after NetStableCount times heartbeat using MinHeartInterval

//signalling transmits timeout related constants
const static unsigned int kBaseFirstPackageWifiTimeout = 12*1000;           // 好大：首包 WIFI 默认超时 12秒
const static unsigned int kBaseFirstPackageGPRSTimeout = 15*1000;           // 好大：首包 移动网络 默认超时 15秒

const static unsigned int kMaxFirstPackageWifiTimeout = 25*1000;            // 好大：首包 WI-FI 最大超时 25秒
const static unsigned int kMaxFirstPackageGPRSTimeout = 35*1000;            // 好大：首包 移动网络 最大超时 25秒

const static unsigned int kGPRSMinRate = 3*1024;                            // 好大：移动网络 最低网速 3K／秒
const static unsigned int kWifiMinRate = 10*1024;                           // 好大：WIFI 最低网速 10K／秒

const static unsigned int kWifiPackageInterval = 8*1000;
const static unsigned int kGPRSPackageInterval = 12*1000;

const static unsigned int kWifiTaskDelay = 1500;
const static unsigned int kGPRSTaskDelay = 3000;

const static unsigned int kMaxRecvLen = 64*1024;

//dynamic timeout related constants
const static unsigned int kDynTimeSmallPackageLen = 3*1024;                 // 好大：小包 小于 3K
const static unsigned int kDynTimeMiddlePackageLen = 10*1024;               // 好大：中包 大于 3K 小于 10K
const static unsigned int kDynTimeBigPackageLen = 30*1024;                  // 好大：大包 大于 10K 小于 30K
                                                                            // 好大：超大包 大于 30K

const static unsigned int kDynTimeSmallPackageWifiCosttime = 500;           // 好大：小包 WIFI 最大耗时 500ms
const static unsigned int kDynTimeMiddlePackageWifiCosttime = 2*1000;       // 好大：中包 WIFI 最大耗时 2s
const static unsigned int kDynTimeBigPackageWifiCosttime = 4*1000;          // 好大：大包 WIFI 最大耗时 4s
const static unsigned int kDynTimeBiggerPackageWifiCosttime = 6*1000;       // 好大：超大包 WIFI 最大耗时 6s


const static unsigned int kDynTimeSmallPackageGPRSCosttime = 1*1000;        // 好大：小包 移动网络 最大耗时 1s
const static unsigned int kDynTimeMiddlePackageGPRSCosttime = 3*1000;       // 好大：中包 移动网络 最大耗时 3s
const static unsigned int kDynTimeBigPackageGPRSCosttime = 5*1000;          // 好大：大包 移动网络 最大耗时 5s
const static unsigned int kDynTimeBiggerPackageGPRSCosttime = 7*1000;       // 好大：超大包 移动网络 最大耗时 7s

const static unsigned int kDynTimeFirstPackageWifiTimeout = 7*1000;         // 好大：网络很好的时候，WIFI下 采用的动态首包超时时间 7秒，比 kBaseFirstPackageWifiTimeout 要小5秒
const static unsigned int kDynTimeFirstPackageGPRSTimeout = 10*1000;        // 好大：网络很好的时候，移动网络下 采用的动态首包超时时间 10秒，比 kBaseFirstPackageWifiTimeout 要小5秒
const static unsigned int kDynTimeExtraWifiTimeout = 10*1000;
const static unsigned int kDynTimeExtraGPRSTimeout = 15*1000;

const static unsigned int kDynTimeMaxContinuousExcellentCount = 10;
const static unsigned int kDynTimeMinNormalPkgCount = 6;
const static unsigned long kDynTimeCountExpireTime = 5*60*1000;

const static unsigned int kDynTimeTaskFailedPkgLen = 0xffffffff;

const static int kDynTimeTaskFailedTag = -1;
const static int KDynTimeTaskNormalTag = 0;
const static int kDynTimeTaskMeetExpectTag = 1;                 // 好大：小包 耗时复合预期
const static int kDynTimeTaskMidPkgMeetExpectTag = 2;           // 好大：中包 耗时复合预期
const static int kDynTimeTaskBigPkgMeetExpectTag = 3;           // 好大：大包 耗时复合预期
const static int kDynTimeTaskBiggerPkgMeetExpectTag = 4;        // 好大：超大包 耗时复合预期

//longlink_task_manager
const static unsigned int kFastSendUseLonglinkTaskCntLimit = 0;

//longlink connect params
const static unsigned int kLonglinkConnTimeout = 10 * 1000;
const static unsigned int kLonglinkConnInteral = 4 * 1000;
const static unsigned int kLonglinkConnMax = 3;

//shortlink connect params
const static unsigned int kShortlinkConnTimeout = 10 * 1000;
const static unsigned int kShortlinkConnInterval = 4 * 1000;

#endif /* stn_config_h */
