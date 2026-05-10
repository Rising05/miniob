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
// Created by Meiyi
//

#include "sql/parser/parse.h"
#include "common/log/log.h"
#include "sql/expr/expression.h"

RC parse(char *st, ParsedSqlNode *sqln);

ParsedSqlNode::ParsedSqlNode() : flag(SCF_ERROR) {}

ParsedSqlNode::ParsedSqlNode(SqlCommandFlag _flag) : flag(_flag) {}

ConditionSqlNode::ConditionSqlNode()
    : left_is_attr(0), comp(NO_OP), right_is_attr(0)
{}

ConditionSqlNode::ConditionSqlNode(const ConditionSqlNode &other)
    : left_is_attr(other.left_is_attr),
      left_value(other.left_value),
      left_attr(other.left_attr),
      comp(other.comp),
      right_is_attr(other.right_is_attr),
      right_attr(other.right_attr),
      right_value(other.right_value)
{
  if (other.left_expr) {
    left_expr = other.left_expr->copy();
  }
  if (other.right_expr) {
    right_expr = other.right_expr->copy();
  }
}

ConditionSqlNode::ConditionSqlNode(ConditionSqlNode &&other) noexcept = default;

ConditionSqlNode::~ConditionSqlNode() = default;

ConditionSqlNode &ConditionSqlNode::operator=(const ConditionSqlNode &other)
{
  if (this == &other) {
    return *this;
  }

  left_is_attr  = other.left_is_attr;
  left_value    = other.left_value;
  left_attr     = other.left_attr;
  comp          = other.comp;
  right_is_attr = other.right_is_attr;
  right_attr    = other.right_attr;
  right_value   = other.right_value;
  left_expr     = other.left_expr ? other.left_expr->copy() : nullptr;
  right_expr    = other.right_expr ? other.right_expr->copy() : nullptr;
  return *this;
}

ConditionSqlNode &ConditionSqlNode::operator=(ConditionSqlNode &&other) noexcept = default;

void ParsedSqlResult::add_sql_node(unique_ptr<ParsedSqlNode> sql_node)
{
  sql_nodes_.emplace_back(std::move(sql_node));
}

////////////////////////////////////////////////////////////////////////////////

int sql_parse(const char *st, ParsedSqlResult *sql_result);

RC parse(const char *st, ParsedSqlResult *sql_result)
{
  sql_parse(st, sql_result);
  return RC::SUCCESS;
}
