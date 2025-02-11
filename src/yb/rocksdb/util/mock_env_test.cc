// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// The following only applies to changes made to this file as part of YugaByte development.
//
// Portions Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "yb/rocksdb/util/mock_env.h"

#include "yb/util/status.h"
#include "yb/util/test_macros.h"

namespace rocksdb {

class MockEnvTest : public testing::Test {
 public:
  MockEnv* env_;
  const EnvOptions soptions_;

  MockEnvTest()
      : env_(new MockEnv(Env::Default())) {
  }
  ~MockEnvTest() {
    delete env_;
  }
};

TEST_F(MockEnvTest, Basics) {
  uint64_t file_size;
  unique_ptr<WritableFile> writable_file;
  std::vector<std::string> children;

  ASSERT_OK(env_->CreateDir("/dir"));

  // Check that the directory is empty.
  ASSERT_TRUE(env_->FileExists("/dir/non_existent").IsNotFound());
  ASSERT_TRUE(!env_->GetFileSize("/dir/non_existent", &file_size).ok());
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0U, children.size());

  // Create a file.
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file, soptions_));
  writable_file.reset();

  // Check that the file exists.
  ASSERT_OK(env_->FileExists("/dir/f"));
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(0U, file_size);
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(1U, children.size());
  ASSERT_EQ("f", children[0]);

  // Write to the file.
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file, soptions_));
  ASSERT_OK(writable_file->Append("abc"));
  writable_file.reset();

  // Check for expected size.
  ASSERT_OK(env_->GetFileSize("/dir/f", &file_size));
  ASSERT_EQ(3U, file_size);

  // Check that renaming works.
  ASSERT_TRUE(!env_->RenameFile("/dir/non_existent", "/dir/g").ok());
  ASSERT_OK(env_->RenameFile("/dir/f", "/dir/g"));
  ASSERT_TRUE(env_->FileExists("/dir/f").IsNotFound());
  ASSERT_OK(env_->FileExists("/dir/g"));
  ASSERT_OK(env_->GetFileSize("/dir/g", &file_size));
  ASSERT_EQ(3U, file_size);

  // Check that opening non-existent file fails.
  unique_ptr<SequentialFile> seq_file;
  unique_ptr<RandomAccessFile> rand_file;
  ASSERT_TRUE(!env_->NewSequentialFile("/dir/non_existent", &seq_file,
                                       soptions_).ok());
  ASSERT_TRUE(!seq_file);
  ASSERT_TRUE(!env_->NewRandomAccessFile("/dir/non_existent", &rand_file,
                                         soptions_).ok());
  ASSERT_TRUE(!rand_file);

  // Check that deleting works.
  ASSERT_TRUE(!env_->DeleteFile("/dir/non_existent").ok());
  ASSERT_OK(env_->DeleteFile("/dir/g"));
  ASSERT_TRUE(env_->FileExists("/dir/g").IsNotFound());
  ASSERT_OK(env_->GetChildren("/dir", &children));
  ASSERT_EQ(0U, children.size());
  ASSERT_OK(env_->DeleteDir("/dir"));
}

TEST_F(MockEnvTest, ReadWrite) {
  unique_ptr<WritableFile> writable_file;
  unique_ptr<SequentialFile> seq_file;
  unique_ptr<RandomAccessFile> rand_file;
  Slice result;
  uint8_t scratch[100];

  ASSERT_OK(env_->CreateDir("/dir"));

  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file, soptions_));
  ASSERT_OK(writable_file->Append("hello "));
  ASSERT_OK(writable_file->Append("world"));
  writable_file.reset();

  // Read sequentially.
  ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file, soptions_));
  ASSERT_OK(seq_file->Read(5, &result, scratch));  // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(seq_file->Skip(1));
  ASSERT_OK(seq_file->Read(1000, &result, scratch));  // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_OK(seq_file->Read(1000, &result, scratch));  // Try reading past EOF.
  ASSERT_EQ(0U, result.size());
  ASSERT_OK(seq_file->Skip(100));  // Try to skip past end of file.
  ASSERT_OK(seq_file->Read(1000, &result, scratch));
  ASSERT_EQ(0U, result.size());

  // Random reads.
  ASSERT_OK(env_->NewRandomAccessFile("/dir/f", &rand_file, soptions_));
  ASSERT_OK(rand_file->Read(6, 5, &result, scratch));  // Read "world".
  ASSERT_EQ(0, result.compare("world"));
  ASSERT_OK(rand_file->Read(0, 5, &result, scratch));  // Read "hello".
  ASSERT_EQ(0, result.compare("hello"));
  ASSERT_OK(rand_file->Read(10, 100, &result, scratch));  // Read "d".
  ASSERT_EQ(0, result.compare("d"));

  // Too high offset.
  ASSERT_TRUE(!rand_file->Read(1000, 5, &result, scratch).ok());
}

TEST_F(MockEnvTest, Locks) {
  FileLock* lock;

  // These are no-ops, but we test they return success.
  ASSERT_OK(env_->LockFile("some file", &lock));
  ASSERT_OK(env_->UnlockFile(lock));
}

TEST_F(MockEnvTest, Misc) {
  std::string test_dir;
  ASSERT_OK(env_->GetTestDirectory(&test_dir));
  ASSERT_TRUE(!test_dir.empty());

  unique_ptr<WritableFile> writable_file;
  ASSERT_OK(env_->NewWritableFile("/a/b", &writable_file, soptions_));

  // These are no-ops, but we test they return success.
  ASSERT_OK(writable_file->Sync());
  ASSERT_OK(writable_file->Flush());
  ASSERT_OK(writable_file->Close());
  writable_file.reset();
}

TEST_F(MockEnvTest, LargeWrite) {
  const size_t kWriteSize = 300 * 1024;
  uint8_t* scratch = new uint8_t[kWriteSize * 2];

  std::string write_data;
  for (size_t i = 0; i < kWriteSize; ++i) {
    write_data.append(1, static_cast<char>(i));
  }

  unique_ptr<WritableFile> writable_file;
  ASSERT_OK(env_->NewWritableFile("/dir/f", &writable_file, soptions_));
  ASSERT_OK(writable_file->Append("foo"));
  ASSERT_OK(writable_file->Append(write_data));
  writable_file.reset();

  unique_ptr<SequentialFile> seq_file;
  Slice result;
  ASSERT_OK(env_->NewSequentialFile("/dir/f", &seq_file, soptions_));
  ASSERT_OK(seq_file->Read(3, &result, scratch));  // Read "foo".
  ASSERT_EQ(0, result.compare("foo"));

  size_t read = 0;
  std::string read_data;
  while (read < kWriteSize) {
    ASSERT_OK(seq_file->Read(kWriteSize - read, &result, scratch));
    read_data.append(result.cdata(), result.size());
    read += result.size();
  }
  ASSERT_TRUE(write_data == read_data);
  delete [] scratch;
}

TEST_F(MockEnvTest, Corrupt) {
  const std::string kGood = "this is a good string, synced to disk";
  const std::string kCorrupted = "this part may be corrupted";
  const std::string kFileName = "/dir/f";
  unique_ptr<WritableFile> writable_file;
  ASSERT_OK(env_->NewWritableFile(kFileName, &writable_file, soptions_));
  ASSERT_OK(writable_file->Append(kGood));
  ASSERT_TRUE(writable_file->GetFileSize() == kGood.size());

  std::string scratch;
  scratch.resize(kGood.size() + kCorrupted.size() + 16);
  uint8_t* read_buf = reinterpret_cast<uint8_t*>(&(scratch[0]));
  Slice result;
  unique_ptr<RandomAccessFile> rand_file;
  ASSERT_OK(env_->NewRandomAccessFile(kFileName, &rand_file, soptions_));
  ASSERT_OK(rand_file->Read(0, kGood.size(), &result, read_buf));
  ASSERT_EQ(result.compare(kGood), 0);

  // Sync + corrupt => no change
  ASSERT_OK(writable_file->Fsync());
  ASSERT_OK(dynamic_cast<MockEnv*>(env_)->CorruptBuffer(kFileName));
  result.clear();
  ASSERT_OK(rand_file->Read(0, kGood.size(), &result, read_buf));
  ASSERT_EQ(result.compare(kGood), 0);

  // Add new data and corrupt it
  ASSERT_OK(writable_file->Append(kCorrupted));
  ASSERT_TRUE(writable_file->GetFileSize() == kGood.size() + kCorrupted.size());
  result.clear();
  ASSERT_OK(rand_file->Read(kGood.size(), kCorrupted.size(), &result, read_buf));
  ASSERT_EQ(result.compare(kCorrupted), 0);
  // Corrupted
  ASSERT_OK(dynamic_cast<MockEnv*>(env_)->CorruptBuffer(kFileName));
  result.clear();
  ASSERT_OK(rand_file->Read(kGood.size(), kCorrupted.size(), &result, read_buf));
  ASSERT_NE(result.compare(kCorrupted), 0);
}

TEST_F(MockEnvTest, FakeSleeping) {
  int64_t now = 0;
  auto s = env_->GetCurrentTime(&now);
  ASSERT_OK(s);
  env_->FakeSleepForMicroseconds(3 * 1000 * 1000);
  int64_t after_sleep = 0;
  s = env_->GetCurrentTime(&after_sleep);
  ASSERT_OK(s);
  auto delta = after_sleep - now;
  // this will be true unless test runs for 2 seconds
  ASSERT_TRUE(delta == 3 || delta == 4);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
