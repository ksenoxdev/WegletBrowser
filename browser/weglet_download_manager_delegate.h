// Copyright 2026 Weglet - Licensed under Apache 2.0
//
// ChooseSavePath is for Ctrl+S. DetermineDownloadTarget and GetNextId are
// for everything else content::DownloadManager hands a download -- an
// ordinary <a download> click, a server Content-Disposition, and so on.
// Without them, content's own defaults leave every regular download with
// no id and no target path (see download_manager_delegate.cc), so nothing
// downloads at all.

#ifndef WEGLET_BROWSER_WEGLET_DOWNLOAD_MANAGER_DELEGATE_H_
#define WEGLET_BROWSER_WEGLET_DOWNLOAD_MANAGER_DELEGATE_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "content/public/browser/download_manager_delegate.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace weglet {

class WegletDownloadManagerDelegate : public content::DownloadManagerDelegate,
                                      public ui::SelectFileDialog::Listener {
 public:
  WegletDownloadManagerDelegate();
  WegletDownloadManagerDelegate(const WegletDownloadManagerDelegate&) = delete;
  WegletDownloadManagerDelegate& operator=(const WegletDownloadManagerDelegate&) = delete;
  ~WegletDownloadManagerDelegate() override;

  // content::DownloadManagerDelegate:
  void GetNextId(content::DownloadIdCallback callback) override;
  bool DetermineDownloadTarget(download::DownloadItem* item,
                              download::DownloadTargetCallback* callback) override;
  void ChooseSavePath(content::WebContents* web_contents,
                      const base::FilePath& suggested_path,
                      const base::FilePath::StringType& default_extension,
                      bool can_save_as_complete,
                      content::SavePackagePathPickedCallback callback) override;
  // The default is empty paths, and SavePackage DCHECK-crashes trying to
  // create a directory at an empty one.
  void GetSaveDir(content::BrowserContext* browser_context,
                  base::FilePath* website_save_dir,
                  base::FilePath* download_save_dir) override;

 private:
  // ui::SelectFileDialog::Listener:
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void OnTargetPathReserved(download::DownloadTargetCallback callback,
                            download::PathValidationResult result,
                            const base::FilePath& target_path);

  uint32_t next_download_id_ = 1;

  bool can_save_as_complete_ = false;
  content::SavePackagePathPickedCallback callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  base::WeakPtrFactory<WegletDownloadManagerDelegate> weak_factory_{this};
};

}  // namespace weglet

#endif  // WEGLET_BROWSER_WEGLET_DOWNLOAD_MANAGER_DELEGATE_H_
