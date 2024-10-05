#include "storage/page/page_guard.h"
#include "buffer/buffer_pool_manager.h"

namespace bustub {
// 移动构造函数 (BasicPageGuard(BasicPageGuard &&that) noexcept)： 
// 这是当一个新的 BasicPageGuard 对象通过右值引用初始化时被调用的函数。
// 其作用是将资源从现有的 that 对象转移到新的 BasicPageGuard 对象中。
// 这是用于创建一个新的对象。
BasicPageGuard::BasicPageGuard(BasicPageGuard &&that) noexcept {
//  bpm_（通常是指向 BufferPoolManager 的指针）
    this->bpm_=that.bpm_;
    this->is_dirty_=that.is_dirty_;
    this->page_=that.page_;
    that.bpm_=nullptr;
// 将 that 对象的 bpm_ 成员设置为 nullptr，表示原来的对象不再拥有该资源。
// 目的：避免资源双重释放（double free）问题，明确当前对象接管了原来对象的 bpm_。
    that.page_=nullptr;
    that.is_dirty_=false;
}

void BasicPageGuard::Drop() {
    if(page_!=nullptr)
    {
        bpm_->UnpinPage(page_->GetPageId(),is_dirty_);
    }
    page_=nullptr;
    bpm_=nullptr;
    is_dirty_=false;
}
// 移动赋值操作符 (operator=(BasicPageGuard &&that) noexcept)： 
// 这是当一个已存在的 BasicPageGuard 对象被重新赋值时调用的函数。
// 它将资源从 that 对象转移到当前对象 this 中，并且会首先释放当前对象持有的任何资源（通过 this->Drop()）。
// 这是用于更新一个已存在的对象。
auto BasicPageGuard::operator=(BasicPageGuard &&that) noexcept -> BasicPageGuard & { 
    this->Drop();
    this->bpm_=that.bpm_;
    this->is_dirty_=that.is_dirty_;
    this->page_=that.page_;
    that.bpm_=nullptr;
    that.page_=nullptr;
    that.is_dirty_=false;
    return *this; }
// 移动构造函数：它只负责将 that 对象的资源转移到一个新对象中，不需要考虑当前对象之前是否持有资源，因为它是为一个新对象服务的。
// 移动赋值操作符：它不仅要转移 that 对象的资源，还要先释放当前对象 this 之前所持有的资源（通过 this->Drop()），然后再接受 that 对象的资源。这意味着它处理的对象已经存在并可能持有资源。
// 移动构造函数：用于创建一个新的对象时将资源从另一个对象转移到新对象中。
// 移动赋值操作符：用于将资源从另一个对象转移到一个已存在的对象，并且会首先释放当前对象所持有的资源。
// 两者的核心区别在于：移动构造函数是为初始化新对象而设计的，而移动赋值操作符是为重新赋值已存在的对象而设计的。
BasicPageGuard::~BasicPageGuard(){};  // NOLINT
// 将一个 基本页面保护对象（BasicPageGuard）升级为 读页面保护对象
auto BasicPageGuard::UpgradeRead() -> ReadPageGuard { 
    this->page_->RLatch();
    ReadPageGuard read_guard(this->bpm_,this->page_);
    this->bpm_=nullptr;
    this->page_=nullptr;
    this->is_dirty_=false;
    return read_guard; 
    }

auto BasicPageGuard::UpgradeWrite() -> WritePageGuard { 
    this->page_->WLatch();
    WritePageGuard write_guard(this->bpm_,this->page_);
    this->bpm_=nullptr;
    this->page_=nullptr;
    this->is_dirty_=false;
    return write_guard; 
    }

ReadPageGuard::ReadPageGuard(BufferPoolManager *bpm, Page *page) {
}

ReadPageGuard::ReadPageGuard(ReadPageGuard &&that) noexcept {
    this->guard_=std::move(that.guard_);
};

auto ReadPageGuard::operator=(ReadPageGuard &&that) noexcept -> ReadPageGuard & { 
    this->Drop();
    this->guard_=std::move(that.guard_);
    return *this; 
}

void ReadPageGuard::Drop() {
    auto &guard=this->guard_;
    if(guard.page_!=nullptr)
    {
        guard.bpm_->UnpinPage(this->PageId(),this->guard_.is_dirty_);
        guard.page_->RUnlatch();
    }
    guard.page_=nullptr;
    guard.bpm_=nullptr;
    guard.is_dirty_=false;
}

ReadPageGuard::~ReadPageGuard() {}  // NOLINT

WritePageGuard::WritePageGuard(BufferPoolManager *bpm, Page *page) {}

WritePageGuard::WritePageGuard(WritePageGuard &&that) noexcept {
    this->guard_=std::move(that.guard_);};

auto WritePageGuard::operator=(WritePageGuard &&that) noexcept -> WritePageGuard & { 
    this->Drop();
    this->guard_=std::move(that.guard_);
    return *this;
 }

void WritePageGuard::Drop() {
    auto &guard=this->guard_;
    if(guard.page_!=nullptr)
    {
        guard.bpm_->UnpinPage(this->PageId(),this->guard_.is_dirty_);
        guard.page_->WUnlatch();
    }
    guard.page_=nullptr;
    guard.bpm_=nullptr;
    guard.is_dirty_=false;
}

WritePageGuard::~WritePageGuard() {}  // NOLINT

}  // namespace bustub
