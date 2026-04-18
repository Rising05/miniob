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

  Value real_value = value_;
  if (value_.attr_type() != field_meta->type()) {
    rc = Value::cast_to(value_, field_meta->type(), real_value);
    if (OB_FAIL(rc)) {
      LOG_WARN("failed to cast update value. field=%s, rc=%s", attribute_name_.c_str(), strrc(rc));
      return rc;
    }
  }

  const char *value_ptr = real_value.data();
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

    switch (field_type) {
      case AttrType::CHARS: {
        memset(new_record_data + offset, 0, len);
        const int copy_len = std::min(len, real_value.length());
        if (copy_len > 0) {
          memcpy(new_record_data + offset, value_ptr, copy_len);
        }
      } break;
      case AttrType::INTS:
      case AttrType::FLOATS:
      case AttrType::BOOLEANS:
      case AttrType::DATES: {
        memcpy(new_record_data + offset, value_ptr, len);
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
