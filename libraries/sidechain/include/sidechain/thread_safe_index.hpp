#pragma once

#include <mutex>

namespace sidechain {

struct by_id;

template<class T1>
class thread_safe_index {

public:

   using iterator = typename T1::iterator;
   using iterator_id = typename T1::template index<by_id>::type::iterator;

   std::pair<iterator,bool> insert( const typename T1::value_type& value ) {
      std::lock_guard<std::recursive_mutex> locker( lock );
      return data.insert( value );
   }

   void modify( const typename T1::value_type& obj, const std::function<void( typename T1::value_type& e)>& func ) {
      std::lock_guard<std::recursive_mutex> locker( lock );
      data.modify( data.iterator_to(obj), [&func]( typename T1::value_type& obj ) { func(obj); } );
   }

   void remove( const typename T1::value_type& obj ) {
      std::lock_guard<std::recursive_mutex> locker( lock );
      data.erase( data.iterator_to( obj ) );
   }

   size_t size() {
      std::lock_guard<std::recursive_mutex> locker( lock );
      return data.size();
   }

   std::pair<bool, iterator_id> find( uint64_t id ) {
      std::lock_guard<std::recursive_mutex> locker( lock );
      auto& index = data.template get<by_id>();
      auto it = index.find( id );
      if( it != index.end() ) {
         return std::make_pair(true, it);
      }
      return std::make_pair(false, it);
   }

   template<class T2>
   void safe_for(std::function<void(typename T1::template index<T2>::type::iterator itr1, 
                                    typename T1::template index<T2>::type::iterator itr2)> func) {
       std::lock_guard<std::recursive_mutex> locker( lock );
       auto& index = data.template get<T2>();
       func(index.begin(), index.end());
   }

private:

   std::recursive_mutex lock;
   
   T1 data;

};

}
