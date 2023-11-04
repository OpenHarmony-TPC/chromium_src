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

#include "nweb_impl.h"

#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "base/lazy_instance.h"
#include "base/trace_event/common/trace_event_common.h"
#include "ohos_adapter_helper.h"
#include "nweb_delegate_adapter.h"
#include "nweb_export.h"
#include "nweb_handler.h"
#include "nweb_hilog.h"

#if defined(REPORT_SYS_EVENT)
#include "event_reporter.h"
#endif

namespace {
uint32_t g_nweb_count = 0;
const uint32_t kSurfaceMaxWidth = 7680;
const uint32_t kSurfaceMaxHeight = 7680;

#ifdef OHOS_NWEB_EX
bool g_browser_service_api_enabled = false;
#endif  // OHOS_NWEB_EX

#if defined(REPORT_SYS_EVENT)
  // For maximum count of nweb instance
  uint32_t g_nweb_max_count = 0;
#endif
}

namespace OHOS::NWeb {

typedef std::unordered_map<int32_t, std::weak_ptr<NWebImpl>> NWebMap;
base::LazyInstance<NWebMap>::DestructorAtExit g_nweb_map =
    LAZY_INSTANCE_INITIALIZER;

#ifdef OHOS_NWEB_EX
base::LazyInstance<std::vector<std::string>>::DestructorAtExit g_browser_args =
    LAZY_INSTANCE_INITIALIZER;
#endif  // OHOS_NWEB_EX

void NWebImpl::AddNWebToMap(uint32_t id, std::shared_ptr<NWebImpl>& nweb) {
  if (nweb) {
    std::weak_ptr<NWebImpl> nweb_weak(nweb);
    g_nweb_map.Get().emplace(id, nweb_weak);
  }
}

NWebImpl* NWebImpl::FromID(int32_t nweb_id) {
  NWebMap* map = g_nweb_map.Pointer();
  if (auto it = map->find(nweb_id); it != map->end()) {
    auto nweb = it->second.lock();
    if (nweb) {
      return nweb.get();
    }
  }
  return nullptr;
}

NWebImpl::NWebImpl(uint32_t id) : nweb_id_(id) {}

NWebImpl::~NWebImpl() {
  g_nweb_map.Get().erase(nweb_id_);
}

bool NWebImpl::Init(const NWebCreateInfo& create_info) {
  output_handler_ = NWebOutputHandler::Create(
      create_info.width, create_info.height, create_info.output_render_frame);
  if (output_handler_ == nullptr) {
    return false;
  }
  output_handler_->SetNWebId(nweb_id_);

  ProcessInitArgs(create_info.init_args);

  if (!InitWebEngine(create_info)) {
    WVLOG_E("web engine init fail");
    return false;
  }

  input_handler_ = NWebInputHandler::Create(nweb_delegate_);
  if (input_handler_ == nullptr) {
    return false;
  }

  return true;
}

void NWebImpl::OnDestroy() {
  WVLOG_I("NWebImpl::OnDestroy");
  if (g_nweb_count == 0) {
    return;
  }
  TRACE_EVENT1("NWebImpl", "NWebImpl | DestoryNWeb", "nweb_id", nweb_id_);
  bool is_close_all = (--g_nweb_count) == 0 ? true : false;
  if (nweb_delegate_ != nullptr) {
    nweb_delegate_->OnDestroy(is_close_all);
    nweb_delegate_ = nullptr;
  }

  if (input_handler_ != nullptr) {
    input_handler_->OnDestroy();
    input_handler_ = nullptr;
  }

#if defined(REPORT_SYS_EVENT)
  // Report nweb instance count
  ReportMultiInstanceStats(nweb_id_, g_nweb_count, g_nweb_max_count);
#endif
}

void NWebImpl::ProcessInitArgs(const NWebInitArgs& init_args) {
  if (!init_args.dump_path.empty() && output_handler_ != nullptr) {
    output_handler_->SetDumpPath(init_args.dump_path);
  }
  if (init_args.frame_info_dump && output_handler_ != nullptr) {
    output_handler_->SetFrameInfoDump(init_args.frame_info_dump);
  }

  InitWebEngineArgs(init_args);
}

bool NWebImpl::SetVirtualDeviceRatio() {
  if (fabs(device_pixel_ratio_) < 1e-15) {
    auto display_manager_adapter =
        OHOS::NWeb::OhosAdapterHelper::GetInstance().CreateDisplayMgrAdapter();
    if (display_manager_adapter == nullptr) {
      WVLOG_E("display_manager_adapter is nullptr.");
      return false;
    }
    std::shared_ptr<OHOS::NWeb::DisplayAdapter> display =
        display_manager_adapter->GetDefaultDisplay();
    if (display == nullptr) {
      WVLOG_E("display is nullptr.");
      return false;
    }
    device_pixel_ratio_ = display->GetVirtualPixelRatio();
    if (device_pixel_ratio_ <= 0) {
      WVLOG_E("invalid ratio.");
      return false;
    }
    WVLOG_I("GetVirtualPixelRatio ratio: %{public}f", device_pixel_ratio_);
  }
  return true;
}

uint32_t NWebImpl::NormalizeVirtualDeviceRatio(uint32_t length) {
  float ratio = static_cast<int>(length / device_pixel_ratio_)
    * device_pixel_ratio_;
  return std::ceil(ratio); 
}

bool NWebImpl::InitWebEngine(const NWebCreateInfo& create_info) {
  if (output_handler_ == nullptr) {
    WVLOG_E("fail to init web engine, NWeb output handler is not ready");
    return false;
  }
  if (!SetVirtualDeviceRatio()) {
    WVLOG_E("fail to set virtual device ratio");
    return false;
  }
  if (web_engine_args_.empty()) {
    WVLOG_E("fail to init web engine args");
    return false;
  }

  int argc = web_engine_args_.size();
  const char** argv = new const char*[argc];
  int i = 0;
  for (auto it = web_engine_args_.begin(); i < argc; ++i, ++it) {
    argv[i] = it->c_str();
  }

  is_enhance_surface_ = create_info.init_args.is_enhance_surface;
  void* window = nullptr;
  if (is_enhance_surface_) {
    window = create_info.enhance_surface_info;
  } else {
    window =
      reinterpret_cast<void*>(output_handler_->GetNativeWindowFromSurface(
          create_info.producer_surface));
  }

  if (window == nullptr) {
    WVLOG_E("fail to init web engine, get native window from surface failed");
    delete[] argv;
    return false;
  }
  WVLOG_D("nweb create_info.init_args.is_popup: %{public}d", create_info.init_args.is_popup);
  nweb_delegate_ = NWebDelegateAdapter::CreateNWebDelegate(argc, argv, is_enhance_surface_, window, create_info.init_args.is_popup);
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("fail to create nweb delegate of web engine");
    delete[] argv;
    return false;
  }

  std::weak_ptr<NWebOutputHandler> output_handler_weak(output_handler_);
  auto render_update_cb = [output_handler_weak](const char* buffer) -> void {
    if (!output_handler_weak.expired()) {
      output_handler_weak.lock()->OnRenderUpdate(buffer);
    }
  };

  uint32_t width, height;
  output_handler_->GetWindowInfo(width, height);
  width = NormalizeVirtualDeviceRatio(width);
  height = NormalizeVirtualDeviceRatio(height);
  nweb_delegate_->Resize(width, height);
  nweb_delegate_->RegisterRenderCb(render_update_cb);

  inputmethod_handler_ = new NWebInputMethodHandler();
  nweb_delegate_->SetInputMethodClient(inputmethod_handler_);

#if defined(REPORT_SYS_EVENT)
  nweb_delegate_->SetNWebId(nweb_id_);
#endif

  delete[] argv;
  return nweb_delegate_->IsReady();
}

void NWebImpl::InitWebEngineArgs(const NWebInitArgs& init_args) {
  web_engine_args_.clear();

  web_engine_args_.emplace_back("/system/bin/web_render");
  web_engine_args_.emplace_back("--in-process-gpu");
  web_engine_args_.emplace_back("--disable-dev-shm-usage");
#ifdef GPU_RK3568
  web_engine_args_.emplace_back("--disable-gpu");
#endif
#ifdef RK3568
  web_engine_args_.emplace_back("--off-screen-frame-rate=70");
#else
  web_engine_args_.emplace_back("--off-screen-frame-rate=60");
#endif
  web_engine_args_.emplace_back("--no-unsandboxed-zygote");
  web_engine_args_.emplace_back("--no-zygote");
  web_engine_args_.emplace_back("--enable-features=UseOzonePlatform");
  web_engine_args_.emplace_back("-ozone-platform=headless");
  web_engine_args_.emplace_back("--no-sandbox");
  web_engine_args_.emplace_back("--use-mobile-user-agent");
  web_engine_args_.emplace_back("--enable-gpu-rasterization");
  web_engine_args_.emplace_back("--enable-viewport");
  web_engine_args_.emplace_back(
      "--browser-subprocess-path=/system/bin/web_render");
  web_engine_args_.emplace_back("--zygote-cmd-prefix=/system/bin/web_render");
  web_engine_args_.emplace_back("--remote-debugging-port=9222");
  web_engine_args_.emplace_back("--enable-touch-drag-drop");
  web_engine_args_.emplace_back("--gpu-rasterization-msaa-sample-count=1");
  // enable aggressive domstorage flushing to minimize data loss
  // http://crbug.com/479767
  web_engine_args_.emplace_back("--enable-aggressive-domstorage-flushing");
  if (init_args.is_enhance_surface) {
    WVLOG_I("is_enhance_surface is true");
    web_engine_args_.emplace_back("--ohos-enhance-surface");
  }
  for (auto arg : init_args.web_engine_args_to_delete) {
    auto it = std::find(web_engine_args_.begin(), web_engine_args_.end(), arg);
    if (it != web_engine_args_.end()) {
      web_engine_args_.erase(it);
    }
  }
  for (auto arg : init_args.web_engine_args_to_add) {
    web_engine_args_.emplace_back(arg);
  }
  if (init_args.multi_renderer_process) {
    web_engine_args_.emplace_back("--enable-multi-renderer-process");
  }

#ifdef OHOS_NWEB_EX
  auto args = g_browser_args.Get();
  for (const std::string& arg : args) {
    web_engine_args_.emplace_back(arg);
  }
#endif  // OHOS_NWEB_EX
}

void NWebImpl::PutDownloadCallback(
    std::shared_ptr<NWebDownloadCallback> downloadListener) {
  nweb_delegate_->RegisterDownLoadListener(downloadListener);
}

void NWebImpl::SetNWebHandler(std::shared_ptr<NWebHandler> client) {
  nweb_handle_ = client;
  nweb_delegate_->RegisterNWebHandler(client);
  client->SetNWeb(shared_from_this());
}

const std::shared_ptr<NWebHandler> NWebImpl::GetNWebHandler() const {
  return nweb_handle_;
}

void NWebImpl::Resize(uint32_t width, uint32_t height) {
  if (input_handler_ == nullptr || output_handler_ == nullptr) {
    return;
  }
  width = NormalizeVirtualDeviceRatio(width);
  height = NormalizeVirtualDeviceRatio(height);
  if (width > kSurfaceMaxWidth || height > kSurfaceMaxHeight) {
    return;
  }
  nweb_delegate_->Resize(width, height);
  output_handler_->Resize(width, height);
}

void NWebImpl::OnTouchPress(int32_t id, double x, double y) {
  if (input_handler_ == nullptr) {
    return;
  }

  input_handler_->OnTouchPress(id, x, y);
}

void NWebImpl::OnTouchRelease(int32_t id, double x, double y) {
  if (input_handler_ == nullptr) {
    return;
  }

  input_handler_->OnTouchRelease(id, x, y);
}

void NWebImpl::OnTouchMove(int32_t id, double x, double y) {
  if (input_handler_ == nullptr) {
    return;
  }

  input_handler_->OnTouchMove(id, x, y);
}

void NWebImpl::OnTouchCancel() {
  if (input_handler_ == nullptr) {
    return;
  }
  input_handler_->OnTouchCancel();
}

void NWebImpl::OnNavigateBack() {
  if (input_handler_ == nullptr) {
    return;
  }
  input_handler_->OnNavigateBack();
}

bool NWebImpl::SendKeyEvent(int32_t keyCode, int32_t keyAction) {
  if (input_handler_ == nullptr) {
    return false;
  }
  return input_handler_->SendKeyEvent(keyCode, keyAction);
}

void NWebImpl::SendMouseWheelEvent(double x, double y, double deltaX, double deltaY) {
  if (input_handler_ == nullptr) {
    return;
  }
  input_handler_->SendMouseWheelEvent(x, y, deltaX, deltaY);
}

void NWebImpl::SendMouseEvent(int x, int y, int button, int action, int count) {
  if (input_handler_ == nullptr) {
    return;
  }
  input_handler_->SendMouseEvent(x, y, button, action, count);
}

int NWebImpl::Load(const std::string& url) const {
  if (nweb_delegate_ == nullptr || output_handler_ == nullptr) {
    return NWEB_ERR;
  }

  if (!output_handler_->IsSizeValid()) {
    WVLOG_E("nweb size is invalid, stop Load");
    return NWEB_ERR;
  }

  int result = nweb_delegate_->Load(url);
  output_handler_->StartRenderOutput();
  return result;
}

bool NWebImpl::IsNavigatebackwardAllowed() const {
  if (nweb_delegate_ == nullptr) {
    return false;
  }
  return nweb_delegate_->IsNavigatebackwardAllowed();
}

bool NWebImpl::IsNavigateForwardAllowed() const {
  if (nweb_delegate_ == nullptr) {
    return false;
  }
  return nweb_delegate_->IsNavigateForwardAllowed();
}

bool NWebImpl::CanNavigateBackOrForward(int numSteps) const {
  if (nweb_delegate_ == nullptr) {
    return false;
  }
  return nweb_delegate_->CanNavigateBackOrForward(numSteps);
}

void NWebImpl::NavigateBack() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->NavigateBack();
}

void NWebImpl::NavigateForward() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->NavigateForward();
}

void NWebImpl::NavigateBackOrForward(int step) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->NavigateBackOrForward(step);
}

void NWebImpl::DeleteNavigateHistory() {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->DeleteNavigateHistory();
}

void NWebImpl::ClearSslCache() {
  WVLOG_I("NWebImpl::ClearSslCache");
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->ClearSslCache();
}

void NWebImpl::ClearClientAuthenticationCache() {
  WVLOG_I("NWebImpl::ClearClientAuthenticationCache");
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->ClearClientAuthenticationCache();
}

void NWebImpl::Reload() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->Reload();
}

int NWebImpl::Zoom(float zoomFactor) const {
  if (nweb_delegate_ == nullptr) {
    return NWEB_ERR;
  }
  return nweb_delegate_->Zoom(zoomFactor);
}

int NWebImpl::ZoomIn() const {
  if (nweb_delegate_ == nullptr) {
    return NWEB_ERR;
  }
  return nweb_delegate_->ZoomIn();
}

int NWebImpl::ZoomOut() const {
  if (nweb_delegate_ == nullptr) {
    return NWEB_ERR;
  }
  return nweb_delegate_->ZoomOut();
}

void NWebImpl::Stop() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->Stop();
}

void NWebImpl::ExecuteJavaScript(const std::string& code) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->ExecuteJavaScript(code);
}

void NWebImpl::PutBackgroundColor(int color) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->PutBackgroundColor(color);
}

void NWebImpl::InitialScale(float scale) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->InitialScale(scale);
}

void NWebImpl::OnPause() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->OnPause();
  if (inputmethod_handler_ == nullptr) {
    return;
  }
  inputmethod_handler_->HideTextInput(NWebInputMethodClient::HideTextinputType::FROM_ONPAUSE);
}

void NWebImpl::OnContinue() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->OnContinue();
  inputmethod_handler_->Reattach(NWebInputMethodHandler::ReattachType::FROM_CONTINUE);
}

const std::shared_ptr<NWebPreference> NWebImpl::GetPreference() const {
  if (nweb_delegate_ == nullptr) {
    return nullptr;
  }
  return nweb_delegate_->GetPreference();
}

std::string NWebImpl::Title() {
  if (nweb_delegate_ == nullptr) {
    return "";
  }
  return nweb_delegate_->Title();
}

void NWebImpl::CreateWebMessagePorts(std::vector<std::string>& ports) {

  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI nweb_delegate_ its null");
    return;
  }
  nweb_delegate_->CreateWebMessagePorts(ports);
}

void NWebImpl::PostWebMessage(std::string& message, std::vector<std::string>& ports, std::string& targetUri) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI nweb_delegate_ its null");
    return;
  }
  nweb_delegate_->PostWebMessage(message, ports, targetUri);
}

void NWebImpl::ClosePort(std::string& portHandle) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI nweb_delegate_ its null");
    return;
  }
  nweb_delegate_->ClosePort(portHandle);
}

void NWebImpl::PostPortMessage(std::string& portHandle, std::shared_ptr<NWebMessage> data) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI nweb_delegate_ its null");
    return;
  }
  nweb_delegate_->PostPortMessage(portHandle, data);
}

void NWebImpl::SetPortMessageCallback(std::string& portHandle,
        std::shared_ptr<NWebValueCallback<std::shared_ptr<NWebMessage>>> callback) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI nweb_delegate_ its null");
    return;
  }
  nweb_delegate_->SetPortMessageCallback(portHandle, callback);
}

uint32_t NWebImpl::GetWebId() const {
  return nweb_id_;
}

HitTestResult NWebImpl::GetHitTestResult() const {
  return nweb_delegate_->GetHitTestResult();
}

int NWebImpl::PageLoadProgress() {
  if (nweb_delegate_ == nullptr) {
    return 0;
  }
  return nweb_delegate_->PageLoadProgress();
}

int NWebImpl::ContentHeight() {
  if (nweb_delegate_ == nullptr) {
    return 0;
  }
  return nweb_delegate_->ContentHeight();
}

float NWebImpl::Scale() {
  if (nweb_delegate_ == nullptr) {
    return 0;
  }
  return nweb_delegate_->Scale();
}

int NWebImpl::Load(std::string& url,
                    std::map<std::string, std::string> additionalHttpHeaders) {
  if (nweb_delegate_ == nullptr) {
    return NWEB_ERR;
  }
  return nweb_delegate_->Load(url, additionalHttpHeaders);
}

int NWebImpl::LoadWithDataAndBaseUrl(const std::string& baseUrl,
                                      const std::string& data,
                                      const std::string& mimeType,
                                      const std::string& encoding,
                                      const std::string& historyUrl) {
  if (nweb_delegate_ == nullptr) {
    return NWEB_ERR;
  }
  return nweb_delegate_->LoadWithDataAndBaseUrl(baseUrl, data, mimeType,
                                                encoding, historyUrl);
}

int NWebImpl::LoadWithData(const std::string& data,
                            const std::string& mimeType,
                            const std::string& encoding) {
  if (nweb_delegate_ == nullptr) {
    return NWEB_ERR;
  }
  return nweb_delegate_->LoadWithData(data, mimeType, encoding);
}

void NWebImpl::RegisterArkJSfunction(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->RegisterArkJSfunction(object_name, method_list);
}

void NWebImpl::UnregisterArkJSfunction(
    const std::string& object_name,
    const std::vector<std::string>& method_list) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->UnregisterArkJSfunction(object_name, method_list);
}

void NWebImpl::SetNWebJavaScriptResultCallBack(
    std::shared_ptr<NWebJavaScriptResultCallBack> callback) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->RegisterNWebJavaScriptCallBack(callback);
}

void NWebImpl::ExecuteJavaScript(
    const std::string& code,
    std::shared_ptr<NWebValueCallback<std::string>> callback) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->ExecuteJavaScript(code, callback);
}

void NWebImpl::OnFocus() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->OnFocus();

  if (inputmethod_handler_ == nullptr) {
    return;
  }
  inputmethod_handler_->Reattach(NWebInputMethodHandler::ReattachType::FROM_ONFOCUS);
}

void NWebImpl::OnBlur(const BlurReason& blurReason) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->OnBlur();
  if (inputmethod_handler_ == nullptr) {
    return;
  }
  if (is_enhance_surface_ && blurReason == OHOS::NWeb::BlurReason::WINDOW_BLUR) {
    return;
  }
  inputmethod_handler_->HideTextInput(NWebInputMethodClient::HideTextinputType::FROM_ONBLUR);
}

#ifdef OHOS_NWEB_EX
// static
const std::vector<std::string>& NWebImpl::GetCommandLineArgsForNWebEx() {
  return g_browser_args.Get();
}

void NWebImpl::InitBrowserServiceApi(std::vector<std::string>& browser_args) {
  auto args = g_browser_args.Pointer();
  args->clear();
  for (const std::string& arg : browser_args) {
    args->push_back(arg);
  }
  g_browser_service_api_enabled = true;
}

bool NWebImpl::GetBrowserServiceApiEnabled() {
  return g_browser_service_api_enabled;
}

void NWebImpl::ReloadOriginalUrl() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }

  nweb_delegate_->ReloadOriginalUrl();
}

void NWebImpl::SetBrowserUserAgentString(const std::string& user_agent) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->SetBrowserUserAgentString(user_agent);
}

void NWebImpl::PutWebAppClientExtensionCallback(
    std::shared_ptr<NWebAppClientExtensionCallback>
        web_app_client_extension_listener) {
  nweb_delegate_->RegisterWebAppClientExtensionListener(
      web_app_client_extension_listener);
}

void NWebImpl::RemoveWebAppClientExtensionCallback() {
  nweb_delegate_->UnRegisterWebAppClientExtensionListener();
}
#endif  // OHOS_NWEB_EX

void NWebImpl::PutFindCallback(std::shared_ptr<NWebFindCallback> findListener) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->RegisterFindListener(findListener);
}

void NWebImpl::FindAllAsync(const std::string& search_string) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->FindAllAsync(search_string);
}

void NWebImpl::ClearMatches() const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->ClearMatches();
}

void NWebImpl::FindNext(const bool forward) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->FindNext(forward);
}

void NWebImpl::StoreWebArchive(
    const std::string& base_name,
    bool auto_name,
    std::shared_ptr<NWebValueCallback<std::string>> callback) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }

  nweb_delegate_->StoreWebArchive(base_name, auto_name, callback);
}

void NWebImpl::SendDragEvent(const DragEvent& dragEvent) const {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("nweb_delegate_ is nullptr");
    return;
  }
  DelegateDragEvent event;
  event.action = static_cast<DelegateDragAction>(dragEvent.action);
  event.x = dragEvent.x;
  event.y = dragEvent.y;
  nweb_delegate_->SendDragEvent(event);
}

std::string NWebImpl::GetUrl() const {
  if (nweb_delegate_ == nullptr) {
    return "";
  }
  return nweb_delegate_->GetUrl();
}

void NWebImpl::UpdateLocale(const std::string& language, const std::string& region) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->UpdateLocale(language, region);
}

void NWebImpl::PutReleaseSurfaceCallback(
    std::shared_ptr<NWebReleaseSurfaceCallback> releaseSurfaceListener) {
  nweb_delegate_->RegisterReleaseSurfaceListener(releaseSurfaceListener);
}

const std::string NWebImpl::GetOriginalUrl() const {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("nweb_delegate_ is null");
    return std::string();
  }
  return nweb_delegate_->GetOriginalUrl();
}

bool NWebImpl::GetFavicon(const void** data, size_t& width, size_t& height,
  ImageColorType& colorType, ImageAlphaType& alphaType) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("nweb_delegate_ is null");
    return false;
  }
  return nweb_delegate_->GetFavicon(data, width, height, colorType, alphaType);
}

void NWebImpl::PutNetworkAvailable(bool available) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("nweb_delegate_ is null");
    return;
  }
  nweb_delegate_->PutNetworkAvailable(available);
}

void NWebImpl::HasImages(std::shared_ptr<NWebValueCallback<bool>> callback) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI HasImages nweb_delegate_ is null");
    return;
  }

  nweb_delegate_->GetImages(callback);
}

std::shared_ptr<NWebHistoryList> NWebImpl::GetHistoryList() {
  if (nweb_delegate_ == nullptr) {
    return nullptr;
  }

  return nweb_delegate_->GetHistoryList();
}

void NWebImpl::RemoveCache(bool include_disk_files) {
  if (nweb_delegate_ == nullptr) {
    WVLOG_E("JSAPI RemoveCache nweb_delegate_ is null");
    return;
  }

  nweb_delegate_->RemoveCache(include_disk_files);
}

WebState NWebImpl::SerializeWebState() {
  if (nweb_delegate_ == nullptr) {
    return nullptr;
  }
  return nweb_delegate_->SerializeWebState();
}

bool NWebImpl::RestoreWebState(WebState state) {
  if (nweb_delegate_ == nullptr) {
    return false;
  }
  return nweb_delegate_->RestoreWebState(state);
}

void NWebImpl::PageUp(bool top) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->PageUp(top);
}

void NWebImpl::PageDown(bool bottom) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->PageDown(bottom);
}

void NWebImpl::ScrollTo(float x, float y) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->ScrollTo(x, y);
}

void NWebImpl::ScrollBy(float delta_x, float delta_y) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->ScrollBy(delta_x, delta_y);
}

void NWebImpl::SlideScroll(float vx, float vy) {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  return nweb_delegate_->SlideScroll(vx, vy);
}
#if defined (OHOS_NWEB_EX)
void NWebImpl::SetForceEnableZoom(bool forceEnableZoom) const {
  if (nweb_delegate_ == nullptr) {
    return;
  }
  nweb_delegate_->SetForceEnableZoom(forceEnableZoom);
}

bool NWebImpl::GetForceEnableZoom() const {
  if (nweb_delegate_ == nullptr) {
    return false;
  }
  return nweb_delegate_->GetForceEnableZoom();
}
#endif //OHOS_NWEB_EX

}  // namespace OHOS::NWeb

using namespace OHOS::NWeb;
extern "C" OHOS_NWEB_EXPORT void CreateNWeb(const NWebCreateInfo& create_info,
                                            std::shared_ptr<NWebImpl>& nweb) {
  static uint32_t current_nweb_id = 0;
  uint32_t nweb_id = ++current_nweb_id;
  TRACE_EVENT1("NWebImpl", "NWebImpl | CreateNWeb", "nweb_id", nweb_id);
  WVLOG_I("creating nweb %{public}u, size %{public}u*%{public}u", nweb_id,
          create_info.width, create_info.height);
  nweb = std::make_shared<NWebImpl>(nweb_id);
  if (nweb == nullptr) {
    WVLOG_E("fail to create nweb instance");
    return;
  }

  if (!nweb->Init(create_info)) {
    WVLOG_E("fail to init nweb");
    return;
  }

  nweb->AddNWebToMap(nweb_id, nweb);
  ++g_nweb_count;
#if defined(REPORT_SYS_EVENT)
  // Report nweb instance count
  if (g_nweb_count > g_nweb_max_count) {
    g_nweb_max_count = g_nweb_count;
  }
  ReportMultiInstanceStats(nweb_id, g_nweb_count, g_nweb_max_count);
#endif
}

extern "C" OHOS_NWEB_EXPORT void GetNWeb(int32_t nweb_id,
                                         std::weak_ptr<NWebImpl>& nweb) {
  NWebMap* map = OHOS::NWeb::g_nweb_map.Pointer();
  if (auto it = map->find(nweb_id); it != map->end()) {
    nweb = it->second;
  }
}
