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

#include "rc.h"
#include "common/log/log.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "stmt.h"
#include "math.h"
#include "util/util.h"

RC Stmt::create_stmt(Db *db, Query &query, Stmt *&stmt)
{
  stmt = nullptr;

  switch (query.flag) {
    case SCF_INSERT: {
      return InsertStmt::create(db, query.sstr.insertion, stmt);
    }
    case SCF_DELETE: {
      return DeleteStmt::create(db, query.sstr.deletion, stmt);
    }
    case SCF_UPDATE: {
      return UpdateStmt::create(db, query.sstr.update, stmt);
    }
    case SCF_SELECT: {
      return SelectStmt::create(db, query.sstr.selection, stmt);
    }
    default: {
      LOG_WARN("unknown query command");
    } break;
  }
  return RC::UNIMPLENMENT;
}

RC Stmt::cast_value_to_field_type(Value *value, AttrType field_type)
{
  AttrType value_type = value->type;
  void *data = value->data;
  // 修改对应位置的value
  // TODO how to free the new inited value
  switch (field_type) {
    case CHARS: {
      const char *new_data;
      if (value_type == INTS) {
        new_data = std::to_string(*(int *)data).c_str();
      } else if (value_type == FLOATS) {
        new_data = double2string(*(float *)data).c_str();
      } else {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value_init_string(value, new_data);
    } break;
    case INTS: {
      int new_data;
      if (value_type == CHARS) {
        new_data = (int)round(atof((char *)data));
      } else if (value_type == FLOATS) {
        new_data = (int)round(*(float *)data);
      } else {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value_init_integer(value, new_data);
    } break;
    case FLOATS: {
      float new_data;
      if (value_type == CHARS) {
        new_data = atof((char *)data);
      } else if (value_type == INTS) {
        new_data = *(int *)data;
      } else {
        return RC::SCHEMA_FIELD_TYPE_MISMATCH;
      }
      value_init_float(value, new_data);
    } break;
    default: {
      return RC::SCHEMA_FIELD_TYPE_MISMATCH;
    }
  }
  free(data);
  return RC::SUCCESS;
}
