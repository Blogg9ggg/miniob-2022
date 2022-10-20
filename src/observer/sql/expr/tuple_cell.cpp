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
// Created by WangYunlai on 2022/07/05.
//

#include "sql/expr/tuple_cell.h"
#include "storage/common/field.h"
#include "common/log/log.h"
#include "util/comparator.h"
#include "util/util.h"
#include "math.h"

void TupleCell::to_string(std::ostream &os) const
{
  switch (attr_type_) {
    case INTS: {
      os << *(int *)data_;
    } break;
    // 李立基: 增加 DATE 字段
    case DATES: {
      int v = *(int *)data_;
      os << date2string(v);
    } break;
    case FLOATS: {
      float v = *(float *)data_;
      os << double2string(v);
    } break;
    case CHARS: {
      for (int i = 0; i < length_; i++) {
        if (data_[i] == '\0') {
          break;
        }
      } break;
      default: {
        LOG_WARN("unsupported attr type: %d", attr_type_);
      } break;
    }
  }
}

int TupleCell::compare(const TupleCell &other) const
{
  if (this->attr_type_ == other.attr_type_) {
    switch (this->attr_type_) {
      case INTS: return compare_int(this->data_, other.data_);
      // 李立基: 增加 date 字段
      case DATES: return compare_int(this->data_, other.data_);
      case FLOATS: return compare_float(this->data_, other.data_);
      case CHARS: return compare_string(this->data_, this->length_, other.data_, other.length_);
      default: {
        LOG_WARN("unsupported type: %d", this->attr_type_);
      }
      if (other.attr_type_ == FLOATS) {
        float this_data = atof(this->data_);
        return compare_float(&this_data, other.data_);
      }
    }
  }
  LOG_WARN("not supported");
  return -1;  // TODO return rc?
}
