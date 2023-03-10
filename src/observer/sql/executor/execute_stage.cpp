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
#include<memory>

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
#include "sql/stmt/inner_join_stmt.h"
#include "sql/stmt/aggregate_function.h"
#include "storage/common/table.h"
#include "storage/common/field.h"
#include "storage/index/index.h"
#include "storage/default/default_handler.h"
#include "storage/common/condition_filter.h"
#include "storage/trx/trx.h"
#include "storage/clog/clog.h"
#include "sql/operator/update_operator.h"
#include "sql/operator/cartesian_operator.h"
#include "sql/operator/inner_join_operator.h"

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
      //????????????: ???????????? 
      case SCF_CREATE_UNIQUE_INDEX: {
         do_create_unique_index(sql_event);
      } break;
      case SCF_SHOW_TABLES: {
        do_show_tables(sql_event);
      } break;
      case SCF_DESC_TABLE: {
        do_desc_table(sql_event);
      } break;

        // ?????????: ????????? SCF_SHOW_INDEX ?????????
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
    // TODO: ??????????????????????????????
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
  for (int i = 0; i < tuple.cell_num(); i++) {  // TODO: ?????????
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

  // ????????????????????????????????????????????????????????????????????????????????????????????????????????????
  // ?????????????????????????????????????????????????????????
  // ???????????????????????????????????????????????????????????????????????????????????????
  // ?????????????????????????????????????????????????????????????????????????????????. (???????????????????)
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
    // chenfarong: ???????????????????????????
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
 * ??????: ?????????
 * ??????: ?????? max ????????????
 */
RC do_aggr_func_max(std::ostream &os, ProjectOperator *project_oper)
{
  RC rc = RC::SUCCESS;
  // print_tuple_header(os, project_oper, "MAX");

  rc = project_oper->next();
  if (rc == RC::RECORD_EOF) {
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper->current_tuple();
  TupleCell max_cell, cell; 
  if ((rc = tuple->cell_at(0, max_cell)) != RC::SUCCESS) {
    LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
    return rc;
  }

  while ((rc = project_oper->next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper->current_tuple();
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
      max_cell = cell; // TODO: ?????????????????????
    }
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  max_cell.to_string(os);
  // os << std::endl;

  return RC::SUCCESS;
}

/*
 * ??????: ?????????
 * ??????: ?????? min ????????????
 */
RC do_aggr_func_min(std::ostream &os, ProjectOperator *project_oper)
{
  RC rc = RC::SUCCESS;

  rc = project_oper->next();
  if (rc == RC::RECORD_EOF) {
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper->current_tuple();
  TupleCell min_cell, cell; 
  if ((rc = tuple->cell_at(0, min_cell)) != RC::SUCCESS) {
    LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", 0, strrc(rc));
    return rc;
  }

  while ((rc = project_oper->next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper->current_tuple();
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
      min_cell = cell; // TODO: ?????????????????????
    }
  }
  if (rc != RC::RECORD_EOF) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
  }
  
  min_cell.to_string(os);
  // rc = project_oper.close();

  return RC::SUCCESS;
}

/*
 * ??????: ?????????
 * ??????: ?????? sum ????????????
 */
RC do_aggr_func_sum(std::ostream &os, ProjectOperator *project_oper)
{
  RC rc = RC::SUCCESS;

  float ans = 0;
  rc = project_oper->next();
  if (rc == RC::RECORD_EOF) {
    os << "0" << std::endl;
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper->current_tuple();
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

  while ((rc = project_oper->next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper->current_tuple();
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

  return RC::SUCCESS;
}

/*
 * ??????: ?????????
 * ??????: ?????? avg ????????????
 */
RC do_aggr_func_avg(std::ostream &os, ProjectOperator *project_oper)
{
  RC rc = RC::SUCCESS;

  float ans = 0;
  int cnt = 0;
  rc = project_oper->next();
  if (rc == RC::RECORD_EOF) {
    os << "0" << std::endl;
    return rc;
  } else if (rc != RC::SUCCESS) {
    LOG_WARN("something wrong while iterate operator. rc=%s", strrc(rc));
    return rc;
  }

  Tuple *tuple = project_oper->current_tuple();
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

  while ((rc = project_oper->next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper->current_tuple();
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

  return RC::SUCCESS;
}

/*
 * ??????: ?????????
 * ??????: ?????? count ????????????
 */
RC do_aggr_func_count(std::ostream &os, ProjectOperator *project_oper)
{
  RC rc = RC::SUCCESS;

  Tuple *tuple;
  int ans = 0;
  while ((rc = project_oper->next()) == RC::SUCCESS) {
    // get current record
    // write to response
    tuple = project_oper->current_tuple();
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

  return RC::SUCCESS;
}

// TODO: ????????????????????????????????????, ????????????????????????????????????
RC ExecuteStage::do_select(SQLStageEvent *sql_event)
{
  SelectStmt *select_stmt = (SelectStmt *)(sql_event->stmt());
  SessionEvent *session_event = sql_event->session_event();
  std::stringstream ss;
  RC rc = RC::SUCCESS;

  // ================== Inner Join????????? =================
  if (select_stmt->inner_join_stmt() != nullptr) {
    LOG_INFO("inner join lab.");
    InnerJoinOperator *inner_join_oper = 
      new InnerJoinOperator(select_stmt->inner_join_stmt(), select_stmt->tables().size());
     
    int tid = 0;
    // ??????????????????????????????, ??????????????????????????????
    for (size_t t = 0; t < select_stmt->tables().size(); t++) {
      Table *now_table = select_stmt->tables()[t];
      const TableMeta now_table_meta = now_table->table_meta();
      const char *now_table_name = now_table->name();
      inner_join_oper->update_map(static_cast<std::string>(now_table_name), tid);

      const std::vector<FieldMeta> *field_metas = now_table->table_meta().field_metas();
      for(size_t i = 0; i < field_metas->size(); i++) {
        FieldExpr *field_expr = new FieldExpr(now_table, &field_metas->at(i));
        TupleCellSpec *tuple_cell_spec = new TupleCellSpec(field_expr);
        inner_join_oper->update_spece_vec(tid, tuple_cell_spec);
      }

      Operator *scan_oper = try_to_create_index_scan_operator(select_stmt->filter_stmt(), now_table);
      if (nullptr == scan_oper) {
        scan_oper = new TableScanOperator(now_table);
      }
      PredicateOperator *pred_oper = new PredicateOperator(select_stmt->filter_stmt());
      pred_oper->add_child(scan_oper);
      inner_join_oper->add_child(pred_oper);

      pred_oper->open();
      while ((rc = pred_oper->next()) == RC::SUCCESS) {
        // ?????? TableScanOperator ?????????????????????, ????????? tuple ??????????????????, ??????????????????????????????????????????
        Tuple *tuple = pred_oper->current_tuple();
        
        if (nullptr == tuple) {
          rc = RC::INTERNAL;
          LOG_WARN("failed to get current record. rc=%s", strrc(rc));
          break;
        }

        if (inner_join_oper->update_tuple_vec(tid, tuple) != RC::SUCCESS) {
          LOG_WARN("SOMETHING ERROR.");
        }
      }

      // Endding
      tid++;
    }

    ProjectOperator *project_oper = new ProjectOperator();
    project_oper->add_child(inner_join_oper);

    if ((rc = project_oper->open()) != RC::SUCCESS) {
      return rc;
    }

    std::vector<int> tmp(inner_join_oper->size());
    LOG_INFO("inner join oper size = %d", inner_join_oper->size());
    inner_join_oper->dfs(0, tmp);

    inner_join_oper->test(ss);

    
    // ???????????????
    for (const Field &field : select_stmt->query_fields()) {
      project_oper->add_projection_CP(field);
    }
    project_oper->print_result_CP_new(ss);

    session_event->set_response(ss.str());// ??????

    project_oper->close();
    delete project_oper;
    project_oper = nullptr;

    return RC::SUCCESS;
  }

  // =================== ????????????????????? ===================
  if (select_stmt->tables().size() != 1) {
    CartesianOperator *merge_oper = 
      new CartesianOperator(select_stmt->filter_stmt(), select_stmt->tables().size());

    int tid = 0;
    // for (int t = 0; t < select_stmt->tables().size(); t++) {
    for (int t = select_stmt->tables().size() - 1; t >= 0; t--) {
      Table *now_table = select_stmt->tables()[t];
      const TableMeta now_table_meta = now_table->table_meta();
      const char *now_table_name = now_table->name();
      merge_oper->update_map(static_cast<std::string>(now_table_name), tid);

      const std::vector<FieldMeta> *field_metas = now_table->table_meta().field_metas();
      for(size_t i = 0; i < field_metas->size(); i++) {
        FieldExpr *field_expr = new FieldExpr(now_table, &field_metas->at(i));
        TupleCellSpec *tuple_cell_spec = new TupleCellSpec(field_expr);
        merge_oper->update_speces(tid, tuple_cell_spec);
      }

      Operator *scan_oper = try_to_create_index_scan_operator(select_stmt->filter_stmt(), now_table);
      if (nullptr == scan_oper) {
        scan_oper = new TableScanOperator(now_table);
      }
      PredicateOperator *pred_oper = new PredicateOperator(select_stmt->filter_stmt());
      pred_oper->add_child(scan_oper);

      merge_oper->add_child(pred_oper);

      pred_oper->open();
      while ((rc = pred_oper->next()) == RC::SUCCESS) {
        // get current record
        // write to response
        // ?????? TableScanOperator ?????????????????????, ????????? tuple ??????????????????, ??????????????????????????????????????????
        Tuple *tuple = pred_oper->current_tuple();
        
        if (nullptr == tuple) {
          rc = RC::INTERNAL;
          LOG_WARN("failed to get current record. rc=%s", strrc(rc));
          break;
        }

        if (merge_oper->update_lists(tid, tuple) != RC::SUCCESS) {
          LOG_WARN("SOMETHING ERROR.");
        }
      }

      // Endding
      tid++;
    }

    ProjectOperator *project_oper = new ProjectOperator();
    project_oper->add_child(merge_oper);

    if ((rc = project_oper->open()) != RC::SUCCESS) {
      return rc;
    }

    // ???????????????????????????
    std::vector<int> tmp(merge_oper->size());
    merge_oper->dfs(ss, 0, tmp);
    // merge_oper->test(ss);
    
    // ???????????????
    for (const Field &field : select_stmt->query_fields()) {
      project_oper->add_projection_CP(field);
    }
    project_oper->print_result_CP(ss);

    session_event->set_response(ss.str());// ??????

    project_oper->close();
    delete project_oper;
    project_oper = nullptr;
    // merge_oper->close();
    // delete merge_oper;
    // merge_oper = nullptr;

    return RC::SUCCESS;
  }

  // ?????????: ???????????????????????????????????????????????? case, ????????????????????????????????????.
  // =================== ???????????? ===================
  if (select_stmt->aggr_funcs().size() > 0) {
    Table *default_table = select_stmt->tables()[0]; // ??????
    Operator *scan_oper = try_to_create_index_scan_operator(select_stmt->filter_stmt(), select_stmt->tables()[0]);
    if (nullptr == scan_oper) {
      scan_oper = new TableScanOperator(select_stmt->tables()[0]);
    }
    // DEFER([&]() { delete scan_oper; });
    PredicateOperator *pred_oper = new PredicateOperator(select_stmt->filter_stmt());
    pred_oper->add_child(scan_oper);

    // ?????????: ???????????????
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
        ProjectOperator *project_oper = new ProjectOperator();
        project_oper->add_child(pred_oper);
        // LOG_INFO("table_name: %s, field_name: %s\n", aggr_func.field().table()->name(), aggr_func.field().meta()->name());
        project_oper->add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper->open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_max(ss, project_oper);

        delete project_oper;
        project_oper = nullptr;
      } break;
      case aggregation_fun::min_fun: {
        ProjectOperator *project_oper = new ProjectOperator();
        project_oper->add_child(pred_oper);
        project_oper->add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper->open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_min(ss, project_oper);
        
        delete project_oper;
        project_oper = nullptr;
      } break;
      case aggregation_fun::avg_fun: {
        ProjectOperator *project_oper = new ProjectOperator();
        project_oper->add_child(pred_oper);
        project_oper->add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper->open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_avg(ss, project_oper);
        
        delete project_oper;
        project_oper = nullptr;
      } break;
      case aggregation_fun::sum_fun: {
        ProjectOperator *project_oper = new ProjectOperator();
        project_oper->add_child(pred_oper);
        project_oper->add_projection(aggr_func.field().table(), aggr_func.field().meta());

        rc = project_oper->open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");
          return rc;
        }

        rc = do_aggr_func_sum(ss, project_oper);
        
        delete project_oper;
        project_oper = nullptr;
      } break;
      case aggregation_fun::count_fun: {
        ProjectOperator *project_oper = new ProjectOperator();
        project_oper->add_child(pred_oper);
        
        const char *field_name = aggr_func.count_name().c_str();
        if (strcmp(field_name, "*") != 0 && strcmp(field_name, "1") != 0) {
          if (default_table->table_meta().field(field_name) == nullptr) {
            session_event->set_response("FAILURE\n");

            delete project_oper;
            return RC::SUCCESS;
          }
        }

        // TODO: ????????????????????????, ?????????????????? field, ????????????
        project_oper->add_projection(default_table, default_table->table_meta().field(1));
        //default_table->table_meta().field(1).meta());

        rc = project_oper->open();
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to open operator");

          delete project_oper;
          return rc;
        }

        rc = do_aggr_func_count(ss, project_oper);
        
        delete project_oper;
        project_oper = nullptr;
      } break;
      
      default:
        break;
      }
    }
    ss << std::endl;
    session_event->set_response(ss.str());

    return RC::SUCCESS;
  }

  // =================== ???????????? ===================
  // Table *default_table = select_stmt->tables()[0]; // default_table ??????????????????, ????????????
  Operator *scan_oper = try_to_create_index_scan_operator(select_stmt->filter_stmt(), select_stmt->tables()[0]);
  if (nullptr == scan_oper) {
    LOG_INFO("have no index.");
    scan_oper = new TableScanOperator(select_stmt->tables()[0]);
  }
  // DEFER([&]() { delete scan_oper; });
  PredicateOperator *pred_oper = new PredicateOperator(select_stmt->filter_stmt());
  pred_oper->add_child(scan_oper);

  ProjectOperator *project_oper = new ProjectOperator();
  project_oper->add_child(pred_oper);
  for (const Field &field : select_stmt->query_fields()) {
    project_oper->add_projection(field.table(), field.meta());
  }
  rc = project_oper->open();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open operator");
    return rc;
  }

  print_tuple_header(ss, *project_oper);
  while ((rc = project_oper->next()) == RC::SUCCESS) {
    // get current record
    // write to response
    Tuple *tuple = project_oper->current_tuple();
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
  rc = project_oper->close();
  
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
  RC rc = table->create_index(nullptr, create_index.index_name, create_index.attribute_name,false);

  sql_event->session_event()->set_response(rc == RC::SUCCESS ? "SUCCESS\n" : "FAILURE\n");
  return rc;
}
/*
 * ??????: ????????????
 * ??????: ?????? unique index ?????? 
 * #dodo  ??????do_create_index ???2??????????????????????????????????????????
 */
RC ExecuteStage::do_create_unique_index(SQLStageEvent *sql_event)
{
  SessionEvent *session_event = sql_event->session_event();
  Db *db = session_event->session()->get_current_db();
  const CreateIndex &create_index = sql_event->query()->sstr.create_index;
  Table *table = db->find_table(create_index.relation_name);
  if (nullptr == table) {
    session_event->set_response("FAILURE\n");
    return RC::SCHEMA_TABLE_NOT_EXIST;
  }
  // ????????????????????????table->create_index ????????????????????? create_index ?????????????????? ?????????????????? ????????? ???????????????
  RC rc = table->create_index(nullptr, create_index.index_name, create_index.attribute_name, true);

  sql_event->session_event()->set_response(rc == RC::SUCCESS ? "SUCCESS\n" : "FAILURE\n");
  return rc;
}
/*
 * ??????: ?????????
 * ??????: ?????? show index ??????
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
      // TODO: ??????????????????????????????, ????????????
      // TODO: ????????????????????????, ????????????????????????????????? 1
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
 //????????????????????????????????????inserts???
 // RC rc = table->insert_record(trx, insert_stmt->value_amount(), insert_stmt->values());
 RC rc = table->insert_record_mult_rows(trx, insert_stmt->values_amount(), insert_stmt->values_item());
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
