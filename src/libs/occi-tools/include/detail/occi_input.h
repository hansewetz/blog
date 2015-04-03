#ifndef __OCCI_INPUT_H__
#define __OCCI_INPUT_H__
#include "occi_utils.h"
#include "occi_detail_utils.h"
#include "type-utils/type_utils.h"
#include <occiData.h>
#include <boost/iterator/iterator_facade.hpp>
#include <memory>
#include <tuple>
#include <stdexcept>
namespace occi_utils{

// input iterator
template<typename Row,typename Bind=std::tuple<>,typename ILBind=typename utils::make_indlist_from_tuple<Bind>::type>
class occi_input_iterator:public boost::iterator_facade<occi_input_iterator<Row,Bind>,Row const,boost::single_pass_traversal_tag>{
friend class boost::iterator_core_access;
public:
  // ctor, assign,dtor
  explicit occi_input_iterator(oracle::occi::Connection*conn,std::string const&sql,std::size_t prefetchcount=0,Bind const&bind=Bind{}):
      conn_(conn),sql_(sql),bind_(bind),fetcher_(),stmt_(nullptr),end_(false),prefetchcount_(prefetchcount){
    // create row fetcher and fetch first row
    stmt_=std::shared_ptr<oracle::occi::Statement>{conn_->createStatement(),occi_stmt_deleter(conn_)};
    stmt_->setSQL(sql_);
    if(prefetchcount_>0)stmt_->setPrefetchRowCount(prefetchcount_);
    occi_data_binder binder(stmt_);
    utils::apply_ind_tuple_ntimes_with_indlist(ILBind(),binder,bind_);
    std::shared_ptr<oracle::occi::ResultSet>rs(stmt_->executeQuery(),occi_rs_deleter(stmt_.get()));
    fetcher_=occi_data_fetcher(rs);
    nextrow();
  }
  occi_input_iterator():end_(true),bind_(Bind{}){}
  occi_input_iterator(occi_input_iterator const&)=default;
  occi_input_iterator(occi_input_iterator&&)=default;
  occi_input_iterator&operator=(occi_input_iterator const&)=default;
  occi_input_iterator&operator=(occi_input_iterator&&)=default;
  ~occi_input_iterator()=default;
private:
  // iterator functions
  void increment(){nextrow();}
  bool equal(occi_input_iterator const&other)const{
    // any iterators which is marked with 'end_' are identical
    if(end_||other.end_)return end_&&other.end_;
    return conn_==other.conn_&&sql_==other.sql_&&stmt_==other.stmt_&&fetcher_==other.fetcher_;
  }
  Row const&dereference()const{
    if(end_)throw std::runtime_error("occi_input_iterator<>: attempt to dereference end iterator");
    return currentRow_;
  }
  // get next row
  void nextrow(){
    if(end_)throw std::runtime_error("occi_input_iterator<>: attempt to step past end iterator");
    using IL=typename utils::make_indlist_from_tuple<decltype(currentRow_)>::type;
    if(fetcher_.getResultSet()->next())utils::apply_ind_tuple_ntimes_with_indlist(IL(),fetcher_,currentRow_);
    else end_=true;
  }
  // state
  const std::string sql_;
  const Bind bind_;
  oracle::occi::Connection*conn_;    
  std::shared_ptr<oracle::occi::Statement>stmt_;
  occi_data_fetcher fetcher_;         
  Row currentRow_;           
  bool end_;
  std::size_t prefetchcount_;
};
// source (can be viewed as a 'collection')
template<typename Row,typename Bind=std::tuple<>>
class occi_source{
public:
  // typedefs
  typedef typename occi_input_iterator<Row,Bind>::value_type value_type;
  typedef typename occi_input_iterator<Row,Bind>::pointer pointer;
  typedef typename occi_input_iterator<Row,Bind>::reference reference;
  typedef occi_input_iterator<Row,Bind>iterator;

  // ctor taking authentication
  explicit occi_source(occi_auth const&auth,std::string const&sql,std::size_t prefetchcount=0,Bind const&bind=Bind{}):
      conn_(nullptr),connpool_(nullptr),env_(nullptr),auth_(auth),sql_(sql),bind_(bind),closeConn_(true),
      releaseConn_(false),terminateEnv_(true),prefetchcount_(prefetchcount){
    env_=oracle::occi::Environment::createEnvironment(oracle::occi::Environment::DEFAULT);
    conn_=env_->createConnection(std::get<0>(auth_),std::get<1>(auth_),std::get<2>(auth_));
  } 
  // ctor taking environment + authentication
  explicit occi_source(oracle::occi::Environment*env,occi_auth const&auth,std::string const&sql,std::size_t prefetchcount=0,Bind const&bind=Bind{}):
      conn_(nullptr),connpool_(nullptr),env_(env),auth_(auth),sql_(sql),bind_(bind),closeConn_(true),
      releaseConn_(false),terminateEnv_(false),prefetchcount_(prefetchcount){
    conn_=env_->createConnection(std::get<0>(auth_),std::get<1>(auth_),std::get<2>(auth_));
  } 
  // ctor taking an open connection
  explicit occi_source(oracle::occi::Connection*conn,std::string const&sql,std::size_t prefetchcount=0,Bind const&bind=Bind{}):
      conn_(conn),connpool_(nullptr),env_(nullptr),sql_(sql),bind_(bind),closeConn_(false),
      releaseConn_(false),terminateEnv_(false),prefetchcount_(prefetchcount){
  }
  // ctor taking a stateless connection pool
  explicit occi_source(oracle::occi::StatelessConnectionPool*connpool,std::string const&sql,std::size_t prefetchcount=0,Bind const&bind=Bind{}):
      conn_(nullptr),connpool_(connpool),env_(nullptr),sql_(sql),bind_(bind),closeConn_(false),
      releaseConn_(true),terminateEnv_(false),prefetchcount_(prefetchcount){
    conn_=connpool_->getConnection();
  }
  // ctors, assign (movable but not copyable)
  occi_source()=delete;
  occi_source(occi_source const&)=delete;
  occi_source(occi_source&&s):
      conn_(s.conn_),connpool_(s.connpool_),env_(s.env_),
      closeConn_(s.closeConn_),releaseConn_(s.releaseConn_),terminateEnv_(s.terminateEnv_),
      auth_(s.auth_),sql_(s.sql_),bind_(s.bind_){
    // reset all relevant state
    reset_state(std::forward<occi_source<Row,Bind>>(s));
  }
  occi_source&operator=(occi_source const&)=delete;
  occi_source&operator=(occi_source&&s){
    // std::swap with no throw and reset state of parameter
    swap(s);
    reset_state(std::forward<occi_source<Row,Bind>>(s));
  }
  // dtor - close occi resources if needed
  ~occi_source(){
    // take care of connection
    if(closeConn_)env_->terminateConnection(conn_);else
    if(releaseConn_)connpool_->releaseConnection(conn_);

    // take care of environment
    if(terminateEnv_)oracle::occi::Environment::terminateEnvironment(env_);
  }
  // get begin/end iterators
  iterator begin()const{return iterator(conn_,sql_,prefetchcount_,bind_);}
  iterator end()const{return iterator();}

  // swap function
  void swap(occi_source&s)noexcept(true){
    std::swap(s.conn_,conn_);std::swap(s.connpool_,connpool_);std::swap(s.env_,env_);
    std::swap(s.closeConn_,closeConn_);std::swap(s.releaseConn_,releaseConn_);std::swap(s.terminateEnv_,terminateEnv_);
    std::swap(s.auth_,auth_);std::swap(s.sql_,sql_);std::swap(s.bind_,bind_);
  }
private:
  // reset state of an rvalue
  void reset_state(occi_source&&s){
    s.conn_=nullptr;s.connpool_=nullptr;s.env_=nullptr;
    s.closeConn_=false;s.releaseConn_=false;s.terminateEnv_=false;
    s.auth_=occi_auth{};s.sql_="";s.bind_=Bind{};
  }
  // occi resources
  oracle::occi::Connection*conn_;    
  oracle::occi::StatelessConnectionPool*connpool_;    
  oracle::occi::Environment*env_;

  // authentication + sql statement + bind varables
  occi_auth auth_;
  std::string sql_;
  Bind bind_;

  // controll of what to do with occi resources
  bool closeConn_;
  bool releaseConn_;
  bool terminateEnv_;
  std::size_t prefetchcount_;
};
}
#endif
