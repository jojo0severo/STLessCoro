#include "utils.hpp"

#include <memory>


class Result {
    /**
     * Helper class to reduce code duplication when casting from void* to T
     *
     * */
public:
    /**
     * Get the result casted to the expected type
     * */
    template<typename T>
    ALWAYS_INLINE T result() {
        return *std::static_pointer_cast<T>(m_data).get();
    }

    Result(std::shared_ptr<void> &data) noexcept: m_data{data} {}

    ~Result() {
        m_data.reset();
    }

private:
    std::shared_ptr<void> m_data;  /// Void pointer with the returned value from coroutine
};