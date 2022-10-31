#include "cartesian_operator.h"
#include "sql/stmt/filter_stmt.h"

#include <algorithm>

RC CartesianOperator::next()
{
  return RC::UNIMPLENMENT;
}
RC CartesianOperator::open()
{
  if (filter_stmt_ == nullptr) {
    return RC::SUCCESS;
  }

  const std::vector<FilterUnit *> &filter_units = filter_stmt_->filter_units();
  if (filter_units.empty()) {
    return RC::SUCCESS;
  }

  for (FilterUnit *filter_unit : filter_units) {
  // LOG_INFO("filter_units size: %d", filter_stmt_->filter_units().size());
  // for (FilterUnit *filter_unit : filter_stmt_->filter_units()) {
    if (filter_unit->left()->type() == ExprType::FIELD 
    && filter_unit->right()->type() == ExprType::FIELD) {
      FieldExpr *left_expr = static_cast<FieldExpr *>(filter_unit->left());
      FieldExpr *right_expr = static_cast<FieldExpr *>(filter_unit->right());
      // CompOp comp = filter_unit->comp();
      if (!strcmp(left_expr->table_name(), right_expr->table_name())) {
        continue;
      }
      int ltab = table_name2id_[left_expr->table_name()];
      int rtab = table_name2id_[right_expr->table_name()];
      LOG_INFO("left: %d(%s), right: %d(%s)", ltab, left_expr->field_name(), rtab, right_expr->field_name());
      LOG_INFO("add to %d", std::max(ltab, rtab));
      cen_contition_[std::max(ltab, rtab)].push_back(filter_unit);
    }
  }

  return RC::SUCCESS;
}
RC CartesianOperator::close()
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
Tuple *CartesianOperator::current_tuple()
{
  return nullptr;
}

// 李立基: 这里用了 execute_stage.cpp 中的 tuple_to_string() 函数, 单纯是用于测试的函数
void tuple_to_string1(std::ostream &os, const Tuple &tuple)
{
  TupleCell cell;
  RC rc = RC::SUCCESS;
  bool first_field = true;

  // 0 是 __trx
  for (int i = 1; i < tuple.cell_num(); i++) {
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
    LOG_INFO("debug for get_value. value = %d\n", *(int *)cell.data());
    cell.to_string(os);
  }
}

void CartesianOperator::dfs(std::ostream &os, int now_table, std::vector<int> &rec)
{
  LOG_INFO("now_table: %d, Enter.", now_table);

  if ((size_t)now_table == this->size()) {
    // TODO: result_ 的结果还没测试
    this->result_.push_back(std::vector<int>());
    std::vector<int> &new_result = result_[result_.size() - 1];
    for (size_t table_id = 0; table_id < this->size(); table_id++) {
      new_result.push_back(rec[table_id]);
    }
    return;
  }
  
  for (size_t row_id = 0; row_id < tuple_per_table_[now_table].size(); row_id++) {
    Tuple* tuple = tuple_per_table_[now_table][row_id];
    if (tuple == nullptr) {
      LOG_ERROR("tuple is nullptr.");
    }
    rec[now_table] = row_id;

    bool flag = true;
    std::list<FilterUnit *>::iterator filter_it;
    LOG_INFO("now_table: %d, filter size: %d\n", now_table, cen_contition_[now_table].size());
    for (filter_it = cen_contition_[now_table].begin(); filter_it != cen_contition_[now_table].end(); filter_it++) {
      FilterUnit *filter_unit = *filter_it;

      // 在 open() 函数中就必须保证这 2 个 expr 是 FieldExpr
      FieldExpr *left_expr = (FieldExpr *)filter_unit->left();
      FieldExpr *right_expr = (FieldExpr *)filter_unit->right();
      CompOp comp = filter_unit->comp();

      const Table *ltable = left_expr->get_table();
      if (ltable == nullptr) {
        LOG_ERROR("type(%d) error?", left_expr->type());
      }
      const Table *rtable = right_expr->get_table();
      if (rtable == nullptr) {
        LOG_ERROR("type(%d) error?", right_expr->type());
      }

      int tab_id_l = table_name2id_[ltable->name()];
      int tab_id_r = table_name2id_[rtable->name()];
      LOG_INFO("tab_id_l = %d, tab_id_r = %d\n", tab_id_l, tab_id_r);

      TupleCell left_cell, right_cell;
      if (this->find_cell(left_expr->field(), tab_id_l, rec[tab_id_l], left_cell) != RC::SUCCESS) {
        LOG_WARN("SOMETHING ERROR.");
      }
      if (this->find_cell(right_expr->field(), tab_id_r, rec[tab_id_r], right_cell) != RC::SUCCESS) {
        LOG_WARN("SOMETHING ERROR.");
      }

      const int compare = left_cell.compare(right_cell);
      bool filter_result = false;
      // 冗余代码
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

      if (!filter_result) {
        flag = false;
        break;
      }
    }
    if (flag) {
      dfs(os, now_table+1, rec);
    }
  }

  LOG_INFO("now_table: %d, End.", now_table);
}