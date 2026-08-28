// Copyright 2026 Weglet - Licensed under Apache 2.0

#include "weglet/browser/weglet_file_select_helper.h"

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "ui/shell_dialogs/select_file_policy.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace weglet {

namespace {

// http://whatwg.org/html/number-state.html#attr-input-accept -- accept_types
// contains only valid lowercased MIME types or extensions beginning with a
// period (.).
std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> GetFileTypesFromAcceptType(
    const std::vector<std::u16string>& accept_types) {
  auto base_file_type = std::make_unique<ui::SelectFileDialog::FileTypeInfo>();
  if (accept_types.empty()) {
    return base_file_type;
  }

  auto file_type =
      std::make_unique<ui::SelectFileDialog::FileTypeInfo>(*base_file_type);
  file_type->extensions.resize(1);
  std::vector<base::FilePath::StringType>* extensions =
      &file_type->extensions.back();

  size_t valid_type_count = 0;
  for (const auto& accept_type : accept_types) {
    size_t old_extension_size = extensions->size();
    if (accept_type[0] == '.') {
      base::FilePath::StringType ext =
          base::FilePath::FromUTF16Unsafe(accept_type).value();
      extensions->push_back(ext.substr(1));
    } else {
      if (!base::IsStringASCII(accept_type)) {
        continue;
      }
      std::string ascii_type = base::UTF16ToASCII(accept_type);
      net::GetExtensionsForMimeType(ascii_type, extensions);
    }
    if (extensions->size() > old_extension_size) {
      valid_type_count++;
    }
  }

  // No valid extension found: fall back to no filter rather than one that
  // matches nothing.
  if (valid_type_count == 0) {
    return base_file_type;
  }
  return file_type;
}

}  // namespace

struct WegletFileSelectHelper::ActiveDirectoryEnumeration {
  explicit ActiveDirectoryEnumeration(const base::FilePath& path)
      : path_(path) {}

  std::unique_ptr<net::DirectoryLister> lister_;
  const base::FilePath path_;
  std::vector<base::FilePath> results_;
};

// static
void WegletFileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  // Keeps itself alive until it sends the result.
  scoped_refptr<WegletFileSelectHelper> file_select_helper(
      new WegletFileSelectHelper());
  file_select_helper->RunFileChooser(render_frame_host, std::move(listener),
                                     params.Clone());
}

WegletFileSelectHelper::WegletFileSelectHelper() = default;

WegletFileSelectHelper::~WegletFileSelectHelper() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void WegletFileSelectHelper::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    blink::mojom::FileChooserParamsPtr params) {
  select_file_dialog_ = ui::SelectFileDialog::Create(this, nullptr);
  if (!select_file_dialog_) {
    listener->FileSelectionCanceled();
    return;
  }

  listener_ = std::move(listener);
  web_contents_ = content::WebContents::FromRenderFrameHost(render_frame_host)
                      ->GetWeakPtr();

  select_file_types_ = GetFileTypesFromAcceptType(params->accept_types);
  select_file_types_->allowed_paths =
      params->need_local_path ? ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH
                              : ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
  // 1-based index of default extension to show.
  int file_type_index =
      select_file_types_ && !select_file_types_->extensions.empty() ? 1 : 0;

  dialog_mode_ = params->mode;
  switch (params->mode) {
    case blink::mojom::FileChooserParams::Mode::kOpen:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      break;
    case blink::mojom::FileChooserParams::Mode::kOpenMultiple:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_MULTI_FILE;
      break;
    case blink::mojom::FileChooserParams::Mode::kUploadFolder:
      dialog_type_ = ui::SelectFileDialog::SELECT_UPLOAD_FOLDER;
      break;
    case blink::mojom::FileChooserParams::Mode::kSave:
      dialog_type_ = ui::SelectFileDialog::SELECT_SAVEAS_FILE;
      break;
    default:
      dialog_type_ = ui::SelectFileDialog::SELECT_OPEN_FILE;
      NOTREACHED();
  }

  gfx::NativeWindow owning_window = web_contents_->GetTopLevelNativeWindow();
  select_file_dialog_->SelectFile(dialog_type_, std::u16string(),
                                  base::FilePath(), select_file_types_.get(),
                                  file_type_index, base::FilePath::StringType(),
                                  owning_window, nullptr);

  // Kept alive until the last callback arrives from the file dialog; see
  // RunFileChooserEnd.
  AddRef();
}

void WegletFileSelectHelper::RunFileChooserEnd() {
  if (listener_) {
    listener_->FileSelectionCanceled();
  }
  select_file_dialog_->ListenerDestroyed();
  select_file_dialog_.reset();
  Release();
}

void WegletFileSelectHelper::FileSelected(const ui::SelectedFileInfo& file,
                                          int index) {
  if (dialog_type_ == ui::SelectFileDialog::SELECT_UPLOAD_FOLDER) {
    StartNewEnumeration(file.local_path);
    return;
  }
  ConvertToFileChooserFileInfoList({file});
}

void WegletFileSelectHelper::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  ConvertToFileChooserFileInfoList(files);
}

void WegletFileSelectHelper::FileSelectionCanceled() {
  RunFileChooserEnd();
}

void WegletFileSelectHelper::StartNewEnumeration(const base::FilePath& path) {
  base_dir_ = path;
  auto entry = std::make_unique<ActiveDirectoryEnumeration>(path);
  entry->lister_ = base::WrapUnique(new net::DirectoryLister(
      path, net::DirectoryLister::NO_SORT_RECURSIVE, this));
  entry->lister_->Start();
  directory_enumeration_ = std::move(entry);
}

void WegletFileSelectHelper::OnListFile(
    const net::DirectoryLister::DirectoryListerData& data) {
  if (data.info.IsDirectory()) {
    return;
  }
  directory_enumeration_->results_.push_back(data.path);
}

void WegletFileSelectHelper::OnListDone(int error) {
  if (!web_contents_) {
    // Torn down under us -- notify the listener and release via
    // RunFileChooserEnd.
    RunFileChooserEnd();
    return;
  }

  std::unique_ptr<ActiveDirectoryEnumeration> entry =
      std::move(directory_enumeration_);
  if (error) {
    FileSelectionCanceled();
    return;
  }

  std::vector<blink::mojom::FileChooserFileInfoPtr> chooser_files;
  for (const auto& file_path : entry->results_) {
    chooser_files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(file_path, std::u16string(),
                                          std::vector<std::u16string>())));
  }

  listener_->FileSelected(std::move(chooser_files), base_dir_,
                          blink::mojom::FileChooserParams::Mode::kUploadFolder);
  listener_.reset();
  // No members should be accessed from here on.
  RunFileChooserEnd();
}

void WegletFileSelectHelper::ConvertToFileChooserFileInfoList(
    const std::vector<ui::SelectedFileInfo>& files) {
  if (!web_contents_) {
    RunFileChooserEnd();
    return;
  }

  std::vector<blink::mojom::FileChooserFileInfoPtr> chooser_files;
  for (const auto& file : files) {
    chooser_files.push_back(blink::mojom::FileChooserFileInfo::NewNativeFile(
        blink::mojom::NativeFileInfo::New(
            file.local_path, base::FilePath(file.display_name).AsUTF16Unsafe(),
            std::vector<std::u16string>())));
  }

  listener_->FileSelected(std::move(chooser_files), base::FilePath(),
                          dialog_mode_);
  listener_ = nullptr;
  // No members should be accessed from here on.
  RunFileChooserEnd();
}

}  // namespace weglet
