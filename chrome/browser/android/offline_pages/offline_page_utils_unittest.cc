// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_utils.h"

#include <stdint.h>
#include <utility>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/offline_pages/client_namespace_constants.h"
#include "components/offline_pages/offline_page_feature.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_test_archiver.h"
#include "components/offline_pages/offline_page_test_store.h"
#include "components/offline_pages/offline_page_types.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

const GURL kTestPage1Url("http://test.org/page1");
const GURL kTestPage2Url("http://test.org/page2");
const GURL kTestPage3Url("http://test.org/page3");
const GURL kTestPage4Url("http://test.org/page4");
const int64_t kTestFileSize = 876543LL;
const char* kTestPage1ClientId = "1234";
const char* kTestPage2ClientId = "5678";
const char* kTestPage4ClientId = "9876";

void BoolCallback(bool* actual_result, bool call_result) {
  DCHECK(actual_result);
  *actual_result = call_result;
}

}  // namespace

class OfflinePageUtilsTest
    : public testing::Test,
      public OfflinePageTestArchiver::Observer,
      public base::SupportsWeakPtr<OfflinePageUtilsTest> {
 public:
  OfflinePageUtilsTest();
  ~OfflinePageUtilsTest() override;

  void SetUp() override;
  void RunUntilIdle();

  // Necessary callbacks for the offline page model.
  void OnSavePageDone(SavePageResult result, int64_t offlineId);
  void OnClearAllDone();
  void OnExpirePageDone(bool success);
  void OnGetURLDone(const GURL& url);

  // OfflinePageTestArchiver::Observer implementation:
  void SetLastPathCreatedByArchiver(const base::FilePath& file_path) override;

  TestingProfile* profile() { return &profile_; }

  int64_t offline_id() const { return offline_id_; }

 private:
  void CreateOfflinePages();
  std::unique_ptr<OfflinePageTestArchiver> BuildArchiver(
      const GURL& url,
      const base::FilePath& file_name);

  content::TestBrowserThreadBundle browser_thread_bundle_;
  int64_t offline_id_;
  GURL url_;
  TestingProfile profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

OfflinePageUtilsTest::OfflinePageUtilsTest() = default;

OfflinePageUtilsTest::~OfflinePageUtilsTest() {}

void OfflinePageUtilsTest::SetUp() {
  // Enables offline pages feature.
  // TODO(jianli): Remove this once the feature is completely enabled.
  scoped_feature_list_.InitAndEnableFeature(
      offline_pages::kOfflineBookmarksFeature);

  // Set up the factory for testing.
  OfflinePageModelFactory::GetInstance()->SetTestingFactoryAndUse(
      &profile_, BuildTestOfflinePageModel);
  RunUntilIdle();

  // Make sure the store contains the right offline pages before the load
  // happens.
  CreateOfflinePages();
}

void OfflinePageUtilsTest::RunUntilIdle() {
  base::RunLoop().RunUntilIdle();
}

void OfflinePageUtilsTest::OnSavePageDone(SavePageResult result,
                                          int64_t offline_id) {
  offline_id_ = offline_id;
}

void OfflinePageUtilsTest::OnExpirePageDone(bool success) {
  // Result ignored here.
}

void OfflinePageUtilsTest::OnClearAllDone() {
  // Result ignored here.
}

void OfflinePageUtilsTest::OnGetURLDone(const GURL& url) {
  url_ = url;
}

void OfflinePageUtilsTest::SetLastPathCreatedByArchiver(
    const base::FilePath& file_path) {}

void OfflinePageUtilsTest::CreateOfflinePages() {
  OfflinePageModel* model =
      OfflinePageModelFactory::GetForBrowserContext(profile());

  // Create page 1.
  std::unique_ptr<OfflinePageTestArchiver> archiver(BuildArchiver(
      kTestPage1Url, base::FilePath(FILE_PATH_LITERAL("page1.mhtml"))));
  offline_pages::ClientId client_id;
  client_id.name_space = kDownloadNamespace;
  client_id.id = kTestPage1ClientId;
  model->SavePage(
      kTestPage1Url, client_id, 0l, std::move(archiver),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();

  // Create page 2.
  archiver = BuildArchiver(kTestPage2Url,
                           base::FilePath(FILE_PATH_LITERAL("page2.mhtml")));
  client_id.id = kTestPage2ClientId;
  model->SavePage(
      kTestPage2Url, client_id, 0l, std::move(archiver),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();

  // Create page 4 - expired page.
  archiver = BuildArchiver(kTestPage4Url,
                           base::FilePath(FILE_PATH_LITERAL("page4.mhtml")));
  client_id.id = kTestPage4ClientId;
  model->SavePage(
      kTestPage4Url, client_id, 0l, std::move(archiver),
      base::Bind(&OfflinePageUtilsTest::OnSavePageDone, AsWeakPtr()));
  RunUntilIdle();
  model->ExpirePages(
      std::vector<int64_t>({offline_id()}), base::Time::Now(),
      base::Bind(&OfflinePageUtilsTest::OnExpirePageDone, AsWeakPtr()));
  RunUntilIdle();
}

std::unique_ptr<OfflinePageTestArchiver> OfflinePageUtilsTest::BuildArchiver(
    const GURL& url,
    const base::FilePath& file_name) {
  std::unique_ptr<OfflinePageTestArchiver> archiver(new OfflinePageTestArchiver(
      this, url, OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
      base::string16(), kTestFileSize, base::ThreadTaskRunnerHandle::Get()));
  archiver->set_filename(file_name);
  return archiver;
}

TEST_F(OfflinePageUtilsTest, CheckExistenceOfPagesWithURL) {
  bool page_exists = false;
  // This page should be available.
  OfflinePageUtils::CheckExistenceOfPagesWithURL(
      profile(), kDownloadNamespace, kTestPage1Url,
      base::Bind(&BoolCallback, base::Unretained(&page_exists)));
  RunUntilIdle();
  EXPECT_TRUE(page_exists);
  // This one should be missing
  OfflinePageUtils::CheckExistenceOfPagesWithURL(
      profile(), kDownloadNamespace, kTestPage3Url,
      base::Bind(&BoolCallback, base::Unretained(&page_exists)));
  RunUntilIdle();
  EXPECT_FALSE(page_exists);
}

}  // namespace offline_pages
