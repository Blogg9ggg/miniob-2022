//
// Created by chenfarong on 2022/10/24.
//


#include "sql/stmt/filter_stmt.h"
#include "operator.h"

class MergeOperator : public Operator
{
public:
  MergeOperator(FilterStmt *filter_stmt)
      : filter_stmt_(filter_stmt)
  {}

  virtual ~MergeOperator() = default;

  RC open() override;
  RC next() override;
  RC close() override;

  Tuple * current_tuple() override;
  //int tuple_cell_num() const override;
  //RC tuple_cell_spec_at(int index, TupleCellSpec &spec) const override;
private:
  bool do_merge(const std::vector<std::vector<Tuple *>>& tuples_set);
private:
  FilterStmt *filter_stmt_ = nullptr;
  std::vector<Tuple *> tuples_;
};
