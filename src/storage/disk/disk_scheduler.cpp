//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// disk_scheduler.cpp
//
// Identification: src/storage/disk/disk_scheduler.cpp
//
// Copyright (c) 2015-2023, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/disk/disk_scheduler.h"
#include "common/exception.h"
#include "storage/disk/disk_manager.h"
//实现一个简单的磁盘调度器，接收BufferPoolManager发来的读写磁盘请求放入一个请求队列中；
//然后启动一个新线程，不断从请求队列中获取请求，根据请求类型调用对应DiskManager的读写函数进行磁盘读写。
namespace bustub {

DiskScheduler::DiskScheduler(DiskManager *disk_manager) : disk_manager_(disk_manager) {
  background_thread_.emplace([&] { StartWorkerThread(); });
}

DiskScheduler::~DiskScheduler() {
  // Put a `std::nullopt` in the queue to signal to exit the loop
  request_queue_.Put(std::nullopt);
  if (background_thread_.has_value()) {
    background_thread_->join();
  }
}

void DiskScheduler::Schedule(DiskRequest r) {request_queue_.Put(std::move(r));}

void DiskScheduler::StartWorkerThread() 
{
  std::optional<DiskRequest> request;
  while ((request = request_queue_.Get())) {
    if (request) {
      if (request->is_write_) {
        disk_manager_->WritePage(request->page_id_, request->data_);
        request->callback_.set_value(true);
        continue;
      }
      disk_manager_->ReadPage(request->page_id_, request->data_);
      request->callback_.set_value(true);
    }
  }
}

}  // namespace bustub
