// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests the chrome.alarms extension API.

#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/alarms/alarm_manager.h"
#include "chrome/browser/extensions/api/alarms/alarms_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/background_info.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef extensions::api::alarms::Alarm JsAlarm;

namespace utils = extension_function_test_utils;

namespace extensions {

namespace {

// Test delegate which quits the message loop when an alarm fires.
class AlarmDelegate : public AlarmManager::Delegate {
 public:
  virtual ~AlarmDelegate() {}
  virtual void OnAlarm(const std::string& extension_id,
                       const Alarm& alarm) OVERRIDE {
    alarms_seen.push_back(alarm.js_alarm->name);
    base::MessageLoop::current()->Quit();
  }

  std::vector<std::string> alarms_seen;
};

}  // namespace

void RunScheduleNextPoll(AlarmManager* alarm_manager) {
  alarm_manager->ScheduleNextPoll();
}

class ExtensionAlarmsTest : public BrowserWithTestWindowTest {
 public:
  virtual void SetUp() {
    BrowserWithTestWindowTest::SetUp();

    test_clock_ = new base::SimpleTestClock();
    alarm_manager_ = AlarmManager::Get(browser()->profile());
    alarm_manager_->SetClockForTesting(test_clock_);

    alarm_delegate_ = new AlarmDelegate();
    alarm_manager_->set_delegate(alarm_delegate_);

    extension_ = utils::CreateEmptyExtensionWithLocation(
        extensions::Manifest::UNPACKED);

    // Make sure there's a RenderViewHost for alarms to warn into.
    AddTab(browser(), BackgroundInfo::GetBackgroundURL(extension_.get()));
    contents_ = browser()->tab_strip_model()->GetActiveWebContents();

    test_clock_->SetNow(base::Time::FromDoubleT(10));
  }

  base::Value* RunFunctionWithExtension(
      UIThreadExtensionFunction* function, const std::string& args) {
    scoped_refptr<UIThreadExtensionFunction> delete_function(function);
    function->set_extension(extension_.get());
    function->SetRenderViewHost(contents_->GetRenderViewHost());
    return utils::RunFunctionAndReturnSingleResult(function, args, browser());
  }

  base::DictionaryValue* RunFunctionAndReturnDict(
      UIThreadExtensionFunction* function, const std::string& args) {
    base::Value* result = RunFunctionWithExtension(function, args);
    return result ? utils::ToDictionary(result) : NULL;
  }

  base::ListValue* RunFunctionAndReturnList(
      UIThreadExtensionFunction* function, const std::string& args) {
    base::Value* result = RunFunctionWithExtension(function, args);
    return result ? utils::ToList(result) : NULL;
  }

  void RunFunction(UIThreadExtensionFunction* function,
                   const std::string& args) {
    scoped_ptr<base::Value> result(RunFunctionWithExtension(function, args));
  }

  std::string RunFunctionAndReturnError(UIThreadExtensionFunction* function,
                                        const std::string& args) {
    function->set_extension(extension_.get());
    function->SetRenderViewHost(contents_->GetRenderViewHost());
    return utils::RunFunctionAndReturnError(function, args, browser());
  }

  void CreateAlarm(const std::string& args) {
    RunFunction(new AlarmsCreateFunction(test_clock_), args);
  }

  // Takes a JSON result from a function and converts it to a vector of
  // JsAlarms.
  std::vector<linked_ptr<JsAlarm> > ToAlarmList(base::ListValue* value) {
    std::vector<linked_ptr<JsAlarm> > list;
    for (size_t i = 0; i < value->GetSize(); ++i) {
      linked_ptr<JsAlarm> alarm(new JsAlarm);
      base::DictionaryValue* alarm_value;
      if (!value->GetDictionary(i, &alarm_value)) {
        ADD_FAILURE() << "Expected a list of Alarm objects.";
        return list;
      }
      EXPECT_TRUE(JsAlarm::Populate(*alarm_value, alarm.get()));
      list.push_back(alarm);
    }
    return list;
  }

  // Creates up to 3 alarms using the extension API.
  void CreateAlarms(size_t num_alarms) {
    CHECK(num_alarms <= 3);

    const char* kCreateArgs[] = {
      "[null, {\"periodInMinutes\": 0.001}]",
      "[\"7\", {\"periodInMinutes\": 7}]",
      "[\"0\", {\"delayInMinutes\": 0}]",
    };
    for (size_t i = 0; i < num_alarms; ++i) {
      scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
          new AlarmsCreateFunction(test_clock_), kCreateArgs[i]));
      EXPECT_FALSE(result.get());
    }
  }

  base::SimpleTestClock* test_clock_;
  AlarmManager* alarm_manager_;
  AlarmDelegate* alarm_delegate_;
  scoped_refptr<extensions::Extension> extension_;

 protected:
  content::WebContents* contents_;
};

void ExtensionAlarmsTestGetAllAlarmsCallback(
    const AlarmManager::AlarmList* alarms) {
  // Ensure the alarm is gone.
  ASSERT_FALSE(alarms);
}

void ExtensionAlarmsTestGetAlarmCallback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10000, alarm->js_alarm->scheduled_time);
  EXPECT_FALSE(alarm->js_alarm->period_in_minutes.get());

  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::MessageLoop::current()->Run();

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);

  // Ensure the alarm is gone.
  test->alarm_manager_->GetAllAlarms(test->extension_->id(), base::Bind(
      ExtensionAlarmsTestGetAllAlarmsCallback));
}

TEST_F(ExtensionAlarmsTest, Create) {
  test_clock_->SetNow(base::Time::FromDoubleT(10));
  // Create 1 non-repeating alarm.
  CreateAlarm("[null, {\"delayInMinutes\": 0}]");

  alarm_manager_->GetAlarm(extension_->id(), std::string(), base::Bind(
      ExtensionAlarmsTestGetAlarmCallback, this));
}

void ExtensionAlarmsTestCreateRepeatingGetAlarmCallback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10060, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.001)));

  test->test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::MessageLoop::current()->Run();

  test->test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  // Wait again, and ensure the alarm fires again.
  RunScheduleNextPoll(test->alarm_manager_);
  base::MessageLoop::current()->Run();

  ASSERT_EQ(2u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);
}

TEST_F(ExtensionAlarmsTest, CreateRepeating) {
  test_clock_->SetNow(base::Time::FromDoubleT(10));

  // Create 1 repeating alarm.
  CreateAlarm("[null, {\"periodInMinutes\": 0.001}]");

  alarm_manager_->GetAlarm(extension_->id(), std::string(), base::Bind(
      ExtensionAlarmsTestCreateRepeatingGetAlarmCallback, this));
}

void ExtensionAlarmsTestCreateAbsoluteGetAlarm2Callback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_FALSE(alarm);

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);
}

void ExtensionAlarmsTestCreateAbsoluteGetAlarm1Callback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::IsNull());

  test->test_clock_->SetNow(base::Time::FromDoubleT(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::MessageLoop::current()->Run();

  test->alarm_manager_->GetAlarm(
      test->extension_->id(), std::string(), base::Bind(
          ExtensionAlarmsTestCreateAbsoluteGetAlarm2Callback, test));
}

TEST_F(ExtensionAlarmsTest, CreateAbsolute) {
  test_clock_->SetNow(base::Time::FromDoubleT(9.99));
  CreateAlarm("[null, {\"when\": 10001}]");

  alarm_manager_->GetAlarm(extension_->id(), std::string(), base::Bind(
      ExtensionAlarmsTestCreateAbsoluteGetAlarm1Callback, this));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm3Callback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_THAT(test->alarm_delegate_->alarms_seen, testing::ElementsAre("", ""));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm2Callback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_THAT(test->alarm_delegate_->alarms_seen, testing::ElementsAre(""));

  test->test_clock_->SetNow(base::Time::FromDoubleT(10.7));
  base::MessageLoop::current()->Run();

  test->alarm_manager_->GetAlarm(
      test->extension_->id(), std::string(), base::Bind(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm3Callback,
          test));
}

void ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm1Callback(
    ExtensionAlarmsTest* test, Alarm* alarm) {
  ASSERT_TRUE(alarm);
  EXPECT_EQ("", alarm->js_alarm->name);
  EXPECT_DOUBLE_EQ(10001, alarm->js_alarm->scheduled_time);
  EXPECT_THAT(alarm->js_alarm->period_in_minutes,
              testing::Pointee(testing::DoubleEq(0.001)));

  test->test_clock_->SetNow(base::Time::FromDoubleT(10.1));
  // Now wait for the alarm to fire. Our test delegate will quit the
  // MessageLoop when that happens.
  base::MessageLoop::current()->Run();

  test->alarm_manager_->GetAlarm(
      test->extension_->id(), std::string(), base::Bind(
          ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm2Callback,
          test));
}

TEST_F(ExtensionAlarmsTest, CreateRepeatingWithQuickFirstCall) {
  test_clock_->SetNow(base::Time::FromDoubleT(9.99));
  CreateAlarm("[null, {\"when\": 10001, \"periodInMinutes\": 0.001}]");

  alarm_manager_->GetAlarm(extension_->id(), std::string(), base::Bind(
      ExtensionAlarmsTestCreateRepeatingWithQuickFirstCallGetAlarm1Callback,
      this));
}

void ExtensionAlarmsTestCreateDupeGetAllAlarmsCallback(
    const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_DOUBLE_EQ(430000, (*alarms)[0].js_alarm->scheduled_time);
}

TEST_F(ExtensionAlarmsTest, CreateDupe) {
  test_clock_->SetNow(base::Time::FromDoubleT(10));

  // Create 2 duplicate alarms. The first should be overridden.
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 1}]");
  CreateAlarm("[\"dup\", {\"delayInMinutes\": 7}]");

  alarm_manager_->GetAllAlarms(extension_->id(), base::Bind(
      ExtensionAlarmsTestCreateDupeGetAllAlarmsCallback));
}

TEST_F(ExtensionAlarmsTest, CreateDelayBelowMinimum) {
  // Create an alarm with delay below the minimum accepted value.
  CreateAlarm("[\"negative\", {\"delayInMinutes\": -0.2}]");
  IPC::TestSink& sink = static_cast<content::MockRenderProcessHost*>(
      contents_->GetRenderViewHost()->GetProcess())->sink();
  const IPC::Message* warning = sink.GetUniqueMessageMatching(
      ExtensionMsg_AddMessageToConsole::ID);
  ASSERT_TRUE(warning);
  content::ConsoleMessageLevel level = content::CONSOLE_MESSAGE_LEVEL_DEBUG;
  std::string message;
  ExtensionMsg_AddMessageToConsole::Read(warning, &level, &message);
  EXPECT_EQ(content::CONSOLE_MESSAGE_LEVEL_WARNING, level);
  EXPECT_THAT(message, testing::HasSubstr("delay is less than minimum of 1"));
}

TEST_F(ExtensionAlarmsTest, Get) {
  test_clock_->SetNow(base::Time::FromDoubleT(4));

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  // Get the default one.
  {
    JsAlarm alarm;
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new AlarmsGetFunction(), "[null]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(JsAlarm::Populate(*result, &alarm));
    EXPECT_EQ("", alarm.name);
    EXPECT_DOUBLE_EQ(4060, alarm.scheduled_time);
    EXPECT_THAT(alarm.period_in_minutes,
                testing::Pointee(testing::DoubleEq(0.001)));
  }

  // Get "7".
  {
    JsAlarm alarm;
    scoped_ptr<base::DictionaryValue> result(RunFunctionAndReturnDict(
        new AlarmsGetFunction(), "[\"7\"]"));
    ASSERT_TRUE(result.get());
    EXPECT_TRUE(JsAlarm::Populate(*result, &alarm));
    EXPECT_EQ("7", alarm.name);
    EXPECT_EQ(424000, alarm.scheduled_time);
    EXPECT_THAT(alarm.period_in_minutes, testing::Pointee(7));
  }

  // Get a non-existent one.
  {
    std::string error = RunFunctionAndReturnError(
        new AlarmsGetFunction(), "[\"nobody\"]");
    EXPECT_FALSE(error.empty());
  }
}

TEST_F(ExtensionAlarmsTest, GetAll) {
  // Test getAll with 0 alarms.
  {
    scoped_ptr<base::ListValue> result(RunFunctionAndReturnList(
        new AlarmsGetAllFunction(), "[]"));
    std::vector<linked_ptr<JsAlarm> > alarms = ToAlarmList(result.get());
    EXPECT_EQ(0u, alarms.size());
  }

  // Create 2 alarms, and make sure we can query them.
  CreateAlarms(2);

  {
    scoped_ptr<base::ListValue> result(RunFunctionAndReturnList(
        new AlarmsGetAllFunction(), "[null]"));
    std::vector<linked_ptr<JsAlarm> > alarms = ToAlarmList(result.get());
    EXPECT_EQ(2u, alarms.size());

    // Test the "7" alarm.
    JsAlarm* alarm = alarms[0].get();
    if (alarm->name != "7")
      alarm = alarms[1].get();
    EXPECT_EQ("7", alarm->name);
    EXPECT_THAT(alarm->period_in_minutes, testing::Pointee(7));
  }
}

void ExtensionAlarmsTestClearGetAllAlarms2Callback(
    const AlarmManager::AlarmList* alarms) {
  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_THAT((*alarms)[0].js_alarm->period_in_minutes,
              testing::Pointee(0.001));
}

void ExtensionAlarmsTestClearGetAllAlarms1Callback(
    ExtensionAlarmsTest* test, const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(1u, alarms->size());
  EXPECT_THAT((*alarms)[0].js_alarm->period_in_minutes,
              testing::Pointee(0.001));

  // Now wait for the alarms to fire, and ensure the cancelled alarms don't
  // fire.
  test->test_clock_->Advance(base::TimeDelta::FromMilliseconds(60));
  RunScheduleNextPoll(test->alarm_manager_);
  base::MessageLoop::current()->Run();

  ASSERT_EQ(1u, test->alarm_delegate_->alarms_seen.size());
  EXPECT_EQ("", test->alarm_delegate_->alarms_seen[0]);

  // Ensure the 0.001-minute alarm is still there, since it's repeating.
  test->alarm_manager_->GetAllAlarms(test->extension_->id(), base::Bind(
      ExtensionAlarmsTestClearGetAllAlarms2Callback));
}

TEST_F(ExtensionAlarmsTest, Clear) {
  // Clear a non-existent one.
  {
    std::string error = RunFunctionAndReturnError(
        new AlarmsClearFunction(), "[\"nobody\"]");
    EXPECT_FALSE(error.empty());
  }

  // Create 3 alarms.
  CreateAlarms(3);

  // Clear all but the 0.001-minute alarm.
  RunFunction(new AlarmsClearFunction(), "[\"7\"]");
  RunFunction(new AlarmsClearFunction(), "[\"0\"]");

  alarm_manager_->GetAllAlarms(extension_->id(), base::Bind(
      ExtensionAlarmsTestClearGetAllAlarms1Callback, this));
}

void ExtensionAlarmsTestClearAllGetAllAlarms2Callback(
    const AlarmManager::AlarmList* alarms) {
  ASSERT_FALSE(alarms);
}

void ExtensionAlarmsTestClearAllGetAllAlarms1Callback(
    ExtensionAlarmsTest* test, const AlarmManager::AlarmList* alarms) {
  ASSERT_TRUE(alarms);
  EXPECT_EQ(3u, alarms->size());

  // Clear them.
  test->RunFunction(new AlarmsClearAllFunction(), "[]");
  test->alarm_manager_->GetAllAlarms(
      test->extension_->id(), base::Bind(
          ExtensionAlarmsTestClearAllGetAllAlarms2Callback));
}

TEST_F(ExtensionAlarmsTest, ClearAll) {
  // ClearAll with no alarms set.
  {
    scoped_ptr<base::Value> result(RunFunctionWithExtension(
        new AlarmsClearAllFunction(), "[]"));
    EXPECT_FALSE(result.get());
  }

  // Create 3 alarms.
  CreateAlarms(3);
  alarm_manager_->GetAllAlarms(extension_->id(), base::Bind(
      ExtensionAlarmsTestClearAllGetAllAlarms1Callback, this));
}

class ExtensionAlarmsSchedulingTest : public ExtensionAlarmsTest {
  void GetAlarmCallback(Alarm* alarm) {
    CHECK(alarm);
    const base::Time scheduled_time =
        base::Time::FromJsTime(alarm->js_alarm->scheduled_time);
    EXPECT_EQ(scheduled_time, alarm_manager_->test_next_poll_time_);
  }

  static void RemoveAlarmCallback (bool success) { EXPECT_TRUE(success); }
  static void RemoveAllAlarmsCallback () {}
 public:
  // Get the time that the alarm named is scheduled to run.
  void VerifyScheduledTime(const std::string& alarm_name) {
    alarm_manager_->GetAlarm(extension_->id(), alarm_name, base::Bind(
        &ExtensionAlarmsSchedulingTest::GetAlarmCallback,
        base::Unretained(this)));
  }

  void RemoveAlarm(const std::string& name) {
    alarm_manager_->RemoveAlarm(
        extension_->id(),
        name,
        base::Bind(&ExtensionAlarmsSchedulingTest::RemoveAlarmCallback));
  }

  void RemoveAllAlarms () {
    alarm_manager_->RemoveAllAlarms(extension_->id(), base::Bind(
        &ExtensionAlarmsSchedulingTest::RemoveAllAlarmsCallback));
  }
};

TEST_F(ExtensionAlarmsSchedulingTest, PollScheduling) {
  {
    CreateAlarm("[\"a\", {\"periodInMinutes\": 6}]");
    CreateAlarm("[\"bb\", {\"periodInMinutes\": 8}]");
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    CreateAlarm("[\"a\", {\"delayInMinutes\": 10}]");
    CreateAlarm("[\"bb\", {\"delayInMinutes\": 21}]");
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    test_clock_->SetNow(base::Time::FromDoubleT(10));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 10}]");
    Alarm alarm;
    alarm.js_alarm->name = "bb";
    alarm.js_alarm->scheduled_time = 30 * 60000;
    alarm.js_alarm->period_in_minutes.reset(new double(30));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm);
    VerifyScheduledTime("a");
    RemoveAllAlarms();
  }
  {
    test_clock_->SetNow(base::Time::FromDoubleT(3 * 60 + 1));
    Alarm alarm;
    alarm.js_alarm->name = "bb";
    alarm.js_alarm->scheduled_time = 3 * 60000;
    alarm.js_alarm->period_in_minutes.reset(new double(3));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm);
    base::MessageLoop::current()->Run();
    EXPECT_EQ(alarm_manager_->last_poll_time_ + base::TimeDelta::FromMinutes(3),
              alarm_manager_->test_next_poll_time_);
    RemoveAllAlarms();
  }
  {
    test_clock_->SetNow(base::Time::FromDoubleT(4 * 60 + 1));
    CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
    RemoveAlarm("a");
    Alarm alarm2;
    alarm2.js_alarm->name = "bb";
    alarm2.js_alarm->scheduled_time = 4 * 60000;
    alarm2.js_alarm->period_in_minutes.reset(new double(4));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm2);
    Alarm alarm3;
    alarm3.js_alarm->name = "ccc";
    alarm3.js_alarm->scheduled_time = 25 * 60000;
    alarm3.js_alarm->period_in_minutes.reset(new double(25));
    alarm_manager_->AddAlarmImpl(extension_->id(), alarm3);
    base::MessageLoop::current()->Run();
    EXPECT_EQ(alarm_manager_->last_poll_time_ + base::TimeDelta::FromMinutes(4),
              alarm_manager_->test_next_poll_time_);
    RemoveAllAlarms();
  }
}

TEST_F(ExtensionAlarmsSchedulingTest, ReleasedExtensionPollsInfrequently) {
  extension_ = utils::CreateEmptyExtensionWithLocation(
      extensions::Manifest::INTERNAL);
  test_clock_->SetNow(base::Time::FromJsTime(300000));
  CreateAlarm("[\"a\", {\"when\": 300010}]");
  CreateAlarm("[\"b\", {\"when\": 340000}]");

  // On startup (when there's no "last poll"), we let alarms fire as
  // soon as they're scheduled.
  EXPECT_DOUBLE_EQ(300010, alarm_manager_->test_next_poll_time_.ToJsTime());

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(290000);
  // In released extensions, we set the granularity to at least 1
  // minute, which makes AddAlarm schedule the next poll after the
  // extension requested.
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromMinutes(1)).ToJsTime(),
                    alarm_manager_->test_next_poll_time_.ToJsTime());
}

TEST_F(ExtensionAlarmsSchedulingTest, TimerRunning) {
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"a\", {\"delayInMinutes\": 0.001}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  test_clock_->Advance(base::TimeDelta::FromMilliseconds(60));
  base::MessageLoop::current()->Run();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
  CreateAlarm("[\"bb\", {\"delayInMinutes\": 10}]");
  EXPECT_TRUE(alarm_manager_->timer_.IsRunning());
  RemoveAllAlarms();
  EXPECT_FALSE(alarm_manager_->timer_.IsRunning());
}

TEST_F(ExtensionAlarmsSchedulingTest, MinimumGranularity) {
  extension_ = utils::CreateEmptyExtensionWithLocation(
      extensions::Manifest::INTERNAL);
  test_clock_->SetNow(base::Time::FromJsTime(0));
  CreateAlarm("[\"a\", {\"periodInMinutes\": 2}]");
  test_clock_->Advance(base::TimeDelta::FromSeconds(1));
  CreateAlarm("[\"b\", {\"periodInMinutes\": 2}]");
  test_clock_->Advance(base::TimeDelta::FromMinutes(2));

  alarm_manager_->last_poll_time_ = base::Time::FromJsTime(2 * 60000);
  // In released extensions, we set the granularity to at least 1
  // minute, which makes scheduler set it to 1 minute, rather than
  // 1 second later (when b is supposed to go off).
  alarm_manager_->ScheduleNextPoll();
  EXPECT_DOUBLE_EQ((alarm_manager_->last_poll_time_ +
                    base::TimeDelta::FromMinutes(1)).ToJsTime(),
                    alarm_manager_->test_next_poll_time_.ToJsTime());
}

}  // namespace extensions