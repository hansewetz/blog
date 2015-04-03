// Copyright (c) 2003-2015 Hans Ewetz (hansewetz at hotmail dot com)
// Distributed under the Boost Software License, Version 1.0. 

#ifndef __QUEUE_LISTENER_H__
#define __QUEUE_LISTENER_H__
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <cstddef>
#include <thread>
namespace boost{
namespace asio{

// forward decl
class queue_listener_impl;
template<typename Impl=queue_listener_impl>class basic_queue_listener_service;

// --- IO Object (used by client) -----------------------------
template<typename Service,typename Queue>
class basic_queue_listener:public boost::asio::basic_io_object<Service>{
public:
  // ctor
  explicit basic_queue_listener(boost::asio::io_service&io_service,Queue*q):
      boost::asio::basic_io_object<Service>(io_service),q_(q){
  }
  // async deq operation
  template <typename Handler>
  void async_deq(Handler handler) {
    this->service.async_deq(this->implementation,q_,handler);
  }
  // async deq operation - timed
  template <typename Handler>
  void timed_async_deq(Handler handler,std::size_t ms) {
    this->service.timed_async_deq(this->implementation,q_,handler,ms);
  }
  // wait until we can deq a message from queue in async mode
  template <typename Handler>
  void async_wait_deq(Handler handler) {
    this->service.async_wait_deq(this->implementation,q_,handler);
  }
  // wait until we can deq a message from queue in async mode - timed
  template <typename Handler>
  void timed_async_wait_deq(Handler handler,std::size_t ms) {
    this->service.timed_async_wait_deq(this->implementation,q_,handler,ms);
  }
  // sync deq operation (blocking)
  std::pair<bool,typename Queue::value_type>sync_deq(boost::system::error_code&ec){
    return this->service.sync_deq(this->implementation,q_,ec);
  }
private:
  Queue*q_;
};
// typedefs for using standard queue listeners
template<typename Queue>using queue_listener=basic_queue_listener<basic_queue_listener_service<>,Queue>;

// --- service class -----------------------------
// (for one io_service, only one object created)
template<typename Impl>
class basic_queue_listener_service:public boost::asio::io_service::service{
public:
  // required to have id of service
 static boost::asio::io_service::id id;

  // ctor
  explicit basic_queue_listener_service(boost::asio::io_service&io_service):
      boost::asio::io_service::service(io_service){
  }
  // dtor
  ~basic_queue_listener_service(){
  }
  // get a typedef  for implementation
  using implementation_type=std::shared_ptr<Impl>;

  // mandatory (construct an implementation object)
  void construct(implementation_type&impl){
      impl.reset(new Impl(this->get_io_service()));
  }
  // mandatory (destroy an implementation object)
  void destroy(implementation_type&impl){
    impl.reset();
  }
  // async sync deq operation
  template <typename Handler,typename Queue>
  void async_deq(implementation_type&impl,Queue*q,Handler handler){
    // this is a non-blocking operation so we are OK calling impl object in this thread
    impl->async_deq(impl,q,handler);
  }
  // async sync deq operation - timed
  template <typename Handler,typename Queue>
  void timed_async_deq(implementation_type&impl,Queue*q,Handler handler,std::size_t ms){
    // this is a non-blocking operation so we are OK calling impl object in this thread
    impl->timed_async_deq(impl,q,handler,ms);
  }
  // async sync wait operation
  template <typename Handler,typename Queue>
  void async_wait_deq(implementation_type&impl,Queue*q,Handler handler){
    // this is a non-blocking operation so we are OK calling impl object in this thread
    impl->async_wait_deq(impl,q,handler);
  }
  // async sync wait operation - timed
  template <typename Handler,typename Queue>
  void timed_async_wait_deq(implementation_type&impl,Queue*q,Handler handler,std::size_t ms){
    // this is a non-blocking operation so we are OK calling impl object in this thread
    impl->timed_async_wait_deq(impl,q,handler,ms);
  }
  // sync deq operation (blocking)
  template <typename Queue>
  std::pair<bool,typename Queue::value_type>sync_deq(implementation_type&impl,Queue*q,boost::system::error_code&ec){
    return impl->sync_deq(q,ec);
  }
private:
  // shutdown service (required)
  void shutdown_service(){
  }
};
// definition of id of service (required)
template <typename Impl>
boost::asio::io_service::id basic_queue_listener_service<Impl>::id;

// --- implementation -----------------------------
class queue_listener_impl{
public:
  // ctor (set up work queue for io_service so we don't bail out when executing run())
  queue_listener_impl(boost::asio::io_service&post_io_service):
      impl_work_(new boost::asio::io_service::work(impl_io_service_)),
      impl_thread_([&](){impl_io_service_.run();}),
      post_io_service_(post_io_service){
  }
  // dtor (clear work queue, stop io service and join thread)
  ~queue_listener_impl(){
    impl_work_.reset(nullptr);
    impl_io_service_.stop();
    if(impl_thread_.joinable())impl_thread_.join();
  }
public:
  // deque message (post request to thread)
  template<typename Handler,typename Queue>
  void async_deq(std::shared_ptr<queue_listener_impl>impl,Queue*q,Handler handler){
    impl_io_service_.post(deq_operation<Handler,Queue>(impl,post_io_service_,q,handler));
  }
  // deque message (post request to thread) - timed
  template<typename Handler,typename Queue>
  void timed_async_deq(std::shared_ptr<queue_listener_impl>impl,Queue*q,Handler handler,std::size_t ms){
    impl_io_service_.post(deq_operation<Handler,Queue>(impl,post_io_service_,q,handler,ms));
  }
  // wait to deq message (post request to thread)
  template<typename Handler,typename Queue>
  void async_wait_deq(std::shared_ptr<queue_listener_impl>impl,Queue*q,Handler handler){
    impl_io_service_.post(wait_deq_operation<Handler,Queue>(impl,post_io_service_,q,handler));
  }
  // wait to deq message (post request to thread) - timed
  template<typename Handler,typename Queue>
  void timed_async_wait_deq(std::shared_ptr<queue_listener_impl>impl,Queue*q,Handler handler,std::size_t ms){
    impl_io_service_.post(wait_deq_operation<Handler,Queue>(impl,post_io_service_,q,handler,ms));
  }
  // dequeue message (blocking deq)
  template<typename Queue>
  std::pair<bool,typename Queue::value_type>sync_deq(Queue*q,boost::system::error_code&ec){
    return q->deq(ec);
  }
private:
  // function object calling blocking deq() on queue
  template <typename Handler,typename Queue>
  class deq_operation{
  public:
    // ctor
    deq_operation(std::shared_ptr<queue_listener_impl>impl,boost::asio::io_service&io_service,Queue*q,Handler handler):
        wimpl_(impl),io_service_(io_service),work_(io_service),q_(q),handler_(handler),is_timed_(false),ms_(0) {
    }
    // ctor - timed
    deq_operation(std::shared_ptr<queue_listener_impl>impl,boost::asio::io_service&io_service,Queue*q,Handler handler,std::size_t ms):
        wimpl_(impl),io_service_(io_service),work_(io_service),q_(q),handler_(handler),is_timed_(true),ms_(ms){
    }
    // function calling implementation object - runs in the thread created in ctor
    void operator()(){
      // make sure implementation object is still valid
      std::shared_ptr<queue_listener_impl>impl{wimpl_.lock()};

      // if valid, go ahead and do blocking call on queue, otherwise post aborted message
      boost::system::error_code ec;
      std::pair<bool,typename Queue::value_type>ret;
      if(impl){
        if(is_timed_)ret=q_->timed_deq(ms_,ec);
        else ret=q_->deq(ec);
        this->io_service_.post(boost::asio::detail::bind_handler(handler_,ec,ret.second));
      }else{
        this->io_service_.post(boost::asio::detail::bind_handler(handler_,ec,typename Queue::value_type()));
      }
    }
  private:
    std::weak_ptr<queue_listener_impl>wimpl_;
    boost::asio::io_service&io_service_;
    boost::asio::io_service::work work_;
    Queue*q_;
    Handler handler_;
    bool is_timed_;
    std::size_t ms_;
  };
  // function object calling blocking wait() on queue
  template <typename Handler,typename Queue>
  class wait_deq_operation{
  public:
    // ctor
    wait_deq_operation(std::shared_ptr<queue_listener_impl>impl,boost::asio::io_service &io_service,Queue*q,Handler handler):
        wimpl_(impl),io_service_(io_service),work_(io_service),q_(q),handler_(handler),is_timed_(false),ms_(0){
    }
    // ctor - timed
    wait_deq_operation(std::shared_ptr<queue_listener_impl>impl,boost::asio::io_service &io_service,Queue*q,Handler handler,std::size_t ms):
        wimpl_(impl),io_service_(io_service),work_(io_service),q_(q),handler_(handler),is_timed_(true),ms_(ms){
    }
    // function calling implementation object - runs in the thread created in ctor
    void operator()(){
      // make sure implementation object is still valid
      std::shared_ptr<queue_listener_impl>impl{wimpl_.lock()};

      // if valid, go ahead and do (potentially) blocking call on queue, otherwise post aborted message
      boost::system::error_code ec;
      if(impl){
        if(is_timed_)q_->timed_wait_deq(ms_,ec);
        else q_->wait_deq(ec);
        this->io_service_.post(boost::asio::detail::bind_handler(handler_,ec));
      }else{
        this->io_service_.post(boost::asio::detail::bind_handler(handler_,ec));
      }
    }
  private:
    std::weak_ptr<queue_listener_impl>wimpl_;
    boost::asio::io_service&io_service_;
    boost::asio::io_service::work work_;
    Queue*q_;
    Handler handler_;
    bool is_timed_;
    std::size_t ms_;
  };
  // private data
  boost::asio::io_service impl_io_service_;
  std::unique_ptr<boost::asio::io_service::work>impl_work_;
  std::thread impl_thread_;
  boost::asio::io_service&post_io_service_;
};
}
}
#endif
