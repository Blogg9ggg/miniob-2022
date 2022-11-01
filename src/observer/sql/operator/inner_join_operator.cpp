#include "inner_join_operator.h"
#include "sql/stmt/filter_stmt.h"

#include <algorithm>

RC InnerJoinOperator::next()
{
  return RC::UNIMPLENMENT;
}
RC InnerJoinOperator::open()
{
  vis.resize(table_num_);
  return RC::SUCCESS;
}
RC InnerJoinOperator::close()
{
  for (Operator *child : children_) {
    if (child != nullptr) {
      child->close();
      delete child;
    }
  }
  children_.clear();
  return RC::SUCCESS;
}
Tuple *InnerJoinOperator::current_tuple()
{
  return nullptr;
}


bool check_compare_result(CompOp comp, int compare) {
  bool filter_result = false;
  switch (comp) {
    case EQUAL_TO: {
      filter_result = (0 == compare); 
    } break;
    case LESS_EQUAL: {
      filter_result = (compare <= 0); 
    } break;
    case NOT_EQUAL: {
      filter_result = (compare != 0);
    } break;
    case LESS_THAN: {
      filter_result = (compare < 0);
    } break;
    case GREAT_EQUAL: {
      filter_result = (compare >= 0);
    } break;
    case GREAT_THAN: {
      filter_result = (compare > 0);
    } break;
    default: {
      LOG_WARN("invalid compare type: %d", comp);
    } break;
    }

    return filter_result;
}
RC InnerJoinOperator::dfs(int now_table, std::vector<int> &rec)
{
  RC rc = RC::SUCCESS;
  LOG_INFO("now_table: %d, Enter.", now_table);

  if (now_table == 0) {
    for (int row_id = 0; row_id < tuple_per_table_[now_table].size(); row_id++) {
      rec[now_table] = row_id;
      rc = dfs(now_table + 1, rec);
      if (rc != RC::SUCCESS) {
        return rc;
      }
    }

    return RC::SUCCESS;
  }

  if ((size_t)now_table == this->size()) {
    this->result_.push_back(std::vector<int>());
    std::vector<int> &new_result = result_[result_.size() - 1];
    for (size_t table_id = 0; table_id < this->size(); table_id++) {
      new_result.push_back(rec[table_id]);
    }

    return RC::SUCCESS;
  }
  
  for (size_t row_id = 0; row_id < tuple_per_table_[now_table].size(); row_id++) {
    if (vis[now_table].count(row_id)) {
      continue;
    }

    rec[now_table] = row_id;
  
    FilterStmt *filter_stmt = this->inner_join_stmt_->on_condition_at(now_table - 1);
    std::vector<FilterUnit *> filter_units = filter_stmt->filter_units();

    bool filter_result = true;
    for (FilterUnit *filter_unit : filter_units) {
      CompOp comp = filter_unit->comp();
      TupleCell left_cell, right_cell;

      if (filter_unit->left()->type() == ExprType::FIELD) {
        FieldExpr *left_expr = (FieldExpr *)filter_unit->left();
        const Table *ltable = left_expr->get_table();
        if (ltable == nullptr) {
          LOG_ERROR("type(%d) error?", left_expr->type());
        }
        int tab_id_l = table_name2id_[ltable->name()];
        if (tab_id_l > now_table) {
          LOG_WARN("SOMETHING ERROR.");
          return RC::INVALID_ARGUMENT;
        }
        if (this->find_cell(left_expr->field(), tab_id_l, rec[tab_id_l], left_cell) != RC::SUCCESS) {
          LOG_WARN("SOMETHING ERROR.");
          return RC::INVALID_ARGUMENT;
        }
      } else {
        ValueExpr *left_expr = (ValueExpr *)filter_unit->left();
        left_expr->get_tuple_cell(left_cell);
      }

      if (filter_unit->right()->type() == ExprType::FIELD) {
        FieldExpr *right_expr = (FieldExpr *)filter_unit->right();
        const Table *rtable = right_expr->get_table();
        if (rtable == nullptr) {
          LOG_ERROR("type(%d) error?", right_expr->type());
        }
        int tab_id_r = table_name2id_[rtable->name()];
        if (tab_id_r > now_table) {
          LOG_WARN("SOMETHING ERROR.");
          return RC::INVALID_ARGUMENT;
        }
        if (this->find_cell(right_expr->field(), tab_id_r, rec[tab_id_r], right_cell) != RC::SUCCESS) {
          LOG_WARN("SOMETHING ERROR.");
          return RC::INVALID_ARGUMENT;
        }
      } else {
        ValueExpr *right_expr = (ValueExpr *)filter_unit->right();
        right_expr->get_tuple_cell(right_cell);
      }

      const int compare = left_cell.compare(right_cell);
      if (!check_compare_result(comp, compare)) {
        filter_result = false;
        break;
      }
    }

    if(filter_result) {
      return dfs(now_table + 1, rec);
    }
  }
  LOG_INFO("now_table: %d, End.", now_table);
  return RC::SUCCESS;
}