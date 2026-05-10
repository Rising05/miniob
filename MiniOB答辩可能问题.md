# MiniOB 数据库内核实现答辩可能问题

本文根据实验报告内容和项目源码整理，重点覆盖老师可能追问的设计思路、实现路径、关键代码、边界问题和不足之处。

## 一、项目整体

### 1. 你们这个项目主要做了哪些功能？

主要基于 OceanBase MiniOB 数据库内核做功能扩展，涉及以下模块：

- `DROP TABLE`
- 多列索引
- `TEXT` 数据类型
- Record Manager 记录管理
- Buffer Pool 页面缓存与脏页管理

整体修改覆盖 SQL 解析层、Stmt 层、Executor 层、表管理、索引模块、记录管理和缓冲池。

### 2. 一条 SQL 从输入到执行大概经过哪些阶段？

大致流程是：

```text
SQL 输入 -> 词法/语法解析 -> ParsedSqlNode -> Stmt -> Executor/Optimizer -> PhysicalOperator -> Storage
```

例如 `DROP TABLE t`：

```text
yacc_sql.y 解析 -> DropTableStmt -> DropTableExecutor -> Db::drop_table
```

### 3. MiniOB 的分层结构是什么？

可以概括为四层：

- SQL 层：负责解析 SQL，生成语法树和语句对象。
- 执行器层：负责根据语句类型调度执行逻辑。
- 存储引擎层：负责表、记录、索引等数据组织。
- Buffer Pool 层：负责页面缓存、读写、淘汰和刷盘。

## 二、DROP TABLE

### 4. `DROP TABLE` 是怎么实现的？

执行链路：

```text
DROP TABLE SQL
-> yacc_sql.y 生成 SCF_DROP_TABLE
-> DropTableStmt 保存表名
-> DropTableExecutor 调用 session->get_current_db()->drop_table(table_name)
-> Db::drop_table 删除内存表对象和磁盘文件
```

关键点：

- 先从 `opened_tables_` 查找表是否存在。
- 找不到返回 `RC::SCHEMA_TABLE_NOT_EXIST`。
- 找到后先从 map 中移除。
- `delete table` 释放表对象和文件句柄。
- 删除 `.table`、`.data`、`.lob` 文件。

相关源码：

- `src/observer/sql/parser/yacc_sql.y`
- `src/observer/sql/executor/drop_table_executor.cpp`
- `src/observer/storage/db/db.cpp`

### 5. 为什么删除文件前要先 `delete table`？

因为 `Table` 对象析构时会释放内部资源，特别是关闭数据文件、索引对象、Buffer Pool 关联文件等句柄。如果不先释放，直接删除磁盘文件可能出现文件占用或资源未同步的问题。

### 6. 如果删除不存在的表会怎样？

`Db::drop_table` 会先查 `opened_tables_`：

```cpp
auto iter = opened_tables_.find(table_name);
if (iter == opened_tables_.end()) {
  return RC::SCHEMA_TABLE_NOT_EXIST;
}
```

所以会返回表不存在错误。

### 7. `DROP TABLE` 当前还有什么不足？

需要如实说明：

- 当前主要删除 `.table`、`.data`、`.lob` 文件。
- 索引文件是否完整删除需要进一步完善。
- 没有事务回滚支持，执行后不可撤销。
- 并发场景下如果其他线程正在访问表，还需要更严格的锁和引用计数保护。

## 三、多列索引

### 8. 多列索引的语法是怎么支持的？

语法支持：

```sql
CREATE INDEX idx ON t(a, b);
```

在 `yacc_sql.y` 中通过 `rel_attr_list` 收集多个字段名，然后存入：

```cpp
CreateIndexSqlNode::attribute_names
```

### 9. 多列索引的元数据怎么保存？

原来索引只记录单个字段，现在扩展为：

```cpp
vector<string> fields_;
```

序列化时写入 `field_names` 数组，反序列化时再根据字段名恢复字段元数据。

相关源码：

- `src/observer/storage/index/index_meta.h`
- `src/observer/storage/index/index_meta.cpp`

### 10. 多列索引的 key 是怎么构造的？

按索引字段顺序，把每个字段在 record 中的二进制内容拼接成 composite key：

```cpp
char composite_key[1024];
int offset = 0;
for (const FieldMeta *field : field_metas_) {
  memcpy(composite_key + offset, record + field->offset(), field->len());
  offset += field->len();
}
```

然后将拼接后的 key 插入 B+ 树。

### 11. 多列索引为什么要按字段顺序比较？

B+ 树要求 key 有全序关系。多列索引一般使用字典序：

```text
先比较第一列
第一列相等再比较第二列
第二列相等再比较第三列
直到比较出大小或全部相等
```

这也是最左前缀原则的基础。

### 12. 什么是最左前缀原则？

例如索引是：

```sql
CREATE INDEX idx ON t(a, b, c);
```

可以有效利用索引的查询：

```sql
WHERE a = 1
WHERE a = 1 AND b = 2
WHERE a = 1 AND b = 2 AND c = 3
```

不能很好利用该索引的查询：

```sql
WHERE b = 2
WHERE c = 3
```

因为 B+ 树是先按 `a` 排序，再按 `b`，最后按 `c`。

### 13. 当前项目的多列索引是否完整支持查询优化？

建议谨慎回答：

当前项目完成了多列索引的创建、元数据保存、索引项插入和删除维护。但查询优化阶段还比较基础，源码中主要是根据单个等值条件查找索引，没有完整构造多列联合扫描边界。因此，完整的最左前缀匹配、联合范围扫描、索引下推仍是后续优化方向。

## 四、TEXT 类型

### 14. `TEXT` 类型从语法层怎么接入？

词法层识别 `TEXT`，语法层映射到：

```cpp
AttrType::TEXTS
```

在 `CREATE TABLE` 字段定义中，如果是 `TEXT` 且没有显式长度，默认长度设置为 4096。

相关源码：

- `src/observer/sql/parser/lex_sql.l`
- `src/observer/sql/parser/yacc_sql.y`
- `src/observer/common/type/attr_type.h`

### 15. `TEXT` 类型和 `CHAR` 类型有什么区别？

语义上：

- `CHAR` 更偏定长字符串。
- `TEXT` 表示更长的变长文本。

当前实现中，`TEXT` 已经作为独立类型接入类型系统，并支持基本赋值、比较、输出和类型转换。

### 16. `TEXT` 的比较逻辑是什么？

在 `TextType::compare` 中：

```text
先比较两个字符串公共长度部分
如果公共部分不同，直接返回比较结果
如果公共部分相同，再比较长度
长度也相同则相等
```

这样可以实现类似字符串字典序比较。

### 17. 报告中写了 LOB 文件，源码里是否完整实现？

建议谨慎回答：

源码中已经有 `LobFileHandler` 和 `.lob` 文件路径相关逻辑，但完整的长文本外置 LOB 存储路径还不够完善。目前更准确的说法是：`TEXT` 类型的语法、类型系统和基础存储已经接入，LOB 文件管理是设计方向或部分雏形，完整长文本外置存储仍可继续完善。

## 五、Record Manager

### 18. RID 是什么？

RID 是记录标识符：

```text
RID = page_num + slot_num
```

- `page_num` 表示记录所在的数据页。
- `slot_num` 表示记录在页内的槽位。

通过 RID 可以唯一定位一条记录。

### 19. 页内记录怎么组织？

每个数据页大致结构：

```text
PageHeader | bitmap | record data
```

`PageHeader` 记录：

- 当前记录数 `record_num`
- 每条记录大小 `record_size`
- 页面最大记录数 `record_capacity`
- 数据区偏移 `data_offset`

bitmap 标记每个 slot 是否已经被占用。

### 20. 插入记录时如何找到空闲位置？

流程：

```text
从 free_pages_ 找未满页
-> 初始化 RecordPageHandler
-> bitmap.next_unsetted_bit 找空 slot
-> bitmap.set_bit
-> record_num++
-> memcpy 写入记录数据
-> frame 标脏
```

### 21. 删除记录后空间能复用吗？

可以。

删除时会：

```text
bitmap.clear_bit(slot_num)
record_num--
frame 标脏
页号加入 free_pages_
```

后续插入会优先从 `free_pages_` 中找未满页。

### 22. 更新记录时索引如何维护？

更新逻辑在 `HeapTableEngine::update_record_with_trx` 中：

```text
删除旧索引项
插入新索引项
更新记录本体
如果中途失败，尝试回滚索引项
```

这样可以避免记录和索引不一致。

## 六、Buffer Pool

### 23. Buffer Pool 的作用是什么？

Buffer Pool 是磁盘页缓存层，主要作用是：

- 缓存数据页，减少磁盘 I/O。
- 管理页的脏标记。
- 管理页面 pin/unpin，防止正在使用的页面被淘汰。
- 在内存不足时淘汰可释放页面。

### 24. 读取一个页面的流程是什么？

流程：

```text
frame_manager 查找页面
-> 命中：pin + access，直接返回
-> 未命中：allocate_frame
-> 必要时淘汰旧 frame
-> load_page 从 double write buffer 或磁盘读取
-> 返回 frame
```

相关源码：

- `DiskBufferPool::get_this_page`
- `DiskBufferPool::allocate_frame`
- `DiskBufferPool::load_page`

### 25. `pin_count` 是什么？为什么需要它？

`pin_count` 表示当前有多少使用者正在持有该页面。

作用：

- 页面被访问时 `pin_count++`
- 页面释放时 `pin_count--`
- 只有 `pin_count == 0` 的页面才允许被淘汰

这样可以防止正在读写的页面被 Buffer Pool 淘汰。

### 26. 什么是脏页？

脏页是指内存中已经被修改，但还没有写回磁盘的页面。

例如插入、删除、更新记录后会调用：

```cpp
frame_->mark_dirty();
```

淘汰前或主动 flush 时需要刷盘。

### 27. 页面淘汰大概怎么做？

`BPFrameManager::purge_frames` 会从 LRU 链表尾部开始找可以淘汰的 frame：

```text
从最近最少使用的页面开始扫描
-> 找 pin_count == 0 的页面
-> 如果是脏页，先 flush
-> 从 frame manager 中移除
-> 释放 frame
```

### 28. Double Write 的作用是什么？

Double Write 用来降低页面写入中途崩溃造成的数据页损坏风险。

刷页时先写入 double write buffer，再写入真实数据文件。恢复时如果数据页不完整，可以从 double write buffer 中恢复完整页。

## 七、测试相关

### 29. 你们怎么测试 `DROP TABLE`？

典型测试：

```sql
CREATE TABLE t(id int);
INSERT INTO t VALUES(1);
SELECT * FROM t;
DROP TABLE t;
SELECT * FROM t;
CREATE TABLE t(id int);
SELECT * FROM t;
```

验证点：

- 删除后查询失败。
- 删除后可以重新创建同名表。
- 非空表可以删除。
- 带索引表删除后不影响服务运行。

### 30. 你们怎么测试多列索引？

典型测试：

```sql
CREATE TABLE t(a int, b int, c int);
CREATE INDEX idx_ab ON t(a, b);
INSERT INTO t VALUES(1, 2, 3);
SELECT * FROM t WHERE a = 1 AND b = 2;
DELETE FROM t WHERE a = 1;
UPDATE t SET b = 5 WHERE a = 1;
```

验证点：

- 空表和非空表都能创建索引。
- 插入数据时索引项同步插入。
- 删除和更新时索引项同步维护。

### 31. 你们怎么测试 `TEXT`？

典型测试：

```sql
CREATE TABLE text_table(id int, info text);
INSERT INTO text_table VALUES(1, 'short text');
INSERT INTO text_table VALUES(2, 'this is a very very long string');
SELECT * FROM text_table;
UPDATE text_table SET info = 'new text' WHERE id = 1;
DELETE FROM text_table WHERE id = 2;
```

验证点：

- `TEXT` 建表成功。
- 插入、查询、更新、删除正常。
- 输出文本内容正确。

### 32. 你们怎么测试 Buffer Pool？

可以通过大量插入和查询触发页面分配、缓存命中和淘汰：

```text
连续查询同一批数据，观察是否命中缓存
大量插入数据，触发新页面分配
超过缓存容量后，触发页面淘汰和脏页刷盘
重启服务后验证数据是否还在
```

## 八、项目不足和改进方向

### 33. 当前项目有哪些不足？

可以回答：

- `DROP TABLE` 没有事务回滚，执行后不可撤销。
- 索引文件删除逻辑还可以更完整。
- 多列索引创建和维护已支持，但查询优化还不够完整。
- `TEXT` 类型基础接入已完成，但完整 LOB 外置存储还可继续完善。
- Buffer Pool 主要是基础 LRU，没有处理顺序扫描带来的缓存污染。
- 并发控制较简单，复杂多线程场景还需要更强的锁和事务隔离支持。

### 34. 如果继续优化，你们会做什么？

可以从以下方向回答：

- 完善 `DROP TABLE` 对索引文件、元数据和并发访问的清理。
- 实现多列索引完整最左前缀匹配和范围扫描。
- 给 `TEXT` 增加完整 LOB 外置存储、前缀索引或全文检索。
- Buffer Pool 增加冷热页区分，降低顺序扫描污染。
- 加强事务回滚、redo/undo 日志和并发控制。

## 九、老师可能追问的高风险问题

### 35. 报告里写“多列索引遵循最左前缀原则”，源码真的完整实现了吗？

建议回答：

创建和维护层面已经按多字段顺序保存和拼接 key，这为最左前缀原则提供了基础。但当前查询计划生成阶段还比较基础，主要按单个等值条件选择索引，没有完整构造多列联合边界。因此严格来说，完整最左前缀查询优化还没有完全实现，是后续优化点。

### 36. 报告里写“TEXT 长文本写入 LOB 文件”，源码是否完整？

建议回答：

项目中已经引入 `TEXTS` 类型、`LobFileHandler` 和 `.lob` 文件路径设计，但完整的长文本外置存储路径还不完善。目前完成的是 `TEXT` 的语法、类型系统、基础存储和比较，LOB 文件管理属于设计雏形和后续完善方向。

### 37. 如果 `DROP TABLE` 删除过程中失败怎么办？

当前没有完整事务化 DDL。可以说：

目前 `DROP TABLE` 是按步骤执行，删除内存对象后删除磁盘文件。如果中途文件删除失败，缺少事务回滚机制。后续可以通过 DDL 日志、两阶段删除或回收站机制保证原子性。

### 38. 为什么说 Buffer Pool 能保证数据持久化？

更准确说：

Buffer Pool 通过 dirty 标记、flush、double write 和日志机制提升持久化可靠性。但严格的数据库持久性还依赖 redo log、checkpoint 和恢复流程。当前实现是基础版本，不能和工业级数据库完全等同。

### 39. 多列索引 key 拼接有什么风险？

风险包括：

- 当前使用固定数组 `char composite_key[1024]`，字段很多或字段长度很大时可能溢出。
- 不同类型直接二进制拼接时，需要保证比较器能正确解释顺序。
- `TEXT` 等变长字段不适合直接作为普通定长索引 key。

改进方向是动态分配 key buffer，并为不同类型设计统一编码格式。

### 40. 你们项目中最核心的收获是什么？

可以回答：

最大的收获是把数据库课堂概念和真实内核代码对应起来，理解了 SQL 解析、执行器调度、表和记录管理、B+ 树索引、Buffer Pool、脏页刷盘之间的完整链路。相比单独写算法，这个项目更强调模块协作和系统一致性。

## 十、答辩建议

答辩时建议重点讲清楚四条主线：

1. `DROP TABLE`：SQL 解析到 `Db::drop_table` 的完整执行链路。
2. 多列索引：字段列表解析、元数据保存、composite key 拼接、索引维护。
3. Record Manager：RID、PageHeader、bitmap、free_pages_ 的关系。
4. Buffer Pool：命中、pin/unpin、dirty、淘汰、flush、double write。

遇到不确定的问题，不要硬说已经完整实现。可以说：

```text
这个功能我们完成了基础链路，但工业级完整实现还需要继续完善。
```

这样比过度承诺更稳。
