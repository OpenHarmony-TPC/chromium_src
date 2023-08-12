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

#ifndef OHOS_NWEB_SRC_NWEB_APP_CLIENT_EXTENSION_CALLBACK_H_
#define OHOS_NWEB_SRC_NWEB_APP_CLIENT_EXTENSION_CALLBACK_H_

#include <stddef.h>

#include "nweb_capi_export.h"

struct NWEB_CAPI_EXPORT NWebReceivedIconInfo {
  const char* image_url;
  size_t width;
  size_t height;
  int color_type{0};
  int alpha_type{0};
};

struct NWEB_CAPI_EXPORT NWebAppClientExtensionCallback {
  int NWebID{0};
  void (*OnReceivedFaviconUrl)(const NWebReceivedIconInfo&, int);
  void (*OnLoadStarted)(bool toDifferentDocument, int);
};

#endif  // OHOS_NWEB_SRC_NWEB_APP_CLIENT_EXTENSION_CALLBACK_H_
