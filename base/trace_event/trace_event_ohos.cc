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

#include "base/trace_event/trace_event_ohos.h"
 
#include "base/logging.h"
#include "hitrace_meter.h"
 
bool IsBytraceEnable() {
#if defined(ENABLE_OHOS_BYTRACE)
  return true;
#else
  return false;
#endif
}
 
BytraceArg GetArg(double i) {
  BytraceArg arg;
  arg.value.as_double = i;
  arg.type = TYPE_NUMBER;
  return arg;
}
 
BytraceArg GetArg(const char* i) {
  BytraceArg arg;
  arg.value.as_string = i;
  arg.type = TYPE_STRING;
  return arg;
}
 
std::string GetStringFromArg(const BytraceArg& arg) {
  switch (arg.type) {
    case TYPE_NUMBER: {
      return std::to_string(arg.value.as_double);
    }
    case TYPE_STRING: {
      return arg.value.as_string;
    }
    default: {
      return "UnsupportArgType";
    }
  }
}
 
std::string GetStringWithArgs(const std::string& name) {
  return name;
}
 
void StartBytrace(const std::string& value) {
#if defined(ENABLE_OHOS_BYTRACE)
  StartTrace(HITRACE_TAG_NWEB, value);
#endif
}
 
void FinishBytrace() {
#if defined(ENABLE_OHOS_BYTRACE)
  FinishTrace(HITRACE_TAG_NWEB);
#endif
}
 
void StartAsyncBytrace(const std::string& value, int32_t taskId) {
#if defined(ENABLE_OHOS_BYTRACE)
  StartAsyncTrace(HITRACE_TAG_NWEB, value, taskId);
#endif
}
 
void FinishAsyncBytrace(const std::string& value, int32_t taskId) {
#if defined(ENABLE_OHOS_BYTRACE)
  FinishAsyncTrace(HITRACE_TAG_NWEB, value, taskId);
#endif
}

void CountBytrace(const std::string& name, int64_t count) {
#if defined(ENABLE_OHOS_BYTRACE)
  CountTrace(HITRACE_TAG_NWEB, name, count);
#endif  
}
 
ScopedBytrace::ScopedBytrace(const std::string& proc) : proc_(proc) {
#if defined(ENABLE_OHOS_BYTRACE)
  StartTrace(HITRACE_TAG_NWEB, proc_);
#endif
}
 
ScopedBytrace::~ScopedBytrace() {
#if defined(ENABLE_OHOS_BYTRACE)
  FinishTrace(HITRACE_TAG_NWEB);
#endif
}
