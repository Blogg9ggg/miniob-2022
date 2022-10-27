//
// Created by chenfarong on 2022/10/24.
//

#include "merge_operator.h"
#include "sql/stmt/filter_stmt.h"

#include <algorithm>

RC MergeOperator::next()
{
  return RC::UNIMPLENMENT;
}
RC MergeOperator::open()
{
  for (FilterUnit *filter_unit : filter_stmt_->filter_units()) {
    if (filter_unit->left()->type() == ExprType::FIELD 
    && filter_unit->right()->type() == ExprType::FIELD) {
      FieldExpr *left_expr = static_cast<FieldExpr *>(filter_unit->left());
      FieldExpr *right_expr = static_cast<FieldExpr *>(filter_unit->right());
      CompOp comp = filter_unit->comp();
      if (!strcmp(left_expr->table_name(), right_expr->table_name())) {
        continue;
      }
      int ltab = table_name2id_[left_expr->table_name()];
      int rtab = table_name2id_[right_expr->table_name()];
      LOG_INFO("left: %d(%s), right: %d(%s)\n", ltab, left_expr->field_name(), rtab, right_expr->field_name());

      cen_contition_[std::max(ltab, rtab)].push_back(filter_unit);
    }
  }
}
RC MergeOperator::close()
{
  return RC::UNIMPLENMENT;
}
Tuple *MergeOperator::current_tuple()
{
  return nullptr;
}

// 李立基: 这里用了 execute_stage.cpp 中的 tuple_to_string() 函数, 有空把它们合并一下
void tuple_to_string1(std::ostream &os, const Tuple &tuple)
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
    LOG_INFO("debug for get_value. value = %d\n", *(int *)cell.data());
    cell.to_string(os);
  }
}

void MergeOperator::dfs(std::ostream &os, int now, std::vector<Tuple *> &rec)
{
  // test
  if (now == tuple_per_table_.size()) {
    for (Tuple *tuple : rec) {
      tuple_to_string1(os, *tuple); // 李立基: 粗糙版本
    }
    os << "===============================" << std::endl;
    return;
  }
  for (Tuple *tuple : tuple_per_table_[now]) {
    rec[now] = tuple;
    bool flag = true;
    for (FilterUnit *filter_unit : cen_contition_[now]) {
      Expression *left_expr = filter_unit->left();
      Expression *right_expr = filter_unit->right();
      CompOp comp = filter_unit->comp();
      int ltab = table_name2id_[left_expr->table_name()];
      int rtab = table_name2id_[right_expr->table_name()];
      Tuple *ltuple = rec[ltab];
      Tuple *rtuple = rec[rtab];

      // ====== debug: 看看 ltuple ======
      TupleCell dcell;
      for (int i = 0; i < ltuple->cell_num(); i++) {
        RC rc = ltuple->cell_at(i, dcell);
        if (rc != RC::SUCCESS) {
          LOG_WARN("failed to fetch field of cell. index=%d, rc=%s", i, strrc(rc));
          break;
        }
        LOG_INFO("debug for get_value. value = %d\n", *(int *)dcell.data());
      }
      // ====== debug: 看看 ltuple ======

      TupleCell left_cell, right_cell;
      if (left_expr->get_value(*ltuple, left_cell) != RC::SUCCESS) {
        // TODO
        LOG_WARN("SOMETHING ERROR.");
      }
      if (right_expr->get_value(*rtuple, right_cell) != RC::SUCCESS) {
        // TODO
        LOG_WARN("SOMETHING ERROR.");
      }

      const int compare = left_cell.compare(right_cell);
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

      if (!filter_result) {
        flag = false;
        break;
      }
    }
    if (flag) {
      dfs(os, now+1, rec);
    }
  }
  // if (now == cen_contition_.size()) {

  //   return;
  // }
  // for (FilterUnit *filter_unit : cen_contition_[now]) {
  //   FieldExpr *left_expr = static_cast<FieldExpr *>(filter_unit->left());
  //   FieldExpr *right_expr = static_cast<FieldExpr *>(filter_unit->right());
  //   CompOp comp = filter_unit->comp();
  //   int ltab = table_name2id_[left_expr->table_name()];
  //   int rtab = table_name2id_[right_expr->table_name()];
  //   Tuple *ltuple = rec[ltab];
  //   Tuple *rtuple = rec[rtab];

  //   TupleCell left_cell, right_cell;
  //   if (left_expr->get_value(ltuple, left_cell) != RC::SUCCESS) {
  //     // TODO
  //     LOG_WARN("SOMETHING ERROR.");
  //   }
  //   if (right_expr->get_value(rtuple, right_cell) != RC::SUCCESS) {
  //     // TODO
  //     LOG_WARN("SOMETHING ERROR.");
  //   }

  //   const int compare = left_cell.compare(right_cell);
  //   bool filter_result = false;
  //   switch (comp) {
  //   case EQUAL_TO: {
  //     filter_result = (0 == compare); 
  //   } break;
  //   case LESS_EQUAL: {
  //     filter_result = (compare <= 0); 
  //   } break;
  //   case NOT_EQUAL: {
  //     filter_result = (compare != 0);
  //   } break;
  //   case LESS_THAN: {
  //     filter_result = (compare < 0);
  //   } break;
  //   case GREAT_EQUAL: {
  //     filter_result = (compare >= 0);
  //   } break;
  //   case GREAT_THAN: {
  //     filter_result = (compare > 0);
  //   } break;
  //   default: {
  //     LOG_WARN("invalid compare type: %d", comp);
  //   } break;
  //   }

  //   if (!filter_result) {
  //     return;
  //   }
  // }
  // rec[now] = 
}

//void MergeOperator::do_merge_help(
//    const std::vector<std::vector<Tuple &>> &tuples_set, const std::vector<FilterUnit *> &merge_filter, int index, CompositeTuple& select_tuples)
//{
//  if (index == tuples_set.size()){
//    CompositeTuple *tuple = new CompositeTuple();
//    for (const auto &t : select_tuples->get_tuples()){
//      tuple->add_tuple(t);
//    }
//    tuples_.push_back(tuple);
//    return;
//  }
//  for (Tuple &next : tuples_set[index]){
//    for (FilterUnit *filter : merge_filter){
//      FieldExpr *left = (FieldExpr *)filter->left();
//      FieldExpr *right = (FieldExpr *)filter->right();
//      TupleCell left_cell;
//      TupleCell right_cell;
//      if (RC::SUCCESS == left->get_value(next,left_cell) && right->get_value(select_tuples,right){
//
//}
//    }
//  }
//}