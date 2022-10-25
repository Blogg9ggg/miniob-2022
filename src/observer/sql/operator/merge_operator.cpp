//
// Created by chenfarong on 2022/10/24.
//

#include "merge_operator.h"
RC MergeOperator::next()
{
  return LOCKED_SHAREDCACHE;
}
RC MergeOperator::open()
{
  RC rc = RC::SUCCESS;
  std::vector<std::vector<Tuple *>> tuples_set;
  for (const auto &pred_oper : children_) {
    pred_oper->open();
    std::vector<Tuple *> tuples;
    while ((rc = pred_oper->next()) == RC::SUCCESS) {
      Tuple *tuple = pred_oper->current_tuple();
      tuples.push_back(tuple);
    }
    tuples_set.push_back(tuples);
  }
  do_merge(tuples_set);
  return rc;
}
RC MergeOperator::close()
{
  for (const auto &c : children_) {
    c->close();
  }
  return RC::SUCCESS;
}
Tuple *MergeOperator::current_tuple()
{
  return nullptr;
}
bool MergeOperator::do_merge(const std::vector<std::vector<Tuple *>> &tuples_set)
{

  std::vector<FilterUnit *> merge_filter;
  for (const auto &filter : filter_stmt_->filter_units()) {
    if (filter->left()->type() == ExprType::FIELD && filter->right()->type() == ExprType::FIELD) {
      FieldExpr *left = (FieldExpr *)filter->left();
      FieldExpr *right = (FieldExpr *)filter->right();
      if (strcmp(left->table_name(), right->table_name()) != 0) {
        merge_filter.push_back(filter);
      }
    }
  }

  return false;
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