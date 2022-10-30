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
// Created by Wangyunlai on 2022/6/6.
//


#include "sql/stmt/select_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "common/log/log.h"
#include "common/lang/string.h"
#include "storage/common/db.h"
#include "storage/common/table.h"

SelectStmt::~SelectStmt()
{
  if (nullptr != filter_stmt_) {
    delete filter_stmt_;
    filter_stmt_ = nullptr;
  }
}

static void wildcard_fields(Table *table, std::vector<Field> &field_metas)
{
  const TableMeta &table_meta = table->table_meta();
  const int field_num = table_meta.field_num();
  for (int i = table_meta.sys_field_num(); i < field_num; i++) {
    field_metas.push_back(Field(table, table_meta.field(i)));
  }
}

void test() {
  
}
RC SelectStmt::create(Db *db, const Selects &select_sql, Stmt *&stmt)
{
  if (nullptr == db) {
    LOG_WARN("invalid argument. db is null");
    return RC::INVALID_ARGUMENT;
  }

  // 测试代码
  // inner join 的 table 得反过来看
  // LOG_INFO("join_tables_num = %d\n", select_sql.inner_join.join_tables_num);
  // if (select_sql.inner_join.join_tables_num > 0) {
  //   LOG_INFO("first_relation: %s\n", select_sql.inner_join.first_relation);
  //   for (int i = 0; i < select_sql.inner_join.join_tables_num; i++) {
  //     LOG_INFO("table name: %s\n", select_sql.inner_join.join_tables[i].relation_name);
  //   }
    
  //   return RC::UNIMPLENMENT;
  // }

  // collect tables in `from` statement
  // 李立基: 现阶段的 inner join 只支持全部 join 到一个表上, 
  // 而且 relation_num 和 select_sql.inner_join.join_tables_num 这 2 个字段只有一个会大于 0
  if (select_sql.relation_num > 0 && select_sql.inner_join.join_tables_num > 0) {
    LOG_ERROR("undone.");
    return RC::UNIMPLENMENT;
  }

  if (select_sql.relation_num <= 0 && select_sql.inner_join.join_tables_num <= 0) {
    LOG_ERROR("SOMETHING ERROR.");
    return RC::GENERIC_ERROR;
  }

  std::vector<Table *> tables;
  std::unordered_map<std::string, Table *> table_map;
  for (size_t i = 0; i < select_sql.relation_num; i++) {
    const char *table_name = select_sql.relations[i];
    if (nullptr == table_name) {
      LOG_WARN("invalid argument. relation name is null. index=%d", i);
      return RC::INVALID_ARGUMENT;
    }

    Table *table = db->find_table(table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }

    tables.push_back(table);
    table_map.insert(std::pair<std::string, Table*>(table_name, table));
  }
  
  if (select_sql.inner_join.join_tables_num > 0) {
    const char *first_table_name = select_sql.inner_join.first_relation;
    if (nullptr == first_table_name) {
      LOG_WARN("invalid argument. first table name is null.");
      return RC::INVALID_ARGUMENT;
    }
    Table *table = db->find_table(first_table_name);
    if (nullptr == table) {
      LOG_WARN("no such table. db=%s, table_name=%s", db->name(), first_table_name);
      return RC::SCHEMA_TABLE_NOT_EXIST;
    }
    tables.push_back(table);
    table_map.insert(std::pair<std::string, Table*>(first_table_name, table));

    for (size_t i = select_sql.inner_join.join_tables_num - 1; i >= 0; i--) {
      const char *table_name = select_sql.inner_join.join_tables[i].relation_name;
      if (nullptr == table_name) {
        LOG_WARN("invalid argument. relation name is null. index=%d", i);
        return RC::INVALID_ARGUMENT;
      }

      table = db->find_table(table_name);
      if (nullptr == table) {
        LOG_WARN("no such table. db=%s, table_name=%s", db->name(), table_name);
        return RC::SCHEMA_TABLE_NOT_EXIST;
      }

      tables.push_back(table);
      table_map.insert(std::pair<std::string, Table*>(table_name, table));
    }
  }
  
  Table *default_table = nullptr;
  if (tables.size() == 1) {
    default_table = tables[0];
  }
  
  // create filter statement in `where` statement
  FilterStmt *filter_stmt = nullptr;
  RC rc = FilterStmt::create(db, default_table, &table_map,
           select_sql.conditions, select_sql.condition_num, filter_stmt);
  LOG_INFO("FilterStmt.create: condition_num = %d\n", select_sql.condition_num);
  if (rc != RC::SUCCESS) {
    LOG_WARN("cannot construct filter stmt");
    return rc;
  }

  std::vector<AggrFuncCXX> aggr_funcs_t;  // for 聚合函数
  std::vector<Field> query_fields;        // for 普通查询
  if (select_sql.aggr_func_num > 0) {
    // 李立基: 处理聚合函数, 目前只考虑单表.
    for (int i = select_sql.aggr_func_num - 1; i >= 0; i--) {
      const AggrFunc &aggregation_func = select_sql.aggr_funcs[i];
      switch (aggregation_func.type)
      {
      case aggregation_fun::max_fun: {
        // max 聚合函数只支持 `max(ID)` 一种语法.
        Table *table = tables[0];
        const char *field_name = aggregation_func.attribute.attribute_name;
        const FieldMeta *field_meta = table->table_meta().field(field_name);
        if (nullptr == field_meta) {
          return RC::INVALID_ARGUMENT;
        }
        aggr_funcs_t.push_back(AggrFuncCXX(aggregation_fun::max_fun, Field(table, field_meta)));
      } break;
      case aggregation_fun::min_fun: {
        Table *table = tables[0];
        const char *field_name = aggregation_func.attribute.attribute_name;
        const FieldMeta *field_meta = table->table_meta().field(field_name);
        if (nullptr == field_meta) {
          return RC::INVALID_ARGUMENT;
        }
        aggr_funcs_t.push_back(AggrFuncCXX(aggregation_fun::min_fun, Field(table, field_meta)));
      } break;
      case aggregation_fun::avg_fun: {
        Table *table = tables[0];
        const char *field_name = aggregation_func.attribute.attribute_name;
        const FieldMeta *field_meta = table->table_meta().field(field_name);
        if (nullptr == field_meta) {
          return RC::INVALID_ARGUMENT;
        }
        aggr_funcs_t.push_back(AggrFuncCXX(aggregation_fun::avg_fun, Field(table, field_meta)));
      } break;
      case aggregation_fun::sum_fun: {
        Table *table = tables[0];
        const char *field_name = aggregation_func.attribute.attribute_name;
        const FieldMeta *field_meta = table->table_meta().field(field_name);
        if (nullptr == field_meta) {
          return RC::INVALID_ARGUMENT;
        }
        aggr_funcs_t.push_back(AggrFuncCXX(aggregation_fun::sum_fun, Field(table, field_meta)));
      } break;
      case aggregation_fun::count_fun: {
        // TODO: count 只要 count name
        Table *table = tables[0];
        std::string count_name = aggregation_func.attribute.attribute_name;
        if ((count_name != "*" && count_name != "1") && 
        nullptr == table->table_meta().field(count_name.c_str())) {
          return RC::INVALID_ARGUMENT;
        }
        aggr_funcs_t.push_back(AggrFuncCXX(aggregation_fun::count_fun, count_name));
      } break;
      
      default:
        break;
      }
    }
  } else {
    // collect query fields in `select` statement
    for (int i = select_sql.attr_num - 1; i >= 0; i--) {
      const RelAttr &relation_attr = select_sql.attributes[i];

      if (common::is_blank(relation_attr.relation_name) && 0 == strcmp(relation_attr.attribute_name, "*")) {
        for (int t = tables.size() - 1; t >= 0; t--) {
          Table *table = tables[t];
          wildcard_fields(table, query_fields);
        }
      } else if (!common::is_blank(relation_attr.relation_name)) { // TODO
        const char *table_name = relation_attr.relation_name;
        const char *field_name = relation_attr.attribute_name;

        if (0 == strcmp(table_name, "*")) {
          if (0 != strcmp(field_name, "*")) {
            LOG_WARN("invalid field name while table is *. attr=%s", field_name);
            return RC::SCHEMA_FIELD_MISSING;
          }
          for (int t = tables.size() - 1; t >= 0; t--) {
            Table *table = tables[t];
            wildcard_fields(table, query_fields);
          }
        } else {
          auto iter = table_map.find(table_name);
          if (iter == table_map.end()) {
            LOG_WARN("no such table in from list: %s", table_name);
            return RC::SCHEMA_FIELD_MISSING;
          }

          Table *table = iter->second;
          if (0 == strcmp(field_name, "*")) {
            wildcard_fields(table, query_fields);
          } else {
            const FieldMeta *field_meta = table->table_meta().field(field_name);
            if (nullptr == field_meta) {
              LOG_WARN("no such field. field=%s.%s.%s", db->name(), table->name(), field_name);
              return RC::SCHEMA_FIELD_MISSING;
            }

          query_fields.push_back(Field(table, field_meta));
          }
        }
      } else {
        if (tables.size() != 1) {
          LOG_WARN("invalid. I do not know the attr's table. attr=%s", relation_attr.attribute_name);
          return RC::SCHEMA_FIELD_MISSING;
        }

        Table *table = tables[0];
        const FieldMeta *field_meta = table->table_meta().field(relation_attr.attribute_name);
        if (nullptr == field_meta) {
          LOG_WARN("no such field. field=%s.%s.%s", db->name(), table->name(), relation_attr.attribute_name);
          return RC::SCHEMA_FIELD_MISSING;
        }

        query_fields.push_back(Field(table, field_meta));
      }
    }
    LOG_INFO("got %d tables in from stmt and %d fields in query stmt", tables.size(), query_fields.size());
  }

  // everything alright
  SelectStmt *select_stmt = new SelectStmt();
  select_stmt->tables_.swap(tables);
  select_stmt->query_fields_.swap(query_fields);
  select_stmt->aggr_funcs_.swap(aggr_funcs_t);
  select_stmt->filter_stmt_ = filter_stmt;
  select_stmt->aggr_fun_ = select_sql.aggr_type;
  select_stmt->aggr_arg_num_ = select_sql.aggr_arg_num;
  stmt = select_stmt;
  return RC::SUCCESS;
}
