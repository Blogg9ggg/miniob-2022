/* Copyright (c) 2021 Xie Meiyi(xiemeiyi@hust.edu.cn) and OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once


#include <vector>

#include "rc.h"
#include "sql/stmt/filter_stmt.h"

/*
 * 作者: 李立基
 * 说明: 目前这个类只是用来存放 inner join 语法中 on 后面的条件
 */
class InnerJoinStmt : public Stmt
{
public:
  InnerJoinStmt() = default;
  virtual ~InnerJoinStmt() = default;

public:
  StmtType type() const override { return StmtType::INNER_JOIN; }
  FilterStmt *on_condition_at(size_t ind) const
  {
    return on_conditions_[ind];
  }

  size_t on_condition_size() const { 
    return on_conditions_.size(); 
  }

  RC append_filter_stmt(FilterStmt *filter_stmt)
  {
    on_conditions_.push_back(filter_stmt);
    return RC::SUCCESS;
  }

private:
  std::vector<FilterStmt *>  on_conditions_;
};
