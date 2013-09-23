// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"

namespace content {

class MediaPlayerManagerImpl;

// Native mirror of ContentVideoView.java. This class is responsible for
// creating the Java video view and pass all the player status change to
// it. It accepts media control from Java class, and forwards it to
// MediaPlayerManagerImpl.
class ContentVideoView {
 public:
  // Construct a ContentVideoView object. The |manager| will handle all the
  // playback controls from the Java class.
  ContentVideoView(
      const base::android::ScopedJavaLocalRef<jobject>& context,
      const base::android::ScopedJavaLocalRef<jobject>& client,
      MediaPlayerManagerImpl* manager);

  ~ContentVideoView();

  // To open another video on existing ContentVideoView.
  void OpenVideo();

  static bool RegisterContentVideoView(JNIEnv* env);
  static void KeepScreenOn(bool screen_on);

  // Return true if there is existing ContentVideoView object.
  static bool HasContentVideoView();

  // Getter method called by the Java class to get the media information.
  int GetVideoWidth(JNIEnv*, jobject obj) const;
  int GetVideoHeight(JNIEnv*, jobject obj) const;
  int GetDurationInMilliSeconds(JNIEnv*, jobject obj) const;
  int GetCurrentPosition(JNIEnv*, jobject obj) const;
  bool IsPlaying(JNIEnv*, jobject obj);
  void UpdateMediaMetadata(JNIEnv*, jobject obj);

  // Called when the Java fullscreen view is destroyed. If
  // |release_media_player| is true, |manager_| needs to release the player
  // as we are quitting the app.
  void ExitFullscreen(JNIEnv*, jobject, jboolean release_media_player);

  // Media control method called by the Java class.
  void SeekTo(JNIEnv*, jobject obj, jint msec);
  void Play(JNIEnv*, jobject obj);
  void Pause(JNIEnv*, jobject obj);

  // Called by the Java class to pass the surface object to the player.
  void SetSurface(JNIEnv*, jobject obj, jobject surface);

  // Method called by |manager_| to inform the Java class about player status
  // change.
  void UpdateMediaMetadata();
  void OnMediaPlayerError(int errorType);
  void OnVideoSizeChanged(int width, int height);
  void OnBufferingUpdate(int percent);
  void OnPlaybackComplete();
  void OnExitFullscreen();

  // Return the corresponing ContentVideoView Java object if any.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject(JNIEnv* env);

 private:
  // Destroy the |j_content_video_view_|. If |native_view_destroyed| is true,
  // no further calls to the native object is allowed.
  void DestroyContentVideoView(bool native_view_destroyed);

  // Object that manages the fullscreen media player. It is responsible for
  // handling all the playback controls.
  MediaPlayerManagerImpl* manager_;

  // Weak reference of corresponding Java object.
  JavaObjectWeakGlobalRef j_content_video_view_;

  DISALLOW_COPY_AND_ASSIGN(ContentVideoView);
};

} // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIDEO_VIEW_H_
