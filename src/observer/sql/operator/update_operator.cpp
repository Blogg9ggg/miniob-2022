//
// Created by chenfarong on 2022/10/18.
//

#include "update_operator.h"

#include "common/log/log.h"
#include "sql/operator/update_operator.h"
#include "storage/record/record.h"
#include "storage/common/table.h"
#include "storage/trx/trx.h"
#include "sql/stmt/update_stmt.h"

RC UpdateOperator::open()
{
  if (children_.size() != 1) {
    LOG_WARN("delete operator must has 1 child");
    return RC::INTERNAL;
  }

  Operator *child = children_[0];
  RC rc = child->open();
  if (rc != RC::SUCCESS) {
    LOG_WARN("failed to open child operator: %s", strrc(rc));
    return rc;
  }

  Table *table = update_stmt_->table();
  TableMeta table_meta = table->table_meta();
  while (RC::SUCCESS == (rc = child->next())) {
    Tuple *tuple = child->current_tuple();
    if (nullptr == tuple) {
      LOG_WARN("failed to get current record: %s", strrc(rc));
      return rc;
    }

    RowTuple *row_tuple = static_cast<RowTuple *>(tuple);
    Record &old_record = row_tuple->record();
    // TODO: 多字段update支持
    const FieldMeta *field = table_meta.field(update_stmt_->update_field());
    int record_size = table_meta.record_size();
    char *new_record_data = new char[record_size];
    memcpy(new_record_data,old_record.data(),record_size);

    const Value &value = update_stmt_->values()[0];
    size_t copy_len = field->len();
    if (field->type() == CHARS) {
      const size_t data_len = strlen((const char *)value.data);
      if (copy_len > data_len) {
        copy_len = data_len + 1;
      }
    }
    memcpy(new_record_data+field->offset(),value.data,copy_len);
    Record new_record;
    new_record.set_data(new_record_data);
    new_record.set_rid(old_record.rid());
    rc = table->update_record(trx_, &old_record, &new_record);

    delete[] new_record_data;
    if (rc != RC::SUCCESS) {
      LOG_WARN("failed to delete record: %s", strrc(rc));
      return rc;
    }
  }
  return RC::SUCCESS;
}

RC UpdateOperator::next()
{
  return RC::RECORD_EOF;
}

RC UpdateOperator::close()
{
  children_[0]->close();
  return RC::SUCCESS;
}