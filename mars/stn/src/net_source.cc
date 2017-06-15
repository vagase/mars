// Tencent is pleased to support the open source community by making Mars available.
// Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

// Licensed under the MIT License (the "License"); you may not use this file except in 
// compliance with the License. You may obtain a copy of the License at
// http://opensource.org/licenses/MIT

// Unless required by applicable law or agreed to in writing, software distributed under the License is
// distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
// either express or implied. See the License for the specific language governing permissions and
// limitations under the License.


/* netsource.cc
 *
 *  Created on: 2012-7-17
 *      Author: yerungui
 */

#include "net_source.h"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <sstream>

#include "boost/bind.hpp"

#include "mars/comm/marcotoolkit.h"
#include "mars/comm/socket/unix_socket.h"
#include "mars/comm/xlogger/xlogger.h"
#include "mars/comm/time_utils.h"
#include "mars/comm/strutil.h"
#include "mars/comm/thread/lock.h"
#include "mars/comm/thread/thread.h"
#include "mars/comm/platform_comm.h"
#include "mars/stn/stn.h"
#include "mars/stn/dns_profile.h"
#include "mars/stn/config.h"

using namespace mars::stn;

static const char* const kItemDelimiter = ":";

static const int kNumMakeCount = 5;

//mmnet ipport settings
// 好大：长连接
static std::vector<std::string> sg_longlink_hosts;
static std::vector<uint16_t> sg_longlink_ports;
static std::string sg_longlink_debugip;
static std::vector<uint16_t> sg_lowpriority_longlink_ports;                         // 好大：为长连接设置兜底的端口列表。

// 好大：长连接
static int sg_shortlink_port;
static std::string sg_shortlink_debugip;

static std::map< std::string, std::vector<std::string> > sg_host_backupips_mapping; // 好大：为 DNS 解析设置兜底的 IP 地址
static std::map< std::string, std::string > sg_host_debugip_mapping;

static Mutex sg_ip_mutex;

static Mutex sg_slproxymutex;
static Thread sg_slproxyThread(XLOGGER_TAG"::proxy");
static std::string sg_slproxyip = "";
static uint16_t sg_slproxyport = 0;
static uint64_t sg_slproxytimetick = gettickcount();
static int sg_slproxycount = 0;

// 好大：获取某个 host 对应的系统网络代理信息
static void __GetProxyInfo(uint64_t _timetick, std::string _host) {
    xinfo_function(TSF"_timetick:%_, _host:%_", _timetick, _host);

    int tmp_port = 0;
    std::string tmp_proxy;

    if (!::getProxyInfo(tmp_port, tmp_proxy, _host.empty() ? "" : "http://" + _host)) {
        ScopedLock lock(sg_slproxymutex);

        if (sg_slproxytimetick != _timetick) return;

        ++sg_slproxycount;
        return;
    }

    ScopedLock lock(sg_slproxymutex);

    if (sg_slproxytimetick != _timetick) return;

    ++sg_slproxycount;

    if (tmp_proxy.empty() || 0 == tmp_port) return;

	// 好大：proxy 如果不是 IP 地址而是host，通过正常 DNS 查询 IP 地址。
    static DNS s_dns;
    std::vector<std::string> result;
    s_dns.GetHostByName(tmp_proxy, result);

    if (result.empty()) return;

    sg_slproxyip = result.front();
    sg_slproxyport = (uint16_t)tmp_port;
}

NetSource::DnsUtil::DnsUtil():
new_dns_(OnNewDns) {
}

NetSource::DnsUtil::~DnsUtil() {}

void NetSource::DnsUtil::Cancel(const std::string& host) {
    if (host.empty()) {
        new_dns_.Cancel();
        dns_.Cancel();
    }
    else {
        new_dns_.Cancel(host);
        dns_.Cancel(host);
    }
}

NetSource::NetSource(ActiveLogic& _active_logic)
	: active_logic_(_active_logic)
{
    xdebug_function();
}

NetSource::~NetSource() {
    xdebug_function();
}

/**
 *	host ip port setting from java
 */
void NetSource::SetLongLink(const std::vector<std::string>& _hosts, const std::vector<uint16_t>& _ports, const std::string& _debugip) {
	ScopedLock lock(sg_ip_mutex);

	xgroup2_define(addr_print);
	xinfo2(TSF"task set longlink server addr, ") >> addr_print;
	for (std::vector<std::string>::const_iterator host_iter = _hosts.begin(); host_iter != _hosts.end(); ++host_iter) {
		xinfo2(TSF "host:%_ ", *host_iter) >> addr_print;
	}
	for (std::vector<uint16_t>::const_iterator port_iter = _ports.begin(); port_iter != _ports.end(); ++port_iter) {
		xinfo2(TSF "port:%_ ", *port_iter) >> addr_print;
	}
	xinfo2(TSF"debugip:%_", _debugip) >> addr_print;

    sg_longlink_debugip = _debugip;
    sg_longlink_hosts = _hosts;
	sg_longlink_ports = _ports;
}

void NetSource::SetShortlink(const uint16_t _port, const std::string& _debugip) {
	ScopedLock lock(sg_ip_mutex);

	xinfo2(TSF "task set shortlink server addr, port:%_, debugip:%_", _port, _debugip);

	sg_shortlink_port = _port;
    sg_shortlink_debugip = _debugip;
}

void NetSource::SetBackupIPs(const std::string& _host, const std::vector<std::string>& _ips) {
	ScopedLock lock(sg_ip_mutex);

	xgroup2_define(addr_print);
	xinfo2(TSF"task set backup server addr, host:%_", _host) >> addr_print;
	for (std::vector<std::string>::const_iterator ip_iter = _ips.begin(); ip_iter != _ips.end(); ++ip_iter) {
		xinfo2(TSF "ip:%_ ", *ip_iter) >> addr_print;
	}

	sg_host_backupips_mapping[_host] = _ips;
}

void NetSource::SetDebugIP(const std::string& _host, const std::string& _ip) {
	ScopedLock lock(sg_ip_mutex);

	xinfo2(TSF "task set debugip:%_ for host:%_", _ip, _host);

	if (_ip.empty() && sg_host_debugip_mapping.find(_host) != sg_host_debugip_mapping.end()) {
		sg_host_debugip_mapping.erase(_host);
	}
	else {
		sg_host_debugip_mapping[_host] = _ip;
	}
}

std::string& NetSource::GetLongLinkDebugIP() {
	ScopedLock lock(sg_ip_mutex);

	return sg_longlink_debugip;
}

void NetSource::SetLowPriorityLonglinkPorts(const std::vector<uint16_t>& _lowpriority_longlink_ports) {
    sg_lowpriority_longlink_ports = _lowpriority_longlink_ports;
}

/**
 *
 * longlink functions
 *
 */
std::vector<std::string>& NetSource::GetLongLinkHosts() {
    ScopedLock lock(sg_ip_mutex);
	return sg_longlink_hosts;
}

void NetSource::GetLonglinkPorts(std::vector<uint16_t>& _ports) {
    ScopedLock lock(sg_ip_mutex);
	_ports = sg_longlink_ports;
}

bool NetSource::GetLongLinkItems(std::vector<IPPortItem>& _ipport_items, DnsUtil& _dns_util) {
    
    ScopedLock lock(sg_ip_mutex);

    if (__GetLonglinkDebugIPPort(_ipport_items)) {
        return true;
    }
    
    lock.unlock();

 	std::vector<std::string> longlink_hosts = NetSource::GetLongLinkHosts();
 	if (longlink_hosts.empty()) {
 		xerror2("longlink host empty.");
 		return false;
 	}

 	__GetIPPortItems(_ipport_items, longlink_hosts, _dns_util, true);

	return !_ipport_items.empty();
}

bool NetSource::__GetLonglinkDebugIPPort(std::vector<IPPortItem>& _ipport_items) {

	if (!sg_longlink_debugip.empty()) {
		for (std::vector<uint16_t>::iterator iter = sg_longlink_ports.begin(); iter != sg_longlink_ports.end(); ++iter) {
			IPPortItem item;
			item.str_ip = sg_longlink_debugip;
			item.str_host = sg_longlink_hosts.front();
			item.port = *iter;
			item.source_type = kIPSourceDebug;
			_ipport_items.push_back(item);
		}
		return true;
	}

	for (std::vector<std::string>::iterator ip_iter = sg_longlink_hosts.begin(); ip_iter != sg_longlink_hosts.end(); ++ip_iter) {
		if (sg_host_debugip_mapping.find(*ip_iter) != sg_host_debugip_mapping.end()) {
			for (std::vector<uint16_t>::iterator iter = sg_longlink_ports.begin(); iter != sg_longlink_ports.end(); ++iter) {
				IPPortItem item;
				item.str_ip = (*sg_host_debugip_mapping.find(*ip_iter)).second;
				item.str_host = *ip_iter;
				item.port = *iter;
				item.source_type = kIPSourceDebug;
				_ipport_items.push_back(item);
			}
			return true;
		}
	}

	return false;
}

void NetSource::GetBackupIPs(std::string _host, std::vector<std::string>& _iplist) {
	ScopedLock lock(sg_ip_mutex);
	if (sg_host_backupips_mapping.find(_host) != sg_host_backupips_mapping.end()) {
		_iplist = (*sg_host_backupips_mapping.find(_host)).second;
	}
}

void NetSource::ReportLongIP(bool _is_success, const std::string& _ip, uint16_t _port) {
    xdebug2(TSF"_is_success=%0, ip=%1, port=%2", _is_success, _ip, _port);

    if (_ip.empty() || 0 == _port) return;

    if (kNoNet == getNetInfo()) return;

    ipportstrategy_.Update(_ip, _port, _is_success);
}

void NetSource::RemoveLongBanIP(const std::string& _ip) {
    ipportstrategy_.RemoveBannedList(_ip);
}

/**
 *
 * shortlink functions
 *
 */
uint16_t NetSource::GetShortLinkPort() {
	ScopedLock lock(sg_ip_mutex);
	return sg_shortlink_port;
}

bool NetSource::__HasShortLinkDebugIP(std::vector<std::string> _hostlist) {
	if (!sg_shortlink_debugip.empty()) {
		return true;
	}

	for (std::vector<std::string>::iterator host = _hostlist.begin(); host != _hostlist.end(); ++host) {
		if (sg_host_debugip_mapping.find(*host) != sg_host_debugip_mapping.end()) {
			return true;
		}
	}

	return false;
}

bool NetSource::GetShortLinkItems(std::vector<std::string>& _hostlist, std::vector<IPPortItem>& _ipport_items, DnsUtil& _dns_util) {
	if (_hostlist.empty()) return false;
    
    ScopedLock lock(sg_ip_mutex);
    
	if (__GetShortlinkDebugIPPort(_hostlist, _ipport_items)) {
		return true;
    }
    
    lock.unlock();

    __GetIPPortItems(_ipport_items, _hostlist, _dns_util, false);

	return !_ipport_items.empty();
}

bool NetSource::__GetShortlinkDebugIPPort(std::vector<std::string> _hostlist, std::vector<IPPortItem>& _ipport_items) {
	if (!sg_shortlink_debugip.empty()) {
		IPPortItem item;
		item.str_ip = sg_shortlink_debugip;
		item.str_host = _hostlist.front();
		item.port = sg_shortlink_port;
		item.source_type = kIPSourceDebug;
		_ipport_items.push_back(item);
	}

	for (std::vector<std::string>::iterator host = _hostlist.begin(); host != _hostlist.end(); ++host) {
		if (sg_host_debugip_mapping.find(*host) != sg_host_debugip_mapping.end()) {
			IPPortItem item;
			item.str_ip = (*sg_host_debugip_mapping.find(*host)).second;
			item.str_host = *host;
			item.port = sg_shortlink_port;
			item.source_type = kIPSourceDebug;
			_ipport_items.push_back(item);
		}
	}

	return !_ipport_items.empty();
}

void NetSource::__GetIPPortItems(std::vector<IPPortItem>& _ipport_items, std::vector<std::string> _hostlist, DnsUtil& _dns_util, bool _islonglink) {
	// 好大：为什么要采取这样的策略：active 的时候获取 _hostlist 前面的 host，而非 active 的时候尽量在结果集中平均分布？
	if (active_logic_.IsActive()) {
		unsigned int merge_type_count = 0;
		unsigned int makelist_count = kNumMakeCount;    // 好大：kNumMakeCount = 5，

        // 好大：STEP 1 先获取正常的 IP X PORT
		for (std::vector<std::string>::iterator iter = _hostlist.begin(); iter != _hostlist.end(); ++iter) {
            /**
             * 好大：如果第一个 host 就解析出 kNumMakeCount 个 IP，再添加一个额外的名额，目的是为了让结果列表里面不全是这一个 host 的结果。
             * 不然的话，这个 host 全挂了的话也没有机会可以尝试其他 host 了。
             */
			if (merge_type_count == 1 && _ipport_items.size() == kNumMakeCount) makelist_count = kNumMakeCount + 1;

			if (0 < __MakeIPPorts(_ipport_items, *iter, makelist_count, _dns_util, false, _islonglink)) merge_type_count++;
		}

        // 好大：STEP 2 然后在获取 backup IP X PORT。
        // 好大：注意：如果没有设置 backup ip 和 low priority ports，那么这里可能返回和 STEP 1 重复的记录。
		for (std::vector<std::string>::iterator iter = _hostlist.begin(); iter != _hostlist.end(); ++iter) {
			if (merge_type_count == 1 && _ipport_items.size() == kNumMakeCount) makelist_count = kNumMakeCount + 1;

			if (0 < __MakeIPPorts(_ipport_items, *iter, makelist_count, _dns_util, true, _islonglink)) merge_type_count++;
		}
	}
	else {
		size_t host_count = _hostlist.size();
		size_t ret = (kNumMakeCount - 1) / host_count;
		size_t ret2 = (kNumMakeCount - 1) % host_count;
		size_t i = 0;
		size_t count = 0;
        /**
         * 好大：给一个 demo 数据集合，就明白为什么这样做了。
         *
         *    host_count                                    kNumMakeCount
         *
         *               ┌───────────────────────────────────────────────────────────────────────────────┐
         *               │                                       5                                       │
         *  ┌────────────┼───────────────────────────────────────────────────────────────────────────────┤
         *  │     1      │4/1=4 4%1=0 => i:0 count:4                                                     │
         *  ├────────────┼───────────────────────────────────────────────────────────────────────────────┤
         *  │     2      │4/2=2 4%2=0 => i:0 count:2, i:1 count:4                                        │
         *  ├────────────┼───────────────────────────────────────────────────────────────────────────────┤
         *  │     3      │4/3=1 4%3=1 => i:0 count:2, i:1 count:3, i:2 count:4                           │
         *  ├────────────┼───────────────────────────────────────────────────────────────────────────────┤
         *  │     4      │4/4=1 4%4=0 => i:0 count:1, i:1 count:2, i:2 count:3, i:3 count:4              │
         *  ├────────────┼───────────────────────────────────────────────────────────────────────────────┤
         *  │     5      │4/5=0 4%5=4 => i:0 count:1, i:1 count:2, i:2 count:3, i:3 count:4, i:4 count:4 │
         *  └────────────┴───────────────────────────────────────────────────────────────────────────────┘
         *
         *  这样做的目的就是为了在 kNumMakeCount 限制下，尽量将所有 host 都包含在结果列表里面。
         *
         */

		for (std::vector<std::string>::iterator host_iter = _hostlist.begin(); host_iter != _hostlist.end() && count < kNumMakeCount - 1; ++host_iter) {
			count += i < ret2 ? ret + 1 : ret;
			__MakeIPPorts(_ipport_items, *host_iter, count, _dns_util, false, _islonglink);
			i++;
		}

		for (std::vector<std::string>::iterator host_iter = _hostlist.begin(); host_iter != _hostlist.end() && count < kNumMakeCount; ++host_iter) {
			__MakeIPPorts(_ipport_items, *host_iter, kNumMakeCount, _dns_util, true, _islonglink);
		}
	}
}

size_t NetSource::__MakeIPPorts(std::vector<IPPortItem>& _ip_items, const std::string _host, size_t _count, DnsUtil& _dns_util, bool _isbackup, bool _islonglink) {

	IPSourceType ist = kIPSourceNULL;
	std::vector<std::string> iplist;
    std::vector<uint16_t> ports;

	/**
	 * 好大：非 backup：通过 DNS，NEWDNS 解析出 IP 地址列表；PORTS 读取长连接或短连接端口
	 * 好大：backup 则从 sg_host_backupips_mapping 获得对应 IP 地址，但是如果没有设置 host 的 backup ips，那么会通过 DNS 解析然后将结果更新至 sg_host_backupips_mapping。
	 * 		PORTS 长连接读取 low priority ports，短连接照旧。
	 */
	if (!_isbackup) {
		DnsProfile dns_profile;
		dns_profile.host = _host;

        // 好大：首先尝试通过 new dns 解析 IP 地址
		bool ret = _dns_util.GetNewDNS().GetHostByName(_host, iplist);

		dns_profile.end_time = gettickcount();
		if (!ret) dns_profile.OnFailed();
		ReportDnsProfile(dns_profile);

		xgroup2_define(dnsxlog);
		xdebug2(TSF"link host:%_, new dns ret:%_, size:%_ ", _host, ret, iplist.size()) >> dnsxlog;

        // 好大：如果 new dns 不能解析，再尝试走标准的 DNS 解析
		if (iplist.empty()) {
			dns_profile.Reset();
			dns_profile.dnstype = kType_Dns;

			ist = kIPSourceDNS;
			ret = _dns_util.GetDNS().GetHostByName(_host, iplist);

			dns_profile.end_time = gettickcount();
			if (!ret) dns_profile.OnFailed();
			ReportDnsProfile(dns_profile);

			xdebug2(TSF "dns ret:%_, size:%_,", ret, iplist.size()) >> dnsxlog;
		}
		else {
			ist = kIPSourceNewDns;
		}

		if (_islonglink) {
			NetSource::GetLonglinkPorts(ports);
		}
		else {
			ports.push_back(NetSource::GetShortLinkPort());
		}
	}
	else {
		NetSource::GetBackupIPs(_host, iplist);
		xdebug2(TSF"link host:%_, backup ips size:%_", _host, iplist.size());

		// 好大：如果没有设置 backup ips ，将正常 DNS query 结果设置为 backup ips 。
        if (iplist.empty() && _dns_util.GetDNS().GetHostByName(_host, iplist)) {
            ScopedLock lock(sg_ip_mutex);
            sg_host_backupips_mapping[_host] = iplist;
        }
        
		if (_islonglink) {
			// 好大：如果设置了 sg_lowpriority_longlink_ports 返回 sg_lowpriority_longlink_ports，没有设置就返回正常长连接端口。
            if (sg_lowpriority_longlink_ports.empty()) {
                NetSource::GetLonglinkPorts(ports);
            } else {
                ports = sg_lowpriority_longlink_ports;
            }
		}
		else {
			ports.push_back(NetSource::GetShortLinkPort());
		}
		ist = kIPSourceBackup;
	}

	if (iplist.empty()) return 0;

	size_t len = _ip_items.size();

	std::vector<IPPortItem> temp_items;
	for (std::vector<std::string>::iterator ip_iter = iplist.begin(); ip_iter != iplist.end(); ++ip_iter) {
		for (std::vector<uint16_t>::iterator port_iter = ports.begin(); port_iter != ports.end(); ++port_iter) {
			IPPortItem item;
			item.str_ip = *ip_iter;
			item.source_type = ist;
			item.str_host = _host;
			item.port = *port_iter;
			temp_items.push_back(item);
		}
	}

    // 好大：只允许 _ip_items 里面最多 _count 个记录，加上已有和新的元素。
    if (!_isbackup) {
        // 好大：将 IP X PORTS 的集合进行排序，详细步骤参见 SimpleIPPortSort::SortandFilter，不包括已经在 _ip_items 的记录。
		ipportstrategy_.SortandFilter(temp_items, (int)(_count - len));
		_ip_items.insert(_ip_items.end(), temp_items.begin(), temp_items.end());
	}
	else {
        // 好大：获得 backup ip时，将 IP X PORTS 的集合进行随机处理，没有排序策略。包括已经在 _ip_items 的记录。
		_ip_items.insert(_ip_items.end(), temp_items.begin(), temp_items.end());
		srand((unsigned)gettickcount());
		std::random_shuffle(_ip_items.begin() + len, _ip_items.end());
		_ip_items.resize(std::min(_ip_items.size(), (size_t)_count));
	}

	return _ip_items.size();
}

void NetSource::ReportShortIP(bool _is_success, const std::string& _ip, const std::string& _host, uint16_t _port) {
    xdebug2(TSF"_is_success=%0, ip=%1, port=%2 host=%3", _is_success, _ip, _port, _host);

    if (_ip.empty()) return;

    if (kNoNet == getNetInfo()) return;

    ipportstrategy_.Update(_ip, _port, _is_success);
}

/**
 * use proxy
 */
// 好大：根据提供的 _hostlist ，获取系统代理设置中对应的 proxy IP X PORT。只有短连接可以用 proxy。只会用到 _hostlist 第一个 host。
bool NetSource::GetShortLinkProxyInfo(uint16_t& _port, std::string& _ipproxy, const std::vector<std::string>& _hostlist) {
    if (__HasShortLinkDebugIP(_hostlist)) return false;
    
#ifdef ANDROID

    if (kMobile != getNetInfo()) return false;

#endif

    ScopedLock lock(sg_slproxymutex, false);

    if (!lock.timedlock(500)) return false;

	// 好大：触发异步更新 proxy 信息
    if (sg_slproxycount < 3 || (5 * 1000) > gettickspan(sg_slproxytimetick)) {
        sg_slproxyThread.start(boost::bind(&__GetProxyInfo, sg_slproxytimetick, _hostlist.empty() ? "" : _hostlist.front()));
    }


    if (sg_slproxyip.empty() || 0 == sg_slproxyport) return false;

    _ipproxy = sg_slproxyip;
    _port = (unsigned int)sg_slproxyport;
    return true;
}

void NetSource::__ClearShortLinkProxyInfo() {
    ScopedLock lock(sg_slproxymutex);
    sg_slproxyip = "";
    sg_slproxyport = 0;
    sg_slproxycount = 0;
    sg_slproxytimetick = ::gettickcount();
}

// 好大：1. 清除 proxy 信息; 2. 将所有的记录都放入 banned list 里面
void NetSource::ClearCache() {
    xverbose_function();
    __ClearShortLinkProxyInfo();
    ipportstrategy_.InitHistory2BannedList(true);
}

std::string NetSource::DumpTable(const std::vector<IPPortItem>& _ipport_items) {
    std::stringstream stream;

    for (unsigned int i = 0; i < _ipport_items.size(); ++i) {
        stream << _ipport_items[i].str_ip << kItemDelimiter << _ipport_items[i].port << kItemDelimiter << _ipport_items[i].str_host
               << kItemDelimiter << IPSourceTypeString[_ipport_items[i].source_type];

        if (i != _ipport_items.size() - 1) {
            stream << "|";
        }
    }

    return stream.str();
}

bool NetSource::GetLongLinkSpeedTestIPs(std::vector<IPPortItem>& _ip_vec) {
    xverbose_function();

    return true;
}

void NetSource::ReportLongLinkSpeedTestResult(std::vector<IPPortItem>& _ip_vec) {
}
