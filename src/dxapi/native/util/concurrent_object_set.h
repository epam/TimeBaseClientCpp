#pragma once

#include "platform/platform.h"
#include "util/srw_lock.h"
#include <unordered_set>

namespace DxApiImpl {
    class concurrent_ptr_set_iter;

    class concurrent_ptr_set {
        friend class concurrent_ptr_set_iter;

    public:
        void add(void * v)
        {
            dx_thread::srw_write section(thisLock_);
            if (data_.end() == data_.find(v)) {
                data_.emplace(v);
            }
        }

        size_t size()
        {
            dx_thread::srw_read section(thisLock_);
            return data_.size();
        }


        void remove(void * v)
        {
            dx_thread::srw_write section(thisLock_);
            data_.erase(v);
        }


        template<typename T> T * find(T * v)
        {
            dx_thread::srw_read section(thisLock_);
            return data_.end() == data_.find(v) ? NULL : v;
        }


        // Caution!
        dx_thread::srw_lock& lock() { return thisLock_; }


        void lock_read()
        {
            thisLock_.get_shared();
        }


        void lock_write()
        {
            thisLock_.get_exclusive();
        }


        void unlock_read()
        {
            thisLock_.release_shared();
        }


        void unlock_write()
        {
            thisLock_.release_exclusive();
        }

    protected:
        dx_thread::srw_lock thisLock_;
        std::unordered_set<void *> data_;
    };

    class concurrent_ptr_set_iter {
    public:
        const void * next()
        {
            void * x = NULL;
            for (void * i : data_) {
                x = i;
                break;
            }

            if (NULL != x) {
                data_.erase(x);
            }

            return x;
        }


        size_t size()
        {
            return data_.size();
        }


        concurrent_ptr_set_iter(concurrent_ptr_set &o)
        {
            o.lock_read();
            data_ = o.data_;
            o.unlock_read();
        }

    protected:
        std::unordered_set<void *> data_;
    };


    template<typename T> class concurrent_obj_set {
        template<typename TT> friend class concurrent_obj_set_iter;

    public:
        void add(const T &v)
        {
            dx_thread::srw_write section(thisLock_);
            if (data_.end() == data_.find(v)) {
                data_.emplace(v);
            }
        }


        size_t size()
        {
            dx_thread::srw_read section(thisLock_);
            return data_.size();
        }


        void clear()
        {
            dx_thread::srw_write section(thisLock_);
            data_.clear();
        }

        void remove(const T &v)
        {
            dx_thread::srw_write section(thisLock_);
            data_.erase(v);
        }


        const T* find(const T &v)
        {
            dx_thread::srw_read section(thisLock_);
            return data_.end() == data_.find(v) ? NULL : &v;
        }


        // Caution!
        dx_thread::srw_lock& lock() { return thisLock_; }


        void lock_read()
        {
            thisLock_.get_shared();
        }


        void lock_write()
        {
            thisLock_.get_exclusive();
        }


        void unlock_read()
        {
            thisLock_.release_shared();
        }


        void unlock_write()
        {
            thisLock_.release_exclusive();
        }

    protected:
        dx_thread::srw_lock thisLock_;
        std::unordered_set<T> data_;
    };

    template<typename T> class concurrent_obj_set_iter {
    public:
        bool next(T& to)
        {
            const T * x = NULL;
            for (auto const &i : data_) {
                x = &i;
                to = i;
                break;
            }

            if (NULL != x) {
                data_.erase(*x);
                return true;
            }

            return false;
        }


        size_t size()
        {
            return data_.size();
        }


        concurrent_obj_set_iter(concurrent_obj_set<T> &o)
        {
            o.lock_read();
            data_ = o.data_;
            o.unlock_read();
        }

    protected:
        std::unordered_set<T> data_;
    };

};