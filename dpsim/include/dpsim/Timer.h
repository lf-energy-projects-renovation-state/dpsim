/* Copyright 2017-2021 Institute for Automation of Complex Power Systems,
 *                     EONERC, RWTH Aachen University
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *********************************************************************************/

#pragma once

#include <chrono>

#include <dpsim-models/Logger.h>
#include <dpsim/Config.h>

namespace DPsim {

class Timer {
public:
  struct OverrunException {
    uint64_t overruns;
  };

  using Ticks = std::chrono::nanoseconds;

  using StartClock = std::chrono::system_clock;
  using IntervalClock = std::chrono::steady_clock;

  using StartTimePoint = std::chrono::time_point<StartClock, Ticks>;
  using IntervalTimePoint = std::chrono::time_point<IntervalClock, Ticks>;

protected:
  enum State { running, stopped } mState;

  StartTimePoint mStartAt;
  IntervalTimePoint mNextTick;
  Ticks mTickInterval;

#ifdef HAVE_TIMERFD
  int mTimerFd;
#endif
  long long mOverruns;
  long long mTicks;
  int mFlags;

  /// Timer log level
  CPS::Logger::Level mLogLevel;
  /// Logger
  CPS::Logger::Log mSLog;

public:
  enum Flags : int { fail_on_overrun = 1 };

  Timer(int flags = 0, CPS::Logger::Level logLevel = CPS::Logger::Level::debug);

  ~Timer();

  void start(const StartTimePoint &startAt);

  /// Start real-time timer
  void start();

  /// Stop real-time timer
  void stop();

  /// Suspend thread execution until next tick
  void sleep();

  // Getter
  const long long &overruns() { return mOverruns; }

  long long ticks() { return mTicks; }

  Ticks interval() { return mTickInterval; }

  // Setter
  void setStartTime(const StartTimePoint &start) { mStartAt = start; }

  void setInterval(const Ticks &intv) { mTickInterval = intv; }

  void setInterval(double dt) {
    mTickInterval = Timer::Ticks((uintmax_t)(dt * 1e9));
  }
};

#ifdef HAVE_TIMERFD
template <class Rep, class Period>
struct timespec to_timespec(std::chrono::duration<Rep, Period> dur) {
  auto nsDur = std::chrono::duration_cast<std::chrono::nanoseconds>(dur);

  struct timespec ts;

  ts.tv_sec = nsDur.count() / 1000000000;
  ts.tv_nsec = nsDur.count() % 1000000000;

  return ts;
}
#endif

} // namespace DPsim

#include <ctime>
#include <iomanip>
#include <iostream>

template <typename Clock, typename Duration>
std::ostream &
operator<<(std::ostream &stream,
           const std::chrono::time_point<Clock, Duration> &time_point) {
  const auto sys_time_point =
      std::chrono::time_point_cast<typename Clock::duration, Clock>(time_point);
  const std::time_t time = Clock::to_time_t(sys_time_point);

#if __GNUC__ > 4 ||                                                            \
    ((__GNUC__ == 4) && __GNUC_MINOR__ > 8 && __GNUC_REVISION__ > 1)
  // Maybe the put_time will be implemented later?
  struct tm tm;
  localtime_r(&time, &tm);
  return stream << std::put_time(&tm, "%c"); // Print standard date&time
#elif defined(_MSC_VER)
  char buffer[26];
  ctime_s(buffer, 26, &time);
  buffer[24] = '\0'; // Removes the newline that is added
  return stream << buffer;
#else
  char buffer[26];
  ctime_r(&time, buffer);
  buffer[24] = '\0'; // Removes the newline that is added
  return stream << buffer;
#endif
}

template <typename Rep, typename Period>
std::ostream &operator<<(std::ostream &stream,
                         const std::chrono::duration<Rep, Period> &dur) {

  using namespace std::chrono;

  auto d = dur;

  if (dur == dur.zero())
    stream << "0";
  else {
    auto h = duration_cast<hours>(d);
    d -= h;
    auto m = duration_cast<minutes>(d);
    d -= m;
    auto s = duration_cast<seconds>(d);
    d -= s;
    auto ms = duration_cast<milliseconds>(d);
    d -= ms;
    auto us = duration_cast<microseconds>(d);
    d -= us;
    auto ns = duration_cast<nanoseconds>(d);

    long long vals[] = {h.count(),  m.count(),  s.count(),
                        ms.count(), us.count(), ns.count()};
    const char *units[] = {"hrs", "mins", "secs", "msecs", "usecs", "nsecs"};

    for (int i = 0; i < 6; i++) {
      if (i)
        stream << " ";

      if (vals[i] == 0)
        continue;

      stream << vals[i] << " " << units[i];
    }
  }

  return stream;
}
