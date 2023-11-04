/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NWEB_HILOG_H
#define NWEB_HILOG_H

#if defined(OHOS_NWEB)
#include "hilog/log.h"
constexpr OHOS::HiviewDFX::HiLogLabel kLogLabel = {LOG_CORE, 0xD004500, "NWEB"};

#define FUNC_LINE_FMT " %{public}s<%{public}d>: "

#define WVLOG_D(fmt, ...)                                                   \
  OHOS::HiviewDFX::HiLog::Debug(kLogLabel, FUNC_LINE_FMT fmt, __FUNCTION__, \
                                __LINE__, ##__VA_ARGS__)
#define WVLOG_I(fmt, ...)                                                  \
  OHOS::HiviewDFX::HiLog::Info(kLogLabel, FUNC_LINE_FMT fmt, __FUNCTION__, \
                               __LINE__, ##__VA_ARGS__)
#define WVLOG_W(fmt, ...)                                                  \
  OHOS::HiviewDFX::HiLog::Warn(kLogLabel, FUNC_LINE_FMT fmt, __FUNCTION__, \
                               __LINE__, ##__VA_ARGS__)
#define WVLOG_E(fmt, ...)                                                   \
  OHOS::HiviewDFX::HiLog::Error(kLogLabel, FUNC_LINE_FMT fmt, __FUNCTION__, \
                                __LINE__, ##__VA_ARGS__)
#define WVLOG_F(fmt, ...)                                                   \
  OHOS::HiviewDFX::HiLog::Fatal(kLogLabel, FUNC_LINE_FMT fmt, __FUNCTION__, \
                                __LINE__, ##__VA_ARGS__)

#else  // not OHOS_NWEB

#define WVLOG(fmt, ...)

#define WVLOG_D(fmt, ...) WVLOG(fmt, ##__VA_ARGS__)
#define WVLOG_I(fmt, ...) WVLOG(fmt, ##__VA_ARGS__)
#define WVLOG_W(fmt, ...) WVLOG(fmt, ##__VA_ARGS__)
#define WVLOG_E(fmt, ...) WVLOG(fmt, ##__VA_ARGS__)
#define WVLOG_F(fmt, ...) WVLOG(fmt, ##__VA_ARGS__)

#endif  // OHOS_NWEB

#endif  // NWEB_HILOG_H
