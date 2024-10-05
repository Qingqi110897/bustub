//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// lru_replacer.cpp
//
// Identification: src/buffer/lru_replacer.cpp
//
// Copyright (c) 2015-2019, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/lru_replacer.h"

namespace bustub {

LRUReplacer::LRUReplacer(size_t num_pages) {}

LRUReplacer::~LRUReplacer() = default;

auto LRUReplacer::Victim(frame_id_t *frame_id) -> bool 
{
     data_latch_.lock();
     if(data_idx_.empty())
     {
        data_latch_.unlock();
        return false;
     }
     *frame_id=data_.front();
     data_.pop_front();
     data_idx_.erase(*frame_id);
     data_latch_.unlock();
     return true;     
 }

void LRUReplacer::Pin(frame_id_t frame_id) {
    data_latch_.lock();
    auto it =data_idx_.find(frame_id);
    if(it!=data_idx_.end())
    {
        data_.erase(it->second);
        data_idx_.erase(it);
    }
    data_latch_.unlock();
}

void LRUReplacer::Unpin(frame_id_t frame_id) {
    data_latch_.lock();
    auto it =data_idx_.find(frame_id);
    if(it==data_idx_.end())
    {
        data_.push_back(frame_id);
        data_idx_[frame_id]=prev(data_.end());
    }
    data_latch_.unlock();
}

auto LRUReplacer::Size() -> size_t {
    data_latch_.lock();
    size_t ret=data_idx_.size();
    data_latch_.unlock();
    return ret;
    }

}  // namespace bustub
