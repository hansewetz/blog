#ifndef __OCCI_DETAIL_UTILS_H__
#define __OCCI_DETAIL_UTILS_H__
#include "type-utils/type_utils.h"
#include <occiData.h>
#include <memory>
#include <tuple>
#include <type_traits>
#include <stdexcept>

namespace tutils{
// fetch data from a result set and store in a variable (one struct for each type)
template<typename T>struct occi_data_fetcher_aux;
template<>struct occi_data_fetcher_aux<int>{
  static void fetch(std::size_t ind,std::shared_ptr<oracle::occi::ResultSet>rs,int&i){i=rs->getInt(ind+1);}
};
template<>struct occi_data_fetcher_aux<std::string>{
  static void fetch(std::size_t ind,std::shared_ptr<oracle::occi::ResultSet>rs,std::string&s){s=rs->getString(ind+1);}
};
// NOTE! add more types to select 
// ...
class occi_data_fetcher{
public:
  occi_data_fetcher(std::shared_ptr<oracle::occi::ResultSet>rs):rs_(rs){}
  occi_data_fetcher()=default;
  occi_data_fetcher(occi_data_fetcher const&)=default;
  occi_data_fetcher(occi_data_fetcher&&)=default;
  occi_data_fetcher&operator=(occi_data_fetcher const&)=default;
  occi_data_fetcher&operator=(occi_data_fetcher&&)=default;
  ~occi_data_fetcher()=default;
  std::shared_ptr<oracle::occi::ResultSet>getResultSet()const{return rs_;}
  bool operator==(occi_data_fetcher const&other)const{return rs_==other.rs_;}
  bool operator==(occi_data_fetcher&&other)const{return rs_==other.rs_;}
  template<typename T>
  void operator()(std::size_t ind,T&t)const{
    occi_data_fetcher_aux<T>::fetch(ind,rs_,t);
  }
private:
  std::shared_ptr<oracle::occi::ResultSet>rs_;   
};
// bind data to select statement (one variable at a time)
template<typename T>struct occi_data_binder_aux;
template<>struct occi_data_binder_aux<int>{
  static void bind(std::size_t ind,std::shared_ptr<oracle::occi::Statement>stmt,int val){
    stmt->setInt(ind+1,val);
  }
};
template<>struct occi_data_binder_aux<std::string>{
  static void bind(std::size_t ind,std::shared_ptr<oracle::occi::Statement>stmt,std::string const&val){
    stmt->setString(ind+1,val);
  }
};
// NOTE! add more types to bind 
// ...
class occi_data_binder{
public:
  occi_data_binder(std::shared_ptr<oracle::occi::Statement>stmt):stmt_(stmt){}
  occi_data_binder()=default;
  occi_data_binder(occi_data_binder const&)=default;
  occi_data_binder(occi_data_binder&&)=default;
  occi_data_binder&operator=(occi_data_binder&)=default;
  occi_data_binder&operator=(occi_data_binder&&)=default;
  ~occi_data_binder()=default;
  template<typename T>
  void operator()(std::size_t ind,T const&t){
    occi_data_binder_aux<T>::bind(ind,stmt_,t);
  }
private:
  std::shared_ptr<oracle::occi::Statement>stmt_;
};
// set maximum size a bind variable can have (used in update/insert/delete statements)
template<typename T>struct occi_bind_sizer_aux;
template<>struct occi_bind_sizer_aux<int>{
  static void setsize(std::size_t ind,std::shared_ptr<oracle::occi::Statement>stmt,size_t size){
    // nothing to do - not a variable size type
  }
};
template<>struct occi_bind_sizer_aux<std::string>{
  static void setsize(std::size_t ind,std::shared_ptr<oracle::occi::Statement>stmt,std::size_t size){
    // don't allow string length to have max size == 0
    if(size==0)throw std::runtime_error("invalid size: 0 for std::string bind variable while setting 'stmt->setMaxParamSize(ind,size)'");
    stmt->setMaxParamSize(ind+1,size);
  }
};
// NOTE! add more types to sizer 
// ...
template<typename ... Args>class occi_bind_sizer;
template<typename ... Args>
class occi_bind_sizer<std::tuple<Args ...>>{
public:
  // get bind type
  using Bind=std::tuple<Args ...>;

  occi_bind_sizer(std::shared_ptr<oracle::occi::Statement>stmt):stmt_(stmt){}
  occi_bind_sizer()=default;
  occi_bind_sizer(occi_bind_sizer const&)=default;
  occi_bind_sizer(occi_bind_sizer&&)=default;
  occi_bind_sizer&operator=(occi_bind_sizer&)=default;
  occi_bind_sizer&operator=(occi_bind_sizer&&)=default;
  ~occi_bind_sizer()=default;
  template<std::size_t Ind>
  void apply(size_t size){
    using T=typename std::decay<decltype(std::get<Ind>(Bind()))>::type;
    occi_bind_sizer_aux<T>::setsize(Ind,stmt_,size);
  }
private:
  std::shared_ptr<oracle::occi::Statement>stmt_;
};
}
#endif
