// (C) Copyright Hans Ewetz 2010,2011,2012,2013,2014,2015. All rights reserved.

#include "Service.h"
#include <functional>
using namespace std;
using namespace std::placeholders;
namespace asio=boost::asio;

// ctor
Service::Service(asio::io_service&ios,shared_ptr<QueueDeqBase>qserviceReceive,shared_ptr<QueueEnqBase>qserviceReply):
    serviceReceiver_(make_shared<QueueListener>(ios,qserviceReceive.get())),
    serviceSender_(make_shared<QueueSender>(ios,qserviceReply.get())){
  // setup asio callback
  serviceReceiver_->async_deq(std::bind(&Service::recvHandler,this,_1,_2));
}
// handler for receiving message
void Service::recvHandler(boost::system::error_code const&ec,QueueValueType const&msg){
  // echo back message to output queue
  boost::system::error_code ec1;
  serviceSender_->sync_enq(msg,ec1);

  // trigger for receiving messages again
  serviceReceiver_->async_deq(std::bind(&Service::recvHandler,this,_1,_2));
}
