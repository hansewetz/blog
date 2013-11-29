#ifndef __OCCI_OUTPUT_H__ 
#define __OCCI_OUTPUT_H__ 
#include "occi_utils.h"
#include "occi_detail_utils.h"
#include "type-utils/type_utils.h"
#include <occiData.h>
#include <boost/function_output_iterator.hpp>
#include <memory>
#include <tuple>
#include <stdexcept>
#include <type_traits>

namespace tutils{
// forward decl
template<typename Bind>class occi_output_iterator;

// function called for each modification
// (function delegates back to iterator to do work)
template<typename Bind>
class occi_unary_output:std::unary_function<Bind const&,void>{
public:
  // ctors
  occi_unary_output(occi_output_iterator<Bind>*it):it_(it){}

  // modification function
  void operator()(Bind const&bind){
    it_->operator()(bind);
  }
private:
  occi_output_iterator<Bind>*it_;
};
// wrapper around boost::function_output_iterator
template<typename Bind>
class occi_output_iterator:public boost::function_output_iterator<occi_unary_output<Bind>>{
friend class occi_unary_output<Bind>;
public:
  // typedef for simpler declarations (tuple<int,int,...> with as many elements a Bind)
  using Size=typename uniform_tuple_builder<std::tuple_size<Bind>::value,std::size_t>::type;

  // ctors, assign, dtor
  occi_output_iterator(oracle::occi::Connection*conn,std::string const&sql,std::size_t batchsize=1,Size const&size=Size()):
      boost::function_output_iterator<occi_unary_output<Bind>>(occi_unary_output<Bind>(this)),
      conn_(conn),sql_(sql),batchsize_(batchsize),nwaiting_(0){

    // create statement, set batch modification, create statement and binder
    stmt_=std::shared_ptr<oracle::occi::Statement>{conn_->createStatement(sql_),occi_stmt_deleter(conn_)};
    stmt_->setMaxIterations(batchsize_);

    // set max size of bind variables which have variable length
    if(batchsize==0){
      throw std::runtime_error("invalid batchsize: 0 while constructing occi_output_iterator");
    }else
    if(batchsize>1){
      // set size for variable size bind variables (will throw exception if size==0 for variable size bind variable)
      occi_bind_sizer<Bind>sizer{stmt_};
      apply_with_index_template(sizer,size);
    }
    // create binder object
    binder_=occi_data_binder(stmt_);
  }
  occi_output_iterator(occi_output_iterator const&)=default;
  occi_output_iterator(occi_output_iterator&&)=default;
  occi_output_iterator&operator=(occi_output_iterator&)=default;
  occi_output_iterator&operator=(occi_output_iterator&&)=default;
  ~occi_output_iterator()=default;

  // explicitly execute buffered statements
  void flush(){flushAux();}
private:
  // modification function
  void operator()(Bind const&bind){
    // check if weed to add previous row, bind new row and check if we need to flush (execute)
    ++nwaiting_;
    if(nwaiting_>1)stmt_->addIteration();
    using ILBind=typename make_indlist_from_tuple<Bind>::type;
    apply_ind_tuple_ntimes_with_indlist(ILBind(),binder_,bind);
    if(nwaiting_==batchsize_)flushAux();
  }
  // flush remaining statements
  void flushAux(){
    if(nwaiting_>0){
      stmt_->executeUpdate();
      nwaiting_=0;
    }
  }
private:
  oracle::occi::Connection*conn_;
  std::string const&sql_;
  std::shared_ptr<oracle::occi::Statement>stmt_;
  occi_data_binder binder_;
  std::size_t batchsize_;
  std::size_t nwaiting_;
};
// sink 
template<typename Bind=std::tuple<>>
class occi_sink{
public:
  // typedef for simpler declarations (tuple<int,int,...> with as many elements a Bind)
  using Size=typename uniform_tuple_builder<std::tuple_size<Bind>::value,std::size_t>::type;

  // enum for controlling commit, rollback or do nothing
  enum commit_t:int{Rollback=0,Commit=1,Nop=2};
  // typedefs
  typedef typename occi_output_iterator<Bind>::value_type value_type;
  typedef typename occi_output_iterator<Bind>::pointer pointer;
  typedef typename occi_output_iterator<Bind>::reference reference;
  typedef occi_output_iterator<Bind>iterator;

  // ctor taking authentication (commit default == true since sink manages resources)
  explicit occi_sink(occi_auth const&auth,std::string const&sql,std::size_t batchsize=1,Size const&size=Size(),commit_t commit=Commit):
      conn_(nullptr),connpool_(nullptr),env_(nullptr),auth_(auth),sql(sql),closeConn_(true),
      releaseConn_(false),terminateEnv_(true),batchsize_(batchsize),size_(size),commit_(commit){
    check_batchsize(batchsize_);
    env_=oracle::occi::Environment::createEnvironment(oracle::occi::Environment::DEFAULT);
    conn_=env_->createConnection(std::get<0>(auth_),std::get<1>(auth_),std::get<2>(auth_));
  } 
  // ctor taking environment + authentication (commit default == true since sink manages resources)
  explicit occi_sink(oracle::occi::Environment*env,occi_auth const&auth,std::string const&sql,std::size_t batchsize=1,Size const&size=Size(),commit_t commit=Commit):
      conn_(nullptr),connpool_(nullptr),env_(env),auth_(auth),sql(sql),closeConn_(true),
      releaseConn_(false),terminateEnv_(false),batchsize_(batchsize),size_(size),commit_(commit){
    check_batchsize(batchsize_);
    conn_=env_->createConnection(std::get<0>(auth_),std::get<1>(auth_),std::get<2>(auth_));
  } 
  // ctor taking an open connection (commit default == false since sink does not manage resources)
  explicit occi_sink(oracle::occi::Connection*conn,std::string const&sql,std::size_t batchsize=1,Size const&size=Size(),commit_t commit=Nop):
      conn_(conn),connpool_(nullptr),env_(nullptr),sql(sql),closeConn_(false),
      releaseConn_(false),terminateEnv_(false),batchsize_(batchsize),size_(size),commit_(commit){
    check_batchsize(batchsize_);
  }
  // ctor taking a stateless connection pool (commit default == false since sink does not manage resources)
  explicit occi_sink(oracle::occi::StatelessConnectionPool*connpool,std::string const&sql,std::size_t batchsize=1,Size const&size=Size(),commit_t commit=Nop):
      conn_(nullptr),connpool_(connpool),env_(nullptr),sql(sql),closeConn_(false),
      releaseConn_(true),terminateEnv_(false),batchsize_(batchsize),size_(size),commit_(commit){
    check_batchsize(batchsize_);
    conn_=connpool_->getConnection();
  }
  // ctors, assign (movable but not copyable)
  occi_sink()=delete;
  occi_sink(occi_sink const&)=delete;
  occi_sink(occi_sink&&s):
      conn_(s.conn_),connpool_(s.connpool_),env_(s.env_),
      closeConn_(s.closeConn_),releaseConn_(s.releaseConn_),terminateEnv_(s.terminateEnv_),
      auth_(s.auth_),sql(s.sql){
    // reset all relevant state
    reset_state(std::forward<occi_sink<Bind>>(s));
  }
  occi_sink&operator=(occi_sink const&)=delete;
  occi_sink&operator=(occi_sink&&s){
    // std::swap with no throw and reset state of parameter
    swap(s);
    reset_state(std::forward<occi_sink<Bind>>(s));
  }
  // dtor - close occi resources if needed
  ~occi_sink(){
    // check if we should commit
    if(commit_==Commit)conn_->commit();else
    if(commit_==Rollback)conn_->rollback();

    // take care of connection
    if(closeConn_)env_->terminateConnection(conn_);else
    if(releaseConn_)connpool_->releaseConnection(conn_);

    // take care of environment
    if(terminateEnv_)oracle::occi::Environment::terminateEnvironment(env_);
  }
  // get begin/end iterators
  iterator begin()const{return iterator(conn_,sql,batchsize_,size_);}

  // swap function
  void swap(occi_sink&s)noexcept(true){
    std::swap(s.conn_,conn_);std::swap(s.connpool_,connpool_);std::swap(s.env_,env_);
    std::swap(s.closeConn_,closeConn_);std::swap(s.releaseConn_,releaseConn_);std::swap(s.terminateEnv_,terminateEnv_);
    std::swap(s.auth_,auth_);std::swap(s.sql,sql);swap(s.batchsize_,batchsize_);swap(s.size_,size_);swap(s.commit_,commit_);
  }
private:
  // check if batchsize is valid
  void check_batchsize(std::size_t batchsize){
    if(batchsize==0)throw std::runtime_error("invalid batchsize: 0 while constructing occi_sink");
  }
  // reset state for a sink (only called with r-value)
  void reset_state(occi_sink&&s){
    s.conn_=nullptr;s.connpool_=nullptr;s.env_=nullptr;
    s.closeConn_=false;s.releaseConn_=false;s.terminateEnv_=false;
    s.auth_=occi_auth{};s.sql="";s.batchsize_=0;s.size_=Size{};s.commit_=false;
  }
  // occi resources
  oracle::occi::Connection*conn_;    
  oracle::occi::StatelessConnectionPool*connpool_;    
  oracle::occi::Environment*env_;

  // authentication + sql statement
  occi_auth auth_;
  std::string sql;
  std::size_t batchsize_;
  Size size_;

  // controll of what to do with occi resources
  bool closeConn_;
  bool releaseConn_;
  bool terminateEnv_;
  commit_t commit_;
};
}
#endif
