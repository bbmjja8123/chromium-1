// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/ui_proxy_config_service.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/net/proxy_config_handler.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "grit/generated_resources.h"
#include "net/proxy/proxy_config.h"

namespace chromeos {

namespace {

const char* ModeToString(UIProxyConfig::Mode mode) {
  switch (mode) {
    case UIProxyConfig::MODE_DIRECT:
      return "direct";
    case UIProxyConfig::MODE_AUTO_DETECT:
      return "auto-detect";
    case UIProxyConfig::MODE_PAC_SCRIPT:
      return "pacurl";
    case UIProxyConfig::MODE_SINGLE_PROXY:
      return "single-proxy";
    case UIProxyConfig::MODE_PROXY_PER_SCHEME:
      return "proxy-per-scheme";
  }
  NOTREACHED() << "Unrecognized mode type";
  return "";
}

// Writes the proxy config of |network| to |proxy_config|.  Returns false if no
// proxy was configured for this network.
bool GetProxyConfig(const NetworkState& network,
                    net::ProxyConfig* proxy_config) {
  scoped_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(network);
  if (!proxy_dict)
    return false;
  return PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(*proxy_dict,
                                                           proxy_config);
}

// Returns true if proxy settings of |network| are editable.
bool IsNetworkProxySettingsEditable(const NetworkState& network) {
  onc::ONCSource source = network.onc_source();
  return source != onc::ONC_SOURCE_DEVICE_POLICY &&
      source != onc::ONC_SOURCE_USER_POLICY;
}

}  // namespace

UIProxyConfigService::UIProxyConfigService() {
}

UIProxyConfigService::~UIProxyConfigService() {
}

void UIProxyConfigService::SetPrefs(PrefService* pref_service) {
  pref_service_ = pref_service;
}

void UIProxyConfigService::SetCurrentNetwork(
    const std::string& current_network) {
  const NetworkState* network = NULL;
  if (!current_network.empty()) {
    network = NetworkHandler::Get()->network_state_handler()->GetNetworkState(
        current_network);
    LOG_IF(ERROR, !network)
        << "Can't find requested network " << current_network;
  }
  current_ui_network_ = current_network;
  if (!network) {
    current_ui_network_.clear();
    current_ui_config_ = UIProxyConfig();
    return;
  }

  DetermineEffectiveConfig(*network);
  VLOG(1) << "Current ui network: "
          << network->name()
          << ", " << ModeToString(current_ui_config_.mode) << ", "
          << ProxyPrefs::ConfigStateToDebugString(current_ui_config_.state)
          << ", modifiable:" << current_ui_config_.user_modifiable;
}

void UIProxyConfigService::GetProxyConfig(UIProxyConfig* config) const {
  *config = current_ui_config_;
}

void UIProxyConfigService::SetProxyConfig(const UIProxyConfig& config) {
  current_ui_config_ = config;
  if (current_ui_network_.empty())
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(current_ui_network_);
  if (!network) {
    LOG(ERROR) << "Can't find requested network " << current_ui_network_;
    return;
  }

  // Store config for this network.
  scoped_ptr<base::DictionaryValue> proxy_config_value(
      config.ToPrefProxyConfig());
  ProxyConfigDictionary proxy_config_dict(proxy_config_value.get());

  VLOG(1) << "Set proxy for " << current_ui_network_
          << " to " << *proxy_config_value;

  proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
  current_ui_config_.state = ProxyPrefs::CONFIG_SYSTEM;
}

void UIProxyConfigService::DetermineEffectiveConfig(
    const NetworkState& network) {
  DCHECK(pref_service_);

  // Get prefs proxy config if available.
  net::ProxyConfig pref_config;
  ProxyPrefs::ConfigState pref_state = ProxyConfigServiceImpl::ReadPrefConfig(
      pref_service_, &pref_config);

  // Get network proxy config if available.
  net::ProxyConfig network_config;
  net::ProxyConfigService::ConfigAvailability network_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  if (chromeos::GetProxyConfig(network, &network_config)) {
    // Network is private or shared with user using shared proxies.
    VLOG(1) << this << ": using network proxy: " << network.proxy_config();
    network_availability = net::ProxyConfigService::CONFIG_VALID;
  }

  // Determine effective proxy config, either from prefs or network.
  ProxyPrefs::ConfigState effective_config_state;
  net::ProxyConfig effective_config;
  ProxyConfigServiceImpl::GetEffectiveProxyConfig(
      pref_state, pref_config,
      network_availability, network_config, false,
      &effective_config_state, &effective_config);

  // Store effective proxy into |current_ui_config_|.
  current_ui_config_.FromNetProxyConfig(effective_config);
  current_ui_config_.state = effective_config_state;
  if (ProxyConfigServiceImpl::PrefPrecedes(effective_config_state)) {
    current_ui_config_.user_modifiable = false;
  } else if (!IsNetworkProxySettingsEditable(network)) {
    // TODO(xiyuan): Figure out the right way to set config state for managed
    // network.
    current_ui_config_.state = ProxyPrefs::CONFIG_POLICY;
    current_ui_config_.user_modifiable = false;
  } else {
    current_ui_config_.user_modifiable =
        !ProxyConfigServiceImpl::IgnoreProxy(pref_service_,
                                             network.profile_path(),
                                             network.onc_source());
  }
}

}  // namespace chromeos
