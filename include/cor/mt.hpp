#ifndef _COR_MT_HPP_
#define _COR_MT_HPP_

#include <mutex>
#include <memory>
#include <thread>
#include <condition_variable>

namespace cor {

struct WLock
{
    template <typename T>
    typename T::WLock operator ()(T const &m) { return typename T::WLock(m); }
};

struct RLock
{
    template <typename T>
    typename T::RLock operator ()(T const &m) { return typename T::RLock(m); }
};

template <typename T>
typename T::WLock wlock(T const &m) { return WLock()(m); }

template <typename T>
typename T::RLock rlock(T const &m) { return RLock()(m); }

class Mutex
{
public:

    Mutex() {}
    virtual ~Mutex() {}

    class WLock
    {
    public:
        WLock(Mutex const &m);
        WLock(WLock &&from);
        ~WLock();
        void unlock();
    private:
        Mutex const &m_;
        bool is_locked_;
    };

    friend class Mutex::WLock;
    typedef Mutex::WLock RLock;

private:
    mutable std::mutex l_;
};


struct NoLock
{
    struct RLock
    {
        RLock(NoLock const&) { }
        void unlock() {}
        RLock(RLock&&) { }
    };

    typedef RLock WLock;
};

/// Condition variables are frequently used for a simple pattern with
/// a single mutex. Class wraps this logic. Object is created by one
/// who is waiting
class BasicCondition
{
public:
    BasicCondition() : awake_(false) {}

    template<class Rep, class Period>
    std::cv_status wait(std::chrono::duration<Rep, Period> const& rel_time)
    {
        std::unique_lock<std::mutex> lock_(mutex_);
        return (awake_
                ? std::cv_status::no_timeout
                : done_.wait_for(lock_, rel_time));
    }

    void done()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        awake_ = true;
        done_.notify_one();
    }

private:
    BasicCondition(BasicCondition const&);
    BasicCondition& operator =(BasicCondition const&);

    std::mutex mutex_;
    std::condition_variable done_;
    bool awake_;
};


/***
 * universal future implementation can be used with different kind of
 * threads (not only c++11 but also e.g. with Qt etc.)
 *
 * @todo ability to return value of executed function
 */
class Future
{
public:
    Future() : cond(new BasicCondition()) {}

    /**
     * wrap passed function to be executed as a future. So, Future
     * creator can wait() until function will be executed in another
     * thread
     *
     * @return wrapper function to be passed to another thread
     */
    template <typename ExecutableT>
    std::function<void()> wrap(ExecutableT fn)
    {
        return wrap(fn, [](){});
    }

    template <typename BeforeT, typename AfterT>
    std::function<void()> wrap(BeforeT before, AfterT after)
    {
        auto c = cond;
        return [c, before, after]() {
            before();
            c->done();
            after();
        };
    }

    std::function<void()> waker()
    {
        auto c = cond;
        return [c]() {
            c->done();
        };
    }

    template<class Rep, class Period>
    std::cv_status wait(std::chrono::duration<Rep, Period> const& rel_time) {
        return cond->wait(rel_time);
    }

private:
    Future(Future const&);
    Future& operator =(Future const&);

    std::shared_ptr<BasicCondition> cond;
};


} // cor

#endif // _COR_MT_HPP_
