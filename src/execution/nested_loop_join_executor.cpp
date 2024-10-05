//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// nested_loop_join_executor.cpp
//
// Identification: src/execution/nested_loop_join_executor.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "execution/executors/nested_loop_join_executor.h"
#include "binder/table_ref/bound_join_ref.h"
#include "common/exception.h"
#include <sys/types.h>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>
#include "common/rid.h"
#include "execution/executors/abstract_executor.h"
#include "type/type_id.h"
#include "type/value_factory.h"

namespace bustub {

NestedLoopJoinExecutor::NestedLoopJoinExecutor(ExecutorContext *exec_ctx, const NestedLoopJoinPlanNode *plan,
                                               std::unique_ptr<AbstractExecutor> &&left_executor,
                                               std::unique_ptr<AbstractExecutor> &&right_executor)
    : AbstractExecutor(exec_ctx),
      plan_(plan),
      left_executor_(std::move(left_executor)),
      right_executor_(std::move(right_executor)),
      is_inner_loop_end_(true),
      is_match_(false),
      index_num_(0) {
  if (!(plan->GetJoinType() == JoinType::LEFT || plan->GetJoinType() == JoinType::INNER)) {
    // Note for 2023 Fall: You ONLY need to implement left join and inner join.
    throw bustub::NotImplementedException(fmt::format("join type {} not supported", plan->GetJoinType()));
  }
}

void NestedLoopJoinExecutor::Init() { 
  left_executor_->Init();
  right_executor_->Init();
  Tuple tuple;
  RID rid;
  while(right_executor_->Next(&tuple,&rid))
  {
    inner_table_.emplace_back(tuple,rid);
  }
  // 在嵌套循环连接中，内表（通常是右表）会被多次遍历。
  // 为了提高性能，通常会先将内表的所有元组缓存到内存中，这样避免重复扫描右表的开销，从而加速连接操作。
  // 在嵌套循环连接中，右表通常就是内表，因为右表需要被遍历多次以便与外表的每一个元组进行连接。
  // 内表的元组会在嵌套循环的内层进行遍历。
}

auto NestedLoopJoinExecutor::Next(Tuple *tuple, RID *rid) -> bool { 
  auto join_type=plan_->GetJoinType();
  if(join_type==JoinType::INNER)
  {
//第一次迭代：
// outer_tuple_ 获取 employees 中的第一行。
// 进入内表循环，inner_tuple 遍历 departments 的每一行，进行连接。
// 内表遍历结束后：
// 当内表遍历完后，is_inner_loop_end_ 被设置为 true。
// 触发外层 while 循环，调用 left_executor_->Next(&outer_tuple_, &outer_rid_) 从 employees 中获取第二行。
// 重复此过程：
// 每次获取 outer_tuple_ 后，都会再次遍历 departments 中的所有行。
// 如果 is_inner_loop_end_ 为 false，说明当前内表的遍历还没有结束，循环将继续。
// 如果 is_inner_loop_end_ 为 true，说明内表的遍历已经结束，下一步会尝试获取左表中的新元组。
    while(!is_inner_loop_end_||left_executor_->Next(&outer_tuple_,&outer_rid_))
    {
      is_inner_loop_end_=false;
      Tuple inner_tuple;
      RID inner_rid;
      while(index_num_<inner_table_.size())
      {
        inner_tuple=inner_table_[index_num_].first;
        inner_rid=inner_table_[index_num_].second;
        auto predicate_expr=plan_->Predicate();
        std::vector<Value>join_array;
        uint32_t left_col_cnt=left_executor_->GetOutputSchema().GetColumnCount();
        uint32_t right_col_cnt=right_executor_->GetOutputSchema().GetColumnCount();
        join_array.reserve(left_col_cnt+right_col_cnt);
        for (u_int32_t i = 0; i < left_col_cnt; ++i) 
        {
          join_array.emplace_back(outer_tuple_.GetValue(&left_executor_->GetOutputSchema(), i));
        }
        for (u_int32_t i = 0; i < right_col_cnt; ++i) 
        {
          join_array.emplace_back(inner_tuple.GetValue(&right_executor_->GetOutputSchema(), i));
        }
        index_num_++;
        *tuple=Tuple(join_array,&GetOutputSchema());
        auto val=predicate_expr->EvaluateJoin(&outer_tuple_, left_executor_->GetOutputSchema(), &inner_tuple,right_executor_->GetOutputSchema());
        if (!val.IsNull() && val.GetAs<bool>()) 
        {
          *rid = tuple->GetRid();
          return true;
        }
      }
      is_inner_loop_end_ = true;
      right_executor_->Init();
      index_num_ = 0;
    }
    return false;
  }

   if (join_type == JoinType::LEFT) {
    uint32_t left_col_cnt = left_executor_->GetOutputSchema().GetColumnCount();
    uint32_t right_col_cnt = right_executor_->GetOutputSchema().GetColumnCount();
    while (!is_inner_loop_end_ || left_executor_->Next(&outer_tuple_, &outer_rid_)) 
    {
      if(is_inner_loop_end_)
      {
        is_match_=false;
      }
       is_inner_loop_end_ = false;
       Tuple inner_tuple;
        RID inner_rid;
        while (index_num_ < inner_table_.size()) {
        inner_tuple = inner_table_[index_num_].first;
        inner_rid = inner_table_[index_num_].second;
        auto predicate_expr = plan_->Predicate();
        std::vector<Value> join_array;
        join_array.reserve(left_col_cnt + right_col_cnt);
        for (u_int32_t i = 0; i < left_col_cnt; ++i) {
          join_array.emplace_back(outer_tuple_.GetValue(&left_executor_->GetOutputSchema(), i));
        }
        for (u_int32_t i = 0; i < right_col_cnt; ++i) {
          join_array.emplace_back(inner_tuple.GetValue(&right_executor_->GetOutputSchema(), i));
        }
        index_num_++;
        *tuple = Tuple(join_array, &GetOutputSchema());
        auto val = predicate_expr->EvaluateJoin(&outer_tuple_, left_executor_->GetOutputSchema(), &inner_tuple,right_executor_->GetOutputSchema());
        if (!val.IsNull() && val.GetAs<bool>()) {
          is_match_ = true;
          *rid = tuple->GetRid();
          return true;
        }
      }
      if(!is_match_)
      {
        std::vector<Value>left_join_array;
        left_join_array.reserve(left_col_cnt+right_col_cnt);
        for (u_int32_t i = 0; i < left_col_cnt; ++i) 
        {
          left_join_array.emplace_back(outer_tuple_.GetValue(&left_executor_->GetOutputSchema(), i));
        }
        for (u_int32_t i = 0; i < right_col_cnt; ++i) 
        {
          left_join_array.emplace_back(ValueFactory::GetNullValueByType(INTEGER));
        }
        *tuple = Tuple(left_join_array, &GetOutputSchema());
        *rid = tuple->GetRid();
        is_match_ = true;
        return true;
      }
      is_inner_loop_end_ = true;
      right_executor_->Init();
      index_num_ = 0;
    }
    return false;
   }
}  // namespace bustub
