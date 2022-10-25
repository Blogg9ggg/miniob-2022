#pragma once

#include "storage/common/field.h"
#include <string>

typedef enum
{
  no_fun,
  max_fun,
  min_fun,
  count_fun,
  avg_fun,
  sum_fun,
} aggregation_fun;

class AggrFuncCXX {
public:
  AggrFuncCXX(aggregation_fun type, Field field) : type_(type), field_(field) {}
  AggrFuncCXX(aggregation_fun type, std::string count_name) : type_(type), count_name_(count_name) {}
  int type() const { return type_; }
  const Field &field() const { return field_; }
  void set_count_name(const char *name) {
    count_name_ = name;
  }
  std::string count_name() {
    return count_name_;
  }
  
private:
  aggregation_fun type_;
  Field field_;
  std::string count_name_; // 李立基: for count
};