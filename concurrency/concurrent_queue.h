#ifndef NUTSHELL_ConcurrentQueue_H
#define NUTSHELL_ConcurrentQueue_H

#include <atomic>
#include <memory>
#include <condition_variable>

template <typename T>
class ConcurrentQueue {
 public:
  ConcurrentQueue():
    head_(new Node), tail_(head_.get()) {}

  ConcurrentQueue(const ConcurrentQueue& other)=delete;
  ConcurrentQueue& operator=(const ConcurrentQueue& other)=delete;

  void push(T new_value);

  std::shared_ptr<T> try_pop() {
    std::unique_ptr<Node> old_head = try_pop_head();
    return old_head ? old_head->data : std::shared_ptr<T>();
  }

  bool try_pop(T& value) {
    std::unique_ptr<Node> old_head = try_pop_head();
    return old_head;
  }

  std::shared_ptr<T> wait_and_pop() {
    std::unique_ptr<Node> const old_head = wait_pop_head();
    return old_head->data;
  }

  void wait_and_pop(T& value) {
    std::unique_ptr<Node> const old_head = wait_pop_head(value);
  }

  bool empty() const {
    std::lock_guard<std::mutex> head_lock(head_mutex_);
    return head_.get() == get_tail();
  }

  
 private:
  struct Node {
    std::shared_ptr<T> data;
    std::unique_ptr<Node> next;    // Current node manages the lifetime of the next node
  };

  Node* get_tail() {
    std::lock_guard<std::mutex> tail_lock(tail_mutex_);
    return tail_;
  }

  std::unique_ptr<Node> pop_head() {
    std::unique_ptr<Node> old_head = std::move(head_);
    head_ = std::move(old_head->next);
    return old_head;
  }

  // Waiting until data is available
  std::unique_lock<std::mutex> wait_for_data() {
    std::unique_lock<std::mutex> head_lock(head_mutex_);
    data_cond_.wait(head_lock, [&]{return head_.get() != get_tail();});
    return std::move(head_lock);
  }

  std::unique_ptr<Node> wait_pop_head() {
    std::unique_lock<std::mutex> head_lock(wait_for_data());
    return pop_head();
  }

  std::unique_ptr<Node> wait_pop_head(T& value) {
    std::unique_lock<std::mutex> head_lock(wait_for_data());
    value = std::move(*head_->data);
    return pop_head();
  }

  std::unique_ptr<Node> try_pop_head() {
    std::lock_guard<std::mutex> head_lock(head_mutex_);
    if (head_.get() == get_tail()) {
      return std::unique_ptr<Node>();
    }
    return pop_head();
  }

  std::unique_ptr<Node> try_pop_head(T& value) {
    std::lock_guard<std::mutex> head_lock(head_mutex_);
    if (head_.get() == get_tail()) {
      return std::unique_ptr<Node>();
    }
    value = std::move(*head_->data);
    return pop_head();
  }

  std::mutex head_mutex_;
  std::unique_ptr<Node> head_;

  std::mutex tail_mutex_;
  Node *tail_;                     // Always holds empty data and points to a dummy next

  std::condition_variable data_cond_;
};

template <typename T>
void ConcurrentQueue<T>::push(T new_value) {
  std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
  std::unique_ptr<Node> p(new Node);

  {
    std::lock_guard<std::mutex> tail_lock(tail_mutex_);

    // Set the data of the tail to the new value
    tail_->data = new_data;

    // Link a dummy tail into the end of the queue
    Node* const new_tail = p.get();
    tail_->next = std::move(p);
    tail_ = new_tail;
  }

  data_cond_.notify_one();
}

#endif