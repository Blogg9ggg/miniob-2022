/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
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

#include "sql/stmt/update_stmt.h"
#include "common/log/log.h"
#include "sql/stmt/filter_stmt.h"
#include "storage/common/db.h"

UpdateStmt::UpdateStmt(Table *table, Value *values, int value_amount, char *update_field, FilterStmt *filter_stmt)
    : table_(table),
      values_(values),
      value_amount_(value_amount),
      update_field_(update_field),
      filter_stmt_(filter_stmt)
{}

UpdateStmt::~UpdateStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

RC UpdateStmt::create(Db *db, const Updates &update_sql, Stmt *&stmt)
{
  const char *table_name = update_sql.relation_name;
  char *attribute_name = update_sql.attribute_name;
  const Value update_value = update_sql.value;
  if (nullptr == db || nullptr == table_name || nullptr == attribute_name) {
    LOG_WARN("invalid argument. db=%p, table_name=%p, update_field=%p", db, table_name, attribute_name);
    return RC::INVALID_ARGUMENT;
  }

  // check whether the table exists
  Table *table = db->find_table(table_name);
  if (nullptr == table) {
    LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  // check whether the update_field exists
  const TableMeta &table_meta = table->table_meta();
  const FieldMeta *update_field = table_meta.field(attribute_name);
  if (update_field == nullptr) {
    return RC::SCHEMA_FIELD_NOT_EXIST;
  }
  // check update_field type
  if (update_field->type() != update_value.type) {
    return RC::SCHEMA_FIELD_TYPE_MISMATCH;
  }

  std::unordered_map<std::string, Table *> table_map;
  table_map.insert(std::pair<std::string, Table *>(std::string(table_name), table));

  FilterStmt *filter_stmt = nullptr;

  RC rc = FilterStmt::create(db, table, &table_map, update_sql.conditions, update_sql.condition_num, filter_stmt);
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to create filter statement. rc=%d:%s", rc, strrc(rc));
    return rc;
  }

  Value values[1] = {update_value};
  stmt = new UpdateStmt(table, values, 1, attribute_name, filter_stmt);

  return RC::SUCCESS;
}
