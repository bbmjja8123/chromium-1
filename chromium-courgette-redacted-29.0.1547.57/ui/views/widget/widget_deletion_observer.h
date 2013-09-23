// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_WIDGET_DELETION_OBSERVER_H_
#define UI_VIEWS_WIDGET_WIDGET_DELETION_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;

// A simple WidgetObserver that can be probed for the life of a widget.
class WidgetDeletionObserver : public WidgetObserver {
 public:
  explicit WidgetDeletionObserver(Widget* widget);
  virtual ~WidgetDeletionObserver();

  // Returns true if the widget passed in the constructor is still alive.
  bool IsWidgetAlive() { return widget_ != NULL; }

  // Overridden from WidgetObserver.
  virtual void OnWidgetDestroying(Widget* widget) OVERRIDE;

 private:
  void CleanupWidget();

  Widget* widget_;

  DISALLOW_COPY_AND_ASSIGN(WidgetDeletionObserver);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_WIDGET_DELETION_OBSERVER_H_
