// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_FIND_HELPER_H_
#define ANDROID_WEBVIEW_BROWSER_FIND_HELPER_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"

namespace android_webview {

// Handles the WebView find-in-page API requests.
class FindHelper : public content::WebContentsObserver {
 public:
  class Listener {
   public:
    // Called when receiving a new find-in-page update.
    // This will be triggered when the results of FindAllSync, FindAllAsync and
    // FindNext are available. The value provided in active_ordinal is 0-based.
    virtual void OnFindResultReceived(int active_ordinal,
                                      int match_count,
                                      bool finished) = 0;
    virtual ~Listener() {}
  };

  explicit FindHelper(content::WebContents* web_contents);
  virtual ~FindHelper();

  // Sets the listener to receive find result updates.
  // Does not own the listener and must set to NULL when invalid.
  void SetListener(Listener* listener);

  // Asynchronous API.
  void FindAllAsync(const string16& search_string);
  void HandleFindReply(int request_id,
                       int match_count,
                       int active_ordinal,
                       bool finished);

  // Methods valid in both synchronous and asynchronous modes.
  void FindNext(bool forward);
  void ClearMatches();

 private:
  void StartNewRequest(const string16& search_string);
  void NotifyResults(int active_ordinal, int match_count, bool finished);

  // Listener results are reported to.
  Listener* listener_;

  // Used to check the validity of FindNext operations.
  bool async_find_started_;
  bool sync_find_started_;

  // Used to provide different ids to each request and for result
  // verification in asynchronous calls.
  int find_request_id_counter_;
  int current_request_id_;

  // Required by FindNext and the incremental find replies.
  string16 last_search_string_;
  int last_match_count_;
  int last_active_ordinal_;

  // Used to post synchronous result notifications to ourselves.
  base::WeakPtrFactory<FindHelper> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FindHelper);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_FIND_HELPER_H_
