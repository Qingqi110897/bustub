//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// buffer_pool_manager.cpp
//
// Identification: src/buffer/buffer_pool_manager.cpp
//
// Copyright (c) 2015-2021, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/buffer_pool_manager.h"

#include "common/exception.h"
#include "common/macros.h"
#include "storage/page/page_guard.h"
namespace bustub {

BufferPoolManager::BufferPoolManager(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                     LogManager *log_manager)
    : pool_size_(pool_size), disk_scheduler_(std::make_unique<DiskScheduler>(disk_manager)), log_manager_(log_manager) {
  // TODO(students): remove this line after you have implemented the buffer pool manager
  throw NotImplementedException(
      "BufferPoolManager is not implemented yet. If you have finished implementing BPM, please remove the throw "
      "exception line in `buffer_pool_manager.cpp`.");

  // we allocate a consecutive memory space for the buffer pool
  pages_ = new Page[pool_size_];
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);

  // Initially, every page is in the free list.
  for (size_t i = 0; i < pool_size_; ++i) {
    free_list_.emplace_back(static_cast<int>(i));
  }
}

BufferPoolManager::~BufferPoolManager() { delete[] pages_; }

auto BufferPoolManager::NewPage(page_id_t *page_id) -> Page * {
  std::scoped_lock<std::mutex>lock(latch_);
  frame_id_t replacement_frame_id;
  if(!free_list_.empty())
  {
    replacement_frame_id=free_list_.front();
    free_list_.pop_front();
  }
  else{
    if(!replacer_->Evict(&replacement_frame_id))//Evict 方法的作用是从缓存中选择一个页框进行淘汰，将其页框 ID 赋值给 replacement_frame_id。
    {
      page_id=nullptr;
      return nullptr;
    }
// 函数首先检查 free_list_（空闲页框列表）是否有可用的页框。如果有，则从中取出一个。
// 如果 free_list_ 为空，函数会尝试通过 replacer_ 来进行页面淘汰（eviction）。
// 如果淘汰失败，说明没有可用空间，函数返回 nullptr。
    auto &helper =pages_[replacement_frame_id];
  if(helper.IsDirty())
  {
    auto promise =disk_scheduler_->CreatePromise();
    auto future =promise.get_future();
    disk_scheduler_->Schedule({true,helper.GetData(),helper.page_id_,std::move(promise)});
    future.get();
  }
  page_table_.erase(helper.page_id_);
}
// 如果进行了页面淘汰，首先检查被淘汰的页面是否是 脏页（dirty，表示该页面自从从磁盘读取后被修改过）。
// 如果页面是脏页，则需要通过 disk_scheduler_ 调度将页面写回到磁盘。
// 页面写回完成后，将该页面从 page_table_（页表）中删除。
auto new_page_id =AllocatePage();
*page_id=new_page_id;
pages_[replacement_frame_id].ResetMemory();
pages_[replacement_frame_id].pin_count_=0;
pages_[replacement_frame_id].is_dirty_=false;
page_table_.insert(std::make_pair(new_page_id,replacement_frame_id));
replacer_->SetEvictable(replacement_frame_id,false);
replacer_->RecordAccess(replacement_frame_id);
pages_[replacement_frame_id].pin_count_++;
pages_[replacement_frame_id].page_id_ = new_page_id;
// 在腾出页框后（通过淘汰或从空闲列表获取），函数调用 AllocatePage() 来分配一个新的页面 ID。
// 然后重置该页框的内存，并初始化页面的元数据（例如，pin_count_、is_dirty_ 标志等）。
// 将新的页面插入到 page_table_，方便以后查找。
// 通知 replacer_，该页框现在是 不可被淘汰的，并记录它的访问时间。
   return &pages_[replacement_frame_id];
// 函数增加该页面的 pin_count_（表示该页面正在被使用），并将其 page_id_ 设置为新分配的页面 ID。
// 最后，返回指向这个新页面的指针。
}

auto BufferPoolManager::FetchPage(page_id_t page_id, [[maybe_unused]] AccessType access_type) -> Page * {
  std::scoped_lock<std::mutex>lock(latch_);
  auto iter=page_table_.find(page_id);
  if(iter!=page_table_.end())// 查找页面是否在缓冲池中
  {
    auto frame_id=iter->second;
    replacer_->SetEvictable(frame_id,false);
    replacer_->RecordAccess(frame_id,access_type);
    pages_[iter->second].pin_count_++;
    return &pages_[iter->second];
  }
// 先在 page_table_（页表）中查找 page_id，看是否有这个页面。
// 如果找到了，意味着页面已经在内存中，直接返回对应的页框。
// 解释：
// page_table_ 是一个哈希表，映射 page_id 到缓冲池中的页框 frame_id。
// replacer_->SetEvictable(frame_id, false) 将该页框标记为不可被淘汰。
// replacer_->RecordAccess(frame_id, access_type) 记录页面的访问信息，这通常是为了页面替换算法（如 LRU）更新状态。
// pin_count_++ 表示页面正在被使用，防止它被淘汰。
  frame_id_t replacement_frame_id;
  if(!free_list_.empty())
  {
    replacement_frame_id=free_list_.front();
    free_list_.pop_front();
  }
  else
  {
    if(!replacer_->Evict(&replacement_frame_id))
    {
      return nullptr;
    }
    auto &helper =pages_[replacement_frame_id];
    if(helper.IsDirty())
    {
      auto promise1=disk_scheduler_->CreatePromise();
      auto future1=promise1.get_future();
      disk_scheduler_->Schedule({true,helper.GetData(),helper.page_id_,std::move(promise1)});
      future1.get();
    }
    page_table_.erase(helper.page_id_);
  }
  pages_[replacement_frame_id].ResetMemory();
  pages_[replacement_frame_id].pin_count_=0;
  pages_[replacement_frame_id].is_dirty_=false;
  auto promise2=disk_scheduler_->CreatePromise();
  auto future2=promise2.get_future();
  disk_scheduler_->Schedule({false,pages_[replacement_frame_id].GetData(),page_id,std::move(promise2)});
//异步调度从磁盘读取 page_id 对应的页面数据到内存。
// 参数 {false, pages_[replacement_frame_id].GetData(), page_id, std::move(promise2)} 指定这是一次读取操作 (false)。
// pages_[replacement_frame_id].GetData() 返回页框的内存地址，用于将数据加载到该页框。
// page_id 是要从磁盘读取的页面 ID。
  page_table_.insert(std::make_pair(page_id,replacement_frame_id));
  replacer_->SetEvictable(replacement_frame_id,false);
  replacer_->RecordAccess(replacement_frame_id,access_type);
  pages_[replacement_frame_id].pin_count_++;
  pages_[replacement_frame_id].page_id_=page_id;
// 更新页表并标记页面访问
// 将新加载的页面与页框 replacement_frame_id 关联，并更新页表。
// 将该页框标记为不可被淘汰，并记录其访问。
// 解释：
// page_table_ 插入新的 page_id 和 replacement_frame_id 映射。
// SetEvictable() 标记该页框为不可被替换，RecordAccess() 更新页面访问状态。
// pin_count_++ 增加页面的固定计数，表示页面正在使用。
// page_id_ = page_id 将页框中的页面 ID 更新为新加载的页面。
  future2.get();
  return pages_+replacement_frame_id;
// 功能：返回装载新页面的页框指针。
// 解释：pages_ + replacement_frame_id 返回页框的指针，表示页面已经准备好使用。
}

auto BufferPoolManager::UnpinPage(page_id_t page_id, bool is_dirty, [[maybe_unused]] AccessType access_type) -> bool {
  std::scoped_lock<std::mutex>lock(latch_);
  auto iter=page_table_.find(page_id);
  if(iter==page_table_.end()||pages_[iter->second].pin_count_<=0)
  {
    return false;
  }
// 查找 page_table_ 中是否存在 page_id。
// 如果找不到，或者该页面的 pin_count_（固定计数）已经小于等于 0，则返回 false，表示无法解除固定。
  auto &target_id=iter->second;
  if((--pages_[target_id].pin_count_)==0)
  {
    replacer_->SetEvictable(target_id,true);
  }
// pages_[target_id].pin_count_-- 递减页面的固定计数。
// 如果页面的固定计数减为 0，调用 SetEvictable(target_id, true) 将页面标记为可被淘汰。
  if(is_dirty)
  {
    pages_[target_id].is_dirty_=is_dirty;
  }
  // 如果页面在解除固定时被标记为脏页，is_dirty 为 true，则 pages_[target_id].is_dirty_ 被设置为 true。
  return true;
}

auto BufferPoolManager::FlushPage(page_id_t page_id) -> bool { 
  std::scoped_lock<std::mutex>lock(latch_);
  BUSTUB_ASSERT(page_id!=INVALID_PAGE_ID,"PAGE_ID is invalid");
  auto iter =page_table_.find(page_id);
  if(iter==page_table_.end())
  {
    return false;
  }
  auto targer_id=iter->second;
  auto promise=disk_scheduler_->CreatePromise();
  auto future=promise.get_future();
  disk_scheduler_->Schedule({true,pages_[targer_id].GetData(),pages_[targer_id].page_id_,std::move(promise)});
  future.get();
  pages_[targer_id].is_dirty_=false;
  return true; 
}

void BufferPoolManager::FlushAllPages() {
  std::scoped_lock<std::mutex>lock(latch_);
  for(auto &pair:page_table_)
  {
  page_id_t page_id =pair.first;
  BUSTUB_ASSERT(page_id!=INVALID_PAGE_ID,"page_id is invalid");
  auto iter=page_table_.find(page_id);
  auto target_id=iter->second;
  auto promise = disk_scheduler_->CreatePromise();
  auto future =promise.get_future();
  disk_scheduler_->Schedule({true,pages_[target_id].GetData(),pages_[target_id].page_id_,std::move(promise)});
  future.get();
  pages_[target_id].is_dirty_=false;
  }
}

auto BufferPoolManager::DeletePage(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex>lock(latch_);
  auto iter=page_table_.find(page_id);
  if(iter==page_table_.end())
  {
    return true;
  }
  auto target_id =iter->second;
  if(pages_[target_id].pin_count_>0)
  {
    return false;
  }
  page_table_.erase(page_id);
  free_list_.push_back(target_id);
  pages_[target_id].ResetMemory();
  pages_[target_id].page_id_=INVALID_PAGE_ID;
  pages_[target_id].pin_count_=0;
  pages_[target_id].is_dirty_=false;
  DeallocatePage(page_id);
  // 调用 DeallocatePage(page_id) 释放与页面ID相关的资源，通常是将该页面从磁盘或持久化存储中删除。
   return true; }

auto BufferPoolManager::AllocatePage() -> page_id_t { 
  return next_page_id_++; }
// 用于生成新的唯一页面ID，以便在缓冲池中分配新的页面。
auto BufferPoolManager::FetchPageBasic(page_id_t page_id) -> BasicPageGuard {
  auto pg_ptr=FetchPage(page_id);
   return {this, pg_ptr}; }
// 用于在不指定页面操作权限的情况下获取页面，BasicPageGuard可能用于管理基本的页面生命周期。
auto BufferPoolManager::FetchPageRead(page_id_t page_id) -> ReadPageGuard {
  auto pg_ptr=FetchPage(page_id);
  pg_ptr->RLatch();
  assert(pg_ptr!=nullptr);
   return {this, pg_ptr}; }
// 用于在需要读取页面内容的情况下获取页面，并对页面加读锁，防止其他操作对页面进行修改。
auto BufferPoolManager::FetchPageWrite(page_id_t page_id) -> WritePageGuard { 
  auto pg_ptr=FetchPage(page_id);
  pg_ptr->WLatch();
  assert(pg_ptr!=nullptr);
  return {this, pg_ptr}; }
// 用于在需要写入页面内容的情况下获取页面，并对页面加写锁，防止其他操作对页面进行读取或修改。
auto BufferPoolManager::NewPageGuarded(page_id_t *page_id) -> BasicPageGuard { 
  auto pg_ptr=NewPage(page_id);
  assert(pg_ptr!=nullptr);
  return {this, pg_ptr}; }
// 用于创建新的页面，并获得一个基本的页面保护对象以管理页面生命周期。
}  // namespace bustub
// page_id 存的是页面 targert_id 存的是页框 
// 页面与页框是虚拟内存映射关系 框为物理内存
// pages存的是所有的页框