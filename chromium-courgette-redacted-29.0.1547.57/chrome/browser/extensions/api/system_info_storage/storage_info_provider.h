// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_PROVIDER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_PROVIDER_H_

#include <set>

#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/timer.h"
#include "chrome/browser/extensions/api/system_info/system_info_provider.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_observer.h"
#include "chrome/browser/storage_monitor/removable_storage_observer.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/common/extensions/api/experimental_system_info_storage.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {

namespace systeminfo {

extern const char kStorageTypeUnknown[];
extern const char kStorageTypeFixed[];
extern const char kStorageTypeRemovable[];

}  // namespace systeminfo

typedef std::vector<linked_ptr<
    api::experimental_system_info_storage::StorageUnitInfo> > StorageInfo;

class StorageInfoProvider
    : public SystemInfoProvider<StorageInfo>,
      public chrome::RemovableStorageObserver {
 public:
  // Get the single shared instance of StorageInfoProvider.
  static StorageInfoProvider* Get();

  // Add and remove observer, both can be called from any thread.
  void AddObserver(StorageInfoObserver* obs);
  void RemoveObserver(StorageInfoObserver* obs);

  // Start and stop watching the given storage |id|.
  virtual void StartWatching(const std::string& id);
  virtual void StopWatching(const std::string& id);

  // Get the information for the storage unit specified by the |id| parameter,
  // and output the result to the |info|.
  virtual bool QueryUnitInfo(const std::string& id,
      api::experimental_system_info_storage::StorageUnitInfo* info) = 0;

 protected:
  StorageInfoProvider();
  virtual ~StorageInfoProvider();

  void SetWatchingIntervalForTesting(size_t ms) { watching_interval_ = ms; }

 private:
  typedef std::map<std::string, double> StorageIDToSizeMap;

  // chrome::RemovableStorageObserver implementation.
  virtual void OnRemovableStorageAttached(
      const chrome::StorageInfo& info) OVERRIDE;
  virtual void OnRemovableStorageDetached(
      const chrome::StorageInfo& info) OVERRIDE;

  // Query the new attached removable storage info on the blocking pool.
  void QueryAttachedStorageInfoOnBlockingPool(const std::string& id);

  // Posts a task to check for free space changes on the blocking pool.
  // Should be called on the UI thread.
  void CheckWatchedStorages();

  // Check if the free space changes for the watched storages by iterating over
  // the |storage_id_to_size_map_|. It is called on blocking pool.
  void CheckWatchedStoragesOnBlockingPool();

  // Provides an explicit hook that tests can wait for to determine that the
  // call to CheckWatchedStorages() has completed.
  virtual void OnCheckWatchedStoragesFinishedForTesting() {}

  // Add and remove the storage to be watched.
  void AddWatchedStorageOnBlockingPool(const std::string& id);
  void RemoveWatchedStorageOnBlockingPool(const std::string& id);

  void StartWatchingTimerOnUIThread();
  // Force to stop the watching timer or there is no any one storage to be
  // watched. It is called on UI thread.
  void StopWatchingTimerOnUIThread();

  // Mapping of the storage being watched and the recent free space value. It
  // is maintained on the blocking pool.
  StorageIDToSizeMap storage_id_to_size_map_;

  // The timer used for watching the storage free space changes periodically.
  base::RepeatingTimer<StorageInfoProvider> watching_timer_;

  // The thread-safe observer list that observe the changes happening on the
  // storages.
  scoped_refptr<ObserverListThreadSafe<StorageInfoObserver> > observers_;

  // The time interval for watching the free space change, in milliseconds.
  // Only changed for testing purposes.
  size_t watching_interval_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INFO_STORAGE_STORAGE_INFO_PROVIDER_H_
