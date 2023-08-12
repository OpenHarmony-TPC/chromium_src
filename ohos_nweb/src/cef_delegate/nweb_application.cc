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

#include "nweb_application.h"

#include <cstdlib>
#include <thread>

#include "cef/include/wrapper/cef_closure_task.h"
#include "cef/include/wrapper/cef_helpers.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "nweb_handler_delegate.h"
#include "nweb_impl.h"

namespace OHOS::NWeb {
NWebApplication::NWebApplication(
    std::shared_ptr<NWebPreferenceDelegate> preference_delegate,
    std::string url,
    CefRefPtr<NWebHandlerDelegate> handler_delegate,
    void* window)
    : preference_delegate_(preference_delegate),
      url_(url),
      handler_delegate_(handler_delegate),
      window_(window){}

NWebApplication::~NWebApplication() {}

std::string NWebApplication::GetURL() {
  return url_;
}

/* CefApp methods begin */
CefRefPtr<CefBrowserProcessHandler>
NWebApplication::GetBrowserProcessHandler() {
  return this;
}

CefRefPtr<CefRenderProcessHandler> NWebApplication::GetRenderProcessHandler() {
  return this;
}

std::vector<std::string> NWebApplication::CustomSchemeCmdLineSplit(std::string str, const char split)
{
  std::istringstream inStream(str);
  std::vector<std::string> ret;
  std::string token;
  while (getline(inStream, token, split)) {
    if (!token.empty()) {
      ret.push_back(token);
      token.clear();
    }
  }
  return ret;
}

void NWebApplication::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
{
  LOG(INFO) << "OnRegisterCustomSchemes";
  CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();
  if (command_line->HasSwitch(::switches::kOhosCustomScheme)) {
    std::string cmdline_scheme = command_line->GetSwitchValue(::switches::kOhosCustomScheme).ToString();
    LOG(INFO) << "cmdline scheme:" << cmdline_scheme;
    std::vector<std::string> schemesInfo = CustomSchemeCmdLineSplit(cmdline_scheme, ';');
    for (auto it = schemesInfo.begin(); it != schemesInfo.end(); ++it) {
        int options = 0;
        std::vector<std::string> scheme = CustomSchemeCmdLineSplit(*it, ',');
        if (scheme.size() != 3) {
          break;
        }
        if (scheme[1] == std::string("1")) {
          options = (options | CEF_SCHEME_OPTION_CORS_ENABLED);
        }

        if (scheme[2] == std::string("1")) {
          options = (options | CEF_SCHEME_OPTION_FETCH_ENABLED);
        }
        LOG(INFO) << "scheme name:" << *it << " scheme options:" << options;
        registrar->AddCustomScheme(scheme[0], options);
    }
  }
}
/* CefApp methods end */

/* CefBrowserProcessHandler methods begin */
void NWebApplication::OnContextInitialized() {
  LOG(INFO) << "NWebApplication::OnContextInitialized";
  CEF_REQUIRE_UI_THREAD();
  CreateBrowser();
  auto runWebInitedCallback = OhosAdapterHelper::GetInstance().GetInitWebAdapter()->GetRunWebInitedCallback();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&NWebApplication::RunWebInitedCallback, this,
                                runWebInitedCallback));

  OnContextInitializedInternal();
}

void NWebApplication::RunWebInitedCallback(WebRunInitedCallback* callback)
{
  if (callback != nullptr) {
    callback->RunInitedCallback();
    delete callback;
    callback = nullptr;
  } else {
    LOG(ERROR) << "There is no web inited callback to run.";
  }
}

void NWebApplication::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line)
{
  LOG(INFO) << "NWebApplication::OnBeforeChildProcessLaunch";
  if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(::switches::kOhosCustomScheme)) {
    command_line->AppendSwitchWithValue(::switches::kOhosCustomScheme, CefCommandLine::GetGlobalCommandLine()->GetSwitchValue(::switches::kOhosCustomScheme).ToString());
  }

  if (CefCommandLine::GetGlobalCommandLine()->HasSwitch(::switches::kOhosHapPath)) {
    LOG(INFO) << "hap package is not decompresssed";
    command_line->AppendSwitchWithValue(::switches::kOhosHapPath,
      CefCommandLine::GetGlobalCommandLine()->GetSwitchValue(::switches::kOhosHapPath).ToString());
  }

#ifdef OHOS_NWEB_EX
  for (const auto& arg : NWebImpl::GetCommandLineArgsForNWebEx()) {
    command_line->AppendSwitch(arg);
  }
#endif  // OHOS_NWEB_EX
}
/* CefBrowserProcessHandler methods end */

/* CefRenderProcessHandler methods begin */
void NWebApplication::OnWebKitInitialized() {
  LOG(INFO) << "OnWebKitInitialized";
}
/* CefRenderProcessHandler methods begin */

CefRefPtr<CefClient> NWebApplication::GetDefaultClient() {
  // Called when a new browser window is created via the Chrome runtime UI
  return handler_delegate_;
}

void NWebApplication::PopulateCreateSettings(
    CefRefPtr<CefCommandLine> command_line,
    CefBrowserSettings& browser_settings) {
  if (command_line->HasSwitch(switches::kOffScreenFrameRate)) {
    browser_settings.windowless_frame_rate =
        atoi(command_line->GetSwitchValue(switches::kOffScreenFrameRate)
                 .ToString()
                 .c_str());
  }
}

void NWebApplication::CreateBrowser() {
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  // Specify CEF browser settings here.
  CefBrowserSettings browser_settings;
  PopulateCreateSettings(command_line, browser_settings);
  browser_settings.background_color = 0xffffffff;
  preference_delegate_->ComputeBrowserSettings(browser_settings);

  if (command_line->HasSwitch(switches::kForTest)) {
    preference_delegate_->PutHasInternetPermission(true);
    preference_delegate_->PutBlockNetwork(false);
  }

  std::string url;
  url = command_line->GetSwitchValue(switches::kUrl);
  if (url.empty()) {
    url = GetURL();
  }

  // Information used when creating the native window.
  CefWindowInfo window_info;
  CefWindowHandle handle = kNullWindowHandle;
  window_info.SetAsWindowless(handle);

  // Create the first browser window.
  CefBrowserHost::CreateBrowser(window_info, handler_delegate_, url,
                                browser_settings, nullptr, nullptr);
  if (handler_delegate_ == nullptr) {
    return;
  }
  auto browser = handler_delegate_->GetBrowser();
  if (browser && browser->GetHost()) {
    browser->GetHost()->SetNativeWindow(window_);
  }
}
}  // namespace OHOS::NWeb
