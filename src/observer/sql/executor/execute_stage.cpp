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
// Created by Meiyi & Longda on 2021/4/13.
//

#include <string>
#include <sstream>
#include <iomanip>

#include "execute_stage.h"

#include "util/typecast.h"
#include "util/util.h"
#include "common/io/io.h"
#include "common/log/log.h"
#include "common/lang/defer.h"
#include "common/seda/timer_stage.h"
#include "common/lang/string.h"
#include "session/session.h"
#include "event/storage_event.h"
#include "event/sql_event.h"
#include "event/session_event.h"
#include "sql/expr/tuple.h"
#include "sql/operator/table_scan_operator.h"
#include "sql/operator/index_scan_operator.h"
#include "sql/operator/predicate_operator.h"
#include "sql/operator/delete_operator.h"
#include "sql/operator/project_operator.h"
#include "sql/stmt/stmt.h"
#include "sql/stmt/select_stmt.h"
#include "sql/stmt/update_stmt.h"
#include "sql/stmt/delete_stmt.h"
#include "sql/stmt/insert_stmt.h"
#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/aggregate_function.h"
#include "storage/common/table.h"
#include "storage/common/field.h"
#include "storage/index/index.h"
#include "storage/default/default_handler.h"
#include "storage/common/condition_filter.h"
#include "storage/trx/trx.h"
#include "storage/clog/clog.h"
#include "sql/operator/update_operator.h"
#include "sql/operator/merge_operator.h"

using namespace common;

// RC create_selection_executor(
//    Trx *trx, const Selects &selects, const char *db, const char *table_name, SelectExeNode &select_node);

//! Constructor
ExecuteStage::ExecuteStage(const char *tag) : Stage(tag)
{}

//! Destructor
ExecuteStage::~ExecuteStage()
{}

//! Parse properties, instantiate a stage object
Stage *ExecuteStage::make_stage(const std::string &tag)
{
  ExecuteStage *stage = new (std::nothrow) ExecuteStage(tag.c_str());
  if (stage == nullptr) {
    LOG_ERROR("new ExecuteStage failed");
    return nullptr;
  }
  stage->set_properties();
  return stage;
}

//! Set properties for this object set in stage specific properties
bool ExecuteStage::set_properties()
{
  //  std::string stageNameStr(stageName);
  //  std::map<std::string, std::string> section = theGlobalProperties()->get(
  //    stageNameStr);
  //
  //  std::map<std::string, std::string>::iterator it;
  //
  //  std::string key;

  return true;
}

//! Initialize stage params and validate outputs
bool ExecuteStage::initialize()
{
  LOG_TRACE("Enter");

  std::list<Stage *>::iterator stgp = next_stage_list_.begin();
  default_storage_stage_ = *(stgp++);
  mem_storage_stage_ = *(stgp++);

  LOG_TRACE("Exit");
  return true;
}

//! Cleanup after disconnection
void ExecuteStage::cleanup()
{
  LOG_TRACE("Enter");

  LOG_TRACE("Exit");
}

void ExecuteStage::handle_event(StageEvent *event)
{
  LOG_TRACE("Enter\n");

  handle_request(event);

  LOG_TRACE("Exit\n");
  return;
}

void ExecuteStage::callback_event(StageEvent *event, CallbackContext *context)
{
  LOG_TRACE("Enter\n");

  // here finish read all data from disk or network, but do nothing here.

  LOG_TRACE("Exit\n");
  return;
}

void ExecuteStage::handle_request(common::StageEvent *event)
{
  SQLStageEvent *sql_event = static_cast<SQLStageEvent *>(event);
  SessionEvent *session_event = sql_event->session_event();
  Stmt *stmt = sql_event->stmt();
  Session *session = session_event->session();
  Query *sql = sql_event->query();

  if (stmt != nullptr) {
    switch (stmt->type()) {
      case StmtType::SELECT: {
        do_select(sql_event);
      } break;
      case StmtType::INSERT: {
        do_insert(sql_event);
      } break;
      case StmtType::UPDATE: {
        do_update(sql_event);
      } break;
      case StmtType::DELETE: {
        do_delete(sql_event);
      } break;
      default: {
        LOG_WARN("should not happen. please implenment");
      } break;
    }
  } else {
    switch (sql->flag) {
      case SCF_HELP: {
        do_help(sql_event);
      } break;
      case SCF_CREATE_TABLE: {
        do_create_table(sql_event);
      } break;
      case SCF_CREATE_INDEX: {
        do_create_index(sql_event);
      } break;
      case SCF_SHOW_TABLES: {
        do_show_tables(sql_event);
      } break;
      case SCF_DESC_TABLE: {
        do_desc_table(sql_event);
      } break;

        // 李立基: 增加对 SCF_SHOW_INDEX 的处理
      case SCF_SHOW_INDEX: {
        do_show_index(sql_event);
      } break;

      case SCF_DROP_TABLE:
      case SCF_DROP_INDEX:
      case SCF_LOAD_DATA: {
        default_storage_stage_->handle_event(event);
      } break;
      case SCF_SYNC: {
        /*
        RC rc = DefaultHandler::get_default().sync();
        session_event->set_response(strrc(rc));
        */
      } break;
      case SCF_BEGIN: {
        do_begin(sql_event);
        /*
        session_event->set_response("SUCCESS\n");
        */
      } break;
      case SCF_COMMIT: {
        do_commit(sql_event);
        /*
        Trx *trx = session->current_trx();
        RC rc = trx->commit();
        session->set_trx_multi_operation_mode(false);
        session_event->set_response(strrc(rc));
        */
      } break;
      case SCF_CLOG_SYNC: {
        do_clog_sync(sql_event);
      }
      case SCF_ROLLBACK: {
        Trx *trx = session_event->get_client()->session->current_trx();
        RC rc = trx->rollback();
        session->set_trx_multi_operation_mode(false);
        session_event->set_response(strrc(rc));
      } break;
      case SCF_EXIT: {
        // do nothing
        const char *response = "Unsupported\n";
        session_event->set_response(response);
      } break;
      default: {
        LOG_ERROR("Unsupported command=%d\n", sql->flag);
      }
    }
  }
}

void end_trx_if_need(Session *session, Trx *trx, bool all_right)
{
  if (!session->is_trx_multi_operation_mode()) {
    if (all_right) {
      trx->commit();
    } else {
      trx->rollback();
    }
  }
}

void print_tuple_header(std::ostream &os, const ProjectOperator &oper, const char *aggr_func_name_p)
{
  const int cell_num = oper.tuple_cell_num();
  if (cell_num != 1) {
    // TODO: 保证有且仅有一个字段
  }

  const TupleCellSpec *cell_spec = nullptr;
  oper.tuple_cell_spec_at(0, cell_spec);
  if (cell_spec->alias()) {
    os << aggr_func_name_p << "(" << cell_spec->alias() << ")";
  }

  if (cell_num > 0) {
    os << '\n';
  }
}
void print_tuple_header(std::ostream &os, const ProjectOperator &oper)
{
  const int cell_num = oper.tuple_cell_num();
  const TupleCellSpec *cell_spec = nullptr;
  for (int i = 0; i < cell_num; i++) {
    oper.tuple_cell_spec_at(i, cell_spec);
    if (i != 0) {
      os << " | ";
    }

    if (cell_spec->alias()) {
      os << cell_spec->alias();
    }
  }

  if (cell_num > 0) {
    os << '\n';
  }
}
void tuple_to_string(std::ostream &os, const Tuple &tuple)
{
  TupleCell cell;
  RC rc = RC::SUCCESS;
  bool first_field = true;
  for (int i = 0; i < tuple.cell_num(); i++) {
    rc = tuple.cell_at(i, cell);
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", i, strrc(rc));
      break;
    }

    if (!first_field) {
      os << " | ";
    } else {
      first_field = false;
    }
    cell.to_string(os);
  }
}

IndexScanOperator *try_to_create_index_scan_operator(FilterStmt *filter_stmt, Table *table)
{
  const std::vector<FilterUnit *> &filter_units = filter_stmt->filter_units();
  if (filter_units.empty()) {
    return nullptr;
  }

  // 在所有过滤条件中，找到字段与值做比较的条件，然后判断字段是否可以使用索引
  // 如果是多列索引，这里的处理需要更复杂。
  // 这里的查找规则是比较简单的，就是尽量找到使用相等比较的索引
  // 如果没有就找范围比较的，但是直接排除不等比较的索引查询. (你知道为什么?)
  const FilterUnit *better_filter = nullptr;
  for (const FilterUnit *filter_unit : filter_units) {
    if (filter_unit->comp() == NOT_EQUAL) {
      continue;
    }

    Expression *left = filter_unit->left();
    Expression *right = filter_unit->right();
    if (left->type() == ExprType::FIELD && right->type() == ExprType::VALUE) {
    } else if (left->type() == ExprType::VALUE && right->type() == ExprType::FIELD) {
      std::swap(left, right);
    } else {
      continue;
    }
    FieldExpr &left_field_expr = *(FieldExpr *)left;
    // chenfarong: 类型相同才能走索引
    ValueExpr &right_value_expr = *(ValueExpr *)right;
    TupleCell value;
    right_value_expr.get_tuple_cell(value);
    if (value.attr_type() != left_field_expr.field().attr_type()) {
      continue;
    }
    const Field &field = left_field_expr.field();
    if (strcmp(table->name(), field.table()->name()) != 0) {
      continue;
    }
    Index *index = table->find_index_by_field(field.field_name());
    if (index != nullptr) {
      if (better_filter == nullptr) {
        better_filter = filter_unit;
      } else if (filter_unit->comp() == EQUAL_TO) {
        better_filter = filter_unit;
        break;
      }
    }
  }

  if (better_filter == nullptr) {
    return nullptr;
  }

  Expression *left = better_filter->left();
  Expression *right = better_filter->right();
  CompOp comp = better_filter->comp();
  if (left->type() == ExprType::VALUE && right->type() == ExprType::FIELD) {
    std::swap(left, right);
    switch (comp) {
      case EQUAL_TO: {
        comp = EQUAL_TO;
      } break;
      case LESS_EQUAL: {
        comp = GREAT_THAN;
      } break;
      case NOT_EQUAL: {
        comp = NOT_EQUAL;
      } break;
      case LESS_THAN: {
        comp = GREAT_EQUAL;
      } break;
      case GREAT_EQUAL: {
        comp = LESS_THAN;
      } break;
      case GREAT_THAN: {
        comp = LESS_EQUAL;
      } break;
      default: {
        LOG_WARN("should not happen");
      }
    }
  }

  FieldExpr &left_field_expr = *(FieldExpr *)left;
  const Field &field = left_field_expr.field();
  Index *index = table->find_index_by_field(field.field_name());
  assert(index != nullptr);

  ValueExpr &right_value_expr = *(ValueExpr *)right;
  TupleCell value;
  right_value_expr.get_tuple_cell(value);

  const TupleCell *left_cell = nullptr;
  const TupleCell *right_cell = nullptr;
  bool left_inclusive = false;
  bool right_inclusive = false;

  switch (comp) {
    case EQUAL_TO: {
      left_cell = &value;
      right_cell = &value;
      left_inclusive = true;
      right_inclusive = true;
    } break;

    case LESS_EQUAL: {
      left_cell = nullptr;
      left_inclusive = false;
      right_cell = &value;
      right_inclusive = true;
    } break;

    case LESS_THAN: {
      left_cell = nullptr;
      left_inclusive = false;
      right_cell = &value;
      right_inclusive = false;
    } break;

    case GREAT_EQUAL: {
      left_cell = &value;
      left_inclusive = true;
      right_cell = nullptr;
      right_inclusive = false;
    } break;

    case GREAT_THAN: {
      left_cell = &value;
      left_inclusive = false;
      right_cell = nullptr;
      right_inclusive = false;
    } break;

    default: {
      LOG_WARN("should not happen. comp=%d", comp);
    } break;
  }

  IndexScanOperator *oper = new IndexScanOperator(table, index, left_cell, left_inclusive, right_cell, right_inclusive);

  LOG_INFO("use index for scan: %s in table %s", index->index_meta().name(), table->name());
  return oper;
}

/*
 * 作者: 李立基
 * 说明: 处理 max 聚合函数
 */
RC do_aggr_func_max(std::ostream &os, ProjectOperator &project_oper)
{
  RC rc = RC::SUCCESS;
  // print_tuple_header(os, project_oper, "MAX");

  rc = project_oper.next();
  if (rc == RC::RECORD_EOF) {
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper.current_tuple();
  TupleCell max_cell, cell; 
  if ((rc = tuple->cell_at(0, max_cell)) != RC::SUCCESS) {
    LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
    return rc;
  }

  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      return rc;
    }

    if ((rc = tuple->cell_at(0, cell)) != RC::SUCCESS) {
      LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
      return rc;
    }

    if (cell.compare(max_cell) > 0) {
      max_cell = cell; // TODO: 考虑好内存问题
    }
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  max_cell.to_string(os);
  // os << std::endl;
  rc = project_oper.close();

  return rc;
}

/*
 * 作者: 李立基
 * 说明: 处理 min 聚合函数
 */
RC do_aggr_func_min(std::ostream &os, ProjectOperator &project_oper)
{
  RC rc = RC::SUCCESS;

  rc = project_oper.next();
  if (rc == RC::RECORD_EOF) {
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper.current_tuple();
  TupleCell min_cell, cell; 
  if ((rc = tuple->cell_at(0, min_cell)) != RC::SUCCESS) {
    LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
    return rc;
  }

  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      return rc;
    }

    if ((rc = tuple->cell_at(0, cell)) != RC::SUCCESS) {
      LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
      return rc;
    }

    if (cell.compare(min_cell) < 0) {
      min_cell = cell; // TODO: 考虑好内存问题
    }
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  min_cell.to_string(os);
  rc = project_oper.close();

  return rc;
}

/*
 * 作者: 李立基
 * 说明: 处理 sum 聚合函数
 */
RC do_aggr_func_sum(std::ostream &os, ProjectOperator &project_oper)
{
  RC rc = RC::SUCCESS;

  float ans = 0;
  rc = project_oper.next();
  if (rc == RC::RECORD_EOF) {
    os << "0" << std::endl;
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper.current_tuple();
  TupleCell cell; 
  if ((rc = tuple->cell_at(0, cell)) != RC::SUCCESS) {
    LOG_ERROR("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
    return rc;
  }
  int attr_type = cell.attr_type();
  // if (attr_type != AttrType::INTS && attr_type != AttrType::FLOATS) {
  //   LOG_WARN("invalid type for sum().");
  //   return rc;
  // }
  if (attr_type == AttrType::INTS)
    ans += *(int *)cell.data();
  else if (attr_type == AttrType::FLOATS)
    ans += *(float *)cell.data();
  else if (attr_type == AttrType::CHARS)
    ans += string2float((const char *)cell.data());

  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      return rc;
    }

    if ((rc = tuple->cell_at(0, cell)) != RC::SUCCESS) {
      LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
      return rc;
    }
    if (attr_type == AttrType::INTS)
      ans += *(int *)cell.data();
    else if (attr_type == AttrType::FLOATS)
      ans += *(float *)cell.data();
    else if (attr_type == AttrType::CHARS)
      ans += string2float((const char *)cell.data());
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  os << double2string(ans);
  
  rc = project_oper.close();

  return rc;
}

/*
 * 作者: 李立基
 * 说明: 处理 avg 聚合函数
 */
RC do_aggr_func_avg(std::ostream &os, ProjectOperator &project_oper)
{
  RC rc = RC::SUCCESS;

  float ans = 0;
  int cnt = 0;
  rc = project_oper.next();
  if (rc == RC::RECORD_EOF) {
    os << "0" << std::endl;
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper.current_tuple();
  TupleCell cell; 
  if ((rc = tuple->cell_at(0, cell)) != RC::SUCCESS) {
    LOG_ERROR("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
    return rc;
  }
  int attr_type = cell.attr_type();
  // if (attr_type != AttrType::INTS && attr_type != AttrType::FLOATS) {
  //   LOG_WARN("invalid type for sum().");
  //   return rc;
  // }
  if (attr_type == AttrType::INTS)
    ans += *(int *)cell.data();
  else if (attr_type == AttrType::FLOATS)
    ans += *(float *)cell.data();
  else if (attr_type == AttrType::CHARS)
    ans += string2float((const char *)cell.data());
  cnt++;

  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      return rc;
    }

    if ((rc = tuple->cell_at(0, cell)) != RC::SUCCESS) {
      LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
      return rc;
    }
    if (attr_type == AttrType::INTS)
      ans += *(int *)cell.data();
    else if (attr_type == AttrType::FLOATS)
      ans += *(float *)cell.data();
    else if (attr_type == AttrType::CHARS)
      ans += string2float((const char *)cell.data());
    cnt++;
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  os << double2string(ans/cnt);
  
  rc = project_oper.close();

  return rc;
}

/*
 * 作者: 李立基
 * 说明: 处理 count 聚合函数
 */
RC do_aggr_func_count(std::ostream &os, ProjectOperator &project_oper)
{
  RC rc = RC::SUCCESS;

  Tuple *tuple;
  int ans = 0;
  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      return rc;
    }

    ans++;
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  os << ans;
  rc = project_oper.close();

  return rc;
}

RC ExecuteStage::do_select(SQLStageEvent *sql_event)
{
  SelectStmt *select_stmt = (SelectStmt *)(sql_event->stmt());
  SessionEvent *session_event = sql_event->session_event();
  std::stringstream ss;
  RC rc = RC::SUCCESS;

  if (select_stmt->tables().size() != 1) {
    // 多表查询
    // FilterStmt *filterStmt = select_stmt->filter_stmt();
    // std::vector<std::vector<Tuple *>> tuples_set;
    // MergeOperator merge_oper(filterStmt);
    // for (const auto &table : select_stmt->tables()) {
    //   Operator *scan_oper = try_to_create_index_scan_operator(filterStmt, table);
    //   if (nullptr == scan_oper) {
    //     scan_oper = new TableScanOperator(table);
    //   }
    //   PredicateOperator pred_oper(filterStmt);
    //   pred_oper.add_child(scan_oper);
    //   merge_oper.add_child(&pred_oper);
    // }
    return RC::UNIMPLENMENT;
  }

  Table *default_table = select_stmt->tables()[0]; // 单表
  Operator *scan_oper = try_to_create_index_scan_operator(select_stmt->filter_stmt(), select_stmt->tables()[0]);
  if (nullptr == scan_oper) {
    scan_oper = new TableScanOperator(select_stmt->tables()[0]);
  }
  DEFER([&]() { delete scan_oper; });

  PredicateOperator pred_oper(select_stmt->filter_stmt());
  pred_oper.add_child(scan_oper);

  if (select_stmt->aggr_funcs().size() > 0) {
    // 李立基: 先打印头部
    const char *aggr_func_name[] = {"", "MAX", "MIN", "COUNT", "AVG", "SUM"};
    bool first = true;
    for(AggrFuncCXX aggr_func : select_stmt->aggr_funcs()) {
      if (first)  first = false;
      else  ss << " | ";

      if (aggr_func.type() == aggregation_fun::count_fun) {
        ss << aggr_func_name[aggr_func.type()] << "(" << aggr_func.count_name() << ")";
      } else {
        ss << aggr_func_name[aggr_func.type()] << "(" << aggr_func.field().field_name() << ")";
      }
      
    }
    ss << std::endl;

    first = true;
    for(AggrFuncCXX aggr_func : select_stmt->aggr_funcs()) {
      if (first)  first = false;
      else  ss << " | ";

      switch (aggr_func.type())
      {
      case aggregation_fun::max_fun: {
        ProjectOperator project_oper;
        project_oper.add_child(&pred_oper);
        LOG_INFO("table_name: %s, field_name: %s\n", aggr_func.field().table()->name(), aggr_func.field().meta()->name());
        project_oper.add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper.open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_max(ss, project_oper);

        project_oper.close();
      } break;
      case aggregation_fun::min_fun: {
        ProjectOperator project_oper;
        project_oper.add_child(&pred_oper);
        project_oper.add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper.open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_min(ss, project_oper);
        project_oper.close();
      } break;
      case aggregation_fun::avg_fun: {
        ProjectOperator project_oper;
        project_oper.add_child(&pred_oper);
        project_oper.add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper.open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_avg(ss, project_oper);
        project_oper.close();
      } break;
      case aggregation_fun::sum_fun: {
        ProjectOperator project_oper;
        project_oper.add_child(&pred_oper);
        project_oper.add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper.open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_sum(ss, project_oper);
        project_oper.close();
      } break;
      case aggregation_fun::count_fun: {
        ProjectOperator project_oper;
        project_oper.add_child(&pred_oper);
        
        const char *field_name = aggr_func.count_name().c_str();
        if (strcmp(field_name, "*") != 0 && strcmp(field_name, "1") != 0) {
          if (default_table->table_meta().field(field_name) == nullptr) {
            session_event->set_response("FAILURE\n");

            return RC::SUCCESS;
          }
        }

        // TODO: 这里只考虑单表哦, 直接取第一个 field, 非常粗暴
        project_oper.add_projection(default_table, default_table->table_meta().field(1));
        //default_table->table_meta().field(1).meta());

        rc = project_oper.open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_count(ss, project_oper);
        project_oper.close();
      } break;
      
      default:
        break;
      }
    }
    ss << std::endl;
    session_event->set_response(ss.str());

    return RC::SUCCESS;
  }

  // ============================================
  ProjectOperator project_oper;
  project_oper.add_child(&pred_oper);
  for (const Field &field : select_stmt->query_fields()) {
    project_oper.add_projection(field.table(), field.meta());
  }
  rc = project_oper.open();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open operator");
    return rc;
  }

  print_tuple_header(ss, project_oper);
  while ((rc = project_oper.next()) == RC::SUCCESS) {
    // get current record
    // write to response
    Tuple *tuple = project_oper.current_tuple();
    if (nullptr == tuple) {
      rc = RC::INTERNAL;
      LOG_WARN("failed to get current record. rc=%s", strrc(rc));
      break;
    }

    tuple_to_string(ss, *tuple);
    ss << std::endl;
  }

  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  } 

  session_event->set_response(ss.str());
  rc = project_oper.close();
  
  return rc;
}

RC ExecuteStage::do_help(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  const char *response = "show tables;\n"
                         "desc `table name`;\n"
                         "create table `table name` (`column name` `column type`, ...);\n"
                         "create index `index name` on `table` (`column`);\n"
                         "insert into `table` values(`value1`,`value2`);\n"
                         "update `table` set column=value [where `column`=`value`];\n"
                         "delete from `table` [where `column`=`value`];\n"
                         "select [ * | `columns` ] from `table`;\n";
  session_event->set_response(response);
  return RC::SUCCESS;
}

RC ExecuteStage::do_create_table(SQLStageEvent *sql_event)
{
  const CreateTable &create_table = sql_event->query()->sstr.create_table;
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  RC rc = db->create_table(create_table.relation_name, create_table.attribute_count, create_table.attributes);
  if (rc == RC::SUCCESS) {
    session_event->set_response("SUCCESS\n");
  } else {
    session_event->set_response("FAILURE\n");
  }
  return rc;
}
RC ExecuteStage::do_create_index(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  const CreateIndex &create_index = sql_event->query()->sstr.create_index;
  Table *table = db->find_table(create_index.relation_name);
  if (nullptr == table) {
    session_event->set_response("FAILURE\n");
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }

  RC rc = table->create_index(nullptr, create_index.index_name, create_index.attribute_name);
  sql_event->session_event()->set_response(rc == RC::SUCCESS ? "SUCCESS\n" : "FAILURE\n");
  return rc;
}

/*
 * 作者: 李立基
 * 说明: 实现 show index 功能
 */
RC ExecuteStage::do_show_index(SQLStageEvent *sql_event)
{
  Query *query = sql_event->query();
  Db *db = sql_event->session_event()->session()->get_current_db();
  const char *table_name = query->sstr.show_index.relation_name;
  Table *table = db->find_table(table_name);

  std::string title = "TABLE | NON_UNIQUE | KEY_NAME | SEQ_IN_INDEX | COLUMN_NAME";

  std::stringstream ss;
  if (table != nullptr) {
    const std::vector<Index *> *indexes = table->indexes();
    ss << title << std::endl;
    for (Index *ind : *indexes) {
      // TODO: 现在默认不是唯一索引, 后续修改
      // TODO: 目前没有多列索引, 列在索引中的序号默认为 1
      ss << table->name() << " | "
         << "1"
         << " | " << ind->index_meta().name() << " | "
         << "1"
         << " | " << ind->index_meta().field() << std::endl;
    }
  } else {
    ss << "FAILURE" << std::endl;
  }
  sql_event->session_event()->set_response(ss.str().c_str());
  return RC::SUCCESS;
}

RC ExecuteStage::do_show_tables(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  std::vector<std::string> all_tables;
  db->all_tables(all_tables);
  if (all_tables.empty()) {
    session_event->set_response("No table\n");
  } else {
    std::stringstream ss;
    for (const auto &table : all_tables) {
      ss << table << std::endl;
    }
    session_event->set_response(ss.str().c_str());
  }
  return RC::SUCCESS;
}

RC ExecuteStage::do_desc_table(SQLStageEvent *sql_event)
{
  Query *query = sql_event->query();
  Db *db = sql_event->session_event()->session()->get_current_db();
  const char *table_name = query->sstr.desc_table.relation_name;
  Table *table = db->find_table(table_name);
  std::stringstream ss;
  if (table != nullptr) {
    table->table_meta().desc(ss);
  } else {
    ss << "No such table: " << table_name << std::endl;
  }
  sql_event->session_event()->set_response(ss.str().c_str());
  return RC::SUCCESS;
}

RC ExecuteStage::do_insert(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  if (stmt == nullptr) {
    LOG_WARN("cannot find statement");
    return RC::GENERIC_ERROR;
  }

  InsertStmt *insert_stmt = (InsertStmt *)stmt;
  Table *table = insert_stmt->table();

  RC rc = table->insert_record(trx, insert_stmt->value_amount(), insert_stmt->values());
  if (rc == RC::SUCCESS) {
    if (!session->is_trx_multi_operation_mode()) {
      CLogRecord *clog_record = nullptr;
      rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
      if (rc != RC::SUCCESS || clog_record == nullptr) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      rc = clog_manager->clog_append_record(clog_record);
      if (rc != RC::SUCCESS) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      trx->next_current_id();
      session_event->set_response("SUCCESS\n");
    } else {
      session_event->set_response("SUCCESS\n");
    }
  } else {
    session_event->set_response("FAILURE\n");
  }
  return rc;
}

RC ExecuteStage::do_delete(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  if (stmt == nullptr) {
    LOG_WARN("cannot find statement");
    return RC::GENERIC_ERROR;
  }

  DeleteStmt *delete_stmt = (DeleteStmt *)stmt;
  TableScanOperator scan_oper(delete_stmt->table());
  PredicateOperator pred_oper(delete_stmt->filter_stmt());
  pred_oper.add_child(&scan_oper);
  DeleteOperator delete_oper(delete_stmt, trx);
  delete_oper.add_child(&pred_oper);
  RC rc = delete_oper.open();
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
    if (!session->is_trx_multi_operation_mode()) {
      CLogRecord *clog_record = nullptr;
      rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
      if (rc != RC::SUCCESS || clog_record == nullptr) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      rc = clog_manager->clog_append_record(clog_record);
      if (rc != RC::SUCCESS) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      trx->next_current_id();
      session_event->set_response("SUCCESS\n");
    }
  }
  return rc;
}
// chenfr
RC ExecuteStage::do_update(SQLStageEvent *sql_event)
{
  Stmt *stmt = sql_event->stmt();
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  if (stmt == nullptr) {
    LOG_WARN("cannot find statement");
    return RC::GENERIC_ERROR;
  }

  UpdateStmt *update_stmt = (UpdateStmt *)stmt;

  Operator *scan_oper = try_to_create_index_scan_operator(update_stmt->filter_stmt(), update_stmt->table());
  if (nullptr == scan_oper) {
    scan_oper = new TableScanOperator(update_stmt->table());
  }

  DEFER([&]() { delete scan_oper; });

  //  TableScanOperator scan_oper(update_stmt->table());
  PredicateOperator pred_oper(update_stmt->filter_stmt());
  pred_oper.add_child(scan_oper);
  UpdateOperator update_oper(update_stmt, trx);
  update_oper.add_child(&pred_oper);
  RC rc = update_oper.open();

  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
    if (!session->is_trx_multi_operation_mode()) {
      CLogRecord *clog_record = nullptr;
      rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
      if (rc != RC::SUCCESS || clog_record == nullptr) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      rc = clog_manager->clog_append_record(clog_record);
      if (rc != RC::SUCCESS) {
        session_event->set_response("FAILURE\n");
        return rc;
      }

      trx->next_current_id();
      session_event->set_response("SUCCESS\n");
    }
  }
  return rc;
}

RC ExecuteStage::do_begin(SQLStageEvent *sql_event)
{
  RC rc = RC::SUCCESS;
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  session->set_trx_multi_operation_mode(true);

  CLogRecord *clog_record = nullptr;
  rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_BEGIN, trx->get_current_id(), clog_record);
  if (rc != RC::SUCCESS || clog_record == nullptr) {
    session_event->set_response("FAILURE\n");
    return rc;
  }

  rc = clog_manager->clog_append_record(clog_record);
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
  }

  return rc;
}

RC ExecuteStage::do_commit(SQLStageEvent *sql_event)
{
  RC rc = RC::SUCCESS;
  SessionEvent *session_event = sql_event->session_event();
  Session *session = session_event->session();
  Db *db = session->get_current_db();
  Trx *trx = session->current_trx();
  CLogManager *clog_manager = db->get_clog_manager();

  session->set_trx_multi_operation_mode(false);

  CLogRecord *clog_record = nullptr;
  rc = clog_manager->clog_gen_record(CLogType::REDO_MTR_COMMIT, trx->get_current_id(), clog_record);
  if (rc != RC::SUCCESS || clog_record == nullptr) {
    session_event->set_response("FAILURE\n");
    return rc;
  }

  rc = clog_manager->clog_append_record(clog_record);
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
  }

  trx->next_current_id();

  return rc;
}

RC ExecuteStage::do_clog_sync(SQLStageEvent *sql_event)
{
  RC rc = RC::SUCCESS;
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  CLogManager *clog_manager = db->get_clog_manager();

  rc = clog_manager->clog_sync();
  if (rc != RC::SUCCESS) {
    session_event->set_response("FAILURE\n");
  } else {
    session_event->set_response("SUCCESS\n");
  }

  return rc;
}
