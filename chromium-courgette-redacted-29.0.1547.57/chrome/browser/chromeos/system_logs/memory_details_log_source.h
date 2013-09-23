// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_MEMORY_DETAILS_LOG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_MEMORY_DETAILS_LOG_SOURCE_H_

#include "chrome/browser/chromeos/system_logs/system_logs_fetcher.h"

namespace chromeos {

// Fetches memory usage details.
class MemoryDetailsLogSource : public SystemLogsSource {
 public:
  MemoryDetailsLogSource() {}
  virtual ~MemoryDetailsLogSource() {}

  // SystemLogsSource override.
  virtual void Fetch(const SysLogsSourceCallback& request) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoryDetailsLogSource);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_LOGS_MEMORY_DETAILS_LOG_SOURCE_H_
