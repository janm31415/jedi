#pragma once

#include <queue>
#include <string>
#include <mutex>

enum async_message_type
  {
  ASYNC_MESSAGE_LOAD
  };

struct async_message
  {
  async_message_type m;
  std::string str;
  };

class async_messages
  {
  public:

    void push(const async_message& m)
      {
      mut.lock();
      message_queue.push(m);
      mut.unlock();
      }

    async_message pop()
      {
      mut.lock();
      async_message res = message_queue.front();
      message_queue.pop();
      mut.unlock();
      return res;
      }

    bool empty()
      {
      bool res = true;
      if (mut.try_lock())
        {
         res = message_queue.empty();
         mut.unlock();
        }
      return res;
      }

  private:
    std::queue<async_message> message_queue;
    std::mutex mut;
  };