
#include "condition-variable.h"
#include <exlib/include/fiber.h>

namespace v8 {
namespace base {

    ConditionVariable::ConditionVariable()
    {
        native_handle_ = new exlib::CondVar();
    }

    ConditionVariable::~ConditionVariable()
    {
        delete native_handle_;
    }

    void ConditionVariable::NotifyOne()
    {
        native_handle_->notify_all();
    }

    void ConditionVariable::NotifyAll()
    {
        native_handle_->notify_all();
    }

    void ConditionVariable::Wait(Mutex* mutex)
    {
        native_handle_->wait(*mutex->native_handle());
    }

    bool ConditionVariable::WaitFor(Mutex* mutex, const TimeDelta& rel_time)
    {
        native_handle_->wait(*mutex->native_handle());
        return true;
    }
}
} // namespace v8::base
