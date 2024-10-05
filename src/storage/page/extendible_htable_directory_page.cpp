//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_htable_directory_page.cpp
//
// Identification: src/storage/page/extendible_htable_directory_page.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/extendible_htable_directory_page.h"

#include <algorithm>
#include <unordered_map>

#include "common/config.h"
#include "common/logger.h"

namespace bustub {

void ExtendibleHTableDirectoryPage::Init(uint32_t max_depth) {
  max_depth_=max_depth;
  global_depth_=0;
  //初始化全局深度为 0。全局深度表示当前目录中使用的位数，用于映射哈希值到桶的索引。
  for(uint32_t i=0;i<MaxSize();++i)
  {
    bucket_page_ids_[i]=INVALID_PAGE_ID;
  }
}
//将哈希值映射到桶索引。
//计算目录中桶的数量（2^global_depth_），然后减去 1，得到一个掩码（掩码用于提取哈希值的最低有效位）
auto ExtendibleHTableDirectoryPage::HashToBucketIndex(uint32_t hash) const -> uint32_t {
   return hash & ((1<<global_depth_)-1); }

auto ExtendibleHTableDirectoryPage::GetBucketPageId(uint32_t bucket_idx) const -> page_id_t { 
  assert(bucket_idx<pow(2,max_depth_));
  return bucket_page_ids_[bucket_idx];}

void ExtendibleHTableDirectoryPage::SetBucketPageId(uint32_t bucket_idx, page_id_t bucket_page_id) {
  assert(bucket_idx < pow(2, max_depth_));
  bucket_page_ids_[bucket_idx] = bucket_page_id;
}
//计算一个桶的分裂镜像索引,在哈希表扩展时，一个桶被分裂为两个，SplitImageIndex 指向分裂后的另一个桶。
auto ExtendibleHTableDirectoryPage::GetSplitImageIndex(uint32_t bucket_idx) const -> uint32_t { 
  auto local_depth_mask=GetLocalDepthMask(bucket_idx);
  auto local_depth=GetLocalDepth(bucket_idx);
  return (bucket_idx&local_depth_mask)^(1<<(local_depth-1));
   }

auto ExtendibleHTableDirectoryPage::GetGlobalDepth() const -> uint32_t {
  assert(global_depth_<=max_depth_);
   return global_depth_; 
   }

void ExtendibleHTableDirectoryPage::IncrGlobalDepth() {
  assert(global_depth_<=max_depth_);
  if(global_depth_==max_depth_)
  {
    return;
  }
  uint32_t pre_size=Size();
  global_depth_++;
  uint32_t curr_size =Size();
  for(uint32_t i=pre_size;i<curr_size;++i)
  {
    bucket_page_ids_[i]=bucket_page_ids_[i-pre_size];
    local_depths_[i]=local_depths_[i-pre_size];
    //桶分裂的过程中，桶的内容并不会立即迁移或复制到新的桶中。新增的桶只是扩展了目录页的大小，
    //初始时这些桶的页面ID和本地深度与原来的桶一致，但是桶中的数据项并不会自动复制。
  }
}

void ExtendibleHTableDirectoryPage::DecrGlobalDepth() {
  if(global_depth_>0)
  {
    global_depth_--;
  }
}
//检查目录是否可以缩小
auto ExtendibleHTableDirectoryPage::CanShrink() -> bool { 
  if(global_depth_==0)
  {
    return false;
  }
  for(uint32_t i=0;i<Size();++i)
  {
    if(GetLocalDepth(i)==global_depth_)
    {
      return false;
    }
  } 
  return true;
}

auto ExtendibleHTableDirectoryPage::Size() const -> uint32_t { 
  double size_float=pow(2,global_depth_);
  auto size=static_cast<uint32_t>(size_float);
  return size; 
}

auto ExtendibleHTableDirectoryPage::GetLocalDepth(uint32_t bucket_idx) const -> uint32_t { 
  return local_depths_[bucket_idx]; }

void ExtendibleHTableDirectoryPage::SetLocalDepth(uint32_t bucket_idx, uint8_t local_depth) {
  assert(bucket_idx>=0);
  local_depths_[bucket_idx]=local_depth;
}

void ExtendibleHTableDirectoryPage::IncrLocalDepth(uint32_t bucket_idx) {
  local_depths_[bucket_idx] += 1;
}

void ExtendibleHTableDirectoryPage::DecrLocalDepth(uint32_t bucket_idx) {
  assert(bucket_idx>=0);
  local_depths_[bucket_idx]-=1;
}

}  // namespace bustub
// 标题页管理目录页的索引，通过哈希值映射将数据存在合适的目录中
//目录页将哈希值映射到桶，桶是实际存储数据的位置，桶满后分裂
//分裂出来的桶具有相同的id和深度但数据不一样
//桶页内实现插入查找删除等操作
// 分裂桶的作用
// 处理桶的溢出:
// 当一个桶中的数据项超过了桶的容量限制时，就需要分裂桶。通过分裂，可以将数据分散到两个桶中，从而避免单个桶的溢出问题。
// 提升哈希表的查找效率:
// 分裂桶后，数据被分配到更多的桶中，从而使得每个桶中的数据项数量减少。这有助于提高哈希表的查找效率，因为查找时需要检查的数据项变少了。
// 支持动态扩展:
// 分裂桶是一种动态调整哈希表大小的机制，使得哈希表能够在运行时适应数据量的变化，从而保持较好的性能。
