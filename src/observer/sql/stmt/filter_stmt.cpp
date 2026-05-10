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
// Created by Wangyunlai on 2022/5/22.
//

#include "sql/stmt/filter_stmt.h"
#include "common/lang/string.h"
#include "common/log/log.h"
#include "common/sys/rc.h"
#include "sql/parser/expression_binder.h"
#include "storage/db/db.h"
#include "storage/table/table.h"

FilterStmt::~FilterStmt()
{
  for (FilterUnit *unit : filter_units_) {
    delete unit;
  }
  filter_units_.clear();
}

RC FilterStmt::create(Db *db, Table *default_table, unordered_map<string, Table *> *tables,
    const ConditionSqlNode *conditions, int condition_num, FilterStmt *&stmt)
{
  RC rc = RC::SUCCESS;
  stmt  = nullptr;

  FilterStmt *tmp_stmt = new FilterStmt();
  for (int i = 0; i < condition_num; i++) {
    FilterUnit *filter_unit = nullptr;

    rc = create_filter_unit(db, default_table, tables, conditions[i], filter_unit);
    if (rc != RC::SUCCESS) {
      delete tmp_stmt;
      LOG_WARN("failed to create filter unit. condition index=%d", i);
      return rc;
    }
    tmp_stmt->filter_units_.push_back(filter_unit);
  }

  stmt = tmp_stmt;
  return rc;
}

RC get_table_and_field(Db *db, Table *default_table, unordered_map<string, Table *> *tables,
    const RelAttrSqlNode &attr, Table *&table, const FieldMeta *&field)
{
  if (common::is_blank(attr.relation_name.c_str())) {
    table = default_table;
  } else if (nullptr != tables) {
    auto iter = tables->find(attr.relation_name);
    if (iter != tables->end()) {
      table = iter->second;
    }
  } else {
    table = db->find_table(attr.relation_name.c_str());
  }
  if (nullptr == table) {
    LOG_WARN("No such table: attr.relation_name: %s", attr.relation_name.c_str());
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  field = table->table_meta().field(attr.attribute_name.c_str());
  if (nullptr == field) {
    LOG_WARN("no such field in table: table %s, field %s", table->name(), attr.attribute_name.c_str());
    table = nullptr;
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }

  return RC::SUCCESS;
}

RC FilterStmt::create_filter_unit(Db *db, Table *default_table, unordered_map<string, Table *> *tables,
    const ConditionSqlNode &condition, FilterUnit *&filter_unit)
{
  RC rc = RC::SUCCESS;

  CompOp comp = condition.comp;
  if (comp < EQUAL_TO || comp >= NO_OP) {
    LOG_WARN("invalid compare operator : %d", comp);
    return RC::INVALID_ARGUMENT;
  }

  filter_unit = new FilterUnit;

  BinderContext binder_context;
  if (tables != nullptr) {
    for (auto &table_entry : *tables) {
      binder_context.add_table(table_entry.second);
    }
  } else if (default_table != nullptr) {
    binder_context.add_table(default_table);
  }

  ExpressionBinder expression_binder(binder_context);
  vector<unique_ptr<Expression>> bound_expressions;

  unique_ptr<Expression> left_expr;
  if (condition.left_expr) {
    left_expr = condition.left_expr->copy();
  } else if (condition.left_is_attr) {
    left_expr = make_unique<UnboundFieldExpr>(condition.left_attr.relation_name, condition.left_attr.attribute_name);
  } else {
    left_expr = make_unique<ValueExpr>(condition.left_value);
  }
  rc = expression_binder.bind_expression(left_expr, bound_expressions);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to bind left condition expression. rc=%s", strrc(rc));
    delete filter_unit;
    filter_unit = nullptr;
    return rc;
  }
  if (bound_expressions.size() != 1) {
    delete filter_unit;
    filter_unit = nullptr;
    return RC::INVALID_ARGUMENT;
  }
  filter_unit->set_left(std::move(bound_expressions[0]));

  bound_expressions.clear();
  unique_ptr<Expression> right_expr;
  if (condition.right_expr) {
    right_expr = condition.right_expr->copy();
  } else if (condition.right_is_attr) {
    right_expr = make_unique<UnboundFieldExpr>(condition.right_attr.relation_name, condition.right_attr.attribute_name);
  } else {
    right_expr = make_unique<ValueExpr>(condition.right_value);
  }
  rc = expression_binder.bind_expression(right_expr, bound_expressions);
  if (OB_FAIL(rc)) {
    LOG_WARN("failed to bind right condition expression. rc=%s", strrc(rc));
    delete filter_unit;
    filter_unit = nullptr;
    return rc;
  }
  if (bound_expressions.size() != 1) {
    delete filter_unit;
    filter_unit = nullptr;
    return RC::INVALID_ARGUMENT;
  }
  filter_unit->set_right(std::move(bound_expressions[0]));

  filter_unit->set_comp(comp);

  // 检查两个类型是否能够比较
  return rc;
}
