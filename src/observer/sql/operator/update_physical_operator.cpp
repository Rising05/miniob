/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
          http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by WangYunlai on 2022/6/9.
//

#include "sql/operator/update_physical_operator.h"
#include <algorithm>
#include <string>
#include "common/log/log.h"
#include "storage/table/table.h"
#include "storage/trx/trx.h"
#include "common/value.h"

UpdatePhysicalOperator::UpdatePhysicalOperator(Table *table, const char *attribute_name, const Value &value)
    : table_(table), attribute_name_(attribute_name), value_(value)
{}

RC UpdatePhysicalOperator::open(Trx *trx)
{
  if (children_.empty()) {
    return RC::SUCCESS;
  }

  unique_ptr<PhysicalOperator> &child = children_[0];

  RC rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  trx_ = trx;

  const TableMeta &table_meta = table_->table_meta();
  const FieldMeta *field_meta = table_meta.field(attribute_name_.c_str());
  if (nullptr == field_meta) {
    LOG_WARN("field not found: %s", attribute_name_.c_str());
    child->close();
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  vector<Record> records;
  while (OB_SUCC(rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      child->close();
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    records.emplace_back(row_tuple->record());
  }

  if (rc != RC::RECORD_EOF && rc != RC::SUCCESS) {
    child->close();
    return rc;
  }

  child->close();

  const char *value_ptr = value_.data();
  if (value_ptr == nullptr) {
    return RC::INTERNAL;
  }

  for (Record &old_record : records) {
    const char *old_data    = old_record.data();
    const int   record_len  = old_record.len();
    char       *new_record_data = (char *)malloc(record_len);
    if (new_record_data == nullptr) {
      return RC::INTERNAL;
    }
    memcpy(new_record_data, old_data, record_len);

    const int offset = field_meta->offset();
    const int len    = field_meta->len();
    const AttrType field_type = field_meta->type();
    auto safe_int_value = [&]() -> int {
      if (value_.attr_type() != AttrType::CHARS) {
        return value_.get_int();
      }
      try {
        return std::stoi(std::string(value_ptr, value_.length()));
      } catch (...) {
        return 0;
      }
    };
    auto safe_float_value = [&]() -> float {
      if (value_.attr_type() != AttrType::CHARS) {
        return value_.get_float();
      }
      try {
        return std::stof(std::string(value_ptr, value_.length()));
      } catch (...) {
        return 0.0f;
      }
    };
    auto safe_bool_value = [&]() -> bool {
      if (value_.attr_type() != AttrType::CHARS) {
        return value_.get_boolean();
      }
      const std::string str_value(value_ptr, value_.length());
      if (str_value == "0" || str_value == "false" || str_value == "FALSE") {
        return false;
      }
      return !str_value.empty();
    };

    switch (field_type) {
      case AttrType::INTS: {
        int int_value = safe_int_value();
        memcpy(new_record_data + offset, &int_value, sizeof(int));
      } break;
      case AttrType::FLOATS: {
        float float_value = safe_float_value();
        memcpy(new_record_data + offset, &float_value, sizeof(float));
      } break;
      case AttrType::BOOLEANS: {
        bool bool_value = safe_bool_value();
        memcpy(new_record_data + offset, &bool_value, sizeof(bool));
      } break;
      case AttrType::CHARS: {
        memset(new_record_data + offset, 0, len);
        const int copy_len = std::min(len, value_.length());
        if (copy_len > 0) {
          memcpy(new_record_data + offset, value_ptr, copy_len);
        }
      } break;
      default: {
        LOG_WARN("unsupported update field type. field=%s, type=%d", attribute_name_.c_str(), static_cast<int>(field_type));
        free(new_record_data);
        return RC::UNSUPPORTED;
      }
    }

    Record new_record;
    new_record.set_data_owner(new_record_data, record_len);
    new_record.rid() = old_record.rid();

    rc = trx_->update_record(table_, old_record, new_record);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to update record: %s", strrc(rc));
      return rc;
    }
  }

  return RC::SUCCESS;
}

RC UpdatePhysicalOperator::next()
{
  return RC::RECORD_EOF;
}

RC UpdatePhysicalOperator::close()
{
  return RC::SUCCESS;
}