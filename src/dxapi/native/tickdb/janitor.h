#pragma once

#include "tickdb/common.h"
#include "util/critical_section.h"
#include <thread>
#include <unordered_map>

namespace DxApiImpl {

    template<int L> class StrConcatenator {

    protected:

    public: // TODO: temp
        const char *ptr_[L];
    };

    INLINE StrConcatenator<1> concat(const char *s0)
    {
        auto c = StrConcatenator<1>();
        c.ptr_[0] = s0;
        return c;
    }

    INLINE StrConcatenator<2> concat(const char *s0, const char *s1)
    {
        auto c = StrConcatenator<2>();
        c.ptr_[0] = s0;
        c.ptr_[1] = s1;
        return c;
    }


    INLINE StrConcatenator<3> concat(const char *s0, const char *s1, const char *s2)
    {
        auto c = StrConcatenator<3>();
        c.ptr_[0] = s0;
        c.ptr_[1] = s1;
        c.ptr_[2] = s2;
        return c;
    }

    class Janitor;
    extern Janitor g_janitor;

    class Janitor {
    public:
        enum class EntityType : uint8_t {
            // Variants
            CALL        = 0,
            COUNTER     = 2,
            GROUP       = 4,
            
            // Flags
            LOGGED = 1,
            UNLISTED = 0x80,

            // Combinations
            LOGGED_CALL = LOGGED | CALL,
            LOGGED_COUNTER = LOGGED | COUNTER,
        };

        class Guard {
            INLINE uint64_t id() const { return id_; }

            template<int L> INLINE Guard(StrConcatenator<L> name, EntityType t = EntityType::CALL) : id_(g_janitor.checkin(name, t)) {}
            template<int L> INLINE Guard(StrConcatenator<L> name, const char * parentName = NULL) : id_(g_janitor.checkin(name, parentName)) {}
            ~Guard() { g_janitor.checkout(id_); }

        private:
            uint64_t id_;
        };

        bool isRunning() const;

        void init();
        void dispose();

        //template<int L> uint64_t checkin(StrConcatenator<L> name);
        //template<int L> uint64_t checkin(StrConcatenator<L> name, EntityType type);
        template<int L> uint64_t checkin(StrConcatenator<L> name, const char * parentName);
        template<int L> uint64_t checkin(StrConcatenator<L> name, uint64_t parentId);

        void checkout(uint64_t id);

        void setType(uint64_t entityType);

        

    protected:
        void threadProcExceptionWrapper();
        void threadProc();
        void threadProcImpl();
        static void threadProcStatic(Janitor &self);

        void stop();
        void stopped();

    protected:
        volatile bool isRunning_;
        volatile bool shouldStop_;
        volatile bool isStopped_;
        dx_thread::critical_section threadObjSection_; // After thread's creation, its pointer and state are modified within this section
        dx_thread::critical_section visitor_map_; // After thread's creation, its pointer and state are modified within this section
        std::thread * thread_;

        std::unordered_map<std::string, uint64_t> visitorName2Id_;
    };
}