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
        os << data_[i];
      }
    } break;
    default: {
      os << -1;
      LOG_WARN("unsupported attr type: %d", attr_type_);
    } break;
  }
}

int TupleCell::lcompare(const TupleCell &other) const
{
  return lcompare_string(this->data_, other.data_);
}

int TupleCell::compare(const TupleCell &other) const
{
  if (this->attr_type_ == other.attr_type_) {
    switch (this->attr_type_) {
      case INTS:
        return compare_int(this->data_, other.data_);
      // 李立基: 增加 date 字段
      case DATES:
        return compare_int(this->data_, other.data_);
      case FLOATS:
        return compare_float(this->data_, other.data_);
      case CHARS:
        return compare_string(this->data_, this->length_, other.data_, other.length_);
      default: {
        LOG_WARN("unsupported type: %d", this->attr_type_);
      }
    }
  } else {
    float this_data;
    switch (this->attr_type_) {
      case INTS:
        this_data = *(int *)data_;
        break;
      case CHARS:
        this_data = atof(this->data_);
        break;
      case FLOATS:
        this_data = *(float *)this->data_;
        break;
      default: {
        LOG_WARN("unsupported type: %d", this->attr_type_);
        return -1;
      }
    }
    float other_data;
    switch (other.attr_type_) {
      case INTS:
        other_data = *(int *)other.data_;
        break;
      case CHARS:
        other_data = atof(other.data_);
        break;
      case FLOATS:
        other_data = *(float *)other.data_;
        break;
      default: {
        LOG_WARN("unsupported type: %d", this->attr_type_);
        return -1;
      }
    }
    return compare_float(&this_data,&other_data);
  }
  LOG_WARN("not supported");
  return -1;  // TODO return rc?
}