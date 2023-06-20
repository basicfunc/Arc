/******************************************************************************

  ARC Smart Pointer Header
  -------------------------

  Copyright 2023 Rahul

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

******************************************************************************/

#ifndef ARC_H
#define ARC_H

#include <atomic>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <vector>

/**
 * @brief ARC (Atomic Reference Counting) Smart Pointer Implementation
 *
 * This header file contains the implementation of ARC (Atomic Reference
 * Counting) smart pointer. It provides two classes: `Arc` and `WeakArc`, which
 * enable shared ownership and weak references to an object, respectively.
 *
 * The ARC smart pointer uses a reference count to keep track of the number of
 * references to an object and deletes the object when the reference count
 * reaches zero.
 */

// Forward declaration of WeakArc class
template <typename T> class WeakArc;

/**
 * @brief Arc class
 *
 * The Arc class provides shared ownership of an object. It uses reference
 * counting to manage the object's lifetime. Multiple Arc instances can point to
 * the same object, and the object is deleted when the last Arc instance goes
 * out of scope.
 *
 * @tparam T The type of the object being managed.
 */
template <typename T> class Arc {
private:
  /**
   * @brief Struct representing the control block for an Arc instance.
   *
   * The control block holds the reference count, a mutex for thread safety,
   * and a shared pointer to the data.
   */
  struct ArcControlBlock {
    std::atomic<int> ref_count; // Reference count
    std::shared_mutex mutex;    // Mutex for thread-safe access
    std::shared_ptr<T> data;    // Shared pointer to data

    /**
     * @brief Constructor to initialize the control block.
     *
     * @param ptr Pointer to the object being managed by Arc.
     */
    ArcControlBlock(T *ptr) : ref_count(1), data(ptr, [](T *p) { delete p; }) {}
  };

  // Friend declarations
  friend struct WeakArcControlBlock; // Allow access to WeakArcControlBlock
  friend class WeakArc<T>;           // Allow access to WeakArc class

  ArcControlBlock *control_block; // Pointer to the control block

public:
  /**
   * @brief Constructor to create an Arc instance.
   *
   * @param ptr Pointer to the object being managed by Arc.
   */
  explicit Arc(T *ptr) : control_block(new ArcControlBlock(ptr)) {}

  /**
   * @brief Copy constructor.
   *
   * Increments the reference count of the control block.
   *
   * @param other The Arc instance to copy.
   */
  Arc(const Arc &other) : control_block(other.control_block) {
    control_block->ref_count.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @brief Assignment operator.
   *
   * Decrements the reference count of the current control block and acquires
   * locks on both the current and other control blocks. Then assigns the other
   * control block and increments its reference count.
   *
   * @param other The Arc instance to assign.
   * @return Reference to the assigned Arc instance.
   */
  auto operator=(const Arc &other) {
    if (this != &other) {
      std::lock(control_block->mutex, other.control_block->mutex);
      std::lock_guard<std::shared_mutex> self_lock(control_block->mutex,
                                                   std::adopt_lock);
      std::lock_guard<std::shared_mutex> other_lock(other.control_block->mutex,
                                                    std::adopt_lock);

      release();
      control_block = other.control_block;
      control_block->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
    return *this;
  }

  /**
   * @brief Destructor.
   *
   * Decrements the reference count of the control block and releases the
   * control block if the reference count reaches zero.
   */
  ~Arc() { release(); }

  /**
   * @brief Get the raw pointer to the managed object.
   *
   * @return Pointer to the managed object.
   */
  auto get() const { return control_block->data.get(); }

  /**
   * @brief Get the mutex associated with the control block.
   *
   * @return Pointer to the shared_mutex object.
   */
  auto mutex() { return &control_block->mutex; }

  /**
   * @brief Create a clone of the Arc object.
   *
   * Acquires a shared lock on the control block's mutex and creates a new
   * Arc object with the same control block.
   *
   * @return Cloned Arc object.
   */
  auto clone() const {
    std::shared_lock<std::shared_mutex> lock(control_block->mutex);
    return Arc(*this);
  }

  /**
   * @brief Get mutable access to the data.
   *
   * Allows a mutable reference to the data by acquiring a lock on the
   * associated mutex.
   *
   * @tparam U Type of the object.
   * @param arc The Arc instance.
   * @return Mutable pointer to the data.
   */
  template <typename U> friend U *get_mut(Arc<U> &arc);

private:
  /**
   * @brief Release the Arc's control block and delete it if necessary.
   *
   * Decrements the reference count of the control block and deletes the control
   * block if the reference count reaches zero.
   */
  void release() {
    if (control_block &&
        control_block->ref_count.fetch_sub(1, std::memory_order_release) == 1) {

      std::atomic_thread_fence(std::memory_order_acquire);
      std::lock_guard<std::shared_mutex> lock(control_block->mutex);

      if (control_block->ref_count.load(std::memory_order_relaxed) == 0) {
        control_block->data.reset();
        delete control_block;
      }
    }
  }
};

/**
 * @brief Get mutable access to the data.
 *
 * Allows a mutable reference to the data by acquiring a lock on the associated
 * mutex.
 *
 * @tparam U Type of the object.
 * @param arc The Arc instance.
 * @return Mutable pointer to the data.
 */
template <typename U> auto get_mut(Arc<U> &arc) {
  std::lock_guard<std::shared_mutex> lock(*arc.mutex());
  return arc.get();
}

/**
 * @brief WeakArc class
 *
 * The WeakArc class provides weak references to an object managed by Arc.
 * It allows observing the object without keeping it alive. WeakArc can be
 * upgraded to an Arc if the object still exists.
 *
 * @tparam T The type of the object being managed.
 */
template <typename T> class WeakArc {
private:
  /**
   * @brief Struct representing the control block for a WeakArc instance.
   *
   * The control block holds the reference count and a weak pointer to the data.
   */
  struct WeakArcControlBlock {
    std::atomic<int> ref_count; // Reference count
    std::weak_ptr<T> data;      // Weak pointer to data

    /**
     * @brief Constructor to initialize the control block from an Arc instance.
     *
     * @param arc The Arc instance.
     */
    WeakArcControlBlock(Arc<T> &arc)
        : ref_count(1), data(arc.control_block->data) {}
  };

  WeakArcControlBlock *control_block; // Pointer to the control block

public:
  /**
   * @brief Constructor to create a WeakArc instance from an Arc instance.
   *
   * @param arc The Arc instance.
   */
  explicit WeakArc(Arc<T> &arc) : control_block(new WeakArcControlBlock(arc)) {}

  /**
   * @brief Copy constructor.
   *
   * Increments the reference count of the control block.
   *
   * @param other The WeakArc instance to copy.
   */
  WeakArc(const WeakArc &other) : control_block(other.control_block) {
    control_block->ref_count.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @brief Assignment operator.
   *
   * Decrements the reference count of the current control block and assigns
   * the other control block, incrementing its reference count.
   *
   * @param other The WeakArc instance to assign.
   * @return Reference to the assigned WeakArc instance.
   */
  auto operator=(const WeakArc &other) {
    if (this != &other) {
      control_block = other.control_block;
      control_block->ref_count.fetch_add(1, std::memory_order_relaxed);
    }
    return *this;
  }

  /**
   * @brief Destructor.
   *
   * Decrements the reference count of the control block and releases the
   * control block if the reference count reaches zero.
   */
  ~WeakArc() { release(); }

  /**
   * @brief Upgrade the WeakArc to a corresponding Arc instance.
   *
   * Attempts to upgrade the WeakArc to an Arc instance. If the object still
   * exists, an Arc instance pointing to the same object is returned. If the
   * object has been deleted, a nullptr is returned.
   *
   * @return Upgraded Arc instance or nullptr.
   */
  auto upgrade() const {
    std::shared_ptr<T> shared_ptr = control_block->data.lock();
    if (shared_ptr) {
      return Arc<T>(shared_ptr.get());
    } else {
      return Arc<T>(nullptr);
    }
  }

private:
  /**
   * @brief Release the WeakArc's control block and delete it if necessary.
   *
   * Decrements the reference count of the control block and deletes the control
   * block if the reference count reaches zero.
   */
  void release() {
    if (control_block) {
      if (control_block->ref_count.fetch_sub(1, std::memory_order_release) ==
          1) {
        std::atomic_thread_fence(std::memory_order_acquire);
        delete control_block;
      }
    }
  }
};

#endif