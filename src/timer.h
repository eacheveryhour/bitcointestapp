#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <functional>
#include <thread>

class Timer {
public:

  Timer(std::chrono::seconds Delay, std::function<void ()> Callback, bool Asynchronous = true, bool Repeat = true)
  {
      Add(Delay, Callback, Asynchronous, Repeat);
  }

  void Add(std::chrono::seconds Delay, std::function<void ()> Callback, bool Asynchronous = true, bool Repeat = true)
  {
      if (Asynchronous)
      {
        std::thread([=]()
            {
                while(Repeat)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(Delay));
                    Callback();
                }

                std::this_thread::sleep_for(std::chrono::seconds(Delay));
                Callback();
            }
        ).detach();
      }
      else
      {
          while(Repeat)
          {
              std::this_thread::sleep_for(std::chrono::seconds(Delay));
              Callback();
          }

          std::this_thread::sleep_for(std::chrono::seconds(Delay));
          Callback();
      }
  }
};

#endif // TIMER_H
