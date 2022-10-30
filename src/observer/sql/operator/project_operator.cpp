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
// Created by WangYunlai on 2022/07/01.
//

#include "common/log/log.h"
#include "sql/operator/operator.h"
#include "sql/operator/cartesian_operator.h"
#include "sql/operator/project_operator.h"
#include "storage/record/record.h"
#include "storage/common/table.h"

RC ProjectOperator::open()
{
  LOG_INFO("Enter.");
  if (children_.size() != 1) {
    LOG_WARN("project operator must has 1 child");
    return RC::INTERNAL;
  }

  Operator *child = children_[0];
  RC rc = child->open();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  return RC::SUCCESS;
}

RC ProjectOperator::next()
{
  return children_[0]->next();
}

RC ProjectOperator::close()
{
  if (children_[0] != nullptr) {
    children_[0]->close();
    delete children_[0];
  }
  
  children_.clear();
  
  return RC::SUCCESS;
}
Tuple *ProjectOperator::current_tuple()
{
  tuple_.set_tuple(children_[0]->current_tuple());
  return &tuple_;
}

void ProjectOperator::add_projection(const Table *table, const FieldMeta *field_meta)
{
  // 对单表来说，展示的(alias) 字段总是字段名称，
  // 对多表查询来说，展示的alias 需要带表名字
  TupleCellSpec *spec = new TupleCellSpec(new FieldExpr(table, field_meta));
  spec->set_alias(field_meta->name());
  tuple_.add_cell_spec(spec);
}

RC ProjectOperator::tuple_cell_spec_at(int index, const TupleCellSpec *&spec) const
{
  return tuple_.cell_spec_at(index, spec);
}

RC ProjectOperator::add_projection_CP(const Field &field) 
{
  this->fields_CP_.push_back(const_cast<Field *>(&field));
  return RC::SUCCESS;
}

RC ProjectOperator::print_title_CP(std::ostream &os)
{
  bool first_field = true;
  for (Field *field : fields_CP_) {
    if (first_field) first_field = false;
    else os << " | ";

    os << field->table_name() << "." << field->field_name();
  }
  os << std::endl;

  return RC::SUCCESS;
}

RC ProjectOperator::print_result_CP(std::ostream &os)
{
  print_title_CP(os);

  CartesianOperator *cartesian_oper = dynamic_cast<CartesianOperator *>(this->children_[0]);
  if (cartesian_oper == nullptr) {
    LOG_ERROR("isn't a (CartesianOperator *) ptr.");
    return RC::GENERIC_ERROR;
  }

  int len = cartesian_oper->result_size();

  for (int i = 0; i < len; i++) {
    const std::vector<int> *tmp_result = nullptr;
    if (cartesian_oper->result_at(i, tmp_result) != RC::SUCCESS) {
      LOG_WARN("i = %d(%d), SOMETHING ERROR", i, len);
      break;
    }
    if (tmp_result == nullptr) {
      LOG_WARN("SOMETHING ERROR");
      continue;
    }

    for (int j = 0; j < this->fields_CP_.size(); j++) {
      if (j > 0) {
        os << " | ";
      }

      Field &field = *fields_CP_[j];
      const char *table_name = field.table_name();
      int table_id = cartesian_oper->table_name2id(table_name);
      TupleCell cell;

      if (tmp_result->empty() || table_id >= tmp_result->size()) {
        LOG_WARN("SOMETHING ERROR");
        break;
      }
      cartesian_oper->find_cell(field, table_id, tmp_result->at(table_id), cell);

      cell.to_string(os);
    }
    os << std::endl;
  }

  return RC::SUCCESS;
}