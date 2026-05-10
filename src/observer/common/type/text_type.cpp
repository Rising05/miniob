/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "common/lang/comparator.h"
#include "common/log/log.h"
#include "common/type/text_type.h"
#include "common/value.h"

int TextType::compare(const Value &left, const Value &right) const
{
  if (left.data() == nullptr || right.data() == nullptr) {
    return left.data() == right.data() ? 0 : (left.data() < right.data() ? -1 : 1);
  }
  int min_len = std::min(left.length(), right.length());
  int cmp = memcmp(left.data(), right.data(), min_len);
  if (cmp != 0) return cmp;
  if (left.length() != right.length()) {
    return left.length() < right.length() ? -1 : 1;
  }
  return 0;
}

RC TextType::set_value_from_str(Value &val, const string &data) const
{
  val.set_text(data.c_str(), data.length());
  return RC::SUCCESS;
}

RC TextType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::CHARS: {
      if (val.data() != nullptr && val.length() > 0) {
        result.set_string(val.data(), val.length());
      } else {
        result.set_string("", 0);
      }
      return RC::SUCCESS;
    }
    case AttrType::TEXTS: {
      result.set_text(val.data(), val.length());
      return RC::SUCCESS;
    }
    default:
      return RC::UNIMPLEMENTED;
  }
}

int TextType::cast_cost(AttrType type)
{
  if (type == AttrType::TEXTS) {
    return 0;
  }
  if (type == AttrType::CHARS) {
    return 1;
  }
  return INT32_MAX;
}

RC TextType::to_string(const Value &val, string &result) const
{
  if (val.data() != nullptr && val.length() > 0) {
    result.assign(val.data(), val.length());
  } else {
    result = "";
  }
  return RC::SUCCESS;
}
