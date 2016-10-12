#ifndef __LOCK_UTIL_H__
#define __LOCK_UTIL_H__

#pragma once


#include <mutex>


template <typename T>
class Locked
{
   private:
    T* _ele;

    void lock()
    {
        if (_ele)
            _ele->lock();
    }

    void unlock()
    {
        if (_ele)
            _ele->unlock();
    }

   public:
    Locked(T* ele) :
        _ele{ele}
    {
        lock();
    }

    Locked(const Locked&) = delete;
    Locked(Locked&& o) :
        _ele{o._ele}
    {
        o._ele = nullptr;
    }

    ~Locked()
    {
        unlock();
    }

    Locked& operator=(const Locked&) = delete;
    Locked& operator=(Locked&& o)
    {
        unlock();

        _ele = o._ele;
        o._ele = nullptr;

        return *this;
    }

    T& operator*()
    {
        return *_ele;
    }

    const T& operator*() const
    {
        return *_ele;
    }

    T* operator->()
    {
        return _ele;
    }

    const T* operator->() const
    {
        return _ele;
    }
};

template <typename T>
class Lockable
{
   private:
    std::mutex _m;

    void lock()
    {
        _m.lock();
    }

    void unlock()
    {
        _m.unlock();
    }

    bool try_lock()
    {
        _m.try_lock();
    }

    friend class Locked<T>;

   public:
    virtual ~Lockable() = default;

    Locked<T> locked()
    {
        /* We need to dynamically downcast here. */
        return Locked<T>{dynamic_cast<T*>(this)};
    }
};



#endif /* __LOCK_UTIL_H__ */
