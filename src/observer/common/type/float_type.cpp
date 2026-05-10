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
#include "common/lang/sstream.h"
#include "common/log/log.h"
#include "common/type/float_type.h"
#include "common/value.h"
#include "common/lang/limits.h"
#include "common/value.h"
#include "storage/common/column.h"

int FloatType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::FLOATS, "left type is not float");
  ASSERT(right.attr_type() == AttrType::INTS || right.attr_type() == AttrType::FLOATS, "right type is not numeric");
  float left_val  = left.get_float();
  float right_val = right.get_float();
  return common::compare_float((void *)&left_val, (void *)&right_val);
}

int FloatType::compare(const Column &left, const Column &right, int left_idx, int right_idx) const
{
  ASSERT(left.attr_type() == AttrType::FLOATS, "left type is not float");
  ASSERT(right.attr_type() == AttrType::FLOATS, "right type is not float");
  return common::compare_float((void *)&((float*)left.data())[left_idx],
      (void *)&((float*)right.data())[right_idx]);
}

static bool is_float_null(float val)
{
  return val >= numeric_limits<float>::max() - 1;
}

RC FloatType::add(const Value &left, const Value &right, Value &result) const
{
  float left_val  = left.get_float();
  float right_val = right.get_float();
  if (is_float_null(left_val) || is_float_null(right_val)) {
    result.set_float(numeric_limits<float>::max());
  } else {
    result.set_float(left_val + right_val);
  }
  return RC::SUCCESS;
}
RC FloatType::subtract(const Value &left, const Value &right, Value &result) const
{
  float left_val  = left.get_float();
  float right_val = right.get_float();
  if (is_float_null(left_val) || is_float_null(right_val)) {
    result.set_float(numeric_limits<float>::max());
  } else {
    result.set_float(left_val - right_val);
  }
  return RC::SUCCESS;
}
RC FloatType::multiply(const Value &left, const Value &right, Value &result) const
{
  float left_val  = left.get_float();
  float right_val = right.get_float();
  if (is_float_null(left_val) || is_float_null(right_val)) {
    result.set_float(numeric_limits<float>::max());
  } else {
    result.set_float(left_val * right_val);
  }
  return RC::SUCCESS;
}

RC FloatType::divide(const Value &left, const Value &right, Value &result) const
{
  float left_val  = left.get_float();
  float right_val = right.get_float();
  if (is_float_null(left_val) || is_float_null(right_val)) {
    result.set_float(numeric_limits<float>::max());
  } else if (right_val > -EPSILON && right_val < EPSILON) {
    result.set_float(numeric_limits<float>::max());
  } else {
    result.set_float(left_val / right_val);
  }
  return RC::SUCCESS;
}

RC FloatType::negative(const Value &val, Value &result) const
{
  float val_val = val.get_float();
  if (is_float_null(val_val)) {
    result.set_float(numeric_limits<float>::max());
  } else {
    result.set_float(-val_val);
  }
  return RC::SUCCESS;
}

RC FloatType::set_value_from_str(Value &val, const string &data) const
{
  RC                rc = RC::SUCCESS;
  stringstream deserialize_stream;
  deserialize_stream.clear();
  deserialize_stream.str(data);

  float float_value;
  deserialize_stream >> float_value;
  if (!deserialize_stream || !deserialize_stream.eof()) {
    rc = RC::SCHEMA_FIELD_TYPE_MISMATCH;
  } else {
    val.set_float(float_value);
  }
  return rc;
}

RC FloatType::to_string(const Value &val, string &result) const
{
  stringstream ss;
  ss << common::double_to_str(val.value_.float_value_);
  result = ss.str();
  return RC::SUCCESS;
}
