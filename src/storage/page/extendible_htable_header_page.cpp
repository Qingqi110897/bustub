//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// extendible_htable_header_page.cpp
//
// Identification: src/storage/page/extendible_htable_header_page.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/page/extendible_htable_header_page.h"

#include "common/exception.h"

namespace bustub {
//该函数初始化了哈希表目录，将所有页面 ID 设置为无效。
void ExtendibleHTableHeaderPage::Init(uint32_t max_depth) {
  max_depth_=max_depth;
  //将传入的 max_depth 值赋给类成员变量 max_depth_。这个变量通常用于控制哈希表的层次或桶的数量。
  for(uint32_t i=0;i<MaxSize();++i) 
  {
    directory_page_ids_[i]=INVALID_PAGE_ID;
  }
}

auto ExtendibleHTableHeaderPage::HashToDirectoryIndex(uint32_t hash) const -> uint32_t { 
  uint32_t idx=(max_depth_==0)?0:hash>>(32-max_depth_);//右移位数>=该变量的位数是undefined behavior
  return idx;}

auto ExtendibleHTableHeaderPage::GetDirectoryPageId(uint32_t directory_idx) const -> uint32_t {
  assert(directory_idx<MaxSize());
   return directory_page_ids_[directory_idx]; }

void ExtendibleHTableHeaderPage::SetDirectoryPageId(uint32_t directory_idx, page_id_t directory_page_id) {
  assert(directory_idx<MaxSize());
  directory_page_ids_[directory_idx]=directory_page_id;
}

auto ExtendibleHTableHeaderPage::MaxSize() const -> uint32_t {
  return (1<<max_depth_); }

}  // namespace bustub
