/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <cctype>
#include <cstdlib>
#include <stdio.h>

#include "common/log/log.h"
#include "common/type/date_type.h"
#include "common/value.h"

namespace {

bool is_leap_year(int year)
{
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(int year, int month)
{
  static constexpr int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && is_leap_year(year)) {
    return 29;
  }
  return days[month];
}

bool is_all_digits(const string &str)
{
  if (str.empty()) {
    return false;
  }

  for (char ch : str) {
    if (!std::isdigit(static_cast<unsigned char>(ch))) {
      return false;
    }
  }
  return true;
}

}  // namespace

int DateType::compare(const Value &left, const Value &right) const
{
  ASSERT(left.attr_type() == AttrType::DATES, "left type is not date");
  ASSERT(right.attr_type() == AttrType::DATES, "right type is not date");
  if (left.value_.int_value_ < right.value_.int_value_) {
    return -1;
  }
  if (left.value_.int_value_ > right.value_.int_value_) {
    return 1;
  }
  return 0;
}

RC DateType::cast_to(const Value &val, AttrType type, Value &result) const
{
  switch (type) {
    case AttrType::CHARS:
    case AttrType::TEXTS: {
      const string formatted = format_date_string(val.value_.int_value_);
      if (type == AttrType::TEXTS) {
        result.set_text(formatted.c_str(), static_cast<int>(formatted.size()));
      } else {
        result.set_string(formatted.c_str(), static_cast<int>(formatted.size()));
      }
      return RC::SUCCESS;
    }
    default: return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }
}

RC DateType::set_value_from_str(Value &val, const string &data) const
{
  int date_value = 0;
  RC  rc         = parse_date_string(data, date_value);
  if (OB_FAIL(rc)) {
    return rc;
  }
  val.set_date(date_value);
  return RC::SUCCESS;
}

RC DateType::to_string(const Value &val, string &result) const
{
  result = format_date_string(val.value_.int_value_);
  return RC::SUCCESS;
}

RC DateType::parse_date_string(const string &str, int &date_value)
{
  size_t first_dash = str.find('-');
  size_t second_dash = str.find('-', first_dash == string::npos ? 0 : first_dash + 1);
  if (first_dash == string::npos || second_dash == string::npos || str.find('-', second_dash + 1) != string::npos) {
    LOG_WARN("invalid date format: %s", str.c_str());
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  const string year_str  = str.substr(0, first_dash);
  const string month_str = str.substr(first_dash + 1, second_dash - first_dash - 1);
  const string day_str   = str.substr(second_dash + 1);
  if (!is_all_digits(year_str) || !is_all_digits(month_str) || !is_all_digits(day_str)) {
    LOG_WARN("invalid date component: %s", str.c_str());
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  int year  = atoi(year_str.c_str());
  int month = atoi(month_str.c_str());
  int day   = atoi(day_str.c_str());

  if (year <= 0 || year > 9999 || month <= 0 || month > 12) {
    LOG_WARN("invalid date value: %s", str.c_str());
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  const int max_day = days_in_month(year, month);
  if (day <= 0 || day > max_day) {
    LOG_WARN("invalid date day: %s", str.c_str());
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  date_value = year * 10000 + month * 100 + day;
  return RC::SUCCESS;
}

string DateType::format_date_string(int date_value)
{
  const int year  = date_value / 10000;
  const int month = (date_value / 100) % 100;
  const int day   = date_value % 100;

  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
  return buffer;
}
