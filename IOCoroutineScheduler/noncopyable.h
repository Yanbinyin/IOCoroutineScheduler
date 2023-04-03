/**
 * @file noncopyable.h
 * @author yinyb (990900296@qq.com)
 * @brief 禁止拷贝类
 * @version 1.0
 * @date 2022-04-03
 * @copyright Copyright (c) {2022}
 */

#ifndef __BIN_NONCOPYABLE_H__
#define __BIN_NONCOPYABLE_H__

namespace bin {

/**
 * @brief objects that can`t be copied
 */
class Noncopyable {
public:
  Noncopyable() = default;
  ~Noncopyable() = default;

  Noncopyable(const Noncopyable &) = delete;
  Noncopyable &operator=(const Noncopyable &) = delete;
};

} // namespace bin

#endif
