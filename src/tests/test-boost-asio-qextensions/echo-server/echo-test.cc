// (C) Copyright Hans Ewetz 2010,2011,2012,2013,2014,2015. All rights reserved.

#include "QueueDefs.h"
#include "QueueCreation.h"
#include "Service.h"

#include <boost/asio_queue.hpp>
#include <string>
#include <iostream>
#include <memory>

using namespace std;
using namespace std::placeholders;
namespace asio=boost::asio;

// asio io service
asio::io_service ios;

// client queue sender/receiver
shared_ptr<QueueSender>clientSender;
shared_ptr<QueueListener>clientReceiver;

// handle messages from service
void clientMsgHandler(boost::system::error_code const&ec,QueueValueType const&msg){
  cout<<"Client received message: \""<<msg<<"\""<<endl;
  clientReceiver->async_deq(clientMsgHandler);
}
// timer handler - sends a message each time timer pops
void timerHandler(boost::system::error_code const&ec,boost::asio::deadline_timer*timer){
  // send a message
  boost::system::error_code ec1;
  clientSender->sync_enq("Hello",ec1);

  // set timer again
  timer->expires_from_now(boost::posix_time::milliseconds(1000));
  timer->async_wait(std::bind(timerHandler,_1,timer));
}
// echo test program
int main(){
  // --- create queues
//  queue_struct queues=createMemQueues();
//  queue_struct queues=createAsymetricIpQueues();
  queue_struct queues=createSymetricIpQueues();

  // --- create service
  Service serv(::ios,queues.qserviceReceive,queues.qserviceReply);

  // --- create client sender/receiver
  clientSender=make_shared<QueueSender>(::ios,queues.qclientRequest.get());
  clientReceiver=make_shared<QueueListener>(::ios,queues.qclientReceive.get());

  // --- setup client event handler
  clientReceiver->async_deq(clientMsgHandler);

  // --- setup timer for sending messages
  boost::asio::deadline_timer timer(::ios,boost::posix_time::milliseconds(1000));
  timer.async_wait(std::bind(timerHandler,_1,&timer));

  // --- start asio event loop
  ::ios.run();
}

