/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <memory>
#include <string>
#include <vector>

#include "sql/expr/expression.h"
#include "sql/parser/parse.h"
#include "gtest/gtest.h"

using namespace std;

TEST(ParserTest, DISABLED_aggregation_test)
{
  ParsedSqlResult result;
  {
    const char *sql = "select sum(a), sum(b) from tab";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select sum(a+b*c+d/e-f*(g-h)) from tab";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
}

TEST(ParserTest, DISABLED_group_by_test)
{
  ParsedSqlResult result;
  {
    const char *sql = "select a, sum(b) from tab group by a";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select sum(a+b*c+d/e-f*(g+1)) from tab group by c+d-e";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select a,b,sum(a+b*c+d/e-f*(g+1)),e,f,d from tab group by c+d-e";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select a,b,sum(a+b*c+d/e-f*(g+1)),e,f,d from tab where a>1 and b< 10 and c='abc' group by c+d-e";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select t.a,s.c,sum(u.rr) from t,s,u where t.a<10 and t.b!='vfdv' and u.c='abc' group by s.c+u.q";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select a+a,sum(b+b)+sum(b) from tab group by a";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
  {
    const char *sql = "select a+a,sum(b+b)+sum(b) from tab group by a+a";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  }
}

TEST(ParserTest, join_tables_test)
{
  {
    ParsedSqlResult result;
    const char     *sql = "select * from t inner join t1 on t.id = t1.id, t2 where t2.id = t.id";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
    ASSERT_EQ(result.sql_nodes().size(), 1);

    ParsedSqlNode *node = result.sql_nodes().front().get();
    ASSERT_EQ(node->flag, SCF_SELECT);
    ASSERT_EQ(node->selection.relations.size(), 3);
    EXPECT_EQ(node->selection.relations[0], "t");
    EXPECT_EQ(node->selection.relations[1], "t1");
    EXPECT_EQ(node->selection.relations[2], "t2");
    ASSERT_EQ(node->selection.conditions.size(), 2);
  }

  {
    ParsedSqlResult result;
    const char *sql = "select t.id,t1.id,t2.id from t inner join t1 on t.id = t1.id and t.age = t1.age "
                      "inner join t2 on t2.id = t.id";
    ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
    ASSERT_EQ(result.sql_nodes().size(), 1);

    ParsedSqlNode *node = result.sql_nodes().front().get();
    ASSERT_EQ(node->flag, SCF_SELECT);
    ASSERT_EQ(node->selection.relations.size(), 3);
    EXPECT_EQ(node->selection.relations[0], "t");
    EXPECT_EQ(node->selection.relations[1], "t1");
    EXPECT_EQ(node->selection.relations[2], "t2");
    ASSERT_EQ(node->selection.conditions.size(), 3);
  }
}

TEST(ParserTest, expression_condition_and_scalar_function_test)
{
  ParsedSqlResult result;
  const char *sql = "select id, length(name), round(score), date_format(u_date, '%d,%M,%Y') "
                    "from function_table where 5 + score < id + 6";
  ASSERT_EQ(parse(sql, &result), RC::SUCCESS);
  ASSERT_EQ(result.sql_nodes().size(), 1);

  ParsedSqlNode *node = result.sql_nodes().front().get();
  ASSERT_EQ(node->flag, SCF_SELECT);
  ASSERT_EQ(node->selection.expressions.size(), 4);
  ASSERT_EQ(node->selection.conditions.size(), 1);
  ASSERT_NE(node->selection.conditions[0].left_expr, nullptr);
  ASSERT_NE(node->selection.conditions[0].right_expr, nullptr);
}

int main(int argc, char **argv)
{

  // 分析gtest程序的命令行参数
  testing::InitGoogleTest(&argc, argv);

  // 调用RUN_ALL_TESTS()运行所有测试用例
  // main函数返回RUN_ALL_TESTS()的运行结果
  return RUN_ALL_TESTS();
}
