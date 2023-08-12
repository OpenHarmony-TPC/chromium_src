// Copyright 2022 The Huawei Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/clipboard/ohos/clipboard_ohos_read_data.h"

#include <map>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "url/gurl.h"

using namespace OHOS::NWeb;

namespace ui {
const std::string kImgPasteboardDir = "/data/storage/el2/base/cache/pasteboard";

std::string RemoveFileSchemePerfix(const std::string& img_src) {
  GURL img_url(img_src);
  if (img_url.SchemeIsFile()) {
    return img_url.path();
  }
  return img_src;
}

ClipboardOhosReadData::ClipboardOhosReadData(PasteRecordList& record_list)
    : record_list_(record_list) {
  is_in_app_ = OhosAdapterHelper::GetInstance().GetPasteBoard().IsLocalPaste();
  token_id_ = OhosAdapterHelper::GetInstance().GetPasteBoard().GetTokenId();
  html_ = record_list_[0]->GetHtmlText();
  text_ = record_list_[0]->GetPlainText();

  for (auto& record : record_list_) {
    std::shared_ptr<std::string> new_uri = record->GetUri();
    if (!new_uri) {
      continue;
    }
    LOG(INFO) << "new_uri:" << *new_uri;
    int fd = OhosAdapterHelper::GetInstance().GetPasteBoard().OpenRemoteUri(
        *new_uri);
    if (fd > 0) {
      base::File url_file(fd);
      file_map_.insert(std::make_pair(*new_uri, std::move(url_file)));
    }
  }
}

void ClipboardOhosReadData::SaveImgFile(const std::string& old_uri,
                                        std::string& new_uri,
                                        uint32_t token_id) {
  base::FilePath pasteboard_root_path(kImgPasteboardDir);
  std::string old_uri_without_prefix = RemoveFileSchemePerfix(old_uri);
  base::FilePath dest_file_dir_path(
      pasteboard_root_path.Append(std::to_string(token_id))
          .Append(base::FilePath(old_uri_without_prefix).DirName()));
  base::FilePath new_file_path(dest_file_dir_path.Append(
      base::FilePath(old_uri_without_prefix).BaseName()));
  if (!base::DirectoryExists(dest_file_dir_path) &&
      !base::CreateDirectory(dest_file_dir_path)) {
    return;
  }

  FileUrlMap::iterator iter = file_map_.find(new_uri);
  if (iter == file_map_.end()) {
    return;
  }
  base::File& old_file = iter->second;
  base::File new_file(new_file_path, base::File::Flags::FLAG_CREATE_ALWAYS |
                                         base::File::Flags::FLAG_WRITE);
  if (old_file.IsValid() && new_file.IsValid()) {
    if (base::CopyFileContents(old_file, new_file)) {
      new_uri = "file://" + new_file_path.AsUTF8Unsafe();
    } else {
      LOG(ERROR) << "SaveImgFile copy file failed";
    }
  } else {
    LOG(ERROR) << "SaveImgFile invalid";
  }
}

std::shared_ptr<std::string> ClipboardOhosReadData::ReadHtml() {
  if (!html_) {
    return nullptr;
  }

  if (record_list_.size() > 1 && !is_in_app_ && !has_been_read_html_) {
    ReplaceHTMLImgUriList(*html_);
    has_been_read_html_ = true;
  }
  return html_;
}

void ClipboardOhosReadData::ReplaceHTMLImgUriList(std::string& html) {
  std::map<std::string, std::string> uri_map;
  std::map<int, std::string> offset_map;
  icu::UnicodeString html_data(html.c_str(), html.size());
  for (size_t index = 1; index < record_list_.size(); index++) {
    auto& record = record_list_[index];
    std::shared_ptr<std::string> new_uri = record->GetUri();
    std::shared_ptr<PasteCustomData> uri_offset_list = record->GetCustomData();
    if (!uri_offset_list || !new_uri) {
      LOG(ERROR) << "uri_offset_list get failed";
      continue;
    }
    PasteCustomData::iterator iter;
    for (iter = uri_offset_list->begin(); iter != uri_offset_list->end();
         iter++) {
      SaveImgFile(iter->first, *new_uri, token_id_);
      uri_map.insert(std::make_pair(iter->first, *new_uri));
      const int* offset_data =
          reinterpret_cast<const int*>(iter->second.data());
      std::vector<int> offset_list(
          offset_data, offset_data + iter->second.size() / sizeof(int));
      for (size_t i = 0; i < offset_list.size(); i++) {
        offset_map.insert(std::make_pair(offset_list[i], iter->first));
      }
    }
  }

  std::map<int, std::string>::iterator iter;
  std::map<std::string, std::string>::iterator find_uri_map_iter;
  int cum_replace_offset = 0;
  for (iter = offset_map.begin(); iter != offset_map.end(); ++iter) {
    find_uri_map_iter = uri_map.find(iter->second);
    if (find_uri_map_iter == uri_map.end()) {
      continue;
    }
    std::string new_uri = find_uri_map_iter->second;
    icu::UnicodeString new_uri_uni(new_uri.c_str());
    html_data = html_data.replace(iter->first + cum_replace_offset,
                                  iter->second.length(), new_uri_uni);
    cum_replace_offset += new_uri.length() - iter->second.length();
  }
  html.clear();
  html_data.toUTF8String(html);
}

ClipboardOhosReadData::~ClipboardOhosReadData() {
  file_map_.clear();
}

}  // namespace ui