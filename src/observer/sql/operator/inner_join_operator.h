#include "sql/stmt/filter_stmt.h"
#include "sql/stmt/inner_join_stmt.h"
#include "operator.h"
#include "sql/expr/tuple.h"

#include <memory>
#include <map>
#include <list>

class InnerJoinOperator : public Operator
{
public:
  InnerJoinOperator(InnerJoinStmt *inner_join_stmt, int table_num)
      : inner_join_stmt_(inner_join_stmt), table_num_(table_num)
  {
    tuple_per_table_.resize(table_num);
    speces_per_table_.resize(table_num);
  }
  virtual ~InnerJoinOperator() {
    // 这些 tuple 对象所占用的资源都是 InnerJoinOperator 申请的, 
    // 所以也应该由 InnerJoinOperator 销毁, 由此可见 CartesianOperator 的析构函数似乎有点问题.
    for (size_t i = 0; i < tuple_per_table_.size(); i++) {
      // for (Tuple *tuple : tuple_per_table_[i]) {
      //   delete tuple;
      // }
      tuple_per_table_[i].clear();
    }
    tuple_per_table_.clear();
    inner_join_stmt_ = nullptr;
    table_name2id_.clear();

    for (size_t i = 0; i < speces_per_table_.size(); i++) {
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
  RC update_tuple_vec(int id, Tuple *tuple) {
    if (id < 0 || (size_t)id >= this->size()) {
      return RC::NOTFOUND;
    }

    LOG_INFO("id: %d, Enter.", id);
    RowTuple *new_tuple = new RowTuple(*(RowTuple *)tuple);
    tuple_per_table_[id].push_back(new_tuple);

    return RC::SUCCESS;
  }
  RC update_spece_vec(int id, TupleCellSpec *spec) {
    if (id < 0 || (size_t)id >= this->size()) {
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
    for (size_t i = 1; i < speces_per_table_[table_id].size(); i++) {
      const FieldExpr *field_expr = (const FieldExpr *)speces_per_table_[table_id][i]->expression();
      const Field &field_tmp = field_expr->field();
      if (0 == strcmp(field_name, field_tmp.field_name())) {
        return tuple_per_table_[table_id][row_id]->cell_at(i, cell);
      }
    }

    return RC::GENERIC_ERROR;
  }

  int table_name2id(const char *table_name) { return table_name2id_[table_name]; }
  const size_t size() const { return table_num_; }
  const size_t result_size() const { return result_.size(); }
  RC result_at(int ind, std::vector<int> *&t_result) 
  {
    if (ind < 0 || (size_t)ind >= this->result_.size())
      return RC::INVALID_ARGUMENT;
    t_result = &result_[ind];
    return RC::SUCCESS;
  }

  RC open() override;
  RC next() override;
  RC close() override;
  Tuple * current_tuple() override;

  RC test(std::ostream &os) {
    RC rc = RC::SUCCESS;

    for (size_t i = 0; i < this->result_.size(); i++) {
      std::string s; 
      for (size_t j = 0; j < this->result_[i].size(); j++) {
        s += std::to_string(result_[i][j]);
        s += ", ";
      }
      LOG_INFO("result %d: %s", i, s.c_str());
    }

    // for (size_t i = 0; i < this->size(); i++) {
    //   LOG_INFO("table %d: ", i);
    //   for (size_t j = 0; j < this->tuple_per_table_[i].size(); j++) {
    //     LOG_INFO("ptr%d: %lx", j, (long)this->tuple_per_table_[i][j]);
    //   }
    // }

    // for (size_t i = 0; i < this->size(); i++) {
    //   for (size_t j = 0; j < speces_per_table_[i].size(); j++) {
    //     const FieldExpr *field_expr = (const FieldExpr *)speces_per_table_[i][j]->expression();
    //     const Field &field_tmp = field_expr->field();
    //     os << field_tmp.table_name() << "." << field_tmp.field_name() << " | ";
    //   } os << std::endl;

    //   for (size_t row_id = 0; row_id < tuple_per_table_[i].size(); row_id++) {
    //     Tuple *tuple = tuple_per_table_[i][row_id];
        
    //     for (size_t j = 0; j < speces_per_table_[i].size(); j++) {
    //       TupleCell cell;
    //       tuple->cell_at(j, cell);
    //       cell.to_string(os); 
    //       os << " | ";
    //     } os << std::endl;
    //   }
    // }

    return rc;
  }
  RC dfs(int now_table, std::vector<int> &rec);
  
private:
  int table_num_;
  InnerJoinStmt *inner_join_stmt_ = nullptr;
  // 李立基: 分表存储 tuple, tuple_per_table[table_id]: 该表过滤后剩余的 tuple
  std::vector<std::vector<Tuple *>> tuple_per_table_;
  // 李立基: 分表存储 spec 映射
  std::vector<std::vector<TupleCellSpec *>> speces_per_table_;
  // 将表名映射成 id
  std::map<std::string, int> table_name2id_;
  // 存放 dfs 的结果
  std::vector<std::vector<int>> result_;
  // 防止一鱼多吃
  std::vector<std::set<int>> vis; 
};
