//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// aggregation_executor.cpp
//
// Identification: src/execution/aggregation_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#include <memory>
#include <vector>

#include "execution/executors/aggregation_executor.h"

namespace bustub {

AggregationExecutor::AggregationExecutor(ExecutorContext *exec_ctx, const AggregationPlanNode *plan,
                                         std::unique_ptr<AbstractExecutor> &&child_executor)
    : AbstractExecutor(exec_ctx),plan_(plan),
      child_executor_(std::move(child_executor)),
      aht_(SimpleAggregationHashTable(plan->aggregates_, plan->agg_types_)),
      aht_iterator_(aht_.Begin()),
      special_case_end_(false) {}

void AggregationExecutor::Init() {
    child_executor_->Init();
    aht_.GenerateInitialAggregateValue();
    // 初始化聚合哈希表的聚合值，以便处理空输入的情况。
    // 通常，数据库执行器在没有任何输入时，仍需要处理 COUNT 或其他聚合操作。
    // 例如，SELECT COUNT(*) 即使表为空也应该返回 0。
    Tuple tuple;
    RID rid;
    while(child_executor_->Next(&tuple,&rid))
    {
        auto key_set=MakeAggregateKey(&tuple);
        auto value_set=MakeAggregateValue(&tuple);
        aht_.InsertCombine(key_set,value_set);
        // 将 key_set（聚合键）和 value_set（聚合值）插入到聚合哈希表中。
        // 如果哈希表中已经存在该 key_set，则会合并（combine）其对应的聚合值
        //（即更新聚合函数的值，如 SUM，COUNT 的累加）。
    }
    aht_iterator_=aht_.Begin();
}

auto AggregationExecutor::Next(Tuple *tuple, RID *rid) -> bool { 
    while(aht_iterator_!=aht_.End())
    {
        auto &agg_val=aht_iterator_.Val();
        auto &agg_key=aht_iterator_.Key();
        std::vector<Value>output_values;
        output_values.reserve(agg_val.aggregates_.size()+agg_key.group_bys_.size());
// agg_key.group_bys_.size()：表示 GROUP BY 中的分组列数。
// 例如，如果你按两个列分组（department 和 job_title），那么 agg_key.group_bys_.size() 就等于 2。
// agg_val.aggregates_.size()：表示聚合函数的数量。
// 例如，如果查询中有两个聚合函数（如 COUNT(*) 和 SUM(salary)），那么 agg_val.aggregates_.size() 就等于 2。
// 因此，agg_val.aggregates_.size() + agg_key.group_bys_.size() 的结果是所有 GROUP BY 列加上所有聚合列的总数。
// 这个总数对应于每个分组输出的元组中包含的所有列的数量。
        for(auto &val:agg_key.group_bys_)
        {
            output_values.emplace_back(val);
        }
        for(auto&val:agg_val.aggregates_)
        {
            output_values.emplace_back(val);
        }
        ++aht_iterator_;
        *tuple=Tuple(output_values,&GetOutputSchema());
        *rid=tuple->GetRid();
        return true;
    }
    if(aht_.Begin()==aht_.End()&&!special_case_end_)
    {
        if(!plan_->GetGroupBys().empty())
        {
            return false;
        }
         auto agg_val = aht_.GenerateInitialAggregateValue();
        std::vector<Value> output_vals = agg_val.aggregates_;
        *tuple = Tuple(output_vals, &GetOutputSchema());
        *rid = tuple->GetRid();
        special_case_end_ = true;
        return true;
    }    
return false; }

auto AggregationExecutor::GetChildExecutor() const -> const AbstractExecutor * { return child_executor_.get(); }

}  // namespace bustub
