//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_scan_executor.cpp
//
// Identification: src/execution/index_scan_executor.cpp
//
// Copyright (c) 2015-19, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include "execution/executors/index_scan_executor.h"

namespace bustub {
IndexScanExecutor::IndexScanExecutor(ExecutorContext *exec_ctx, const IndexScanPlanNode *plan)
    : AbstractExecutor(exec_ctx) {}

void IndexScanExecutor::Init() {
    table_oid_t table_id=plan_->GetIndexOid();
    table_info_=exec_ctx_->GetCatalog()->GetTable(table_id);
    index_oid_t index_id=plan_->index_oid_;
    index_info_=exec_ctx_->GetCatalog()->GetIndex(index_id);
    htable_=dynamic_cast<HashTableIndexForTwoIntegerColumn *>(index_info_->index_.get());
    //将索引对象转换为特定类型的哈希索引（HashTableIndexForTwoIntegerColumn）。这表明索引是基于哈希表实现的，并且是针对两个整数列设计的特殊哈希索引结构。
    //index_info_->index_.get()：获取索引对象的指针。
    std::vector<Value>values{};
    assert(!plan_->pred_keys_.empty());
   // values.push_back(plan_->pred_keys_);
    Tuple key_tuple=Tuple(values,&index_info_->key_schema_);
    htable_->ScanKey(key_tuple,&rids_,exec_ctx_->GetTransaction());
    index_num_=0;
}
auto IndexScanExecutor::Next(Tuple *tuple, RID *rid) -> bool {
     auto &table_heap=table_info_->table_;
     while(true)
     {
        if(index_num_>=rids_.size())
        {
            return false;
        }
        *rid = rids_[index_num_++];
        *tuple=table_heap->GetTuple(*rid).second;
        //table_heap->GetTuple(*rid) 返回的是一个 std::pair<TupleMeta, Tuple>，
        //这个 pair 的第二个元素（.second）是我们实际需要的元组（Tuple）
        auto tuple_meta=table_heap->GetTupleMeta(*rid);
        if(!tuple_meta.is_deleted_)
        {
            if(plan_->filter_predicate_)
            {
                auto &filter_expr=plan_->filter_predicate_;
                Value value=filter_expr->Evaluate(tuple,GetOutputSchema());
                if(!value.IsNull()&&value.GetAs<bool>())
                {
                    return true;
                }
            }
            return true;
        }
     return false; 
    }


}  // namespace bustub
