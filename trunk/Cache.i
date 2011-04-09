//////////////////////////////////////////////////////////////////////
// CacheLevel

template <class KeyT, class ValueT>
CacheLevel<KeyT, ValueT>::CacheLevel () :
  next_ (0)
{
}

template <class KeyT, class ValueT>
CacheLevel<KeyT, ValueT>::CacheLevelT *CacheLevel<KeyT, ValueT>::setNextLevel (CacheLevelT *nextLevel)
{
  CacheLevelT *prev = next_;
  next_ = nextLevel;
  return prev;
}

//////////////////////////////////////////////////////////////////////
// MemCache

template <class KeyT, class ValueT>
MemCache<KeyT, ValueT>::RecordT *MemCache<KeyT, ValueT>::reserve ()
{
  RecordT *rec;
  if (storeSize_ < size_) {
    // Make new record
    rec = new RecordT ();
    ++storeSize_;
  } else {
    // Reuse old one
    rec = lru_.prev;
    int count = 0;
    while (rec->dirty && count++ < purgeSize_) rec = rec->prev;
    if (rec->dirty) {
      RecordT *rec1 = lru_.prev;
      for (count = 0; count < purgeSize_; ++count) {
        next_->set (rec1->key, rec1->val);
        rec1->dirty = false;
        rec1 = rec1->prev;
      }
    }
    map_.erase (rec->key);
    rec->prev->next = rec->next;
    rec->next->prev = rec->prev;
  }
  // Push rec into lru list front
  rec->push_front (lru_);

  return rec;
}
// Public interface

template <class KeyT, class ValueT>
MemCache<KeyT, ValueT>::MemCache<KeyT, ValueT>() :
  CacheLevel<KeyT, ValueT> (),
  size_ (0),
  storeSize_ (0),
  purgeSize_ (0)
{
}

template <class KeyT, class ValueT>
MemCache<KeyT, ValueT>::~MemCache ()
{
  this->flush ();
  while (lru_.prev != &lru_) {
    RecordT *rec = lru_.prev;
    rec->erase ();
    delete rec;
  }
}

template <class KeyT, class ValueT>
bool MemCache<KeyT, ValueT>::get (const KeyT &key, ValueT *pval)
{
  MapT::iterator i;
  i = map_.find (key); // try to use just &key and look at error message !!!
  if (i != map_.end ()) {
    // Found in cache
    *pval = (*i).second->val;
    (*i).second->move_front (lru_);
  } else {
    // Ask next level
    bool res = next_->get (key, pval);
    if (!res) return false;
    RecordT *rec = reserve ();
    rec->key = key;
    rec->val = *pval;
    map_[key] = rec;
  }
  return true;
}

template <class KeyT, class ValueT>
void MemCache<KeyT, ValueT>::forall (const KeyT &key, EnumCallBack_t ecb, void *pData)
{
  // Flush buffer
  this->flush ();
  // Pass this to underlying level
  // ??? substitute original ecb with wrapper that stores picked values ?
  next_->forall (key, ecb, pData);
}

template <class KeyT, class ValueT>
bool MemCache<KeyT, ValueT>::set (const KeyT &key, const ValueT &val)
{
  MapT::iterator i;
  i = map_.find (key);
  if (i != map_.end ()) {
    // Update
    (*i).second->val = val;
    (*i).second->dirty = true;
    (*i).second->move_front (lru_);
  } else {
    // New key-value pair
    RecordT *rec = reserve ();
    rec->key = key;
    rec->val = val;
    rec->dirty = true;
    map_[key] = rec;
  }
  return true;
}

template <class KeyT, class ValueT>
void MemCache<KeyT, ValueT>::erase (const KeyT &key)
{
  // find the record in lru and remove
  RecordT *rec = lru_.next;
  for (; rec != &lru_; rec = rec->next) {
    if (rec->key == key) {
      rec->erase ();
      break;
    }
  }
  // remove from map
  map_.erase (key);
}

template <class KeyT, class ValueT>
bool MemCache<KeyT, ValueT>::flush ()
{
  RecordT *rec = lru_.next;
  // Set all dirty records in underlying level
  for (; rec != &lru_; rec = rec->next) {
    if (rec->dirty && !next_->set (rec->key, rec->val)) return false;
    else rec->dirty = false;
  }
  if (next_) return next_->flush ();
  else return true;
}

template <class KeyT, class ValueT>
bool MemCache<KeyT, ValueT>::setSize (long size)
{
  if (size < 0) size = 0;
  if (size < size_) {
    // Shrink
    while (storeSize_ > size) {
      RecordT *rec = lru_.prev;
      if (rec->dirty && !next_->set (rec->key, rec->val)) return false;
      rec->erase ();
      --storeSize_;
    }
  }
  size_ = size;
  return true;
}

template <class KeyT, class ValueT>
bool MemCache<KeyT, ValueT>::setPurgeSize (long purgeSize)
{
  if (purgeSize < 0) purgeSize = 0;
  else if (purgeSize > size_) purgeSize = size_;
  purgeSize_ = purgeSize;
  return true;
}

template <class KeyT, class ValueT>
long MemCache<KeyT, ValueT>::getSize ()
{
  return size_;
}
