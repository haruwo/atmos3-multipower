#ifndef HEADER_7075ab2db3a7279e55c075b6010ee5e0
#define HEADER_7075ab2db3a7279e55c075b6010ee5e0

#include <functional>
#include <type_traits>
#include <utility>

class defer_t {
private:
  std::function<void(void)> f;
public:
  template<class T, class = typename std::enable_if<std::is_void<decltype((std::declval<T>())())>::value>::type>
  defer_t(T const& f) : f(f) {}
  ~defer_t(void) { f(); }
};

#define defer_helper2(line) defer_tmp ## line
#define defer_helper(line) defer_helper2(line)
#define defer defer_t defer_helper(__LINE__) = [&](void)->void

#endif
