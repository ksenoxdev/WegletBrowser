// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// Ported from content/shell/browser/shell_file_select_helper.{h,cc}, which
// content_shell itself is not a dependency any embedder can link against.
// Handles the native file-picker dialog behind <input type=file> and
// window.showSaveFilePicker.

#ifndef WEGLET_BROWSER_WEGLET_FILE_SELECT_HELPER_H_
#define WEGLET_BROWSER_WEGLET_FILE_SELECT_HELPER_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_select_listener.h"
#include "net/base/directory_lister.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom-forward.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace ui {
struct SelectedFileInfo;
}

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace weglet {

class WegletFileSelectHelper
    : public base::RefCountedThreadSafe<
          WegletFileSelectHelper,
          content::BrowserThread::DeleteOnUIThread>,
      public ui::SelectFileDialog::Listener,
      private net::DirectoryLister::DirectoryListerDelegate {
 public:
  WegletFileSelectHelper(const WegletFileSelectHelper&) = delete;
  WegletFileSelectHelper& operator=(const WegletFileSelectHelper&) = delete;

  // Show the file chooser dialog.
  static void RunFileChooser(
      content::RenderFrameHost* render_frame_host,
      scoped_refptr<content::FileSelectListener> listener,
      const blink::mojom::FileChooserParams& params);

 private:
  friend class base::RefCountedThreadSafe<WegletFileSelectHelper>;
  friend class base::DeleteHelper<WegletFileSelectHelper>;
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;

  WegletFileSelectHelper();
  ~WegletFileSelectHelper() override;

  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      blink::mojom::FileChooserParamsPtr params);

  // Cleans up and releases this instance. Called after the last callback
  // arrives from the file dialog.
  void RunFileChooserEnd();

  // SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

  // Kicks off a new directory enumeration, for kUploadFolder.
  void StartNewEnumeration(const base::FilePath& path);

  // net::DirectoryLister::DirectoryListerDelegate:
  void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data) override;
  void OnListDone(int error) override;

  void ConvertToFileChooserFileInfoList(
      const std::vector<ui::SelectedFileInfo>& files);

  base::FilePath base_dir_;

  struct ActiveDirectoryEnumeration;
  std::unique_ptr<ActiveDirectoryEnumeration> directory_enumeration_;

  base::WeakPtr<content::WebContents> web_contents_;
  scoped_refptr<content::FileSelectListener> listener_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  std::unique_ptr<ui::SelectFileDialog::FileTypeInfo> select_file_types_;
  ui::SelectFileDialog::Type dialog_type_ =
      ui::SelectFileDialog::SELECT_OPEN_FILE;
  blink::mojom::FileChooserParams::Mode dialog_mode_ =
      blink::mojom::FileChooserParams::Mode::kOpen;
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_FILE_SELECT_HELPER_H_
