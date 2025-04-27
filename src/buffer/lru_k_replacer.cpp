//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_k_replacer.cpp
//
// Identification: src/buffer/lru_k_replacer.cpp
//
// Copyright (c) 2015-2022, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_k_replacer.h"
#include "common/exception.h"
#include<memory>
#include<utility>
#include<mutex>
namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) : replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool 
{     
    std::scoped_lock<std::mutex>lock(latch_);
    size_t largest_bkd =0;// 存储最大的向后K距离
    size_t largest_distance =0;//存储从当前时间戳开始的最大距离
    bool success_evict =false;//指示是否成功进行了淘汰
    bool is_inf =false;//标志是否有节点的访问次数少于 k_ 次。
    LRUKNode *evict_node_ptr =nullptr;//指向即将被淘汰的节点的指针
    for(auto &pair :node_store_)
    {
        auto &node =pair.second;
        if(!node.is_evictable_)
        {
            continue;
        }
        success_evict=true;
        if(node.history_.empty())
        {
            *frame_id =pair.first;
            evict_node_ptr=&node;
            break;
        }
//检查访问历史是否达到 K 次 (node.history_.size() == k_) 并且 is_inf 为 false:
// 这部分代码处理那些访问历史刚好达到 K 次的缓存项。
// current_bkd 是当前时间与第 K 次最近访问时间的差值。
// 如果 current_bkd 大于 largest_bkd，说明当前缓存项在最近 K 次访问之后，已经很长时间没有被访问过，因此该缓存项成为当前最可能被淘汰的候选项。
        if(node.history_.size()==k_&&!is_inf)
        {
            auto current_bkd =current_timestamp_ - node.GetBackKTimeStamp();
            if(current_bkd>largest_bkd)
            {
                largest_bkd=current_bkd;
                *frame_id=pair.first;
                evict_node_ptr=&node;
            }
        }
// ode.history_.size() < k_ 时，意味着该缓存项的访问历史不足 K 次。
// 此时，设置 is_inf = true，表示当前缓存中存在访问历史不足 K 次的项，通常会优先考虑淘汰这种项。
// current_distance 是当前时间与最近一次访问时间的差值。
// 如果 current_distance 大于 largest_distance，说明当前缓存项比之前所有项更长时间未被访问过，因此它是被淘汰的候选项。
        else if(node.history_.size()<k_)
        {
            is_inf =true;
            auto current_distance =current_timestamp_ - node.GetLatestTimeStamp();//current_distance 通常代表当前缓存项最后一次被访问到当前时间的差值，较大的值表示该缓存项很长时间没有被访问。
            if(current_distance>largest_distance)//当前缓存项比之前所有检查过的缓存项都更少被访问
            {
                largest_distance=current_distance;
                *frame_id=pair.first;
                evict_node_ptr=&node;
            }
        }
    }
    if(success_evict)
    {
        curr_size_--;
        evict_node_ptr->history_.clear();
        node_store_.erase(*frame_id);
    }
    
    return success_evict; 
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id, [[maybe_unused]] AccessType access_type) {
    std::scoped_lock<std::mutex>lock(latch_);
    auto helper=static_cast<size_t>(frame_id);
    BUSTUB_ASSERT(frame_id<=replacer_size_,"invalid frame_id");
    auto iter = node_store_.find(frame_id);
    if(iter==node_store_.end())
    {
        auto new_node_ptr =std::make_unique<LRUKNode>();
        if(access_type!=AccessType::Scan)
        {
            new_node_ptr->history_.push_back(current_timestamp_++);
        }
        node_store_.insert(std::make_pair(frame_id,*new_node_ptr));
    }
    else{
        auto &node =iter->second;
        if(access_type!=AccessType::Scan)
        {
            if(node.history_.size()==k_)
            {
                node.history_.pop_front();
            }
            node.history_.push_back(current_timestamp_++);
        }
    }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
    std::scoped_lock<std::mutex>lock(latch_);
    auto helper =static_cast<size_t>(frame_id);
    BUSTUB_ASSERT(helper<=replacer_size_,"invalid frame_id");
    std::unique_ptr<LRUKNode>new_node_ptr;
    auto iter = node_store_.find(frame_id);
    if(iter == node_store_.end())
    {
        new_node_ptr=std::make_unique<LRUKNode>();
        node_store_.insert(std::make_pair(frame_id,*new_node_ptr));
    }
    auto &node=(iter == node_store_.end())? *new_node_ptr : iter->second;
    if(set_evictable&&!node.is_evictable_)
    {
        node.is_evictable_=set_evictable;
        curr_size_--;
    }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
    std::scoped_lock<std::mutex>lock(latch_);
    auto it=node_store_.find(frame_id);
    if(it == node_store_.end())
    {
        return;
    }
    auto &node = it->second;
    BUSTUB_ASSERT(node.is_evictable_,"Called on a non-evictable frame");
    node.history_.clear();
    node_store_.erase(frame_id);
    curr_size_--;
}

auto LRUKReplacer::Size() -> size_t {

     return curr_size_; 
}

}  // namespace bustub
