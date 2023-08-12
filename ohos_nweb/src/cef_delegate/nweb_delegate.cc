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

#include "nweb_delegate.h"

#include <thread>
#include "nweb_application.h"
#include "nweb_handler_delegate.h"
#include "nweb_history_list_impl.h"
#include "nweb_render_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "cef/include/base/cef_logging.h"
#include "cef/include/cef_app.h"
#include "cef/include/cef_base.h"
#include "cef/include/cef_request_context.h"
#include "content/public/common/content_switches.h"
#include "nweb_find_delegate.h"
#include "nweb_preference_delegate.h"
#include "url/gurl.h"

#include "cef/libcef/browser/navigation_state_serializer.h"

namespace OHOS::NWeb {

static const float maxZoomFactor = 10.0;

class JavaScriptResultCallbackImpl : public CefJavaScriptResultCallback {
 public:
  JavaScriptResultCallbackImpl(
      std::shared_ptr<NWebValueCallback<std::string>> callback)
      : callback_(callback){};
  void OnJavaScriptExeResult(const CefString& result) override {
    if (callback_ != nullptr) {
      callback_->OnReceiveValue(result.ToString());
    }
  }

 private:
  std::shared_ptr<NWebValueCallback<std::string>> callback_;

  IMPLEMENT_REFCOUNTING(JavaScriptResultCallbackImpl);
};

class CefWebMessageReceiverImpl : public CefWebMessageReceiver {
 public:
  CefWebMessageReceiverImpl(
      std::shared_ptr<NWebValueCallback<std::shared_ptr<NWebMessage>>> callback)
      : callback_(callback){};
  void OnMessage(CefRefPtr<CefValue> message) override {
    if (callback_ != nullptr) {
        auto data = std::make_shared<OHOS::NWeb::NWebMessage>(NWebValue::Type::NONE);
        if (message->GetType() == VTYPE_STRING) {
          data->SetType(NWebValue::Type::STRING);
          data->SetString(message->GetString());
        } else if (message->GetType() == VTYPE_BINARY) {
          CefRefPtr<CefBinaryValue> binValue = message->GetBinary();
          size_t len = binValue->GetSize();
          std::vector<uint8_t> arr(len);
          binValue->GetData(&arr[0], len, 0);
          data->SetType(NWebValue::Type::BINARY);
          data->SetBinary(arr);
        } else {
          LOG(ERROR) << "OnMessage not support type";
          return;
        }
        callback_->OnReceiveValue(data);
    }
  }

 private:
  std::shared_ptr<NWebValueCallback<std::shared_ptr<NWebMessage>>> callback_;

  IMPLEMENT_REFCOUNTING(CefWebMessageReceiverImpl);
};

class StoreWebArchiveResultCallbackImpl
    : public CefStoreWebArchiveResultCallback {
 public:
  StoreWebArchiveResultCallbackImpl(
      std::shared_ptr<NWebValueCallback<std::string>> callback)
      : callback_(callback){};
  void OnStoreWebArchiveDone(const CefString& result) override {
    if (callback_ != nullptr) {
      callback_->OnReceiveValue(result.ToString());
    }
  }

 private:
  std::shared_ptr<NWebValueCallback<std::string>> callback_;

  IMPLEMENT_REFCOUNTING(StoreWebArchiveResultCallbackImpl);
};

class GetImagesCallbackImpl : public CefGetImagesCallback {
 public:
  explicit GetImagesCallbackImpl(
      std::shared_ptr<NWebValueCallback<bool>> callback)
      : callback_(callback){};

  void GetImages(bool response) override {
    if (callback_ != nullptr) {
      callback_->OnReceiveValue(response);
    }
  }

 private:
  std::shared_ptr<NWebValueCallback<bool>> callback_;

  IMPLEMENT_REFCOUNTING(GetImagesCallbackImpl);
};

class NavigationEntryVisitorImpl : public CefNavigationEntryVisitor {
 public:
  NavigationEntryVisitorImpl()
      : history_list_(std::make_shared<NWebHistoryListImpl>()){};

  bool Visit(CefRefPtr<CefNavigationEntry> entry,
             bool current,
             int index,
             int total) override {
    if (!history_list_) {
      return false;
    }
    if (current) {
      history_list_->SetCurrentIndex(index);
    }
    history_list_->AddHistoryItem(entry);
    return true;
  }

  std::shared_ptr<NWebHistoryListImpl> GetHistoryList() const {
    return history_list_;
  }

 private:
  std::shared_ptr<NWebHistoryListImpl> history_list_;
  IMPLEMENT_REFCOUNTING(NavigationEntryVisitorImpl);
};

NWebDelegate::NWebDelegate(int argc, const char* argv[])
    : argc_(argc), argv_(argv) {}

NWebDelegate::~NWebDelegate() {
  if (display_listener_ != nullptr && display_manager_adapter_ != nullptr) {
    display_manager_adapter_->UnregisterDisplayListener(display_listener_);
  }
}

bool NWebDelegate::Init(bool is_enhance_surface, void* window) {
  preference_delegate_ = std::make_shared<NWebPreferenceDelegate>();
  find_delegate_ = std::make_shared<NWebFindDelegate>();
  is_enhance_surface_ = is_enhance_surface;
  display_manager_adapter_ =
      OhosAdapterHelper::GetInstance().CreateDisplayMgrAdapter();
  if (display_manager_adapter_ == nullptr) {
    return false;
  }

  display_listener_ =
      std::make_shared<DisplayScreenListener>(shared_from_this());
  if (display_listener_ == nullptr) {
    return false;
  }

  if (!display_manager_adapter_->RegisterDisplayListener(display_listener_)) {
    LOG(ERROR) << "RegisterDisplayListener failed";
  }

  render_handler_ = NWebRenderHandler::Create();
  if (render_handler_ == nullptr) {
    display_manager_adapter_->UnregisterDisplayListener(display_listener_);
    return false;
  }

  event_handler_ = NWebEventHandler::Create();
  if (event_handler_ == nullptr) {
    display_manager_adapter_->UnregisterDisplayListener(display_listener_);
    return false;
  }

  std::string url_for_init = "";
  InitializeCef(url_for_init, is_enhance_surface_, window);

  std::shared_ptr<DisplayAdapter> display =
      display_manager_adapter_->GetDefaultDisplay();
  if (display != nullptr) {
    NotifyScreenInfoChanged(display->GetRotation(), display->GetOrientation());
    SetVirtualPixelRatio(display->GetVirtualPixelRatio());
  }

  return true;
}

void NWebDelegate::OnDestroy(bool is_close_all) {
  if (display_listener_ != nullptr && display_manager_adapter_ != nullptr) {
    display_manager_adapter_->UnregisterDisplayListener(display_listener_);
  }
  if (handler_delegate_ != nullptr) {
    handler_delegate_->OnDestroy();
  }
  if (preference_delegate_ != nullptr) {
    preference_delegate_->OnDestroy();
  }
  if (!GetBrowser().get()) {
    return;
  }
  GetBrowser()->GetHost()->DestroyAllWebMessagePorts();
}

void NWebDelegate::RegisterDownLoadListener(
    std::shared_ptr<NWebDownloadCallback> download_listener) {
  if (handler_delegate_ == nullptr) {
    LOG(ERROR) << "fail to register download listener, NWEB handler is nullptr";
    return;
  }
  handler_delegate_->RegisterDownLoadListener(download_listener);
}

void NWebDelegate::RegisterReleaseSurfaceListener(
    std::shared_ptr<NWebReleaseSurfaceCallback> releaseSurfaceListener) {
  if (handler_delegate_ == nullptr) {
    LOG(ERROR) << "fail to register release surface, NWEB handler is nullptr";
    return;
  }
  handler_delegate_->RegisterReleaseSurfaceListener(releaseSurfaceListener);
}

void NWebDelegate::RegisterFindListener(
    std::shared_ptr<NWebFindCallback> find_listener) {
  if (find_delegate_ == nullptr) {
    LOG(ERROR) << "fail to register FindListener, find_delegate_ is nullptr";
    return;
  }
  find_delegate_->SetListener(find_listener);
}

void NWebDelegate::FindAllAsync(const std::string& search_string) const {
  if (find_delegate_ == nullptr) {
    LOG(ERROR) << "fail to FindAllAsync, find_delegate_ is nullptr";
    return;
  }
  find_delegate_->FindAllAsync(GetBrowser().get(),
                               base::UTF8ToUTF16(search_string));
}

void NWebDelegate::ClearMatches() const {
  if (find_delegate_ == nullptr) {
    LOG(ERROR) << "fail to ClearMatches, find_delegate_ is nullptr";
    return;
  }

  find_delegate_->ClearMatches(GetBrowser().get());
}

void NWebDelegate::FindNext(const bool forward) const {
  if (find_delegate_ == nullptr) {
    LOG(ERROR) << "fail to FindNext, find_delegate_ is nullptr";
    return;
  }
  find_delegate_->FindNext(GetBrowser().get(), forward);
}

void NWebDelegate::RegisterWebAppClientExtensionListener(
    std::shared_ptr<NWebAppClientExtensionCallback>
        web_app_client_extension_listener) {
  if (handler_delegate_ == nullptr) {
    LOG(ERROR) << "fail to register web app client extension listener, nweb "
                  "handler delegate is nullptr";
    return;
  }
  handler_delegate_->RegisterWebAppClientExtensionListener(
      web_app_client_extension_listener);
}

void NWebDelegate::UnRegisterWebAppClientExtensionListener() {
  if (handler_delegate_ == nullptr) {
    LOG(ERROR) << "fail to unregister web app client extension listener, nweb "
                  "handler delegate is nullptr";
    return;
  }
  handler_delegate_->UnRegisterWebAppClientExtensionListener();
}

void NWebDelegate::RegisterNWebHandler(std::shared_ptr<NWebHandler> handler) {
  if (handler_delegate_ == nullptr) {
    LOG(ERROR)
        << "fail to register nweb handler, nweb handler delegate is nullptr";
    return;
  }
  handler_delegate_->RegisterNWebHandler(handler);
}

void NWebDelegate::SetInputMethodClient(
    CefRefPtr<NWebInputMethodClient> client) {
  if (render_handler_ == nullptr) {
    LOG(ERROR)
        << "fail to register inputmethod client, render handler is nullptr";
    return;
  }
  render_handler_->SetInputMethodClient(client);
}

void NWebDelegate::RegisterRenderCb(
    std::function<void(const char*)> render_update_cb) {
  if (render_handler_ != nullptr) {
    render_handler_->RegisterRenderCb(render_update_cb);
  }
}

void NWebDelegate::Resize(uint32_t width, uint32_t height) {
  if (render_handler_ != nullptr) {
    render_handler_->Resize(width, height);
  }
  auto browser = GetBrowser();
  if (browser != nullptr && browser->GetHost() != nullptr) {
    if (width != width_ || height != height_) {
      width_ = width;
      height_ = height;
      browser->GetHost()->WasResized();
    }
  }
}

void NWebDelegate::OnTouchPress(int32_t id, double x, double y) {
  if (event_handler_ != nullptr) {
    event_handler_->OnTouchPress(id, x / default_virtual_pixel_ratio_,
                                 y / default_virtual_pixel_ratio_);
  }
}

void NWebDelegate::OnTouchRelease(int32_t id, double x, double y) {
  if (event_handler_ != nullptr) {
    event_handler_->OnTouchRelease(id, x / default_virtual_pixel_ratio_,
                                   y / default_virtual_pixel_ratio_);
  }
}

void NWebDelegate::OnTouchMove(int32_t id, double x, double y) {
  if (event_handler_ != nullptr) {
    event_handler_->OnTouchMove(id, x / default_virtual_pixel_ratio_,
                                y / default_virtual_pixel_ratio_);
  }
}

void NWebDelegate::OnTouchCancel() {
  if (event_handler_ != nullptr) {
    event_handler_->OnTouchCancel();
  }
}

bool NWebDelegate::SendKeyEvent(int32_t keyCode, int32_t keyAction) {
  bool retVal = false;
  if (event_handler_ != nullptr) {
    retVal = event_handler_->SendKeyEvent(keyCode, keyAction);
  }
  return retVal;
}

void NWebDelegate::SendMouseWheelEvent(double x,
                                       double y,
                                       double deltaX,
                                       double deltaY) {
  if (event_handler_ != nullptr) {
    event_handler_->SendMouseWheelEvent(x / default_virtual_pixel_ratio_,
                                        y / default_virtual_pixel_ratio_,
                                        deltaX / default_virtual_pixel_ratio_,
                                        deltaY / default_virtual_pixel_ratio_);
  }
}

void NWebDelegate::SendMouseEvent(int x,
                                  int y,
                                  int button,
                                  int action,
                                  int count) {
  if (event_handler_ != nullptr) {
    event_handler_->SendMouseEvent(x / default_virtual_pixel_ratio_,
                                   y / default_virtual_pixel_ratio_, button,
                                   action, count);
  }
}

void NWebDelegate::NotifyScreenInfoChanged(RotationType rotation,
                                           OrientationType orientation) {
  if (render_handler_ != nullptr) {
    if (display_manager_adapter_ == nullptr) {
      LOG(ERROR) << "Get display_manager_adapter_ failed";
      return;
    }
    std::shared_ptr<DisplayAdapter> display =
        display_manager_adapter_->GetDefaultDisplay();
    if (display == nullptr) {
      LOG(ERROR) << "Get display failed";
      return;
    }
    double display_ratio = display->GetVirtualPixelRatio();
    if (display_ratio <= 0) {
      LOG(ERROR) << "Invalid display_ratio, display_ratio = " << display_ratio;
      return;
    }
    int width = display->GetWidth() / display_ratio;
    int height = display->GetHeight() / display_ratio;
    bool default_portrait = display_manager_adapter_->IsDefaultPortrait();
    render_handler_->SetScreenInfo({rotation, orientation, width, height,
                                    display_ratio, default_portrait});
    auto browser = GetBrowser();
    if (browser != nullptr && browser->GetHost() != nullptr) {
      browser->GetHost()->NotifyScreenInfoChanged();
    }
  }
}

void NWebDelegate::SetVirtualPixelRatio(float ratio) {
  if (ratio <= 0 || ratio == default_virtual_pixel_ratio_) {
    return;
  }
  default_virtual_pixel_ratio_ = ratio;
  auto browser = GetBrowser();
  if (browser != nullptr && browser->GetHost() != nullptr) {
    browser->GetHost()->SetVirtualPixelRatio(ratio);
  }
}

std::shared_ptr<NWebPreference> NWebDelegate::GetPreference() const {
  return preference_delegate_;
}

int NWebDelegate::Load(const std::string& url) {
  GURL gurl = GURL(url);
  if (gurl.is_empty() || !gurl.is_valid()) {
    GURL gurlWithHttp = GURL("https://" + url);
    if (!gurlWithHttp.is_valid()) {
      return NWEB_INVALID_URL;
    }
  }
  LOG(INFO) << "NWebDelegate::Load url=" << url;
  auto browser = GetBrowser();
  if (browser == nullptr) {
    return NWEB_ERR;
  }
  browser->GetMainFrame()->LoadURL(CefString(url));
  RequestVisitedHistory();
  return NWEB_OK;
}

bool NWebDelegate::IsNavigatebackwardAllowed() const {
  LOG(INFO) << "NWebDelegate::IsNavigatebackwardAllowed";
  if (GetBrowser().get()) {
    return GetBrowser()->CanGoBack();
  }
  return false;
}

bool NWebDelegate::IsNavigateForwardAllowed() const {
  LOG(INFO) << "NWebDelegate::IsNavigateForwardAllowed";
  if (GetBrowser().get()) {
    return GetBrowser()->CanGoForward();
  }
  return false;
}

bool NWebDelegate::CanNavigateBackOrForward(int num_steps) const {
  LOG(INFO) << "NWebDelegate::CanNavigateBackOrForward";
  if (GetBrowser().get()) {
    return GetBrowser()->CanGoBackOrForward(num_steps);
  }
  return false;
}

void NWebDelegate::NavigateBack() const {
  LOG(INFO) << "NWebDelegate::NavigateBack";
  if (GetBrowser().get()) {
    GetBrowser()->GoBack();
  }
}

void NWebDelegate::NavigateForward() const {
  LOG(INFO) << "NWebDelegate::NavigateForward";
  if (GetBrowser().get()) {
    GetBrowser()->GoForward();
  }
}

void NWebDelegate::NavigateBackOrForward(int step) const {
  LOG(INFO) << "NWebDelegate::NavigateBackOrForward";
  if (GetBrowser().get()) {
    GetBrowser()->GoBackOrForward(step);
  }
}

void NWebDelegate::DeleteNavigateHistory() {
  LOG(INFO) << "NWebDelegate::DeleteNavigateHistory";
  if (GetBrowser().get()) {
    GetBrowser()->DeleteHistory();
  }
}

void NWebDelegate::ClearSslCache() {
  LOG(INFO) << "NWebDelegate::ClearSslCache";
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  if (context != nullptr) {
    context->ClearCertificateExceptions(nullptr);
  }
}

void NWebDelegate::ClearClientAuthenticationCache() {
  LOG(INFO) << "NWebDelegate::ClearClientAuthenticationCache";
  CefRefPtr<CefRequestContext> context = CefRequestContext::GetGlobalContext();
  if (context != nullptr) {
    context->ClearClientAuthenticationCache(nullptr);
  }
}

void NWebDelegate::Reload() const {
  LOG(INFO) << "NWebDelegate::Reload";
  if (GetBrowser().get()) {
    GetBrowser()->Reload();
  }
}

void NWebDelegate::ReloadOriginalUrl() const {
  LOG(INFO) << "NWebDelegate::ReloadOriginalUrl";
  if (GetBrowser().get()) {
    GetBrowser()->ReloadOriginalUrl();
  }
}

const std::string NWebDelegate::GetOriginalUrl() {
  LOG(INFO) << "NWebDelegate::GetOriginalUrl";
  if (GetBrowser().get()) {
    return GetBrowser()->GetHost()->GetOriginalUrl();
  }
  return std::string();
}

bool NWebDelegate::GetFavicon(const void** data,
                              size_t& width,
                              size_t& height,
                              ImageColorType& colorType,
                              ImageAlphaType& alphaType) {
  LOG(INFO) << "NWebDelegate::getFavicon";
  if (handler_delegate_) {
    return handler_delegate_->GetFavicon(data, width, height, colorType,
                                         alphaType);
  } else {
    LOG(ERROR) << "handler_delegate_ is null";
    return false;
  }
}

void NWebDelegate::PutNetworkAvailable(bool avaiable) {
  LOG(INFO) << "NWebDelegate::PutNetworkAvailable";
  if (GetBrowser().get()) {
    GetBrowser()->GetHost()->PutNetworkAvailable(avaiable);
  }
}

void NWebDelegate::SetBrowserUserAgentString(const std::string& user_agent) {
  LOG(INFO) << "NWebDelegate::SetBrowserUserAgentString";
  if (GetBrowser().get()) {
    GetBrowser()->SetBrowserUserAgentString(user_agent);
  }
}

void NWebDelegate::StoreWebArchive(
    const std::string& base_name,
    bool auto_name,
    std::shared_ptr<NWebValueCallback<std::string>> callback) const {
  if (GetBrowser().get()) {
    CefRefPtr<StoreWebArchiveResultCallbackImpl> save_webarchive_callback =
        new StoreWebArchiveResultCallbackImpl(callback);
    GetBrowser()->GetHost()->StoreWebArchive(base_name, auto_name,
                                             save_webarchive_callback);
  }
}

int NWebDelegate::Zoom(float zoomFactor) const {
  LOG(INFO) << "NWebDelegate::Zoom";
  if (!preference_delegate_) {
    LOG(ERROR) << "preference_delegate_ get fail";
    return NWEB_ERR;
  }
  if (!preference_delegate_->ZoomingfunctionEnabled()) {
    return NWEB_FUNCTION_NOT_ENABLE;
  }
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI Zoom can not get browser";
    return NWEB_ERR;
  }
  double curFactor = GetBrowser()->GetHost()->GetZoomLevel();
  if (zoomFactor + curFactor > maxZoomFactor || zoomFactor + curFactor < 0) {
    LOG(ERROR) << "JSAPI Zoom can no more zoom";
    return NWEB_ERR;
  }
  GetBrowser()->GetHost()->SetZoomLevel(zoomFactor + curFactor);
  return NWEB_OK;
}

int NWebDelegate::ZoomIn() const {
  LOG(INFO) << "NWebDelegate::ZoomIn";
  if (!preference_delegate_) {
    return NWEB_ERR;
  }
  if (!preference_delegate_->ZoomingfunctionEnabled()) {
    return NWEB_FUNCTION_NOT_ENABLE;
  }
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI ZoomIn can not get browser";
    return NWEB_ERR;
  }
  double curFactor = GetBrowser()->GetHost()->GetZoomLevel();
  if (zoom_in_factor_ + curFactor > maxZoomFactor) {
    LOG(ERROR) << "JSAPI ZoomIn can no more zoom in";
    return NWEB_ERR;
  }
  GetBrowser()->GetHost()->SetZoomLevel(zoom_in_factor_ + curFactor);
  return NWEB_OK;
}

int NWebDelegate::ZoomOut() const {
  LOG(INFO) << "NWebDelegate::ZoomOut";
  if (!preference_delegate_) {
    return NWEB_ERR;
  }
  if (!preference_delegate_->ZoomingfunctionEnabled()) {
    return NWEB_FUNCTION_NOT_ENABLE;
  }
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI ZoomOut can not get browser";
    return NWEB_ERR;
  }
  double curFactor = GetBrowser()->GetHost()->GetZoomLevel();
  if (zoom_in_factor_ + curFactor < 0) {
    LOG(ERROR) << "JSAPI ZoomOut can no more zoom out";
    return NWEB_ERR;
  }
  GetBrowser()->GetHost()->SetZoomLevel(zoom_out_factor_ + curFactor);
  return NWEB_OK;
}

bool NWebDelegate::SetZoomInFactor(float factor) {
  LOG(INFO) << "NWebDelegate::SetZoomInFactor";
  if (factor <= 0) {
    return false;
  }
  zoom_in_factor_ = factor;
  return true;
}

bool NWebDelegate::SetZoomOutFactor(float factor) {
  LOG(INFO) << "NWebDelegate::SetZoomOutFactor";
  if (factor >= 0) {
    return false;
  }
  zoom_out_factor_ = factor;
  return true;
}

void NWebDelegate::Stop() const {
  LOG(INFO) << "NWebDelegate::Stop";
  if (GetBrowser().get()) {
    GetBrowser()->StopLoad();
  }
}

void NWebDelegate::ExecuteJavaScript(const std::string& code) const {
  LOG(INFO) << "NWebDelegate::ExecuteJavaScript";
  if (GetBrowser().get()) {
    GetBrowser()->GetMainFrame()->ExecuteJavaScript(
        code, GetBrowser()->GetMainFrame()->GetURL(), 0);
  }
}

void NWebDelegate::ExecuteJavaScript(
    const std::string& code,
    std::shared_ptr<NWebValueCallback<std::string>> callback) const {
  LOG(INFO) << "NWebDelegate::ExecuteJavaScript with callback";

  if (GetBrowser().get()) {
    CefRefPtr<JavaScriptResultCallbackImpl> JsResultCb =
        new JavaScriptResultCallbackImpl(callback);
    GetBrowser()->GetHost()->ExecuteJavaScript(code, JsResultCb);
  }
}

void NWebDelegate::PutBackgroundColor(int color) const {
  LOG(INFO) << "NWebDelegate::PutBackgroundColor";
  if (GetBrowser().get()) {
    GetBrowser()->GetHost()->SetBackgroundColor(color);
  }
}

void NWebDelegate::InitialScale(float scale) const {
  LOG(INFO) << "NWebDelegate::InitialScale";
  if (scale == intial_scale_ || !render_handler_) {
    return;
  }
  float ratio = render_handler_->GetVirtualPixelRatio();
  if (GetBrowser().get()) {
    GetBrowser()->GetHost()->SetInitialScale(scale / ratio);
  }
}

void NWebDelegate::OnPause() {
  LOG(INFO) << "NWebDelegate::OnPause";
  if (!GetBrowser().get()) {
    return;
  }

  // Remove focus from the browser.
  GetBrowser()->GetHost()->SetFocus(false);

  if (!hidden_) {
    // Set the browser as hidden.
    GetBrowser()->GetHost()->WasHidden(true);
    hidden_ = true;
  }
}

void NWebDelegate::OnContinue() {
  LOG(INFO) << "NWebDelegate::OnContinue";
  if (!GetBrowser().get()) {
    return;
  }

  if (hidden_) {
    // Set the browser as visible.
    GetBrowser()->GetHost()->WasHidden(false);
    hidden_ = false;
  }

  // Give focus to the browser.
  GetBrowser()->GetHost()->SetFocus(true);
}

void NWebDelegate::InitializeCef(std::string url,
                                 bool is_enhance_surface,
                                 void* window) {
  handler_delegate_ = NWebHandlerDelegate::Create(
      preference_delegate_, render_handler_, event_handler_, find_delegate_,
      is_enhance_surface, window);
  nweb_app_ =
      new NWebApplication(preference_delegate_, url, handler_delegate_, window);

  CefMainArgs mainargs(argc_, const_cast<char**>(argv_));
  int exitcode = CefExecuteProcess(mainargs, nweb_app_, NULL);
  if (exitcode >= 0) {
    LOG(INFO) << "CefExecuteProcess returned : " << exitcode;
    return;
  }

  CefSettings settings;
  settings.windowless_rendering_enabled = true;
  settings.log_severity = LOGSEVERITY_INFO;
  settings.multi_threaded_message_loop = false;
  settings.persist_session_cookies = true;

#if !defined(CEF_USE_SANDBOX)
  settings.no_sandbox = true;
#endif

  static bool is_initialized = false;
  static std::mutex init_mtx;
  std::unique_lock<std::mutex> lk(init_mtx);
  if (is_initialized) {
    return nweb_app_->CreateBrowser();
  }
  if (!CefInitialize(mainargs, settings, nweb_app_, NULL)) {
    LOG(ERROR) << "CefInitialize failed";
  } else {
    is_initialized = true;
  }
}

void NWebDelegate::RunMessageLoop() {
  // Run the CEF message loop.
  // This will block until CefQuitMessageLoop() is called.
  CefRunMessageLoop();
}

std::string NWebDelegate::Title() {
  if (!GetBrowser().get()) {
    return "";
  }
  return GetBrowser()->GetHost()->Title();
}

void NWebDelegate::CreateWebMessagePorts(std::vector<std::string>& ports) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI CreateWebMessagePorts can not get browser";
    return;
  }
  std::vector<CefString> cefPorts;
  GetBrowser()->GetHost()->CreateWebMessagePorts(cefPorts);

  for (CefString port : cefPorts) {
    ports.push_back(port.ToString());
  }
}

void NWebDelegate::PostWebMessage(std::string& message,
                                  std::vector<std::string>& ports,
                                  std::string& targetUri) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI PostWebMessage can not get browser";
    return;
  }

  std::vector<CefString> cefPorts;
  for (std::string port : ports) {
    CefString portCef;
    portCef.FromString(port);
    cefPorts.push_back(portCef);
  }

  CefString msgCef;
  msgCef.FromString(message);
  CefString uri;
  uri.FromString(targetUri);

  GetBrowser()->GetHost()->PostWebMessage(msgCef, cefPorts, uri);
}

void NWebDelegate::ClosePort(std::string& portHandle) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI ClosePort can not get browser";
    return;
  }
  CefString handleCef;
  handleCef.FromString(portHandle);

  GetBrowser()->GetHost()->ClosePort(handleCef);
}

void NWebDelegate::PostPortMessage(std::string& portHandle, std::shared_ptr<NWebMessage> data) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI PostPortMessage can not get browser";
    return;
  }
  CefString handleCef;
  handleCef.FromString(portHandle);

  CefRefPtr<CefValue> message = CefValue::Create();
  if (data->GetType() == NWebValue::Type::STRING) {
    message->SetString(data->GetString());
  } else if (data->GetType() == NWebValue::Type::BINARY) {
    std::vector<uint8_t> vecBinary = data->GetBinary();
    CefRefPtr<CefBinaryValue> value = CefBinaryValue::Create(vecBinary.data(), vecBinary.size());
    message->SetBinary(value);
  }

  GetBrowser()->GetHost()->PostPortMessage(handleCef, message);
}

void NWebDelegate::SetPortMessageCallback(std::string& portHandle,
    std::shared_ptr<NWebValueCallback<std::shared_ptr<NWebMessage>>> callback) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI SetPortMessageCallback can not get browser";
    return;
  }
  CefRefPtr<CefWebMessageReceiver> JsResultCb = new CefWebMessageReceiverImpl(callback);
  CefString handleCef;
  handleCef.FromString(portHandle);
  GetBrowser()->GetHost()->SetPortMessageCallback(handleCef, JsResultCb);
}

std::string NWebDelegate::GetUrl() const {
  LOG(INFO) << "NWebDelegate::get url";
  if (GetBrowser().get()) {
    auto entry = GetBrowser()->GetHost()->GetVisibleNavigationEntry();
    if (entry) {
      return entry->GetDisplayURL().ToString();
    }
  }
  return "";
}

HitTestResult NWebDelegate::GetHitTestResult() const {
  HitTestResult data;
  if (!GetBrowser().get()) {
    return data;
  }
  int type;
  CefString extra_data;
  GetBrowser()->GetHost()->GetHitData(type, extra_data);
  data.SetType(type);
  data.SetExtra(extra_data.ToString());
  return data;
}

int NWebDelegate::PageLoadProgress() {
  if (!GetBrowser().get()) {
    return 0;
  }
  return GetBrowser()->GetHost()->PageLoadProgress();
}

float NWebDelegate::Scale() {
  if (!GetBrowser().get()) {
    return 0;
  }
  return GetBrowser()->GetHost()->Scale();
}

int NWebDelegate::Load(
    std::string& url,
    std::map<std::string, std::string> additionalHttpHeaders) {
  GURL gurl = GURL(url);
  if (gurl.is_empty() || !gurl.is_valid()) {
    GURL gurlWithHttp = GURL("https://" + url);
    if (!gurlWithHttp.is_valid()) {
      return NWEB_INVALID_URL;
    }
  }
  std::map<std::string, std::string>::iterator iter;
  std::string extra = "";
  for (iter = additionalHttpHeaders.begin();
       iter != additionalHttpHeaders.end(); iter++) {
    const std::string& key = iter->first;
    const std::string& value = iter->second;
    if (!key.empty()) {
      // Delimit with "\r\n".
      if (!value.empty())
        extra += "\r\n";
      extra += std::string(key) + ": " + std::string(value);
    }
  }
  auto browser = GetBrowser();
  if (browser == nullptr) {
    return NWEB_ERR;
  }
  browser->GetMainFrame()->LoadHeaderUrl(CefString(url), CefString(extra));
  RequestVisitedHistory();
  return NWEB_OK;
}

int NWebDelegate::LoadWithDataAndBaseUrl(const std::string& baseUrl,
                                         const std::string& data,
                                         const std::string& mimeType,
                                         const std::string& encoding,
                                         const std::string& historyUrl) {
  LOG(INFO) << "NWebDelegate::LoadWithDataAndBaseUrl";
  if (!GetBrowser().get()) {
    return NWEB_ERR;
  }
  GetBrowser()->GetHost()->LoadWithDataAndBaseUrl(baseUrl, data, mimeType,
                                                  encoding, historyUrl);
  RequestVisitedHistory();
  return NWEB_OK;
}

int NWebDelegate::LoadWithData(const std::string& data,
                               const std::string& mimeType,
                               const std::string& encoding) {
  LOG(INFO) << "NWebDelegate::LoadWithData";
  if (!GetBrowser().get()) {
    return NWEB_ERR;
  }
  GetBrowser()->GetHost()->LoadWithData(data, mimeType, encoding);
  RequestVisitedHistory();
  return NWEB_OK;
}

const CefRefPtr<CefBrowser> NWebDelegate::GetBrowser() const {
  if (handler_delegate_) {
    return handler_delegate_->GetBrowser();
  }
  return nullptr;
}

bool NWebDelegate::IsReady() {
  return GetBrowser() != nullptr;
}

void NWebDelegate::RequestVisitedHistory() {
  if (!GetBrowser().get()) {
    return;
  }
  if (!has_requested_visited_history) {
    has_requested_visited_history = true;
    if (handler_delegate_) {
      std::vector<std::string> outUrls = handler_delegate_->GetVisitedHistory();
      std::vector<CefString> urls = std::vector<CefString>();
      for (auto url : outUrls) {
        urls.push_back(url);
      }
      GetBrowser()->GetHost()->AddVisitedLinks(urls);
    }
  }
}

int NWebDelegate::ContentHeight() {
  if (render_handler_ != nullptr) {
    return render_handler_->ContentHeight();
  }
  return 0;
}

void NWebDelegate::RegisterArkJSfunction(
    const std::string& object_name,
    const std::vector<std::string>& method_list) const {
  LOG(INFO) << "RegisterArkJSfunction name : " << object_name.c_str();
  std::vector<CefString> method_vector;
  for (std::string method : method_list) {
    method_vector.push_back(method);
  }
  GetBrowser()->GetHost()->RegisterArkJSfunction(object_name, method_vector);
}

void NWebDelegate::UnregisterArkJSfunction(
    const std::string& object_name,
    const std::vector<std::string>& method_list) const {
  LOG(INFO) << "UnregisterArkJSfunction name : " << object_name.c_str();
  std::vector<CefString> method_vector;
  for (std::string method : method_list) {
    method_vector.push_back(method);
  }
  GetBrowser()->GetHost()->UnregisterArkJSfunction(object_name, method_vector);
}

void NWebDelegate::RegisterNWebJavaScriptCallBack(
    std::shared_ptr<NWebJavaScriptResultCallBack> callback) {
  if (handler_delegate_ == nullptr) {
    LOG(ERROR) << "fail to register NWEB client, NWEB handler is nullptr";
    return;
  }
  handler_delegate_->RegisterNWebJavaScriptCallBack(callback);
}

void NWebDelegate::OnFocus() const {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "NWebDelegate::OnFocus GetBrowser().get() fail";
    return;
  }
  if (handler_delegate_ && !handler_delegate_->GetFocusState()) {
    GetBrowser()->GetHost()->SetFocus(true);
  }
}

void NWebDelegate::OnBlur() const {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "NWebDelegate::OnBlur GetBrowser().get() fail";
    return;
  }

  if (handler_delegate_ && handler_delegate_->GetFocusState()) {
    handler_delegate_->SetFocusState(false);
    GetBrowser()->GetHost()->SetFocus(false);
  }
}

void NWebDelegate::UpdateLocale(const std::string& language,
                                const std::string& region) {
  if (!GetBrowser().get()) {
    return;
  }

  CefString locale = "";
  if (language == "en" && region == "US") {
    locale = "en-US";
  } else if (language == "zh") {
    locale = "zh-CN";
  } else {
    // Now only support zh and en.
    return;
  }

  GetBrowser()->GetHost()->UpdateLocale(locale);
}

#if defined(REPORT_SYS_EVENT)
void NWebDelegate::SetNWebId(uint32_t nwebId) {
  nweb_id_ = nwebId;
  if (nweb_id_ != 0) {
    handler_delegate_->SetNWebId(nweb_id_);
  }
}
#endif

void NWebDelegate::SendDragEvent(const DelegateDragEvent& dragEvent) const {
  if (!GetBrowser().get() || !render_handler_) {
    LOG(ERROR) << "browser or render_handler is nullptr";
    return;
  }
  CefMouseEvent event;
  float ratio = render_handler_->GetVirtualPixelRatio();
  event.x = dragEvent.x / ratio;
  event.y = dragEvent.y / ratio;
  event.modifiers = EVENTFLAG_LEFT_MOUSE_BUTTON;
  switch (dragEvent.action) {
    case DelegateDragAction::DRAG_START:
      break;
    case DelegateDragAction::DRAG_ENTER:
      if (render_handler_) {
        LOG(INFO) << "SendDragEvent enter";
        GetBrowser()->GetHost()->DragTargetDragEnter(
            render_handler_->GetDragData(), event, DRAG_OPERATION_MOVE);
      }
      break;
    case DelegateDragAction::DRAG_LEAVE:
      LOG(INFO) << "SendDragEvent leave";
      GetBrowser()->GetHost()->DragTargetDragLeave();
      break;
    case DelegateDragAction::DRAG_OVER:
      GetBrowser()->GetHost()->DragTargetDragOver(event, DRAG_OPERATION_MOVE);
      break;
    case DelegateDragAction::DRAG_DROP:
      event.modifiers = EVENTFLAG_NONE;
      LOG(INFO) << "SendDragEvent drop";
      GetBrowser()->GetHost()->DragTargetDrop(event);
      break;
    case DelegateDragAction::DRAG_END:
      LOG(INFO) << "SendDragEvent end";
      GetBrowser()->GetHost()->DragSourceEndedAt(event.x, event.y,
                                                 DRAG_OPERATION_MOVE);
      GetBrowser()->GetHost()->DragSourceSystemDragEnded();
      break;
    case DelegateDragAction::DRAG_CANCEL:
      LOG(INFO) << "SendDragEvent cancel";
      GetBrowser()->GetHost()->DragSourceSystemDragEnded();
      break;
    default:
      LOG(INFO) << "invalid drag action";
      break;
  }
}

void NWebDelegate::GetImages(
    std::shared_ptr<NWebValueCallback<bool>> callback) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI GetImages can not get browser";
    return;
  }

  CefRefPtr<GetImagesCallbackImpl> GetImagesCb =
      new GetImagesCallbackImpl(callback);
  GetBrowser()->GetMainFrame()->GetImages(GetImagesCb);
}

void NWebDelegate::RemoveCache(bool include_disk_files) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI RemoveCache can not get browser";
    return;
  }

  GetBrowser()->GetHost()->RemoveCache(include_disk_files);
}

std::shared_ptr<NWebHistoryList> NWebDelegate::GetHistoryList() {
  if (!GetBrowser().get()) {
    return nullptr;
  }

  CefRefPtr<NavigationEntryVisitorImpl> visitor =
      new NavigationEntryVisitorImpl();
  GetBrowser()->GetHost()->GetNavigationEntries(visitor, false);
  return visitor->GetHistoryList();
}

void NWebDelegate::PageUp(bool top) {
  if (!GetBrowser().get() || !render_handler_ || !handler_delegate_) {
    return;
  }
  float ratio = render_handler_->GetVirtualPixelRatio();
  float scale = handler_delegate_->GetScale() / 100.0;
  if (ratio <= 0 || scale <= 0) {
    LOG(ERROR) << "get ratio and scale invalid " << ratio << " " << scale;
    return;
  }
  GetBrowser()->GetHost()->ScrollPageUpDown(true, !top,
                                            height_ / ratio / scale);
}

void NWebDelegate::PageDown(bool bottom) {
  if (!GetBrowser().get() || !render_handler_ || !handler_delegate_) {
    return;
  }
  float ratio = render_handler_->GetVirtualPixelRatio();
  float scale = handler_delegate_->GetScale() / 100.0;
  if (ratio <= 0 || scale <= 0) {
    LOG(ERROR) << "get ratio and scale invalid " << ratio << " " << scale;
    return;
  }
  GetBrowser()->GetHost()->ScrollPageUpDown(false, !bottom,
                                            height_ / ratio / scale);
}

void NWebDelegate::ScrollTo(float x, float y) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI ScrollTo can not get browser";
    return;
  }

  GetBrowser()->GetHost()->ScrollTo(x, y);
}

void NWebDelegate::ScrollBy(float delta_x, float delta_y) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI ScrollBy can not get browser";
    return;
  }

  GetBrowser()->GetHost()->ScrollBy(delta_x, delta_y);
}

void NWebDelegate::SlideScroll(float vx, float vy) {
  if (!GetBrowser().get()) {
    LOG(ERROR) << "JSAPI SlideScroll can not get browser";
    return;
  }

  GetBrowser()->GetHost()->SlideScroll(vx, vy);
}

WebState NWebDelegate::SerializeWebState() {
  CefRefPtr<CefBinaryValue> state_value =
      GetBrowser()->GetHost()->GetWebState();
  if (!state_value || !GetBrowser().get()) {
    return nullptr;
  }
  size_t state_size = state_value->GetSize();
  if (state_size == 0) {
    return nullptr;
  }
  WebState state = std::make_shared<std::vector<uint8_t>>(state_size);
  size_t read_size = state_value->GetData(state->data(), state_size, 0);
  if (read_size != state_size) {
    LOG(ERROR) << "SerializeWebState failed";
    return nullptr;
  }
  return state;
}

bool NWebDelegate::RestoreWebState(WebState state) {
  if (!GetBrowser().get() || !state || state->size() == 0) {
    return false;
  }
  auto web_state = CefBinaryValue::Create(state->data(), state->size());
  return GetBrowser()->GetHost()->RestoreWebState(web_state);
}
}  // namespace OHOS::NWeb
