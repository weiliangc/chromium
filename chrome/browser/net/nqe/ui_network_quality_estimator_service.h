// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_
#define CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality_estimator.h"

class PrefRegistrySimple;
class Profile;

namespace net {
class NetworkQualitiesPrefsManager;
}

// UI service to determine the current EffectiveConnectionType.
class UINetworkQualityEstimatorService
    : public KeyedService,
      public net::NetworkQualityEstimator::NetworkQualityProvider {
 public:
  explicit UINetworkQualityEstimatorService(Profile* profile);
  ~UINetworkQualityEstimatorService() override;

  // Registers the profile-specific network quality estimator prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // The current EffectiveConnectionType.
  net::EffectiveConnectionType GetEffectiveConnectionType() const override;

  // Tests can manually set EffectiveConnectionType, but browser tests should
  // expect that the EffectiveConnectionType could change.
  void SetEffectiveConnectionTypeForTesting(net::EffectiveConnectionType type);

  // Reads the prefs from the disk, parses them into a map of NetworkIDs and
  // CachedNetworkQualities, and returns the map.
  std::map<net::nqe::internal::NetworkID,
           net::nqe::internal::CachedNetworkQuality>
  ForceReadPrefsForTesting() const;

 private:
  class IONetworkQualityObserver;

  // KeyedService implementation:
  void Shutdown() override;

  // Called when the EffectiveConnectionType has updated to |type|.
  // NetworkQualityEstimator::EffectiveConnectionType is an estimate of the
  // quality of the network that may differ from the actual network type
  // reported by NetworkchangeNotifier::GetConnectionType.
  void EffectiveConnectionTypeChanged(net::EffectiveConnectionType type);

  // The current EffectiveConnectionType.
  net::EffectiveConnectionType type_;

  // IO thread based observer that is owned by this service. Created on the UI
  // thread, but used and deleted on the IO thread.
  std::unique_ptr<IONetworkQualityObserver> io_observer_;

  // Prefs manager that is owned by this service. Created on the UI thread, but
  // used and deleted on the IO thread.
  std::unique_ptr<net::NetworkQualitiesPrefsManager> prefs_manager_;

  base::WeakPtrFactory<UINetworkQualityEstimatorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UINetworkQualityEstimatorService);
};

#endif  // CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_
