#ifndef LRUCACHE_H
#define LRUCACHE_H

#include <cassert>
#include <list>
#include <map>
#include <functional>

// adapted from http://stackoverflow.com/a/25093143/44729
template< typename K, typename V, class Comp = std::less< K > >
class LruCache
{
public:
    LruCache( size_t aCapacity )
        : mCapacity( aCapacity )
    { }

    // insert a new key-value pair in the cache
    bool insert( const K& aKey, const V& aValue )
    {
        if( mCache.find( aKey ) != mCache.end() )
            return false;

        // make space if necessary
        if( mList.size() == mCapacity )
        {
            // Purge the least-recently used element in the cache
            // identify least-recently-used key
            const typename Cache::iterator it = mCache.find( mList.front() );

            //erase both elements to completely purge record
            mCache.erase( it );
            mList.pop_front();
        }

        // record k as most-recently-used key
        typename List::iterator it = mList.insert( mList.end(), aKey );

        // create key-value entry, linked to the usage record
        mCache.insert( std::make_pair( aKey, std::make_pair( aValue, it ) ) );
        return true;
    }

    bool get( V& aValue, const K& aKey, const bool updateUsage = true )
    {
        // cache-miss: did not find the key
        typename Cache::iterator it = mCache.find( aKey );
        if( it == mCache.end() )
            return false;

        if( updateUsage )
        {
            // cache-hit
            // Update access record by moving accessed key to back of the list
            mList.splice( mList.end(), mList, (it)->second.second );
        }

        // return the retrieved value
        aValue = (it)->second.first;
        return true;
    }

    void clear()
    {
        mCache.clear();
        mList.clear();
    }

private:
    size_t mCapacity;

    // Key access history, most recent at back
    typedef std::list< K > List;
    List mList;

    // Key to value and key history iterator
    typedef std::map< K, std::pair< V, typename List::iterator >, Comp > Cache;
    Cache mCache;
};

#endif
