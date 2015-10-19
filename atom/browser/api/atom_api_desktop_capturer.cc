// Copyright (c) 2015 GitHub, Inc.
// Use of this source code is governed by the MIT license that can be
// found in the LICENSE file.

#include "atom/browser/api/atom_api_desktop_capturer.h"

#include "atom/common/api/atom_api_native_image.h"
#include "atom/common/node_includes.h"
#include "atom/common/native_mate_converters/gfx_converter.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/media/desktop_media_list.h"
#include "native_mate/dictionary.h"
#include "native_mate/handle.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/screen_capturer.h"
#include "third_party/webrtc/modules/desktop_capture/window_capturer.h"

namespace mate {

template<>
struct Converter<DesktopMediaList::Source> {
  static v8::Local<v8::Value> ToV8(v8::Isolate* isolate,
                                   const DesktopMediaList::Source& source) {
    mate::Dictionary dict(isolate, v8::Object::New(isolate));
    content::DesktopMediaID id = source.id;
    dict.Set("name", base::UTF16ToUTF8(source.name));
    dict.Set("id", id.ToString());
    dict.Set(
        "thumbnail",
        atom::api::NativeImage::Create(isolate, gfx::Image(source.thumbnail)));
    return ConvertToV8(isolate, dict);
  }
};

}  // namespace mate

namespace atom {

namespace api {

namespace {
const int kThumbnailWidth = 150;
const int kThumbnailHeight = 150;

bool GetCapturerTypes(const mate::Dictionary& args,
                      bool* show_windows,
                      bool* show_screens) {
  *show_windows = false;
  *show_screens = false;
  std::vector<std::string> sources;
  if (!args.Get("types", &sources))
    return false;
  for (const auto& source_type : sources) {
    if (source_type == "screen")
      *show_screens = true;
    else if (source_type == "window")
      *show_windows = true;
  }
  return !show_windows && !show_screens ? false : true;
}

}  // namespace

DesktopCapturer::DesktopCapturer() {
}

DesktopCapturer::~DesktopCapturer() {
}

void DesktopCapturer::StartHandling(const mate::Dictionary& args) {
  bool show_screens = false;
  bool show_windows = false;
  if (!GetCapturerTypes(args, &show_windows, &show_screens)) {
    Emit("handling-finished",
         "Invalid options.",
         std::vector<DesktopMediaList::Source>());
    return;
  }

  webrtc::DesktopCaptureOptions options =
      webrtc::DesktopCaptureOptions::CreateDefault();

#if defined(OS_WIN)
  // On windows, desktop effects (e.g. Aero) will be disabled when the Desktop
  // capture API is active by default.
  // We keep the desktop effects in most times. Howerver, the screen still
  // fickers when the API is capturing the window due to limitation of current
  // implemetation. This is a known and wontFix issue in webrtc (see:
  // http://code.google.com/p/webrtc/issues/detail?id=3373)
  options.set_disable_effects(false);
#endif

  scoped_ptr<webrtc::ScreenCapturer> screen_capturer(
      show_screens ? webrtc::ScreenCapturer::Create(options) : nullptr);
  scoped_ptr<webrtc::WindowCapturer> window_capturer(
      show_windows ? webrtc::WindowCapturer::Create(options) : nullptr);
  media_list_.reset(new NativeDesktopMediaList(screen_capturer.Pass(),
      window_capturer.Pass()));

  gfx::Size thumbnail_size(kThumbnailWidth, kThumbnailHeight);
  args.Get("thumbnailSize", &thumbnail_size);

  media_list_->SetThumbnailSize(thumbnail_size);
  media_list_->StartUpdating(this);
}

void DesktopCapturer::OnSourceAdded(int index) {
}

void DesktopCapturer::OnSourceRemoved(int index) {
}

void DesktopCapturer::OnSourceMoved(int old_index, int new_index) {
}

void DesktopCapturer::OnSourceNameChanged(int index) {
}

void DesktopCapturer::OnSourceThumbnailChanged(int index) {
}

bool DesktopCapturer::OnRefreshFinished() {
  std::vector<DesktopMediaList::Source> sources;
  for (int i = 0; i < media_list_->GetSourceCount(); ++i)
    sources.push_back(media_list_->GetSource(i));
  media_list_.reset();
  Emit("handling-finished", "", sources);
  return false;
}

mate::ObjectTemplateBuilder DesktopCapturer::GetObjectTemplateBuilder(
      v8::Isolate* isolate) {
  return mate::ObjectTemplateBuilder(isolate)
      .SetMethod("startHandling", &DesktopCapturer::StartHandling);
}

// static
mate::Handle<DesktopCapturer> DesktopCapturer::Create(v8::Isolate* isolate) {
  return mate::CreateHandle(isolate, new DesktopCapturer);
}

}  // namespace api

}  // namespace atom

namespace {

void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> unused,
                v8::Local<v8::Context> context, void* priv) {
  v8::Isolate* isolate = context->GetIsolate();
  mate::Dictionary dict(isolate, exports);
  dict.Set("desktopCapturer", atom::api::DesktopCapturer::Create(isolate));
}

}  // namespace

NODE_MODULE_CONTEXT_AWARE_BUILTIN(atom_browser_desktop_capturer, Initialize);
