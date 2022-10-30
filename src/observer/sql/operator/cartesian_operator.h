//
// Created by chenfarong on 2022/10/24.
//


#include "sql/stmt/filter_stmt.h"
#include "operator.h"
#include "sql/expr/tuple.h"

#include <memory>
#include <list>

class CartesianOperator : public Operator
{
public:
  CartesianOperator(FilterStmt *filter_stmt, int table_num)
      : filter_stmt_(filter_stmt), table_num_(table_num)
  {
    tuple_per_table_.resize(table_num);
    cen_contition_.resize(table_num);
    speces_per_table_.resize(table_num);

  }

  virtual ~CartesianOperator() {
    tuple_per_table_.clear(); // TODO: 这么释放?

    filter_stmt_ = nullptr;
    table_name2id_.clear();
    for (int i = 0; i < speces_per_table_.size(); i++) {
      for (TupleCellSpec *spec : speces_per_table_[i]) {
        delete spec;
      }
      speces_per_table_[i].clear();
    }
    speces_per_table_.clear();
  }

  void update_map(std::string name, int id) {
    table_name2id_[name] = id;
  }
  RC update_lists(int id, Tuple *tuple) {
    if (id < 0 || id >= this->size()) {
      return RC::NOTFOUND;
    }

    LOG_INFO("id: %d, Enter.", id);
    RowTuple *new_tuple = new RowTuple(*(RowTuple *)tuple);
    tuple_per_table_[id].push_back(new_tuple);

    // test
    // TupleCell cell;
    // for (int i = 0; i < 2; i++) {
    //   tuple->cell_at(i, cell);
    //   LOG_INFO("check update: %d", *(int *)cell.data());
    // }
    // LOG_INFO("id: %d, End.", id);

    return RC::SUCCESS;
  }
  RC update_speces(int id, TupleCellSpec *spec) {
    if (id < 0 || id >= this->size()) {
      return RC::NOTFOUND;
    }
    
    speces_per_table_[id].push_back(spec);
    return RC::SUCCESS;
  }

  // 李立基: 这里其实应该写一个该操作专属的 tuple 的.
  // 输入: field, 用于找列; table_id, 指示表的 id; row_id, 指示行号
  // 输出: cell
  RC find_cell(const Field &field, int table_id, int row_id, TupleCell &cell) {
    const char *field_name = field.field_name();

    // 第 0 列是 __trx
    for (int i = 1; i < speces_per_table_[table_id].size(); i++) {
      const FieldExpr *field_expr = (const FieldExpr *)speces_per_table_[table_id][i]->expression();
      const Field &field_tmp = field_expr->field();
      if (0 == strcmp(field_name, field_tmp.field_name())) {
        return tuple_per_table_[table_id][row_id]->cell_at(i, cell);
      }
    }

    return RC::GENERIC_ERROR;
  }

  int table_name2id(const char *table_name) {
    return table_name2id_[table_name];
  }

  RC open() override;
  RC next() override;
  RC close() override;
  Tuple * current_tuple() override;

  RC test(std::ostream &os) {
    RC rc = RC::SUCCESS;

    for (int i = 0; i < this->result_.size(); i++) {
      std::string s; 
      for (int j = 0; j < this->result_[i].size(); j++) {
        s += std::to_string(result_[i][j]);
        s += ", ";
      }
      LOG_INFO("result %d: %s", i, s.c_str());
    }

    for (int i = 0; i < this->size(); i++) {
      LOG_INFO("table %d: ", i);
      for (int j = 0; j < this->tuple_per_table_[i].size(); j++) {
        LOG_INFO("ptr%d: %lx", j, (long)this->tuple_per_table_[i][j]);
      }
    }

    for (int i = 0; i < this->size(); i++) {
      for (int j = 0; j < speces_per_table_[i].size(); j++) {
        const FieldExpr *field_expr = (const FieldExpr *)speces_per_table_[i][j]->expression();
        const Field &field_tmp = field_expr->field();
        os << field_tmp.table_name() << "." << field_tmp.field_name() << " | ";
      } os << std::endl;

      for (int row_id = 0; row_id < tuple_per_table_[i].size(); row_id++) {
        Tuple *tuple = tuple_per_table_[i][row_id];
        
        for (int j = 0; j < speces_per_table_[i].size(); j++) {
          TupleCell cell;
          tuple->cell_at(j, cell);
          cell.to_string(os); 
          os << " | ";
        } os << std::endl;
      }
    }

    return rc;
  }
  void dfs(std::ostream &os, int now_table, std::vector<int> &rec);

  const int size() const { return table_num_; }
  const int result_size() const { return result_.size(); }
  RC result_at(int ind, std::vector<int> *&t_result) 
  {
    if (ind < 0 || ind >= this->result_.size())
      return RC::INVALID_ARGUMENT;
    t_result = &result_[ind];
    return RC::SUCCESS;
  }
  
  //int tuple_cell_num() const override;
  //RC tuple_cell_spec_at(int index, TupleCellSpec &spec) const override;
private:
  bool do_merge(const std::vector<std::vector<Tuple *>>& tuples_set);
private:
  int table_num_;
  FilterStmt *filter_stmt_ = nullptr;
  // 李立基: 分表存储 tuple, tuple_per_table[table_id]: 该表过滤后剩余的 tuple
  std::vector<std::vector<Tuple *>> tuple_per_table_;
  // 李立基: 分表存储 spec 映射
  std::vector<std::vector<TupleCellSpec *>> speces_per_table_;
  // 将表名映射成 id
  std::map<std::string, int> table_name2id_;
  // dfs 各层需要满足的条件
  std::vector<std::list<FilterUnit *>> cen_contition_;
  // 存放 dfs 的结果
  std::vector<std::vector<int>> result_;
};
