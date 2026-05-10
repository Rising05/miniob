/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "storage/record/lob_handler.h"

RC LobFileHandler::create_file(const char *file_name)
{
  return file_.create_file(file_name);
}

RC LobFileHandler::open_file(const char *file_name)
{
  std::ifstream file(file_name);
  if (file.good()) {
    return file_.open_file(file_name);
  } else {
    return RC::FILE_NOT_EXIST;
  }
  return RC::INTERNAL;
}

RC LobFileHandler::insert_data(int64_t &offset, int64_t length, const char *data)
{
  RC       rc         = RC::SUCCESS;
  int64_t  out_size   = 0;
  int64_t end_offset = 0;
  rc                  = file_.append(length, data, &out_size, &end_offset);
  if (OB_FAIL(rc)) {
    return rc;
  }
  if (out_size != length) {
    return RC::IOERR_WRITE;
  }
  offset = end_offset;

  return rc;
}

RC LobFileHandler::write_text_locator(char *locator_data, const char *data, int length)
{
  if (locator_data == nullptr || length < 0 || length > TEXT_MAX_LENGTH || (length > 0 && data == nullptr)) {
    return RC::INVALID_ARGUMENT;
  }

  LobLocator locator;
  locator.length = length;
  if (length > 0) {
    RC rc = insert_data(locator.offset, length, data);
    if (OB_FAIL(rc)) {
      return rc;
    }
  }

  memcpy(locator_data, &locator, sizeof(locator));
  return RC::SUCCESS;
}

RC LobFileHandler::read_text_locator(const char *locator_data, string &data)
{
  if (locator_data == nullptr) {
    return RC::INVALID_ARGUMENT;
  }

  LobLocator locator;
  memcpy(&locator, locator_data, sizeof(locator));
  if (locator.length < 0 || locator.length > TEXT_MAX_LENGTH) {
    return RC::INVALID_ARGUMENT;
  }

  data.clear();
  if (locator.length == 0) {
    return RC::SUCCESS;
  }

  data.resize(locator.length);
  return get_data(locator.offset, locator.length, data.data());
}
