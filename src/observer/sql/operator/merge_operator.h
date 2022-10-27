//
// Created by chenfarong on 2022/10/24.
//


#include "sql/stmt/filter_stmt.h"
#include "operator.h"
#include <list>

class MergeOperator : public Operator
{
public:
  MergeOperator(FilterStmt *filter_stmt, int table_num)
      : filter_stmt_(filter_stmt)
  {
    tuple_per_table_.resize(table_num);
    cen_contition_.resize(table_num);
  }

  virtual ~MergeOperator() {
    filter_stmt_ = nullptr;
    tuple_per_table_.clear();
    table_name2id_.clear();
  }

  void update_map(std::string name, int id) {
    table_name2id_[name] = id;
  }
  void update_lists(int id, Tuple *tuple) {
    tuple_per_table_[id].push_back(tuple);
  }
  RC open() override;
  RC next() override;
  RC close() override;
  Tuple * current_tuple() override;

  void dfs(std::ostream &os, int now, std::vector<Tuple *> &rec);
  //int tuple_cell_num() const override;
  //RC tuple_cell_spec_at(int index, TupleCellSpec &spec) const override;
private:
  bool do_merge(const std::vector<std::vector<Tuple *>>& tuples_set);
private:
  FilterStmt *filter_stmt_ = nullptr;
  // 李立基: 分表存储 tuple, tuple_per_table[table_id]: 该表过滤后剩余的 tuple
  std::vector<std::list<Tuple *>> tuple_per_table_;
  // 将表名映射成 id
  std::map<std::string, int> table_name2id_;
  // dfs 各层需要满足的条件
  std::vector<std::list<FilterUnit *>> cen_contition_;
};
