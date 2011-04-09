#ifndef __Cache_h__
#define __Cache_h__

#ifdef _MSC_VER
#pragma warning (disable : 4786)
#pragma warning (disable : 4503)
#endif

#ifdef USE_SGI_STL
#include <hash_map>
#else
#include <map>
#endif

#ifdef USE_SGI_STL
__STL_TEMPLATE_NULL struct std::hash<const std::string>
{
  size_t operator()(const std::string __s) const
  {
    return __stl_hash_string(__s.c_str ());
  }
};
#endif

template <class KeyT, class ValueT>
class CacheLevel
{
public:
  typedef CacheLevel<KeyT, ValueT> CacheLevelT;
protected:
  CacheLevelT *next_;
public:
  // Types
  typedef class KeyT KeyT_;
  typedef class ValueT ValueT_;
  typedef bool (*EnumCallBack_t) (const ValueT &key, void *pData);
  // Methods
  CacheLevel ();
  virtual ~CacheLevel () {}
  CacheLevelT *setNextLevel (CacheLevelT *nextLevel);
  CacheLevelT *getNextLevel () { return next_; }
  virtual bool isInCache (const KeyT &key) { return false; }
  virtual bool get (const KeyT &key, ValueT *pval) = 0;
  virtual void forall (const KeyT &key, EnumCallBack_t ecb, void *pData) = 0;
  virtual bool set (const KeyT &key, const ValueT &val) = 0;
  virtual void erase (const KeyT &key) {} // removes the key / value pair from the cache
  virtual void droped (const KeyT &key) {} // called by higher level when the key is dropped out of cache. Lower level may overwrite this to close resources etc.
  virtual bool flush () = 0;
  virtual bool setSize (long size) = 0; // ??? may be {}
  virtual long getSize () = 0; // ??? may be {}
} ;

template <class KeyT, class ValueT>
class MemCache : public CacheLevel <KeyT, ValueT>
{
  template <class KeyT, class ValueT>
  class Record
  {
  public:
    Record () : dirty (false)
    {
      prev = this;
      next = this;
    }
    void erase ()
    {
      prev->next = next;
      next->prev = prev;
      prev = this;
      next = this;
    }
    void push_front (Record &list)
    {
      next = list.next;
      prev = &list;
      next->prev = this;
      list.next = this;
    }
    void move_front (Record &list)
    {
      // Free from existing place
      prev->next = next;
      next->prev = prev;
      // Insert to front
      push_front (list);
    }
    Record *prev;
    Record *next;
    bool dirty;
    KeyT   key;
    ValueT val;
  } ;
  typedef Record<KeyT, ValueT> RecordT;
  RecordT *reserve ();
  RecordT lru_;
#ifdef USE_SGI_STL
  typedef std::hash_map<const KeyT, RecordT *> MapT;
#else
  typedef std::map<const KeyT, RecordT *> MapT;
#endif
  MapT map_;
  long storeSize_;
  long size_;
  long purgeSize_;
public:
  // CacheLevel protocol
  MemCache ();
  virtual ~MemCache ();
  virtual bool isInCache (const KeyT &key) { return map_.find (key) != map_.end (); }
  virtual bool get (const KeyT &key, ValueT *pval);
  virtual void forall (const KeyT &key, EnumCallBack_t ecb, void *pData);
  virtual bool set (const KeyT &key, const ValueT &val);
  virtual void erase (const KeyT &key);
  virtual bool flush ();
  virtual bool setSize (long size);
  virtual long getSize ();
  // MemCache protocol
  virtual bool setPurgeSize (long purgeSize);
} ;

#include "Cache.i"

#endif/* __Cache_h__ */
