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
*  dynamic_timeout.cc
*  network
*
*  Created by caoshaokun on 15/10/28.
*  Copyright © 2015 Tencent. All rights reserved.
*/

#include "dynamic_timeout.h"

#include <sstream>
#include <string>

#include "mars/comm/time_utils.h"
#include "mars/comm/platform_comm.h"
#include "mars/comm/xlogger/xlogger.h"
#include "mars/stn/config.h"

using namespace mars::stn;

DynamicTimeout::DynamicTimeout()
    : dyntime_status_(kEValuating)
    , dyntime_continuous_good_count_(0)
    , dyntime_latest_bigpkg_goodtime_(0)
    , dyntime_fncount_latstmodify_time_(0)
    , dyntime_fncount_pos_(-1)
{
    dyntime_failed_normal_count_.set();
}

DynamicTimeout::~DynamicTimeout() {
}

// 好大：统计 CGI 任务的耗时，参数中 _cgi_uri 并没有什么实际用处，用来打 log 。
void DynamicTimeout::CgiTaskStatistic(std::string _cgi_uri, unsigned int _total_size, uint64_t _cost_time) {
    int task_status = (_total_size == kDynTimeTaskFailedPkgLen || _cost_time == 0) ? kDynTimeTaskFailedTag : KDynTimeTaskNormalTag;
    
    if (task_status == KDynTimeTaskNormalTag) {

        /**
         *  cost time, unit: ms
         *            ┌─────────┬─────────┬─────────┬─────────┐
         *            │  small  │ middle  │   big   │ bigger  │
         *  ┌─────────┼─────────┼─────────┼─────────┼─────────┤
         *  │ mobile  │  1000   │  3000   │  5000   │  7000   │
         *  ├─────────┼─────────┼─────────┼─────────┼─────────┤
         *  │  wifi   │   500   │  2000   │  4000   │  6000   │
         *  └─────────┴─────────┴─────────┴─────────┴─────────┘
         */
        unsigned int small_pkg_costtime = kMobile != getNetInfo() ? kDynTimeSmallPackageWifiCosttime : kDynTimeSmallPackageGPRSCosttime;
        unsigned int middle_pkg_costtime = kMobile != getNetInfo() ? kDynTimeMiddlePackageWifiCosttime : kDynTimeMiddlePackageGPRSCosttime;
        unsigned int big_pkg_costtime = kMobile != getNetInfo() ? kDynTimeBigPackageWifiCosttime : kDynTimeBigPackageGPRSCosttime;
        unsigned int bigger_pkg_costtime = kMobile != getNetInfo() ? kDynTimeBiggerPackageWifiCosttime : kDynTimeBiggerPackageGPRSCosttime;

        /**
         *  package size
         *  ┌─────────┬─────────┬─────────┬─────────┐
         *  │  small  │ middle  │   big   │ bigger  │
         *  ├─────────┼─────────┼─────────┼─────────┤
         *  │  <=3K   │  <=10K  │  <=30K  │  > 30K  │
         *  └─────────┴─────────┴─────────┴─────────┘
         */
        if (_total_size < kDynTimeSmallPackageLen && _cost_time <= small_pkg_costtime) {
            task_status = kDynTimeTaskMeetExpectTag;
        }
        else if (_total_size <= kDynTimeMiddlePackageLen && _cost_time <= middle_pkg_costtime) {
            task_status = kDynTimeTaskMidPkgMeetExpectTag;
        }
        else if (_total_size <= kDynTimeBigPackageLen && _cost_time <= big_pkg_costtime) {
            task_status = kDynTimeTaskBigPkgMeetExpectTag;
        }
        else if (_total_size > kDynTimeBigPackageLen && _cost_time <= bigger_pkg_costtime) {
            task_status = kDynTimeTaskBiggerPkgMeetExpectTag;
        }
        /*else {
            task_status = DYNTIME_TASK_NORMAL_TAG;
             xdebug2(TSF"totalSize:%_, costTime:%_", totalSize, costTime);
        }*/
    }
    
    __StatusSwitch(_cgi_uri, task_status);
}

void DynamicTimeout::ResetStatus() {
    
    dyntime_status_ = kEValuating;
    dyntime_latest_bigpkg_goodtime_ = 0;
    dyntime_continuous_good_count_ = 0;
    dyntime_failed_normal_count_.set();
    dyntime_fncount_latstmodify_time_ = 0;
    dyntime_fncount_pos_ = -1;
}

int DynamicTimeout::GetStatus() {
    return dyntime_status_;
}

void DynamicTimeout::__StatusSwitch(std::string _cgi_uri, int _task_status) {

    // 好大：每隔 5 分钟重置 dyntime_failed_normal_count_
    if (dyntime_fncount_latstmodify_time_ == 0 || (gettickcount() - dyntime_fncount_latstmodify_time_) > kDynTimeCountExpireTime) {
        dyntime_fncount_latstmodify_time_ = gettickcount();
        dyntime_fncount_pos_ = -1;
        if (dyntime_status_ == kBad) {
            dyntime_failed_normal_count_.reset();
        }
        else {
            dyntime_failed_normal_count_.set();
        }
    }

    // 好大：修正 index，如果到末尾了，则重新指向开始；只记录最近 10 次的结果。
    dyntime_fncount_pos_ = ++dyntime_fncount_pos_ >= dyntime_failed_normal_count_.size() ? 0 : dyntime_fncount_pos_;

    /**
     * 好大：
     * dyntime_latest_bigpkg_goodtime_  ：dyntime_status_ 处于 kEValuating时，最近一次大包 MeetExpectTag 的时间戳
     * dyntime_continuous_good_count_   ：dyntime_status_ 处于 kEValuating时，连续 MeetExpectTag 的次数。
     * dyntime_failed_normal_count_     ：是一个环形令牌，MeetExpectTag 或者 NormalTag 就设置为 1，否则为 0。和当前 dyntime_status_ 并没有关系。
     */
    switch (_task_status) {
        // 好大：中包、大包
        case kDynTimeTaskMidPkgMeetExpectTag:
        case kDynTimeTaskBigPkgMeetExpectTag:
        case kDynTimeTaskBiggerPkgMeetExpectTag:
        {
            if (dyntime_status_ == kEValuating) {
                dyntime_latest_bigpkg_goodtime_ = gettickcount();
            }
        }
        /* no break, next case*/
        // 好大：小包
        case kDynTimeTaskMeetExpectTag:
        {
            if (dyntime_status_ == kEValuating) {
                dyntime_continuous_good_count_++;
            }

            dyntime_failed_normal_count_.set(dyntime_fncount_pos_);
        }
            break;
        case KDynTimeTaskNormalTag:
        {
            // 好大：在评估过程中，只要有一个包不是 MeetExpectTag，就重置 dyntime_continuous_good_count_
            if (dyntime_status_ == kEValuating) {
                dyntime_continuous_good_count_ = 0;
                dyntime_latest_bigpkg_goodtime_ = 0;
            }

            // 好大：虽然没有 MeetExpectTag，但是还是在 dyntime_failed_normal_count_ 记录为成功
            dyntime_failed_normal_count_.set(dyntime_fncount_pos_);
        }
            break;
        case kDynTimeTaskFailedTag:
        {
            dyntime_continuous_good_count_ = 0;
            dyntime_latest_bigpkg_goodtime_ = 0;
            dyntime_failed_normal_count_.reset(dyntime_fncount_pos_);
        }
            break;
        default:
            break;
    }
    
    switch (dyntime_status_) {
        case kEValuating:
        {
            // 好大：连续 MeetExpectTag 的次数 >= 10 且 最近 5 分钟内有中大包复合预期，设置为 excellent
            if (dyntime_continuous_good_count_ >= kDynTimeMaxContinuousExcellentCount && (gettickcount() - dyntime_latest_bigpkg_goodtime_) <= kDynTimeCountExpireTime) {
                xassert2(kDynTimeMaxContinuousExcellentCount >= 10, TSF"max_continuous_good_count:%_", kDynTimeMaxContinuousExcellentCount);
                dyntime_status_ = kExcellent;
            }
            // 好大：最近10次，有 4 次及以上失败（非连续），设置为 bad
            else if (dyntime_failed_normal_count_.count() <= kDynTimeMinNormalPkgCount /* 6 */){
                xassert2(kDynTimeMinNormalPkgCount < dyntime_failed_normal_count_.size(), TSF"DYNTIME_MIN_NORMAL_PKG_COUNT:%_, dyntime_failed_normal_count_:%_", kDynTimeMinNormalPkgCount, dyntime_failed_normal_count_.size());
                dyntime_status_ = kBad;

                // 好大：evaluating -> bad 重置 dyntime_failed_normal_count_
                dyntime_fncount_latstmodify_time_ = 0;
            }
            // 好大：其余情况，保持状态不变
        }
            break;
        case kExcellent:
        {
            // 好大：到达 excellent 状态以后，只要有包没有 MeetExpectTag ，就重置为 kEValuating
            if (dyntime_continuous_good_count_ == 0 && dyntime_latest_bigpkg_goodtime_ == 0){
                dyntime_status_ = kEValuating;
            }
        }
            break;
        case kBad:
        {
            // 好大：在 bad 情况下，已经有 6 次以上包成功了（非 kDynTimeTaskFailedTag）就重置为 kEValuating
            if (dyntime_failed_normal_count_.count() > kDynTimeMinNormalPkgCount) {
                xassert2(kDynTimeMinNormalPkgCount < dyntime_failed_normal_count_.size(), TSF"DYNTIME_MIN_NORMAL_PKG_COUNT:%_, dyntime_failed_normal_count_:%_", kDynTimeMinNormalPkgCount, dyntime_failed_normal_count_.size());
                dyntime_status_ = kEValuating;

                // 好大：bad -> evaluating 重置 dyntime_failed_normal_count_
                dyntime_fncount_latstmodify_time_ = 0;
            }
        }
            break;
        default:
            break;
    }
    
    xdebug2(TSF"task_status:%_, good_count:%_, good_time:%_, dyntime_status:%_, dyntime_failed_normal_count_NORMAL:%_, cgi:%_", _task_status, dyntime_continuous_good_count_, dyntime_latest_bigpkg_goodtime_, dyntime_status_, dyntime_failed_normal_count_.count(), _cgi_uri);
}
