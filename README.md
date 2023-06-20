# ARC Smart Pointer

This is an implementation of the ARC (Atomic Reference Counting) smart pointer in C++. The ARC smart pointer provides shared ownership of an object using reference counting. It allows multiple pointers to refer to the same object, and the object is deleted only when the last smart pointer referencing it goes out of scope.

**Note:** This implementation is my personal implementation and may not be 100% accurate or secure. It was developed as a learning exercise to understand the concepts and inner workings of reference counting pointers.

## Usage

The ARC smart pointer is provided through two classes: `Arc` and `WeakArc`.

### Arc

The `Arc` class represents a strong reference to an object. It provides the following methods:

#### Constructor

```cpp
explicit Arc(T *ptr)
```

- Creates a new `Arc` instance that manages the given raw pointer `ptr`.

#### Copy Constructor

```cpp
Arc(const Arc &other)
```

- Creates a new `Arc` instance as a copy of the `other` `Arc` instance.
- Increments the reference count of the control block.

#### Assignment Operator

```cpp
Arc &operator=(const Arc &other)
```

- Assigns the `other` `Arc` instance to the current `Arc` instance.
- Decrements the reference count of the current control block and increments the reference count of the `other` control block.
- Properly handles self-assignment.

#### Destructor

```cpp
~Arc()
```

- Decrements the reference count of the control block.
- Releases the control block if the reference count reaches zero.

#### Get Raw Pointer

```cpp
T *get() const
```

- Returns the raw pointer to the managed object.

#### Get Mutex

```cpp
std::shared_mutex *mutex()
```

- Returns a pointer to the `shared_mutex` associated with the control block.

#### Clone

```cpp
Arc clone() const
```

- Creates a new `Arc` instance as a clone of the current `Arc` instance.
- Acquires a shared lock on the control block's mutex.

#### Get Mutable Access

```cpp
template <typename U> friend U *get_mut(Arc<U> &arc)
```

- Allows mutable access to the managed object by acquiring a lock on the associated mutex.

### WeakArc

The `WeakArc` class represents a weak reference to an object managed by `Arc`. It provides the following methods:

#### Constructor

```cpp
explicit WeakArc(Arc<T> &arc)
```

- Creates a new `WeakArc` instance from an `Arc` instance.

#### Copy Constructor

```cpp
WeakArc(const WeakArc &other)
```

- Creates a new `WeakArc` instance as a copy of the `other` `WeakArc` instance.
- Increments the reference count of the control block.

#### Assignment Operator

```cpp
WeakArc &operator=(const WeakArc &other)
```

- Assigns the `other` `WeakArc` instance to the current `WeakArc` instance.
- Decrements the reference count of the current control block and increments the reference count of the `other` control block.

#### Destructor

```cpp
~WeakArc()
```

- Decrements the reference count of the control block.
- Releases the control block if the reference count reaches zero.

#### Upgrade

```cpp
Arc<T> upgrade() const
```

- Upgrades the `WeakArc` to an `Arc` instance if the object still exists.
- Returns the upgraded `Arc` instance or `nullptr` if the object has been deleted.
