// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

/*
	Q: what advantage would it be to use inotify for event notification
		- possibly using normal copying of files into a directory
		- we would not need a cache
	D: add shared memory to keep track of #elements in queue
	D: (add a base class to queues (queue_base))
	D: test queue through producer/consumer in separate processes (requires: boost 1.56)
	D: possibly add callback when an event happens at the queue level
	D: when serializing/de-serializing maybe we should return a bool indicating if the operation should
	   be processed or if message should be ignored. For example, we might not want to throw an exception but instead 
	   skip the message - in that case the return code should be a 'false' and the queue will log a warning message
	   and continue

*/
#ifndef __POLLDIR_QUEUE_H__
#define __POLLDIR_QUEUE_H__
#include "detail/queue_empty_base.hpp"
#include "detail/queue_support.hpp"
#include <string>
#include <utility>
#include <list>
#include <memory>
#include <boost/thread/thread_time.hpp>
#include <boost/filesystem.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/named_condition.hpp>

namespace boost{
namespace asio{

namespace fs=boost::filesystem;
namespace ipc=boost::interprocess;
namespace pt=boost::posix_time;

// a simple threadsafe/interprocess-safe queue using directory as queue and files as storage media for queue items
// (mutex/condition variable names are derived from the queue name)
// (enq/deq have locks around them so that we cannot read partial messages)
template<typename T,typename DESER,typename SERIAL,typename Base=detail::base::queue_empty_base<T>>
class polldir_queue:public Base{
public:
  // ctors,assign,dtor
  // (if maxsize == 0 checking for max numbert of queue elements is ignored)
  polldir_queue(std::string const&qname,std::size_t maxsize,fs::path const&dir,DESER deser,SERIAL serial,bool removelocks):
      qname_(qname),maxsize_(maxsize),dir_(dir),deser_(deser),serial_(serial),removelocks_(removelocks),
      ipcmtx_(std::make_shared<boost::interprocess::named_mutex>(ipc::open_or_create,qname.c_str())),
      ipcond_(std::make_shared<boost::interprocess::named_condition>(ipc::open_or_create,qname.c_str())){
    // make sure path is a directory
    if(!fs::is_directory(dir_))throw std::logic_error(std::string("polldir_queue::polldir_queue: dir_: ")+dir.string()+" is not a directory");
  }
  polldir_queue(polldir_queue const&)=delete;
  polldir_queue(polldir_queue&&other):
      qname_(std::move(other.qname_)),maxsize_(other.maxsize_),dir_(other.dir_),deser_(std::move(other.deser_)),serial_(std::move(other.serial_)),
      removelocks_(other.removelocks_),deq_enabled_(other.deq_enabled_),enq_enabled_(other.enq_enabled_),
      ipcmtx_(std::move(other.ipcmtx_)),ipcond_(std::move(other.ipcond_)),cache_(std::move(other.cache_)){
    other.ipcmtx_=nullptr;
    other.ipcond_=nullptr;
    other.removelocks_=false; // make sure we don't remove locks twice
  }
  polldir_queue&operator=(polldir_queue const&)=delete;
  polldir_queue&operator=(polldir_queue&&other){
    qname_=std::move(other.qname_);
    maxsize_=other.maxsize_;
    dir_=std::move(other.dir_);
    removelocks_=other.removelocks_;
    deser_=std::move(other.deser_);
    serial_=std::move(other.serial_);
    deq_enabled_=other.deq_enabled_;
    enq_enabled_=other.enq_enabled_;
    ipcmtx_=std::move(other.ipcmtx_);
    ipcond_=std::move(other.ipcond_);
    other.ipcmtx_=nullptr;
    other.ipcond_=nullptr;
    cache_=std::move(other.cache_);
    other.removelocks_=false; // make sure we don't remove locks twice
    return*this;
  }
  ~polldir_queue(){
    if(removelocks_)removeLockVariables(qname_);
  }
  // put a message into queue
  // (returns true if message was enqueued, false if enqueing was disabled)
  bool enq(T t,boost::system::error_code&ec){
    // wait for state of queue is such that we can enque an element
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    ipcond_->wait(lock,[&](){return !enq_enabled_||!fullNolock();});

    // if enq is disabled we'll return
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    // we know we can now write message
    detail::queue_support::write(t,dir_,serial_);
    ipcond_->notify_all();
    return true;
  }
  // put a message into queue - timeout if waiting too lo
  // (returns true if message was enqueued, false if enqueing was disabled)
  bool timed_enq(T t,std::size_t ms,boost::system::error_code&ec){
    // wait for state of queue is such that we can enque an element
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    boost::system_time const tmo_ms=boost::get_system_time()+boost::posix_time::milliseconds(ms);
    bool tmo=!ipcond_->timed_wait(lock,tmo_ms,[&](){return !enq_enabled_||!fullNolock();});

    if(tmo){
      ec=boost::asio::error::timed_out;
      return false;
    }
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    // we know we can now write message
    detail::queue_support::write(t,dir_,serial_);
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // wait until we can put a message in queue
  // (returns false if enqueing was disabled, else true)
  bool wait_enq(boost::system::error_code&ec){
    // wait for the state of queue is such that we can return something
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    ipcond_->wait(lock,[&](){return !enq_enabled_||!fullNolock();});

    // if enq is disabled we'll return
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    // we know queue is not full
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // wait until we can put a message in queue - timeout if waiting too long
  bool timed_wait_enq(std::size_t ms,boost::system::error_code&ec){
    // wait for the state of queue is such that we can return something
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    boost::system_time const tmo_ms=boost::get_system_time()+boost::posix_time::milliseconds(ms);
    bool tmo=!ipcond_->timed_wait(lock,tmo_ms,[&](){return !enq_enabled_||!fullNolock();});
    if(tmo){
      ec=boost::asio::error::timed_out;
      return false;
    }
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // dequeue a message (return.first == false if deq() was disabled)
  std::pair<bool,T>deq(boost::system::error_code&ec){
    // wait for the state of queue is such that we can return something
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    ipcond_->wait(lock,[&](){return !deq_enabled_||!emptyNolock();});

    // if deq is disabled or timeout
    if(!enq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    // we know the cache is not empty now so no need to check
    fs::path file{cache_.front()};
    cache_.pop_front();
    T ret{detail::queue_support::read<T>(file,deser_)};
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return std::make_pair(true,ret);
  }
  // dequeue a message (return.first == false if deq() was disabled) - timeout if waiting too long
  std::pair<bool,T>timed_deq(std::size_t ms,boost::system::error_code&ec){
    // wait for the state of queue is such that we can return something
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    boost::system_time const tmo_ms=boost::get_system_time()+boost::posix_time::milliseconds(ms);
    bool tmo=!ipcond_->timed_wait(lock,tmo_ms,[&](){return !deq_enabled_||!emptyNolock();});

    // if deq is disabled or queue is empty return or timeout
    if(tmo){
      ec=boost::asio::error::timed_out;
       return std::make_pair(false,T{});
    }
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return std::make_pair(false,T{});
    }
    // we know the cache is not empty now so no need to check
    fs::path file{cache_.front()};
    cache_.pop_front();
    T ret{detail::queue_support::read<T>(file,deser_)};
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return std::make_pair(true,ret);
  }
  // wait until we can retrieve a message from queue
  bool wait_deq(boost::system::error_code&ec){
    // wait for the state of queue is such that we can return something
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    ipcond_->wait(lock,[&](){return !deq_enabled_||!emptyNolock();});

    // check if dequeue was disabled
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    // we know the cache is not empty now so no need to check
    fs::path file{cache_.front()};
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // wait until we can retrieve a message from queue -  timeout if waiting too long
  bool timed_wait_deq(std::size_t ms,boost::system::error_code&ec){
    // wait for the state of queue is such that we can return something
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    boost::system_time const tmo_ms=boost::get_system_time()+boost::posix_time::milliseconds(ms);
    bool tmo=!ipcond_->timed_wait(lock,tmo_ms,[&](){return !deq_enabled_||!emptyNolock();});
    if(tmo){
      ec=boost::asio::error::timed_out;
      return false;
    }
    if(!deq_enabled_){
      ec=boost::asio::error::operation_aborted;
      return false;
    }
    // we know the cache is not empty now so no need to check
    fs::path file{cache_.front()};
    ipcond_->notify_all();
    ec=boost::system::error_code();
    return true;
  }
  // cancel deq operations (will also release blocking threads)
  void disable_deq(bool disable){
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    deq_enabled_=!disable;
    ipcond_->notify_all();
  }
  // cancel enq operations (will also release blocking threads)
  void disable_enq(bool disable){
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    enq_enabled_=!disable;
    ipcond_->notify_all();
  }
  // set max size of queue
  void set_maxsize(std::size_t maxsize){
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    return maxsize_;
  }
  // check if queue is empty
  bool empty()const{
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    return emptyNolock();
  }
  // check if queue is full
  bool full()const{
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    return fullNolock();
  }
  // get #of items in queue
  std::size_t size()const{
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    return sizeNolock();
  }
  // get max items in queue
  std::size_t maxsize()const{
    ipc::scoped_lock<ipc::named_mutex>lock(*ipcmtx_);
    return maxsize_;
  }
  // get name of queue
  std::string qname()const{
    return qname_;
  }
  // remove lock variables for queue
  // (name of lock variables are computed from the path to the queue directory)
  static void removeLockVariables(std::string const&name){
    ipc::named_mutex::remove(name.c_str());
    ipc::named_condition::remove(name.c_str());
  }
private:
  // check if queue is full
  // (lock must be held when calling this function)
  bool fullNolock()const{
    // if maxsize_ == 0 we ignore checking the size of the queue
    if(maxsize_==0)return false;
    return sizeNolock()>=maxsize_;
  }
  // check if queue is empty 
  // (lock must be held when calling this function)
  bool emptyNolock()const{
    // cleanup cache and check if we have queue elements
    cleanCacheNolock();
    if(cache_.size()>0)return false;

    // cache is empty, fill it up and check if we are still empty
    fillCacheNolock(true);
    return cache_.size()==0;
  }
  // fill cache
  // (must be called when having the lock)
  void fillCacheNolock(bool forceRefill)const{
    // trash cache and re-read if 'forceRefill' is set or if cache is empty
    if(forceRefill||cache_.empty()){
      std::list<fs::path>sortedFiles{getTsOrderedFiles()};
      cache_.swap(sortedFiles);
    }
  }
  // remove any file in cache which do not exist in dir_
  // (must be called when having the lock)
  void cleanCacheNolock()const{
    std::list<fs::path>tmpCache;
    for(auto const&p:cache_)
      if(fs::exists(p))tmpCache.push_back(p);
    swap(tmpCache,cache_);
  }
  // get size of queue
  // (lock must be held when calling this function)
  size_t sizeNolock()const{
    // must forcefill cache to know the queue size
    fillCacheNolock(true);
    return cache_.size();
  }
  // get oldest file + manage cache_ if needed
  // (lock must be held when calling this function)
  std::pair<bool,fs::path>nextFileNolock()const{
    // clean cache and only fill cache if it's empty
    cleanCacheNolock();
    fillCacheNolock(false);
    if(cache_.empty())return std::make_pair(false,fs::path());
    std::pair<bool,fs::path>ret{std::make_pair(true,cache_.front())};
    cache_.pop_front();
    return ret;
  }
  // get all filenames in time sorted order in a dircetory
  std::list<fs::path>getTsOrderedFiles()const{
    // need a map with key=time, value=filename
    typedef std::multimap<time_t,fs::path>time_file_map_t;
    time_file_map_t time_file_map;

    // insert all files together with time as key into map
    fs::directory_iterator dir_end_iter;
    for(fs::directory_iterator it(dir_);it!=dir_end_iter;++it){
      if(!is_regular_file(*it))continue;
      time_t time_stamp(last_write_time(*it));
      time_file_map.insert(time_file_map_t::value_type(time_stamp,*it));
    }
    // create return list and return
    std::list<fs::path>ret;
    for(auto const&f:time_file_map)ret.push_back(f.second);
    return ret;
  }
  // user specified characteristics of queue
  std::string qname_;
  std::size_t maxsize_;
  fs::path dir_;

  // serialization/deserialization functions
  DESER deser_;
  SERIAL serial_;

  // should locks be removed
  bool removelocks_;

  // state of queue
  bool deq_enabled_=true;
  bool enq_enabled_=true;

  // mutex/condition variables
  // (note: must have pointers since no move ctor/assifn for these types)
  mutable std::shared_ptr<boost::interprocess::named_mutex>ipcmtx_;
  mutable std::shared_ptr<boost::interprocess::named_condition>ipcond_;

  // cache of message
  mutable std::list<fs::path>cache_;
};
}
}
#endif
