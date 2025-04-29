//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_extendible_hash_table.cpp
//
// Identification: src/container/disk/hash/disk_extendible_hash_table.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "common/config.h"
#include "common/exception.h"
#include "common/logger.h"
#include "common/macros.h"
#include "common/rid.h"
#include "common/util/hash_util.h"
#include "container/disk/hash/disk_extendible_hash_table.h"
#include "storage/index/hash_comparator.h"
#include "storage/page/extendible_htable_bucket_page.h"
#include "storage/page/extendible_htable_directory_page.h"
#include "storage/page/extendible_htable_header_page.h"
#include "storage/page/page_guard.h"

namespace bustub {

template <typename K, typename V, typename KC>
DiskExtendibleHashTable<K, V, KC>::DiskExtendibleHashTable(const std::string &name, BufferPoolManager *bpm,
                                                           const KC &cmp, const HashFunction<K> &hash_fn,
                                                           uint32_t header_max_depth, uint32_t directory_max_depth,
                                                           uint32_t bucket_max_size)
    : bpm_(bpm),
      cmp_(cmp),
      hash_fn_(std::move(hash_fn)),//对键 K 进行哈希操作的哈希函数
      header_max_depth_(header_max_depth),//头部页的最大深度（ExtendibleHTableHeaderPage 存储的最大深度）
      directory_max_depth_(directory_max_depth),
      bucket_max_size_(bucket_max_size) {
        index_name_=name;
        //创建头部页
        page_id_t page_id;
        auto tmp_header_guard=bpm->NewPageGuarded(&page_id);
        auto header_guard=tmp_header_guard.UpgradeWrite();
        auto header_page=header_guard.AsMut<ExtendibleHTableHeaderPage>();
        // AsMut<T> 是一个模板方法，T 是目标类型。
        // 在这个上下文中，AsMut<ExtendibleHTableHeaderPage>() 的作用是
        // 将 header_guard 管理的页面视为 ExtendibleHTableHeaderPage 类型的对象，允许你获取到一个可以修改的指针。
        header_page->Init(header_max_depth);
        header_page_id_=page_id;
}
/*​头部页（Header Page）​​
作用：顶层索引，管理目录页的分布
关键参数：
header_max_depth_：最大深度，决定能管理的目录页数量（2^header_max_depth）
通过哈希值的高位直接定位目录页（HashToDirectoryIndex）
​目录页（Directory Page）​​
作用：维护哈希值到桶页的映射
关键参数：
directory_max_depth_：目录的全局深度（Global Depth）
每个桶有局部深度（Local Depth），用于控制分裂范围
​桶页（Bucket Page）​​
作用：实际存储键值对
关键参数：
bucket_max_size_：单个桶的容量限制
满时触发分裂（Split），空时可能合并（Merge）
*/
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
// 这段代码的目的是根据给定的键 key，通过哈希值找到对应的目录索引，然后从头部页中找到相应的目录页 ID。
// 如果目录页有效，则接下来可以继续查找对应的桶页中的值
/*1. ​查找（GetValue）​​
​流程​：
计算键的哈希值。
通过头部页定位目录页。
通过目录页定位桶页。
在桶页中线性搜索目标键。*/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::GetValue(const K &key, std::vector<V> *result, Transaction *transaction) const
    -> bool {
    // 1. 读取头部页
      auto header_guard=bpm_->FetchPageRead(header_page_id_);
      auto header_page=header_guard.As<ExtendibleHTableHeaderPage>();
   // 2. 计算哈希并定位目录页
      uint32_t hash=Hash(key);
      auto directory_index=header_page->HashToDirectoryIndex(hash);
      page_id_t directory_page_id =header_page->GetDirectoryPageId(directory_index);
      if(directory_page_id==INVALID_PAGE_ID)
      {
        return false;
      }
  // 3. 读取目录页并定位桶页
      ReadPageGuard directory_guard=bpm_->FetchPageRead(directory_page_id);
      auto directory_page = directory_guard.As<ExtendibleHTableDirectoryPage>();
      auto bucket_index=directory_page->HashToBucketIndex(hash);
      auto bucket_page_id =directory_page->GetBucketPageId(bucket_index);
      if(bucket_page_id==INVALID_PAGE_ID)
      {
        return false;
      }
  // 4. 读取桶页并查找键
      ReadPageGuard bucket_guard =bpm_->FetchPageRead(bucket_page_id);
      auto bucket_page =bucket_guard.As<ExtendibleHTableBucketPage<K,V,KC>>();
      V value;
      bool lookup_success=bucket_page->Lookup(key,value,cmp_);
      if(!lookup_success)
      {
        return false;
      }
      result->push_back(value);
      return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*2. ​插入（Insert）​​
​基础流程​：

类似查找，定位到目标桶。
若桶未满，直接插入；若满则触发分裂。
​分裂逻辑​：

​目录扩容​：当局部深度等于全局深度时，目录深度翻倍（IncrGlobalDepth）。
​桶分裂​：
创建新桶，调整原桶的局部深度。
重新哈希原桶的所有键值对，分配到新旧桶。
更新目录映射（UpdateDirectoryMapping）。
​递归处理​：若新桶仍满，继续分裂。
​特殊情况​：

首次插入时需初始化目录页和桶页（InsertToNewDirectory/InsertToNewBucket）。*/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Insert(const K &key, const V &value, Transaction *transaction) -> bool {
   // 1. 定位目录页和桶页（类似GetValue）
    // 2. 若桶页不存在，初始化新桶（InsertToNewBucket）
    // 3. 若桶页存在但未满，直接插入
    // 4. 若桶页已满，触发分裂：
  auto header_guard=bpm_->FetchPageWrite(header_page_id_);
  auto header_page=header_guard.AsMut<ExtendibleHTableHeaderPage>();
  uint32_t hash=Hash(key);
  uint32_t directory_index=header_page->HashToDirectoryIndex(hash);
  auto directory_page_id=header_page->GetDirectoryPageId(directory_index);
  if(directory_page_id==INVALID_PAGE_ID)
  {
    auto insert_success=InsertToNewDirectory(header_page,directory_index,hash,key,value);
    return insert_success;
  }
  header_guard.Drop();
  WritePageGuard directory_guard =bpm_->FetchPageWrite(directory_page_id);
  auto directory_page=directory_guard.As<ExtendibleHTableDirectoryPage>();
  auto bucket_index=directory_page->HashToBucketIndex(hash);
  auto bucket_page_id=directory_page->GetBucketPageId(bucket_index);
  if(bucket_page_id==INVALID_PAGE_ID)
  {
    auto insert_success=InsertToNewBucket(directory_page,bucket_index,key,value,cmp_);
    return insert_success;
  }
  bool insert_success =false;
  WritePageGuard bucket_guard=bpm_->FetchPageWrite(bucket_page_id);
  auto bucket_page=bucket_guard.As<ExtendibleHTableBucketPage<V,K,KC>>();
  V tmp_val;
//   调用 Lookup(key, tmp_val, cmp_) 在桶中查找是否已经存在相同的键。
// 如果找到了键，说明该键已经存在，直接返回 false，插入失败，避免重复插入。
  if(bucket_page->Lookup(key,tmp_val,cmp_))
  {
    return false;
  }
  if(!bucket_page->IsFull())
  {
    insert_success=bucket_page->Insert(key,value,cmp_);
    return insert_success;
  }
  while(!insert_success&&bucket_page->IsFull())
  {
    if(directory_page->GetGlobalDepth()==directory_page->GetLocalDepth(bucket_index))
    {
      if(directory_page->GetGlobalDepth()==directory_page->GetMaxDepth())
      {
        return false;
      }
      directory_page->IncrGlobalDepth();
    }
    page_id_t new_bucket_page_id;
    auto tmp_new_bucket_guard=bpm_->NewPageGuarded(&new_bucket_page_id);
    auto new_bucket_guard=tmp_new_bucket_guard.UpgradeWrite();
    auto new_bucket_page=new_bucket_guard.AsMut<ExtendibleHTableBucketPage<K,V,KC>>();
    new_bucket_page->Init(bucket_max_size_);
    directory_page->IncrLocalDepth(bucket_index);
    auto new_local_depth=directory_page->GetLocalDepth(bucket_index);
    auto local_depth_mask=directory_page->GetLocalDepthMask(bucket_index);
    auto new_bucket_idx=UpdateDirectoryMapping(directory_page,bucket_index,new_bucket_page_id,new_local_depth,local_depth_mask);

    page_id_t rehash_page_id;
    std::vector<uint32_t>remove_array;
    for(uint32_t i=0;i<bucket_page.size();++i)
    {
      auto k=bucket_page->KeyAt(i);
      auto v=bucket_page->ValueAt(i);
      uint32_t hash_k=Hash(k);
      auto rehash_idx=directory_page->HashToBucketIndex(hash_k);
      rehash_page_id =directory_page->GetBucketPageId(rehash_idx);
      if(rehash_page_id==new_bucket_page_id)
      {
        new_bucket_page->insert(k,v,cmp_);
        remove_array.push_back(i);
      }
    }
    auto helper =0;
    for(auto &remove_id:remove_array)
    {
      bucket_page->RemoveAt(remove_id-helper);
      helper++;
    }
    bucket_index=directory_page->HashToBucketIndex(hash);
    rehash_page_id=directory_page->GetBucketPageId(bucket_index);
    if(rehash_page_id==new_bucket_page_id)
    {
      insert_success=new_bucket_page->Insert(key,value,cmp_);
      if(!insert_success&&new_bucket_guard->IsFull())
      {
        bucket_guard=std::move(new_bucket_guard);
        //std::move 并不会移动对象的内容，它只是将对象转换为右值引用，允许该对象被移动。
        //实际的移动操作是由移动构造函数或移动赋值运算符完成的。
//使用 std::move 后，被移动的对象通常会变成一个无效状态（例如，指针被置为 nullptr），
//因此你应避免在后续代码中使用它。
        bucket_page_id=new_bucket_page_id;
        bucket_page=new_bucket_page;
        bucket_index=new_bucket_idx;
      }
    }
    else{
      insert_success=bucket_page->Insert(key,value,cmp_);
    }
  }
  return insert_success;
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewDirectory(ExtendibleHTableHeaderPage *header, uint32_t directory_idx,uint32_t hash, const K &key, const V &value) -> bool {
  page_id_t dir_page_id;
  auto tmp_directory_guard=bpm_->NewPageGuarded(&dir_page_id);
  auto directory_guard=tmp_directory_guard.UpgradeWrite();
  auto directory_page =directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
  directory_page->Init(directory_max_depth_);
  header->SetDirectoryPageId(directory_idx,dir_page_id);
  auto bucket_idx=directory_page->HashToBucketIndex(hash);
  bool insert_success=InsertToNewBucket(directory_page,bucket_idx,key,value);
  return insert_success;
}

template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::InsertToNewBucket(ExtendibleHTableDirectoryPage *directory, uint32_t bucket_idx,const K &key, const V &value) -> bool {
  page_id_t bucket_page_id;
  auto tmp_bucket_guard=bpm_->NewPageGuarded(&bucket_page_id);
  auto bucket_guard=tmp_bucket_guard.UpgradeWrite();
  auto bucket_page=bucket_guard.AsMut<ExtendibleHTableBucketPage<K,V,KC>>();
  bucket_page->Init();
  directory->SetBucketPageId(bucket_idx,bucket_page);
  directory->SetLocalDepth(bucket_idx,0);
  assert(directory->GetLocalDepth(bucket_idx)<=directory->GetGlobalDepth());
  auto insert_success =bucket_page->Insert(key,value,cmp_);
  return insert_success;
}
                                                                                                                                                                                                 
template <typename K, typename V, typename KC>
void DiskExtendibleHashTable<K, V, KC>::UpdateDirectoryMapping(ExtendibleHTableDirectoryPage *directory,
uint32_t new_bucket_idx, page_id_t new_bucket_page_id,uint32_t new_local_depth, uint32_t local_depth_mask) {
  uint32_t distance=pow(2,new_local_depth);
  // 计算更新步长，即在新的局部深度下，隔多少个桶更新一次 
  new_bucket_idx=(new_bucket_idx>>(new_local_depth-1)==0?(new_bucket_idx+(distance/2)):(new_bucket_idx-(distance/2)));
  // 确定新的第一个桶索引
  for(uint32_t i=new_bucket_idx,i<directory_page->Size();i+=distance)
  {
    directory_page->SetBucketPageId(i,new_bucket_page_id);
    directory_page->SetLocalDepth(i,new_local_depth);
    dierctory_page->SetLocalDepth(new_bucket_idx,new_local_depth);
    assert(directory_page->GetLocalDepth(i)<=directory_page->GetGlobalDepth());
    assert(directory_page->GetLocalDepth(new_bucket_idx)<=directory_page->GetGlobalDepth());
    new_bucket_idx+=distance;
  }
 return new_bucket_idx; 
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*3. ​删除（Remove）​​
​基础流程​：
定位桶页并删除键值对。
若桶为空，尝试合并。
​合并逻辑​：
​条件​：当前桶与分裂镜像桶（Split Image）的局部深度相同。
​操作​：
将空桶的映射指向镜像桶。
降低相关目录项的局部深度。
若全局深度可缩减（所有局部深度 < 全局深度），则缩减（DecrGlobalDepth）。
​级联合并​：合并后若镜像桶仍空，继续向上合并。*/
template <typename K, typename V, typename KC>
auto DiskExtendibleHashTable<K, V, KC>::Remove(const K &key, Transaction *transaction) -> bool {
  auto header_guard=bpm_->FetchPageRead(header_page_id_);
  auto header_page=header_guard.As<ExtendibleHTableHeaderPage>();
  uint32_t hash=Hash(key);
  auto directory_index=header_page->HashToDirectoryIndex(hash);
  auto directory_page_id=header_page->GetDirectoryPageId(directory_index);
  if(directory_page_id==INVALID_PAGE_ID)
  {
    return false;
  }
  WritePageGuard directory_guard=bpm_->FetchPageWrite(directory_page_id);
  auto directory_page=directory_guard.AsMut<ExtendibleHTableDirectoryPage>();
  auto bucket_index=directory_page->HashToBucketIndex(hash);
  auto bucket_page_id=directory_page->GetBucketPageId(bucket_index);
  if(bucket_page_id==INVALID_PAGE_ID)
  {
    return false;
  }
  WritePageGuard bucket_guard=bpm_->FetchPageWrite(bucket_page_id);
  auto bucket_page=bucket_guard.AsMut<ExtendibleHTableBucketPage<K,V,KC>>();
  bool remove_success=bucket_page->Remove(key,cmp_);
  if(!remove_success)
  {
    return false;
  }
    // 2. 若桶为空，尝试合并
  while(bucket_page->IsEmpty())
  {
    bucket_guard.Drop();
    auto bucket_local_depth = directory_page->GetLocalDepth(bucket_index);
    if(bucket_local_depth==0)
    {
      break;
    }// 无法合并（已是最小深度）

//获取当前桶的split image桶。split image桶是和当前桶共享同一部分哈希空间的桶
    /*分裂镜像桶就是由桶分裂来的，但是镜像桶也有可能再分裂导致深度不一样，这样就没法合并了*/
    auto merge_bucket_index = directory_page->GetSplitImageIndex(bucket_index);
    auto merge_bucket_local_depth = directory_page->GetLocalDepth(merge_bucket_index);
    auto merge_bucket_page_id = directory_page->GetBucketPageId(merge_bucket_index);
//检查桶和split桶的局部深度是否相同 如果相同则可以合并 如果不同则正在使用不同的部分哈希值无法合并
/* 为什么局部深度相同才能合并？​​
​核心原因：确保分裂镜像桶（Split Image Bucket）存在且可安全合并​
​分裂镜像桶的定义​：
当桶分裂时，原桶和新桶会共享相同的局部深度。例如：

原桶索引 01（局部深度=2）分裂后，新桶索引 11（局部深度=2），此时 01 和 11 互为分裂镜像桶。
它们的哈希前缀仅在最高位不同（0 vs 1），其余低位相同。
​合并条件​：
只有两个桶的局部深度相同，才能保证它们原本是从同一个桶分裂出来的。此时：

​数据分布安全​：合并后，原属于这两个桶的键值对可以安全地归并到一个桶中（因为它们的哈希前缀在合并后的深度下是相同的）。
​目录一致性​：合并后，目录中所有指向这两个桶的项会被统一指向同一个桶，且局部深度减1。
​反例​：
如果局部深度不同（例如一个桶的深度为2，另一个为3），说明它们不是从同一个桶分裂出来的，强行合并会导致哈希映射混乱。
*/
    if(merge_bucket_local_depth==bucket_local_depth)
    {
      uint32_t traverse_bucket_idx=std::min(bucket_index&directory_page->GetLocalDepthMask(bucket_index),merge_buket_index);
      //获取当前桶和split image桶中的较小者，以确保我们更新从这个索引开始的桶
      uint32_t distance=1<<(bucket_local_depth-1);
      uint32_t new_local_depth=bucket_local_depth-1;
      for(uint32_t i=traverse_bucket_idx;i<directory_page->Size();i+=distance)
      {
        directory_page->SetBucketPageId(i,merge_bucket_page_id);
        directory_page->SetLocalDepth(i,new_local_depth);
      }
      if(new_local_depth==0)
      {
        break;
      }
      auto split_image_bucket_index = directory_page->GetSplitImageIndex(merge_bucket_index);
      auto split_image_bucket_page_id=directory_page->GetBucketPageId(split_image_bucket_index);
      WritePageGuard split_image_bucket_guard=bpm_->FetchPageWrite(split_image_bucket_page_id);
      if(split_image_bucket_page_id==INVALID_PAGE_ID)
      {
        break;//没有分裂图像存在
      }
      auto helper=bucket_page_id;
      bucket_index=split_image_bucket_index;
      bucket_page_id=split_image_bucket_page_id;
      bucket_guard=std::move(split_image_bucket_guard);
      bucket_page=bucket_guard.AsMut<ExtendibleHTableBucketPage<K,C,KC>>();
      bpm_->DeletePage(helper);
    }else
    {
      break;
    }
    
  }
  while(directory_page->CanShrink())
    {
      directory_page->DecrGlobalDepth();
    }
  return remove_success;
}

template class DiskExtendibleHashTable<int, int, IntComparator>;
template class DiskExtendibleHashTable<GenericKey<4>, RID, GenericComparator<4>>;
template class DiskExtendibleHashTable<GenericKey<8>, RID, GenericComparator<8>>;
template class DiskExtendibleHashTable<GenericKey<16>, RID, GenericComparator<16>>;
template class DiskExtendibleHashTable<GenericKey<32>, RID, GenericComparator<32>>;
template class DiskExtendibleHashTable<GenericKey<64>, RID, GenericComparator<64>>;
}  // namespace bustub
